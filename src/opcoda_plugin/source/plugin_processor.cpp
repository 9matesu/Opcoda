#include "plugin_processor.h"

#include "factory_presets.h"

#include "opcoda_core/dsp/biquad.h"
#include "opcoda_core/dsp/lfo.h"
#include "opcoda_core/dsp/window.h"
#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/byte_to_sample.h"
#include "opcoda_core/pe/column_reduction.h"
#include "opcoda_core/pe/pe_parser.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace opcoda {

PluginProcessor::PluginProcessor()
: juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "Opcoda", createParameterLayout()) {
    jassert(getParameters().size() == 66);

    for (int i = 0; i < dsp::kMaxModRoutes; ++i) {
        const auto n = juce::String {i + 1};
        const auto source = parameters_.getRawParameterValue(("m" + n + "src").toRawUTF8());
        const auto destination =
            parameters_.getRawParameterValue(("m" + n + "dst").toRawUTF8());
        const auto amount = parameters_.getRawParameterValue(("m" + n + "amt").toRawUTF8());
        jassert(source != nullptr && destination != nullptr && amount != nullptr);
        routeRaw_[static_cast<std::size_t>(i)] = {source, destination, amount};
    }

    migrateLegacyModMatrix();
}

dsp::ModMatrix PluginProcessor::readModMatrix() const noexcept {
    dsp::ModMatrix matrix;
    for (int i = 0; i < dsp::kMaxModRoutes; ++i) {
        const auto& raw = routeRaw_[static_cast<std::size_t>(i)];
        const auto source =
            dsp::sanitizeModSource(static_cast<int>(raw.source->load(std::memory_order_relaxed) + 0.5f));
        const auto destination =
            dsp::sanitizeModDest(static_cast<int>(raw.destination->load(std::memory_order_relaxed) + 0.5f));
        const auto amount =
            dsp::sanitizeModAmount(raw.amount->load(std::memory_order_relaxed));
        if (source != dsp::ModSource::kNone && destination != dsp::ModDest::kNone) {
            matrix.routes[static_cast<std::size_t>(i)] = {source, destination, amount};
        }
    }
    return matrix;
}

void PluginProcessor::migrateLegacyModMatrix() {
    const auto node = parameters_.state.getChildWithName(kModMatrixNode);
    if (!node.isValid()) {
        return;
    }

    const int count = juce::jmin(node.getNumChildren(), dsp::kMaxModRoutes);
    for (int i = 0; i < count; ++i) {
        const auto child = node.getChild(i);
        const auto n = juce::String {i + 1};
        const auto set = [this](const char* id, float plain) {
            if (auto* parameter = parameters_.getParameter(id)) {
                parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
            }
        };
        set(("m" + n + "src").toRawUTF8(),
            static_cast<float>(static_cast<int>(child.getProperty("source"))));
        set(("m" + n + "dst").toRawUTF8(),
            static_cast<float>(static_cast<int>(child.getProperty("dest"))));
        set(("m" + n + "amt").toRawUTF8(),
            static_cast<float>(static_cast<int>(child.getProperty("amount"))) / 1000.0f);
    }

    parameters_.state.removeChild(node, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout() {
    // Automatizacao: so o que e' continuo e vale a pena gravar numa automacao.
    // Escolhas, interruptores e rotas discretas ficam de fora (rumo no Live).
    // Os nomes levam a unidade, e os valores saem formatados para o host.
    const auto autoAttr = [](bool automatable) {
        return automatable ? juce::AudioParameterFloatAttributes {}
                           : juce::AudioParameterFloatAttributes {}.withAutomatable (false);
    };

    const auto range = [&autoAttr](const char* id, const char* name, float lo, float hi,
                                   float def, const char* unit = nullptr,
                                   bool automatable = true) {
        auto attributes = autoAttr (automatable);
        if (unit != nullptr) {
            attributes = attributes.withLabel (unit);
        }
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {id, 1}, juce::String {name},
            juce::NormalisableRange<float> {lo, hi}, def, attributes);
    };

    const auto intRange = [](const char* id, const char* name, int lo, int hi, int def,
                             bool automatable = true) {
        auto attributes = juce::AudioParameterIntAttributes {};
        if (!automatable) {
            attributes = attributes.withAutomatable (false);
        }
        return std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID {id, 1}, juce::String {name}, lo, hi, def, attributes);
    };

    const auto boolParam = [](const char* id, const char* name, bool def,
                              const char* onText, const char* offText) {
        auto attributes = juce::AudioParameterBoolAttributes {};
        if (onText != nullptr) {
            attributes = attributes.withStringFromValueFunction (
                [onText, offText](float value, int) {
                    return juce::String {value > 0.5f ? onText : offText};
                });
        }
        return std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID {id, 1}, juce::String {name}, def, attributes);
    };

    const auto logCutoff = [](const char* id, const char* name, float def) {
        juce::NormalisableRange<float> cutoff {20.0f, 20000.0f};
        cutoff.setSkewForCentre (1000.0f);
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {id, 1}, juce::String {name}, cutoff, def,
            juce::AudioParameterFloatAttributes {}.withLabel ("Hz"));
    };

    const auto grainSize = [] {
        juce::NormalisableRange<float> size {dsp::GranularEngine::kMinGrainMs,
                                            dsp::GranularEngine::kMaxGrainMs};
        size.setSkewForCentre (100.0f);
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {"grain", 1}, "Grain Size (ms)", size, 40.0f,
            juce::AudioParameterFloatAttributes {}.withLabel ("ms"));
    };

    const auto choice = [](const char* id, const char* name, juce::StringArray options, int def,
                           bool automatable = false) {
        auto attributes = juce::AudioParameterChoiceAttributes {};
        if (!automatable) {
            attributes = attributes.withAutomatable (false);
        }
        return std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID {id, 1}, juce::String {name}, std::move (options), def,
            attributes);
    };

    juce::AudioProcessorValueTreeState::ParameterLayout layout {
        grainSize(),
        range("density", "Density (grains/s)", 1.0f, 200.0f, 20.0f, "grains/s"),
        range("position", "Position", 0.0f, 1.0f, 0.5f),
        range("spray", "Spray", 0.0f, 1.0f, 0.0f),
        range("pitch", "Pitch (semitones)", -48.0f, 48.0f, 0.0f, "st"),
        range("volume", "Volume (dB)", -60.0f, 0.0f, 0.0f, "dB"),
        choice("window", "Window", {"Hann", "Gaussian", "Hamming", "Blackman"}, 0),
        range("pan", "Pan", -1.0f, 1.0f, 0.0f),
        range("grainlevel", "Grain Level", 0.0f, 1.0f, 1.0f),
        range("pitchrand", "Pitch Random (semitones)", 0.0f, 12.0f, 0.0f, "st"),
        range("scanspeed", "Scan Speed (x)", 0.25f, 4.0f, 1.0f, "x"),
        range("attack", "Attack (s)", 0.001f, 2.0f, 0.005f, "s"),
        range("decay", "Decay (s)", 0.001f, 2.0f, 0.1f, "s"),
        range("sustain", "Sustain", 0.0f, 1.0f, 1.0f),
        range("release", "Release (s)", 0.001f, 2.0f, 0.05f, "s"),
        range("atkcurve", "Attack Curve", -1.0f, 1.0f, 0.0f, nullptr, false),
        range("deccurve", "Decay Curve", -1.0f, 1.0f, 0.0f, nullptr, false),
        range("relcurve", "Release Curve", -1.0f, 1.0f, 0.0f, nullptr, false),
        choice("f1type", "Filter 1 Type", {"Low-pass", "High-pass", "Band-pass", "Notch"}, 0),
        logCutoff("f1cutoff", "Filter 1 Cutoff (Hz)", 20000.0f),
        range("f1q", "Filter 1 Q", 0.5f, 12.0f, 0.7071f),
        boolParam("f1slope", "Filter 1 Slope", false, "24 dB/oct", "12 dB/oct"),
        range("f1drive", "Filter 1 Drive (dB)", 0.0f, 24.0f, 0.0f, "dB", false),
        range("f1keytrack", "Filter 1 Keytrack", 0.0f, 1.0f, 0.0f, nullptr, false),
        range("f1env", "Filter 1 Env Amount", -1.0f, 1.0f, 0.0f, nullptr, false),
        choice("f2type", "Filter 2 Type", {"Low-pass", "High-pass", "Band-pass", "Notch"}, 0),
        logCutoff("f2cutoff", "Filter 2 Cutoff (Hz)", 20000.0f),
        range("f2q", "Filter 2 Q", 0.5f, 12.0f, 0.7071f),
        range("lforate", "LFO Rate (Hz)", 0.0f, 20.0f, 1.0f, "Hz"),
        range("lfodepth", "LFO Depth", 0.0f, 1.0f, 0.0f),
        choice("lfotarget", "LFO Target", {"Pitch", "Density", "Cutoff", "Position"}, 0),
        choice("lfowave", "LFO Wave", {"Sine", "Tri", "Saw", "Square", "S&H"}, 0),
        range("glide", "Glide (s)", 0.0f, 2.0f, 0.0f, "s"),
        boolParam("lfosync", "LFO Sync", false, "Sync", "Free"),
        choice("lfodiv", "LFO Division",
               {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                "1/4.", "1/8.", "1/4T", "1/8T", "1/16T"}, 2),
        intRange("lfosteps", "LFO Steps", 1, 16, 16, false),
        range("width", "Stereo Width", 0.0f, 2.0f, 1.0f),
    };

    const juce::StringArray sources {"Off", "LFO", "Envelope", "Velocity", "Key",
                                     "Mod Wheel", "CC74", "CC71", "Entropy"};
    const juce::StringArray destinations {
        "Off",  "Pitch",  "Density", "Position", "Spray",     "Grain Size",
        "Cut 1", "Q 1",   "Cut 2",   "Q 2",      "Pan",       "Grain Level",
        "Drive", "Stereo Width"};
    // src/dst sao discretos: fora da automacao. amount fica, e' o que se grava.
    for (int i = 1; i <= dsp::kMaxModRoutes; ++i) {
        const auto n = juce::String {i};
        const auto srcId = ("m" + n + "src");
        const auto dstId = ("m" + n + "dst");
        const auto amtId = ("m" + n + "amt");
        const auto srcName = ("Route " + n + " Source");
        const auto dstName = ("Route " + n + " Destination");
        const auto amtName = ("Route " + n + " Amount");
        layout.add(choice(srcId.toRawUTF8(), srcName.toRawUTF8(), sources, 0));
        layout.add(choice(dstId.toRawUTF8(), dstName.toRawUTF8(), destinations, 0));
        layout.add(range(amtId.toRawUTF8(), amtName.toRawUTF8(), -1.0f, 1.0f, 0.0f));
    }
    return layout;
}

dsp::GranularParams PluginProcessor::currentParams() const noexcept {
    const auto value = [this](const char* id) {
        return static_cast<float>(*parameters_.getRawParameterValue(id));
    };

    const auto choiceIndex = [&value](const char* id) {
        return static_cast<int>(value(id) + 0.5f);
    };

    dsp::GranularParams params;
    params.grainSizeMs = value("grain");
    params.densityGrainsPerSec = value("density");
    params.position = value("position");
    params.spray = value("spray");
    params.pitchSemitones = value("pitch");
    params.volumeDb = value("volume");
    params.window = dsp::sanitizeWindowType(static_cast<dsp::WindowType>(choiceIndex("window")));
    params.pan = value("pan");
    params.widthStereo = value("width");
    params.grainLevel = value("grainlevel");
    params.pitchRandomSemitones = value("pitchrand");
    params.attackSeconds = value("attack");
    params.decaySeconds = value("decay");
    params.sustainLevel = value("sustain");
    params.releaseSeconds = value("release");
    params.attackCurve = value("atkcurve");
    params.decayCurve = value("deccurve");
    params.releaseCurve = value("relcurve");
    params.f1type = dsp::sanitizeFilterType(static_cast<dsp::FilterType>(choiceIndex("f1type")));
    params.f1cutoffHz = value("f1cutoff");
    params.f1q = value("f1q");
    params.f1Slope = value("f1slope") > 0.5f ? 1 : 0;
    params.f1DriveDb = value("f1drive");
    params.f1Keytrack = value("f1keytrack");
    params.f1Env = value("f1env");
    params.f2type = dsp::sanitizeFilterType(static_cast<dsp::FilterType>(choiceIndex("f2type")));
    params.f2cutoffHz = value("f2cutoff");
    params.f2q = value("f2q");
    params.lforateHz = value("lforate");
    params.lfodepth = value("lfodepth");
    params.lfotarget = dsp::sanitizeLFOTarget(static_cast<dsp::LFOTarget>(choiceIndex("lfotarget")));
    params.lfowave = dsp::sanitizeLFOWave(static_cast<dsp::LFOWave>(choiceIndex("lfowave")));

    // F018: a nota da pilha mono vira transposicao no motor.
    params.noteNumber = notes_.activeNote();
    params.glideSeconds = value("glide");

    params.lfoSync = value("lfosync") > 0.5f;
    params.lfoDiv = dsp::sanitizeLFODiv(choiceIndex("lfodiv"));
    params.lfoSteps = dsp::sanitizeLFOSteps(static_cast<int>(value("lfosteps") + 0.5f));
    params.hostBpm = hostBpm_.load(std::memory_order_relaxed);
    params.hostPpq = hostPpq_.load(std::memory_order_relaxed);

    params.velocity = notes_.velocity();
    params.modWheel = notes_.controller(kModWheelController);
    params.cc74 = notes_.controller(kDensityController);
    params.cc71 = notes_.controller(kPositionController);
    params.modMatrix = readModMatrix();
    return params;
}

void PluginProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    engine_.prepare(sampleRate, maximumExpectedSamplesPerBlock);
    transport_.prepare(sampleRate);

    // analisador de espectro: 512 amostras, Hann, 64 bandas log entre 20 Hz e
    // 20 kHz. Tudo prealocado -- o processBlock nao aloca nada.
    constexpr int kFftOrder {9};
    constexpr int kFftSize {1 << kFftOrder};
    spectrumSampleRate_ = sampleRate;
    spectrumFifo_.assign(static_cast<std::size_t>(kFftSize), 0.0f);
    spectrumFftBuffer_.assign(static_cast<std::size_t>(2 * kFftSize), 0.0f);
    spectrumWindow_.resize(static_cast<std::size_t>(kFftSize));
    for (int i = 0; i < kFftSize; ++i) {
        spectrumWindow_[static_cast<std::size_t>(i)] =
            0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi *
                                   static_cast<float>(i) /
                                   static_cast<float>(kFftSize - 1));
    }

    const auto bins = static_cast<std::size_t>(Telemetry::kSpectrumBins);
    const auto nyquist = 0.5 * sampleRate;
    spectrumBinStart_.assign(bins, 1);
    spectrumBinEnd_.assign(bins, 2);
    for (std::size_t b = 0; b < bins; ++b) {
        const auto f0 = 20.0 * std::pow(1000.0, static_cast<double>(b) / bins);
        const auto f1 = 20.0 * std::pow(1000.0, static_cast<double>(b + 1) / bins);
        auto k0 = static_cast<int>(f0 / nyquist * (kFftSize / 2));
        auto k1 = static_cast<int>(f1 / nyquist * (kFftSize / 2));
        k0 = juce::jmax(1, juce::jmin(k0, kFftSize / 2 - 1));
        k1 = juce::jmax(k0 + 1, juce::jmin(k1, kFftSize / 2));
        spectrumBinStart_[b] = k0;
        spectrumBinEnd_[b] = k1;
    }

    spectrumWrite_ = 0;
    spectrumFill_ = 0;
    for (auto& bin : telemetry_.spectrum) {
        bin.store(-100.0f, std::memory_order_relaxed);
    }
}

void PluginProcessor::releaseResources() {
    engine_.reset();
    engine_.setSource(nullptr, 0);
transport_.reset();
    incoming_.clear();
    returned_.clear();
    owned_.clear();
    live_.store(nullptr, std::memory_order_release);
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool PluginProcessor::ingest(const juce::String& path) {
    std::ifstream file(path.toStdString(), std::ios::binary | std::ios::ate);
    if (!file) {
        lastError_ = "E_OPEN";
        return false;
    }

    const auto end = file.tellg();
    if (end <= 0) {
        lastError_ = "E_EMPTY";
        return false;
    }
    const auto size = static_cast<std::size_t>(end);
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(size);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
        lastError_ = "E_TRUNCATED";
        return false;
    }

    const auto parsed = pe::parse(bytes.data(), size);
    if (!parsed.ok()) {
        lastError_ = juce::String(pe::toString(parsed.error));
        return false;
    }

sourceName_ = juce::File(path).getFileName();
    sourcePath_ = path;
    sourceInfo_.name = sourceName_;
    sourceInfo_.formatTag = "[64-bit PE]";
    sourceInfo_.sizeBytes = static_cast<std::int64_t>(size);

    // bytes crus ficam para o seletor converter qualquer janela sem voltar ao disco.
    sourceBytes_ = std::move(bytes);

    // secoes para o display, uma entrada por secao do PE.
    sourceInfo_.sections.clear();
    sourceInfo_.sections.reserve(parsed.image.numberOfSections);
    for (std::size_t i = 0; i < parsed.image.numberOfSections; ++i) {
        const auto& parsedSection = parsed.image.sections[i];
        SourceInfo::SectionInfo entry;
        entry.name = juce::String::fromUTF8(parsedSection.name,
                                            static_cast<int>(
                                                std::find(parsedSection.name,
                                                          parsedSection.name + 8, '\0')
                                                - parsedSection.name));
        entry.rawOffset = parsedSection.rawOffset;
        entry.rawSize = parsedSection.rawSize;
        entry.entropy = parsedSection.entropy;
        sourceInfo_.sections.push_back(std::move(entry));
    }


    byteRange_ = {};
    lastExactRange_ = {};
    for (const auto& section : parsed.image.sections) {
        if (section.rawSize > 0) {
            byteRange_ = {section.rawOffset,
                          static_cast<std::uint64_t>(section.rawOffset) + section.rawSize};
            lastExactRange_ = byteRange_;
            break;
        }
    }
    if (byteRange_.empty()) {
        byteRange_ = {0, size};
        lastExactRange_ = byteRange_;
    }

    lastError_.clear();

    return publishByteRange();
}

bool PluginProcessor::setByteRange(std::uint64_t start, std::uint64_t end,
                                   bool rememberExactRange) {
    pe::ByteRange next {start, end};
    if (!pe::clampByteRange(next, static_cast<std::uint64_t>(sourceBytes_.size()))) {
        return false;
    }

    if (next.start == byteRange_.start && next.end == byteRange_.end) {
        return true;
    }

    const auto previous = byteRange_;
    byteRange_ = next;
    if (!publishByteRange()) {
        byteRange_ = previous;
        return false;
    }

    if (rememberExactRange) {
        lastExactRange_ = next;
    }
    return true;
}

bool PluginProcessor::snapByteRangeToSection() {
    const auto size = static_cast<std::uint64_t>(sourceBytes_.size());
    if (size == 0) {
        return false;
    }

    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> sizes;
    offsets.reserve(sourceInfo_.sections.size());
    sizes.reserve(sourceInfo_.sections.size());
    for (const auto& section : sourceInfo_.sections) {
        offsets.push_back(section.rawOffset);
        sizes.push_back(section.rawSize);
    }

    const auto previous = byteRange_;
    const auto next = pe::sectionSnapToggle(previous, lastExactRange_, size,
                                            offsets.data(), sizes.data(), offsets.size());

    if (next.start == previous.start && next.end == previous.end) {
        // nada a mudar. Republicar material identico interromperia o audio sem ganho.
        return true;
    }

    if (!setByteRange(next.start, next.end, false)) {
        return false;
    }

    if (!byteRangeIsSectionAligned()) {
        lastExactRange_ = next;
    }

    return true;
}

bool PluginProcessor::byteRangeIsSectionAligned() const noexcept {
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> sizes;
    offsets.reserve(sourceInfo_.sections.size());
    sizes.reserve(sourceInfo_.sections.size());
    for (const auto& section : sourceInfo_.sections) {
        offsets.push_back(section.rawOffset);
        sizes.push_back(section.rawSize);
    }

    return pe::isSectionAligned(byteRange_, offsets.data(), sizes.data(), offsets.size());
}

bool PluginProcessor::publishByteRange() {
    const auto range = byteRange_;
    const auto size = static_cast<std::uint64_t>(sourceBytes_.size());
    if (range.empty() || range.end > size) {
        return false;
    }

    auto buffer = std::make_shared<const std::vector<float>>(
        pe::toSamples(sourceBytes_.data(), static_cast<std::size_t>(range.start),
                      static_cast<std::size_t>(range.length())));
    if (buffer->empty()) {
        return false;
    }

    std::vector<float> curve(dsp::GranularEngine::kMaxEntropyPoints, 0.0f);
    {
        std::vector<pe::Column> columns;
        constexpr std::uint32_t kCurveColumns {256};
        if (pe::reduceToColumns(sourceBytes_.data(), size, range.start, range.end,
                                kCurveColumns, columns)) {
            for (std::size_t i = 0; i < curve.size(); ++i) {
                const auto index = (i * columns.size()) / curve.size();
                curve[i] = index < columns.size() ? columns[index].entropyBits : 0.0f;
            }
        }
    }

    owned_.push_back(std::move(buffer));
    entropyCurves_.push_back(std::move(curve));

    const PublishedMaterial material {owned_.back().get(), entropyCurves_.back().data()};
    if (!incoming_.push(material)) {
        owned_.pop_back();
        entropyCurves_.pop_back();
        lastError_ = "E_BUSY";
        return false;
    }

    transport_.setRegionLength(range.length());
    telemetry_.playDurationSeconds.store(transport_.durationSeconds(),
                                          std::memory_order_relaxed);
    return true;
}

void PluginProcessor::drainIncomingQueue() noexcept {
    PublishedMaterial incoming {};
    while (incoming_.pop(incoming)) {
        const auto previous = live_.load(std::memory_order_relaxed);
        if (previous != nullptr) {
            returned_.push({previous, nullptr});
        }
        live_.store(incoming.samples, std::memory_order_release);
        engine_.setSource(incoming.samples->data(), incoming.samples->size(),
                          incoming.entropyCurve, dsp::GranularEngine::kMaxEntropyPoints);
    }
}

void PluginProcessor::releaseReturnedBuffers() {
    PublishedMaterial returned {};
    while (returned_.pop(returned)) {
        const auto index = static_cast<std::size_t>(
            std::find_if(owned_.begin(), owned_.end(),
                         [&returned](const SampleBuffer& held) {
                             return held.get() == returned.samples;
                         }) -
            owned_.begin());

        if (index < owned_.size()) {
            owned_.erase(owned_.begin() + static_cast<std::ptrdiff_t>(index));
            if (index < entropyCurves_.size()) {
                entropyCurves_.erase(entropyCurves_.begin() +
                                     static_cast<std::ptrdiff_t>(index));
            }
        }
    }
}

void PluginProcessor::pushKeyboardNote(int noteNumber, float velocity, bool on) noexcept {
    const int value = juce::jlimit(0, 127, static_cast<int>(velocity * 127.0f + 0.5f));
    static_cast<void>(keyboardNotes_.push({on ? rt::NoteTracker::Event::Kind::noteOn
                                              : rt::NoteTracker::Event::Kind::noteOff,
                                           noteNumber & 0x7F, value}));
}

void PluginProcessor::updateHostTransport() noexcept {
    double bpm = 120.0;
    double ppq = -1.0;
    if (auto* head = getPlayHead()) {
        if (const auto position = head->getPosition()) {
            if (const auto hostBpm = position->getBpm()) {
                if (*hostBpm > 0.0) {
                    bpm = *hostBpm;
                }
            }
            if (const auto hostPpq = position->getPpqPosition()) {
                ppq = *hostPpq;
            }
        }
    }
    hostBpm_.store(bpm, std::memory_order_relaxed);
    hostPpq_.store(ppq, std::memory_order_relaxed);
}

void PluginProcessor::drainKeyboardNotes() noexcept {
    rt::NoteTracker::Event event {};
    bool changed = false;
    while (keyboardNotes_.pop(event)) {
        notes_.handle(event);
        changed = true;
    }

    if (changed) {
        publishPressedNotes();
    }
}

void PluginProcessor::publishPressedNotes() noexcept {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    for (int note = 0; note < 64; ++note) {
        if (notes_.isHeld(note)) {
            low |= std::uint64_t {1} << note;
        }
    }
    for (int note = 64; note < 128; ++note) {
        if (notes_.isHeld(note)) {
            high |= std::uint64_t {1} << (note - 64);
        }
    }
    pressedLow_.store(low, std::memory_order_relaxed);
    pressedHigh_.store(high, std::memory_order_relaxed);
}

void PluginProcessor::readNotes(const juce::MidiBuffer& midi) noexcept {
    if (midi.isEmpty()) {
        return;
    }

    for (const auto metadata : midi) {
        const juce::MidiMessage message = metadata.getMessage();
        midiActivity_.fetch_add(1, std::memory_order_relaxed);

        // nota solta por velocidade zero: pad que manda note-on 0 em vez de note-off.
        const bool noteOn = message.isNoteOn() && message.getVelocity() > 0;
        const bool noteOff = message.isNoteOff() || (message.isNoteOn() && !noteOn);

        auto kind = rt::NoteTracker::Event::Kind::other;
        if (message.isAllNotesOff() || message.isAllSoundOff()
            || message.getControllerNumber() == kCcAllNotesOff
            || message.getControllerNumber() == kCcAllSoundOff) {
            kind = rt::NoteTracker::Event::Kind::allNotesOff;
        } else if (message.getControllerNumber() == kCcSustain) {
            kind = rt::NoteTracker::Event::Kind::sustain;
        } else if (message.getControllerNumber() == kCcDensity
                   || message.getControllerNumber() == kCcPosition
                   || message.getControllerNumber() == kCcModWheel) {
            kind = rt::NoteTracker::Event::Kind::controller;
        } else if (noteOn) {
            kind = rt::NoteTracker::Event::Kind::noteOn;
        } else if (noteOff) {
            kind = rt::NoteTracker::Event::Kind::noteOff;
        }

        const float normalised = message.getControllerValue() / 127.0f;
        switch (message.getControllerNumber()) {
            case kCcDensity:
                notes_.setController(kDensityController, normalised);
                break;
            case kCcPosition:
                notes_.setController(kPositionController, normalised);
                break;
            case kCcModWheel:
                notes_.setController(kModWheelController, normalised);
                break;
            default:
                break;
        }

        notes_.handle({kind, message.getNoteNumber(), message.getControllerValue()});
    }

    publishPressedNotes();
}

void PluginProcessor::applyPendingControllerChanges() {
    if (!notes_.controllersChanged()) {
        return;
    }

    const auto densityDirty = notes_.controllerDirty(kDensityController);
    const auto positionDirty = notes_.controllerDirty(kPositionController);
    notes_.clearControllersChanged();

    const auto set = [this](const char* id, float value) {
        if (auto* parameter = parameters_.getParameter(id)) {
            parameter->setValueNotifyingHost(value);
        }
    };

    if (densityDirty) {
        set("density", notes_.controller(kDensityController));
    }
    if (positionDirty) {
        set("position", notes_.controller(kPositionController));
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;

    drainIncomingQueue();
    readNotes(midi);
    drainKeyboardNotes();
    updateHostTransport();

const auto numSamples = buffer.getNumSamples();

    const auto playing = transport_.isPlaying();
    const auto held = transportHold_.load(std::memory_order_relaxed);

    auto params = currentParams();
    transport_.setRate(static_cast<double>(*parameters_.getRawParameterValue("scanspeed")));
    if (playing) {
        params.position = transport_.advance(held ? 0 : numSamples, playing);
    }

    engine_.setSounding(notes_.sounding() || playing);
    engine_.processBlock(buffer.getWritePointer(0),
                         buffer.getWritePointer(1),
                         numSamples,
                         params);

    publishTelemetry(buffer);
    publishSpectrum(buffer);
}

void PluginProcessor::publishSpectrum(const juce::AudioBuffer<float>& buffer) noexcept {
    const auto n = static_cast<int>(spectrumFifo_.size());
    if (n <= 0 || buffer.getNumSamples() <= 0 || spectrumFftBuffer_.size() < 2 * n) {
        return;
    }

    const auto* left = buffer.getReadPointer(0);
    const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : left;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        spectrumFifo_[static_cast<std::size_t>(spectrumWrite_)] =
            0.5f * (left[i] + right[i]);
        spectrumWrite_ = (spectrumWrite_ + 1) % n;
        spectrumFill_ = juce::jmin(n, spectrumFill_ + 1);
    }

    if (spectrumFill_ < n) {
        return; // a janela ainda esta' a encher
    }

    // ordem cronologica: a amostra mais antiga primeiro.
    auto* fftData = spectrumFftBuffer_.data();
    for (int i = 0; i < n; ++i) {
        const auto index = (spectrumWrite_ + i) % n;
        fftData[i] = spectrumFifo_[static_cast<std::size_t>(index)] *
                     spectrumWindow_[static_cast<std::size_t>(i)];
    }

    // espectro so' de frequencias positivas: devolve (n/2 + 1) magnitudes.
    spectrumFft_.performFrequencyOnlyForwardTransform(fftData, true);

    const auto bins = static_cast<std::size_t>(Telemetry::kSpectrumBins);
    const auto toDb = [](float magnitude) {
        return magnitude > 1.0e-6f ? juce::Decibels::gainToDecibels(magnitude) : -100.0f;
    };
    // ganho do analisador: 2/N em vez de 1/N. A onda sobe 6 dB e passa a
    // ler-se por cima do ruido de fundo do grafico.
    const auto kNormalise = 2.0f / static_cast<float>(n);

    for (std::size_t b = 0; b < bins; ++b) {
        const auto begin = spectrumBinStart_[b];
        const auto end = spectrumBinEnd_[b];
        float sum = 0.0f;
        for (int k = begin; k < end; ++k) {
            sum += fftData[static_cast<std::size_t>(k)];
        }
        const auto count = static_cast<float>(juce::jmax(1, end - begin));
        const auto magnitude = (sum / count) * kNormalise;
        const auto target = toDb(magnitude);

        // suavizacao entre quadros: ataque rapido, descida lenta. Sem isto as
        // barras tremem a cada bloco e a leitura fica nervosa.
        const auto previous = telemetry_.spectrum[b].load(std::memory_order_relaxed);
        const auto coefficient = target > previous ? 0.55f : 0.10f;
        const auto smoothed = previous + (target - previous) * coefficient;
        telemetry_.spectrum[b].store(smoothed, std::memory_order_relaxed);
    }
}

void PluginProcessor::publishTelemetry(const juce::AudioBuffer<float>& buffer) noexcept {
    float peak = 0.0f;
    float peakLeft = 0.0f;
    float peakRight = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        const auto samples = buffer.getReadPointer(channel);
        float channelPeak = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            channelPeak = std::max(channelPeak, std::abs(samples[i]));
        }
        peak = std::max(peak, channelPeak);
        if (channel == 0) {
            peakLeft = channelPeak;
        } else if (channel == 1) {
            peakRight = channelPeak;
        }
    }

    const auto toDb = [](float value) {
        return (value > 1.0e-6f) ? juce::Decibels::gainToDecibels(value) : -100.0f;
    };
    const auto decibels = toDb(peak);

    telemetry_.peakDb.store(decibels, std::memory_order_relaxed);
    telemetry_.peakLeftDb.store(toDb(peakLeft), std::memory_order_relaxed);
    telemetry_.peakRightDb.store(toDb(peakRight), std::memory_order_relaxed);
    telemetry_.activeVoices.store(engine_.lastActiveVoices(), std::memory_order_relaxed);

    const auto audible = notes_.sounding() || transport_.isPlaying();
    telemetry_.sounding.store(audible, std::memory_order_relaxed);
    telemetry_.playing.store(transport_.isPlaying(), std::memory_order_relaxed);
    telemetry_.held.store(transportHold_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
    telemetry_.playheadFraction.store(transport_.positionFraction(),
                                      std::memory_order_relaxed);

    telemetry_.envelopeLevel.store(engine_.gateLevel(), std::memory_order_relaxed);
    telemetry_.envelopeStage.store(engine_.envelopeStagePhase(), std::memory_order_relaxed);
    telemetry_.envelopePhase.store(engine_.envelopePhaseIndex(), std::memory_order_relaxed);
    telemetry_.cutoff1Hz.store(engine_.f1CutoffHz(), std::memory_order_relaxed);
    telemetry_.cutoff2Hz.store(engine_.f2CutoffHz(), std::memory_order_relaxed);
    telemetry_.lfoPhase.store(static_cast<float>(engine_.lfoPhase()),
                              std::memory_order_relaxed);
    telemetry_.lfoValue.store(engine_.lfoBlockValue(), std::memory_order_relaxed);
}


void PluginProcessor::applyFactoryPreset(int index) {
    const auto& presets = factoryPresets();
    if (index < 0 || static_cast<std::size_t>(index) >= presets.size()) {
        return;
    }
    const auto& preset = presets[static_cast<std::size_t>(index)];

    const auto set = [this](const char* id, float plainValue) {
        if (auto* parameter = parameters_.getParameter(id)) {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
        }
    };
    set("grain", preset.grain);
    set("density", preset.density);
    set("position", preset.position);
    set("spray", preset.spray);
    set("pitch", preset.pitch);
    set("volume", preset.volume);
    set("window", preset.window);
    set("pan", preset.pan);
    set("grainlevel", preset.grainlevel);
    set("pitchrand", preset.pitchrand);
    set("scanspeed", preset.scanspeed);
    set("attack", preset.attack);
    set("decay", preset.decay);
    set("sustain", preset.sustain);
    set("release", preset.release);
    set("f1type", preset.f1type);
    set("f1cutoff", preset.f1cutoff);
    set("f1q", preset.f1q);
    set("f2type", preset.f2type);
    set("f2cutoff", preset.f2cutoff);
    set("f2q", preset.f2q);
    set("lforate", preset.lforate);
    set("lfodepth", preset.lfodepth);
    set("lfotarget", preset.lfotarget);
    set("lfowave", preset.lfowave);
    set("width", preset.width);
    set("glide", preset.glide);
    set("f1slope", preset.f1slope);
    set("f1drive", preset.f1drive);
    set("f1keytrack", preset.f1keytrack);
    set("f1env", preset.f1env);
    set("atkcurve", preset.atkcurve);
    set("deccurve", preset.deccurve);
    set("relcurve", preset.relcurve);
}

std::unique_ptr<juce::XmlElement> PluginProcessor::stateXml() {
    auto state = parameters_.copyState();

    state.setProperty("sourcePath", sourcePath_, nullptr);

    state.setProperty("byteStart", static_cast<juce::int64>(byteRange_.start), nullptr);
    state.setProperty("byteEnd", static_cast<juce::int64>(byteRange_.end), nullptr);

    return state.createXml();
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = stateXml()) {
        copyXmlToBinary(*xml, destData);
    }
}

bool PluginProcessor::loadStateXml(const juce::XmlElement& xml) {
    if (!xml.hasTagName(parameters_.state.getType())) {
        return false;
    }

    parameters_.replaceState(juce::ValueTree::fromXml(xml));

    migrateLegacyModMatrix();

    const auto tree = parameters_.state;
    const auto path = tree.getProperty("sourcePath").toString();
    if (path.isNotEmpty() && juce::File(path).existsAsFile()) {
        if (ingest(path)) {
            const auto start = static_cast<std::uint64_t>(
                juce::jmax(static_cast<juce::int64>(0), tree.getProperty("byteStart").toString()
                                                     .getLargeIntValue()));
            const auto end = static_cast<std::uint64_t>(
                juce::jmax(static_cast<juce::int64>(0), tree.getProperty("byteEnd").toString()
                                                   .getLargeIntValue()));
            if (end > start) {
                static_cast<void>(setByteRange(start, end));
            }
        }
    } else if (path.isNotEmpty()) {
        lastError_ = "E_SOURCE_MISSING";
    }
    return true;
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) {
        return;
    }

    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr) {
        return;
    }
    static_cast<void>(loadStateXml(*xml));
}

bool PluginProcessor::savePresetToFile(const juce::File& file) {
    auto xml = stateXml();
    if (xml == nullptr) {
        return false;
    }
    return file.replaceWithText(xml->toString());
}

bool PluginProcessor::loadPresetFromFile(const juce::File& file) {
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) {
        return false;
    }
    return loadStateXml(*xml);
}

} // namespace opcoda

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new opcoda::PluginProcessor();
}