#include "plugin_processor.h"

#include "opcoda_core/entropy/shannon_entropy.h"
#include "opcoda_core/pe/byte_to_sample.h"
#include "opcoda_core/pe/pe_parser.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace opcoda {

PluginProcessor::PluginProcessor()
    // So saida. Um barramento de entrada deixaria a trilha de audio do Ableton
    // esperando um sinal que o plugin nunca usa, e obrigaria o host a tratar o
    // Opcoda como efeito em vez de instrumento.
: juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "Opcoda", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout() {
    const auto range = [](const char* id, const char* name, float lo, float hi, float def) {
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {id, 1}, juce::String {name}, juce::NormalisableRange<float> {lo, hi}, def);
    };

    return {
        range("grain", "Tamanho de grao", 1.0f, 100.0f, 40.0f),
        range("density", "Densidade", 1.0f, 200.0f, 20.0f),
        range("position", "Posicao", 0.0f, 1.0f, 0.5f),
        range("spray", "Spray", 0.0f, 1.0f, 0.0f),
        range("pitch", "Afinacao", -24.0f, 24.0f, 0.0f),
        range("volume", "Volume", -60.0f, 0.0f, 0.0f),
    };
}

dsp::GranularParams PluginProcessor::currentParams() const noexcept {
    const auto value = [this](const char* id) {
        return static_cast<float>(*parameters_.getRawParameterValue(id));
    };

    dsp::GranularParams params;
    params.grainSizeMs = value("grain");
    params.densityGrainsPerSec = value("density");
    params.position = value("position");
    params.spray = value("spray");
    params.pitchSemitones = value("pitch");
    params.volumeDb = value("volume");
    return params;
}

void PluginProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    engine_.prepare(sampleRate, maximumExpectedSamplesPerBlock);
}

void PluginProcessor::releaseResources() {
    engine_.reset();
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

    const auto& section = parsed.image.sections[0];
    auto buffer = std::make_shared<const std::vector<float>>(
        pe::toSamples(bytes.data(), section.rawOffset, section.rawSize));
    if (buffer->empty()) {
        lastError_ = "E_EMPTY";
        return false;
    }

    // Se a fila de entrada esta cheia, a thread de audio ainda nao consumiu a
    // publicacao anterior. Recusar e melhor do que crescer: a memoria e' do
    // produtor, e uma alocacao ali custaria o contrato de tempo real.
    owned_.push_back(std::move(buffer));
    if (!incoming_.push(owned_.back().get())) {
        owned_.pop_back();
        lastError_ = "E_BUSY";
        return false;
    }

sourceName_ = juce::File(path).getFileName();
    sourcePath_ = path;
    sourceInfo_.name = sourceName_;
    sourceInfo_.formatTag = "[64-bit PE]";
    sourceInfo_.sizeBytes = static_cast<std::int64_t>(size);

    // As secoes e a curva vao para o estado do editor. Antes eram descartadas
    // aqui, e o display nao tinha o que desenhar.
    sourceInfo_.sections.clear();
    sourceInfo_.sections.reserve(parsed.image.numberOfSections);
    for (std::size_t i = 0; i < parsed.image.numberOfSections; ++i) {
        const auto& parsedSection = parsed.image.sections[i];
        SourceInfo::SectionInfo entry;
        // O campo de nome tem 8 bytes e pode nao estar terminado, por isso o
        // comprimento vai explícito em vez de usar strlen.
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

    sourceInfo_.entropyCurve = buildEntropyCurve(bytes.data(), size);
    lastError_.clear();
    return true;
}

std::vector<float> PluginProcessor::buildEntropyCurve(const std::uint8_t* data,
                                                     std::size_t size) {
    // Janelas proporcionais ao tamanho, com um piso e um teto. Sem o piso, um
    // binario de 200 KB teria uma janela de 1 byte e um histograma de 256
    // celulas por ponto, que e' ruido. Sem o teto, um arquivo de 200 MB teria
    // 1500 janelas e a curva ficaria ilegivel.
    constexpr std::size_t kTargetPoints = 320;
    constexpr std::size_t kMinWindow = 256;

    const auto window = juce::jmax(kMinWindow, size / kTargetPoints);
    const auto count = juce::jmax<std::size_t>(1, (size + window - 1) / window);
    if (count > kTargetPoints * 2) {
        return {};
    }

    std::vector<double> raw(count, 0.0);
    const auto written = entropy::shannonCurve(data, size, window, raw.data(), count);

    std::vector<float> curve;
    curve.reserve(written);
    for (std::size_t i = 0; i < written; ++i) {
        // A escala do display e' 0 a 8 bits por byte.
        curve.push_back(static_cast<float>(raw[i]));
    }
    return curve;
}

void PluginProcessor::drainIncomingQueue() noexcept {
    SamplePtr incoming {nullptr};
    while (incoming_.pop(incoming)) {
        const auto previous = live_.load(std::memory_order_relaxed);
        if (previous != nullptr) {
            // Devolve o ponteiro antigo antes de trocar, para que a interface
            // nunca libere memoria que a thread de audio ainda esta lendo.
            returned_.push(previous);
        }
        live_.store(incoming, std::memory_order_release);
        engine_.setSource(incoming->data(), incoming->size());
    }
}

void PluginProcessor::releaseReturnedBuffers() {
    SamplePtr returned {nullptr};
    while (returned_.pop(returned)) {
        owned_.erase(std::remove_if(owned_.begin(), owned_.end(),
                                    [returned](const SampleBuffer& held) {
                                        return held.get() == returned;
                                    }),
                     owned_.end());
    }
}

void PluginProcessor::readNotes(const juce::MidiBuffer& midi) noexcept {
    if (midi.isEmpty()) {
        return;
    }

    // Range-for: MidiBuffer::Iterator foi deprecado no JUCE 8. A construcao de
    // MidiMessage a partir de MidiMessageMetadata nao aloca, porque os bytes
    // sao uma vista sobre o proprio buffer.
    for (const auto metadata : midi) {
        const juce::MidiMessage message = metadata.getMessage();

        auto kind = rt::NoteTracker::Event::Kind::other;
        if (message.isAllNotesOff() || message.isAllSoundOff()) {
            kind = rt::NoteTracker::Event::Kind::allNotesOff;
        } else if (message.getControllerNumber() == kCcSustain) {
            kind = rt::NoteTracker::Event::Kind::sustain;
        } else if (message.getControllerNumber() == kCcDensity
                   || message.getControllerNumber() == kCcPosition) {
            kind = rt::NoteTracker::Event::Kind::controller;
        } else if (message.isNoteOn()) {
            kind = rt::NoteTracker::Event::Kind::noteOn;
        } else if (message.isNoteOff()) {
            kind = rt::NoteTracker::Event::Kind::noteOff;
        }

        if (message.getControllerNumber() == kCcDensity) {
            notes_.setController(kDensityController, message.getControllerValue() / 127.0f);
        } else if (message.getControllerNumber() == kCcPosition) {
            notes_.setController(kPositionController, message.getControllerValue() / 127.0f);
        }

        notes_.handle({kind, message.getControllerNumber(), message.getControllerValue()});
    }
}

void PluginProcessor::applyPendingControllerChanges() {
    if (!notes_.controllersChanged()) {
        return;
    }
    notes_.clearControllersChanged();

    const auto set = [this](const char* id, float value) {
        if (auto* parameter = parameters_.getParameter(id)) {
            parameter->setValueNotifyingHost(value);
        }
    };

    set("density", notes_.controller(kDensityController));
    set("position", notes_.controller(kPositionController));
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;

    // A troca de material e' o unico ponto de cruzamento entre as threads, e
    // acontece por troca de ponteiro, sem alocar e sem esperar.
    drainIncomingQueue();
    readNotes(midi);

const auto numSamples = buffer.getNumSamples();
    engine_.setSounding(notes_.sounding());
    engine_.processBlock(buffer.getWritePointer(0),
                         buffer.getWritePointer(1),
                         numSamples,
                         currentParams());

    publishTelemetry(buffer);
}

// Publica o pico do bloco e as vozes ativas. Roda depois do motor e antes de
// sair: a leitura e' sobre o bloco que acabou de ser escrito.
void PluginProcessor::publishTelemetry(const juce::AudioBuffer<float>& buffer) noexcept {
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        const auto samples = buffer.getReadPointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            peak = std::max(peak, std::abs(samples[i]));
        }
    }

    const auto decibels = (peak > 1.0e-6f) ? juce::Decibels::gainToDecibels(peak) : -1.0f;

    // relaxed e' suficiente: sao valores de leitura para um mostrador, e a
    // ordenacao que interessa e' dentro do proprio bloco, que ja terminou.
    telemetry_.peakDb.store(decibels, std::memory_order_relaxed);
    telemetry_.activeVoices.store(engine_.lastActiveVoices(), std::memory_order_relaxed);
    telemetry_.sounding.store(notes_.sounding(), std::memory_order_relaxed);
}


void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters_.copyState();

    // O binario carregado vai para o estado porque sem ele o projeto abre com
    // os parametros certos e o som errado, que e' pior do que nao abrir nada.
    state.setProperty("sourcePath", sourcePath_, nullptr);

    if (auto xml = state.createXml()) {
        copyXmlToBinary(*xml, destData);
    }
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) {
        return;
    }

    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters_.state.getType())) {
        return;
    }

    parameters_.replaceState(juce::ValueTree::fromXml(*xml));

    // O arquivo e' reingestado pela mesma via de ingest do arrasto: ler disco,
    // converter e publicar na fila. Nao ha caminho paralelo, e' o mesmo codigo
    // que ja' tem teste.
    const auto path = parameters_.state.getProperty("sourcePath").toString();
    if (path.isNotEmpty() && juce::File(path).existsAsFile()) {
        ingest(path);
    } else if (path.isNotEmpty()) {
        lastError_ = "E_SOURCE_MISSING";
    }
}

} // namespace opcoda

// Ponto de entrada que o JUCE procura pelo nome, sem namespace. Dentro de
// opcoda:: ele linka como createPluginFilter@@ e o Standalone nao resolve.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new opcoda::PluginProcessor();
}