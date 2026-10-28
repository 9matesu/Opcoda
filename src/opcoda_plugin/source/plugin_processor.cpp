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
    // so' saida. Um barramento de entrada deixaria a trilha do Ableton a espera de
    // um sinal que o plugin nunca usa, e faria o host trata-lo como efeito.
: juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "Opcoda", createParameterLayout()) {
    // 41 parametros de som mais a largura stereo e as 24 celulas da matriz
    // (8 rotas por fonte, destino e amount).
    jassert(getParameters().size() == 66);

    // ponteiros raw das celulas: ler por id no bloco construiria juce::String
    // no thread de audio, e o guard de alocacao barra. Um null aqui e' erro de
    // contracto entre o layout e este resolvedor; melhor morrer em Debug do que
    // desreferenciar no primeiro bloco.
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
    const auto range = [](const char* id, const char* name, float lo, float hi, float def) {
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {id, 1}, juce::String {name}, juce::NormalisableRange<float> {lo, hi}, def);
    };
    // inteiro com passo 1. O degrau do LFO e' contagem (1..16) e um float continuo
    // daria 3,7 degraus na automacao.
    const auto intRange = [](const char* id, const char* name, int lo, int hi, int def) {
        return std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID {id, 1}, juce::String {name}, lo, hi, def);
    };
    // corte em Hz com centro em 1 kHz. Linear de 20 a 20000 punha a oitava de 40 a
    // 80 Hz nos primeiros 2 graus do knob e o resto era passeio. O skew so' remapeia
    // o gesto; o valor guardado e automatizado continua em Hz.
    const auto logCutoff = [](const char* id, const char* name, float def) {
        juce::NormalisableRange<float> cutoff {20.0f, 20000.0f};
        cutoff.setSkewForCentre(1000.0f);
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {id, 1}, juce::String {name}, cutoff, def);
    };
    // 1 ms a 2 s, o que um granular moderno oferece. em escala linear, 40 ms morava
    // nos primeiros 2 % do curso e o resto era so' material longo; o skew poe o
    // centro do gesto em 100 ms e deixa a metade da viagem para os graos grandes. o
    // valor guardado e automatizado continua em ms.
    const auto grainSize = [] {
        juce::NormalisableRange<float> size {dsp::GranularEngine::kMinGrainMs,
                                            dsp::GranularEngine::kMaxGrainMs};
        size.setSkewForCentre(100.0f);
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {"grain", 1}, "Grain Size", size, 40.0f);
    };
    const auto choice = [](const char* id, const char* name, juce::StringArray options, int def) {
        return std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID {id, 1}, juce::String {name}, std::move(options), def);
    };

    // os seis IDs originais nao mudam — sessao salva referencia por ID — so' o nome
    // exibido passou a ingles. Os novos ja' nascem com ID ingles.
    juce::AudioProcessorValueTreeState::ParameterLayout layout {
        grainSize(),
        range("density", "Density", 1.0f, 200.0f, 20.0f),
        range("position", "Position", 0.0f, 1.0f, 0.5f),
        range("spray", "Spray", 0.0f, 1.0f, 0.0f),
        range("pitch", "Pitch", -48.0f, 48.0f, 0.0f),
        range("volume", "Volume", -60.0f, 0.0f, 0.0f),
        choice("window", "Window", {"Hann", "Gaussian", "Hamming", "Blackman"}, 0),
        range("pan", "Pan", -1.0f, 1.0f, 0.0f),
        range("grainlevel", "Grain Level", 0.0f, 1.0f, 1.0f),
        range("pitchrand", "Pitch Random", 0.0f, 12.0f, 0.0f),
        // F021: a nuvem. Zero em todos devolve o motor anterior, amostra a amostra:
        // espalhamento, dispersao de afinacao e jitter so' entram quando o knob sai do
        // zero.
        range("cloudsize", "Cloud Size", 0.0f, 1.0f, 0.0f),
        range("pitchspread", "Pitch Spread", 0.0f, 24.0f, 0.0f),
        range("grainjit", "Grain Jitter", 0.0f, 1.0f, 0.0f),
        range("scanspeed", "Scan Speed", 0.25f, 4.0f, 1.0f),
        range("attack", "Attack", 0.001f, 2.0f, 0.005f),
        range("decay", "Decay", 0.001f, 2.0f, 0.1f),
        range("sustain", "Sustain", 0.0f, 1.0f, 1.0f),
        range("release", "Release", 0.001f, 2.0f, 0.05f),
        // F020: curvas por estagio. -1 exponencial, 0 linear, +1 logaritmica, forma
        // phase^(2^(-3*curva)), documentada em dsp/curve.h.
        range("atkcurve", "Attack Curve", -1.0f, 1.0f, 0.0f),
        range("deccurve", "Decay Curve", -1.0f, 1.0f, 0.0f),
        range("relcurve", "Release Curve", -1.0f, 1.0f, 0.0f),
        choice("f1type", "Filter 1 Type", {"Low-pass", "High-pass", "Band-pass", "Notch"}, 0),
        logCutoff("f1cutoff", "Filter 1 Cutoff", 20000.0f),
        range("f1q", "Filter 1 Q", 0.5f, 12.0f, 0.7071f),
        // F020: caracter analogico do filtro 1. Drive em dB, keytrack 0..1 (1 = uma
        // oitava de corte por oitava de nota) e envelope com sinal, ate' 5 oitavas para
        // cada lado.
        choice("f1slope", "Filter 1 Slope", {"12 dB", "24 dB"}, 0),
        range("f1drive", "Filter 1 Drive", 0.0f, 24.0f, 0.0f),
        range("f1keytrack", "Filter 1 Keytrack", 0.0f, 1.0f, 0.0f),
        range("f1env", "Filter 1 Env", -1.0f, 1.0f, 0.0f),
        choice("f2type", "Filter 2 Type", {"Low-pass", "High-pass", "Band-pass", "Notch"}, 0),
        logCutoff("f2cutoff", "Filter 2 Cutoff", 20000.0f),
        range("f2q", "Filter 2 Q", 0.5f, 12.0f, 0.7071f),
        range("lforate", "LFO Rate", 0.0f, 20.0f, 1.0f),
        range("lfodepth", "LFO Depth", 0.0f, 1.0f, 0.0f),
        choice("lfotarget", "LFO Target", {"Pitch", "Density", "Cutoff", "Position"}, 0),
        choice("lfowave", "LFO Wave", {"Sine", "Tri", "Saw", "Square", "S&H"}, 0),
        // F018: teclado e escala.
        choice("scale", "Scale", {"Chromatic", "Major", "Natural Minor", "Harmonic Minor",
                                  "Dorian", "Phrygian", "Lydian", "Mixolydian",
                                  "Pentatonic Major", "Pentatonic Minor", "Blues",
                                  "Whole Tone"}, 0),
        choice("root", "Root", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0),
        range("glide", "Glide", 0.0f, 2.0f, 0.0f),
        choice("lfosync", "LFO Sync", {"Free", "Sync"}, 0),
        choice("lfodiv", "LFO Div", {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                                     "1/4.", "1/8.", "1/4T", "1/8T", "1/16T"}, 2),
        intRange("lfosteps", "LFO Steps", 1, 16, 16),
        // largura stereo na saida: 0 colapsa para mono, 1 e' o material, 2 dobra o
        // side. faixa linear; o 1.0 cai no centro, que e' onde o knob nasce.
        range("width", "Stereo Width", 0.0f, 2.0f, 1.0f),
    };

    // matriz de modulacao: 8 rotas com fonte, destino e amount, cada celula um
    // parametro proprio. Os indices seguem dsp::ModSource e dsp::ModDest, com o
    // Off primeiro; a ordem e' contrato do estado.
    const juce::StringArray sources {"Off", "LFO", "Envelope", "Velocity", "Key",
                                     "Mod Wheel", "CC74", "CC71", "Entropy"};
    const juce::StringArray destinations {
        "Off",  "Pitch",  "Density", "Position", "Spray",     "Grain Size",
        "Cut 1", "Q 1",   "Cut 2",   "Q 2",      "Pan",       "Grain Level",
        "Drive", "Stereo Width"};
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
    // getRawParameterValue da o valor guardavel, que e' o que o utilizador ve', nao
    // o normalizado. Trocar os dois espacos entrega 0.4 em vez de 40 e nao da erro
    // nenhum: aparece como som esquisito.
    const auto value = [this](const char* id) {
        return static_cast<float>(*parameters_.getRawParameterValue(id));
    };

    // choice chega como float 0..N-1. O sanitize cobre estado corrompido de sessao,
    // que tem de soar e nao calar.
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
    params.cloudSize = value("cloudsize");
    params.pitchSpreadSemitones = value("pitchspread");
    params.grainJitter = value("grainjit");
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
    params.f1Slope = (choiceIndex("f1slope") == 1) ? 1 : 0;
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

    // F018: a nota da pilha mono vira transposicao no motor. O indice da escala e'
    // saneado aqui, como os outros choices.
    params.noteNumber = notes_.activeNote();
    const int scaleIndex = choiceIndex("scale");
    params.scale = (scaleIndex >= 0 && scaleIndex < dsp::kScaleCount)
                       ? static_cast<dsp::ScaleType>(scaleIndex)
                       : dsp::ScaleType::kChromatic;
    params.scaleRoot = dsp::wrap12(choiceIndex("root"));
    params.glideSeconds = value("glide");

    // F019: modo, divisao, degraus e tempo do host. Em Sync o motor deriva a fase do
    // ppq; em Free ignora os dois doubles e usa lforateHz.
    params.lfoSync = choiceIndex("lfosync") == 1;
    params.lfoDiv = dsp::sanitizeLFODiv(choiceIndex("lfodiv"));
    params.lfoSteps = dsp::sanitizeLFOSteps(static_cast<int>(value("lfosteps") + 0.5f));
    params.hostBpm = hostBpm_.load(std::memory_order_relaxed);
    params.hostPpq = hostPpq_.load(std::memory_order_relaxed);

    // F020: fontes que o motor nao mede sozinho. A matriz le os 24 raws por bloco;
    // velocidade e CCs saem do NoteTracker, que ja' corre no audio.
    params.velocity = notes_.velocity();
    params.modWheel = notes_.controller(kModWheelController);
    params.cc74 = notes_.controller(kDensityController);
    params.cc71 = notes_.controller(kPositionController);
    params.modMatrix = readModMatrix();
    return params;
}

void PluginProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    engine_.prepare(sampleRate, maximumExpectedSamplesPerBlock);
    // o transporte precisa da taxa para saber quantos bytes por amostra consome, e
    // e' aqui que a recebe: quem continua a ter a verdade sobre a taxa e' este
    // metodo.
    transport_.prepare(sampleRate);
}

void PluginProcessor::releaseResources() {
    engine_.reset();
    // o motor guarda um ponteiro cru para o buffer de bytes; solta-lo antes de
    // owned_.clear() evita que o proximo processBlock leia memoria libertada se o
    // host chamar prepareToPlay e tocar sem publicar material novo.
    engine_.setSource(nullptr, 0);
    // reposto pelo metodo do transporte, nao escrevendo position_ a mao: quem
    // escreve ali e' advance().
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
        // o campo de nome tem 8 bytes e pode nao estar terminado, por isso o comprimento
        // vai explicito em vez de strlen.
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

    // a curva de entropia do ficheiro inteiro saiu com o componente que a desenhava.
    // o rodape mede a janela de 256 bytes onde esta' a cabeca de leitura: um
    // histograma por tique em vez de 320 no ingest.

    // arranca na primeira secao com dados, nao em zero: o cabecalho PE nao produz
    // audio reconhecivel e o instrumento soaria errado no primeiro arrasto.
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

    // janela publicada. Se a primeira nao produzir audio, o ingest e' sucesso sem
    // som, que e' o mesmo que recusa sem codigo de erro.
    return publishByteRange();
}

bool PluginProcessor::setByteRange(std::uint64_t start, std::uint64_t end,
                                   bool rememberExactRange) {
    // validacao no nucleo: a regra so' tem teste la', porque o nucleo nao depende de
    // JUCE.
    pe::ByteRange next {start, end};
    if (!pe::clampByteRange(next, static_cast<std::uint64_t>(sourceBytes_.size()))) {
        return false;
    }

    // pedir a mesma janela nao republica: interromperia o audio para trocar por uma
    // copia igual.
    if (next.start == byteRange_.start && next.end == byteRange_.end) {
        return true;
    }

    // falha nao mexe no estado: o audio continua com o material anterior em vez de
    // ficar sem fonte.
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

    // o nucleo decide, porque a regra tem teste la'. Aqui so' se juntam as secoes do
    // PE no formato que a funcao espera e se publica o resultado.
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

    // alinhado, a regiao anterior passa a ser a exacta de retorno. Ao voltar, a
    // exacta continua a mesma: senao o segundo duplo clique saltava para o sitio
    // errado.
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

    // a curva e' calculada AQUI, na interface, e nao no motor. Sao 256 pontos da
    // regiao: custo irrelevante fora do callback, e dentro era uma passagem completa
    // por troca de material.
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
        // sem colunas a curva fica a zeros e o motor usa o valor medio, o mesmo que a
        // curva desativada daria.
    }

    // memoria do produtor. Fila cheia quer dizer que o audio ainda nao consumiu a
    // publicacao anterior; o push recusa com E_BUSY em vez de crescer.
    owned_.push_back(std::move(buffer));
    entropyCurves_.push_back(std::move(curve));

    const PublishedMaterial material {owned_.back().get(), entropyCurves_.back().data()};
    if (!incoming_.push(material)) {
        owned_.pop_back();
        entropyCurves_.pop_back();
        lastError_ = "E_BUSY";
        return false;
    }

    // o transporte so' sabe o comprimento da regiao quando lho dizemos. Sem esta
    // linha a duracao de uma volta seria a do material anterior.
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
            // devolve o antigo antes de trocar, para a interface nunca liberar memoria que o
            // audio ainda esta' lendo.
            returned_.push({previous, nullptr});
        }
        live_.store(incoming.samples, std::memory_order_release);
        // so' copia o ponteiro da curva: no maximo 1024 floats. A medicao foi feita na
        // interface; antes era aqui dentro, a cada troca de material.
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
            // a curva segue o buffer. Apagar so' as amostras desalinhava o indice e a
            // proxima publicacao escrevia a curva no sitio errado.
            if (index < entropyCurves_.size()) {
                entropyCurves_.erase(entropyCurves_.begin() +
                                     static_cast<std::ptrdiff_t>(index));
            }
        }
    }
}

void PluginProcessor::pushKeyboardNote(int noteNumber, float velocity, bool on) noexcept {
    const int value = juce::jlimit(0, 127, static_cast<int>(velocity * 127.0f + 0.5f));
    // push false com a fila cheia: perder uma tecla e' melhor do que bloquear a
    // interface ou alocar.
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

    // o teclado virtual tambem mexe no bitmask: as teclas que ele acende sao as
    // mesmas que o MIDI do host acende, e a interface nao precisa de saber a
    // origem.
    if (changed) {
        publishPressedNotes();
    }
}

void PluginProcessor::publishPressedNotes() noexcept {
    // 128 perguntas por bloco com MIDI, sobre uma pilha de 16: barato e sem
    // estado paralelo para dessincronizar.
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

    // range-for, porque MidiBuffer::Iterator esta' depreciado no JUCE 8. A
    // MidiMessage sai de MidiMessageMetadata sem alocar: os bytes sao vista sobre o
    // proprio buffer.
    for (const auto metadata : midi) {
        const juce::MidiMessage message = metadata.getMessage();

        auto kind = rt::NoteTracker::Event::Kind::other;
        if (message.isAllNotesOff() || message.isAllSoundOff()) {
            kind = rt::NoteTracker::Event::Kind::allNotesOff;
        } else if (message.getControllerNumber() == kCcSustain) {
            kind = rt::NoteTracker::Event::Kind::sustain;
        } else if (message.getControllerNumber() == kCcDensity
                   || message.getControllerNumber() == kCcPosition
                   || message.getControllerNumber() == kCcModWheel) {
            kind = rt::NoteTracker::Event::Kind::controller;
        } else if (message.isNoteOn()) {
            kind = rt::NoteTracker::Event::Kind::noteOn;
        } else if (message.isNoteOff()) {
            kind = rt::NoteTracker::Event::Kind::noteOff;
        }

        // CC1 nao move parametro nenhum: e' fonte da matriz e so' modula onde ha uma
        // rota a ligar. CC74 e CC71 mantem o mapeamento directo que tinham e servem de
        // fonte tambem.
        const float normalised = message.getControllerValue() / 127.0f;
        if (message.getControllerNumber() == kCcDensity) {
            notes_.setController(kDensityController, normalised);
        } else if (message.getControllerNumber() == kCcPosition) {
            notes_.setController(kPositionController, normalised);
        } else if (message.getControllerNumber() == kCcModWheel) {
            notes_.setController(kModWheelController, normalised);
        }

        // o numero tem de ser o da nota. getControllerNumber() devolve 0 numa nota, e
        // passar aquele valor punha todas as notas em 0 (quatro oitavas abaixo) sem que
        // nada falhasse.
        notes_.handle({kind, message.getNoteNumber(), message.getControllerValue()});
    }

    publishPressedNotes();
}

void PluginProcessor::applyPendingControllerChanges() {
    if (!notes_.controllersChanged()) {
        return;
    }

    // so' os controladores que receberam CC neste bloco. escrever os dois sempre
    // fazia o mod wheel repor densidade e posicao no ultimo valor conhecido (0 no
    // arranque) e sujava a automacao do host com um valor que ninguem pediu.
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

    // a troca de material e' o unico ponto de cruzamento entre as threads: troca de
    // ponteiro, sem alocar e sem esperar.
    drainIncomingQueue();
    readNotes(midi);
    drainKeyboardNotes();
    updateHostTransport();

const auto numSamples = buffer.getNumSamples();

    // transporte e gate. O advance vem primeiro e um booleano local decide a
    // posicao que entra nos params e o gate que entra no motor.
    // booleano local e nao duas leituras do atomico: um stop no meio daria posicao
    // de um estado e gate de outro, e o audio sairia de uma regiao ja' largada.
    const auto playing = transport_.isPlaying();
    const auto held = transportHold_.load(std::memory_order_relaxed);

    auto params = currentParams();
    // scan Speed nao e' parametro do motor, e' taxa do transporte. Um store relaxado
    // por bloco e' o custo, e setRate ja' recorta NaN para 1.
    transport_.setRate(static_cast<double>(*parameters_.getRawParameterValue("scanspeed")));
    if (playing) {
        // advance(0) le' e aplica a ancora sem andar. e' o HOLD: a cabeca fica
        // onde esta', o gate continua aberto e os graos continuam a nascer.
        params.position = transport_.advance(held ? 0 : numSamples, playing);
    }

    // o OR com as notas faz o botao soar sem MIDI: o motor so' precisa de gate
    // aberto, e gate aberto sem nota e' o transporte. setSounding e' a mesma porta
    // das notas e e' chamada a cada bloco, por isso o disparo e' na aresta.
    engine_.setSounding(notes_.sounding() || playing);
    engine_.processBlock(buffer.getWritePointer(0),
                         buffer.getWritePointer(1),
                         numSamples,
                         params);

    publishTelemetry(buffer);
}

// pico, vozes ativas e posicao do transporte, depois do motor e antes de sair: a
// leitura e' sobre o bloco que acabou de ser escrito.
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
        // -100 dB e' o silencio. o -1 antigo era um sentinela de "sem leitura",
        // mas o medidor lia-o como um nivel quase cheio e ficava todo acesso
        // mesmo com a saida muda.
        return (value > 1.0e-6f) ? juce::Decibels::gainToDecibels(value) : -100.0f;
    };
    const auto decibels = toDb(peak);

    // relaxado chega: sao valores de mostrador, e a ordenacao que interessa e' dentro
    // do bloco, que ja' terminou.
    telemetry_.peakDb.store(decibels, std::memory_order_relaxed);
    telemetry_.peakLeftDb.store(toDb(peakLeft), std::memory_order_relaxed);
    telemetry_.peakRightDb.store(toDb(peakRight), std::memory_order_relaxed);
    telemetry_.activeVoices.store(engine_.lastActiveVoices(), std::memory_order_relaxed);

    // sounding e' o mesmo OU que o motor recebeu, nao notes_.sounding(): o rodape
    // responde "esta a sair som", e com transporte a resposta e' sim mesmo sem nota.
    const auto audible = notes_.sounding() || transport_.isPlaying();
    telemetry_.sounding.store(audible, std::memory_order_relaxed);
    telemetry_.playing.store(transport_.isPlaying(), std::memory_order_relaxed);
    telemetry_.held.store(transportHold_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
    telemetry_.playheadFraction.store(transport_.positionFraction(),
                                      std::memory_order_relaxed);

    // envelope e cortes vivos: os visualizadores dos modulos ENV e FILTER leem
    // daqui. o corte 2 so' existe depois do primeiro bloco filtrado; zero e'
    // "ainda nao sei" e a vista cai no valor do knob.
    telemetry_.envelopeLevel.store(engine_.gateLevel(), std::memory_order_relaxed);
    telemetry_.envelopeStage.store(engine_.envelopeStagePhase(), std::memory_order_relaxed);
    telemetry_.envelopePhase.store(engine_.envelopePhaseIndex(), std::memory_order_relaxed);
    telemetry_.cutoff1Hz.store(engine_.f1CutoffHz(), std::memory_order_relaxed);
    telemetry_.cutoff2Hz.store(engine_.f2CutoffHz(), std::memory_order_relaxed);
}


void PluginProcessor::applyFactoryPreset(int index) {
    const auto& presets = factoryPresets();
    if (index < 0 || static_cast<std::size_t>(index) >= presets.size()) {
        return;
    }
    const auto& preset = presets[static_cast<std::size_t>(index)];

    // por ID, nao por ordem do layout: a tabela e' a fonte e a ordem pode mudar.
    // convertTo0to1 porque setValueNotifyingHost fala normalizado.
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
    // nuvem, largura, glide e o caracter do filtro 1. sem estes, os presets
    // tonais que dependem do corte (Keys, Bass) abriam com o filtro aberto e a
    // largura a 100 %, que e' o Init disfarcado.
    set("cloudsize", preset.cloudsize);
    set("pitchspread", preset.pitchspread);
    set("grainjit", preset.grainjit);
    set("width", preset.width);
    set("glide", preset.glide);
    set("f1slope", preset.f1slope);
    set("f1drive", preset.f1drive);
    set("f1keytrack", preset.f1keytrack);
    set("f1env", preset.f1env);
    // as tres curvas. sem elas o Pluck decai em rampa e soa a teclado, nao a
    // corda: e' a curva negativa que faz o nivel cair depressa e arrastar a cauda.
    set("atkcurve", preset.atkcurve);
    set("deccurve", preset.deccurve);
    set("relcurve", preset.relcurve);
}

// nao-const porque copyState e' nao-const no JUCE: copiar o estado mexe no undo
// interno da arvore.
std::unique_ptr<juce::XmlElement> PluginProcessor::stateXml() {
    auto state = parameters_.copyState();

    // o binario vai no estado: sem ele o projeto abre com os parametros certos e o
    // som errado.
    state.setProperty("sourcePath", sourcePath_, nullptr);

    // a regiao tambem, senao o projeto abre a tocar outra parte do ficheiro sem o
    // utilizador saber porque. Propriedade e nao PARAM, para nao virar automatizavel.
    state.setProperty("byteStart", static_cast<std::int64_t>(byteRange_.start), nullptr);
    state.setProperty("byteEnd", static_cast<std::int64_t>(byteRange_.end), nullptr);

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

    // estado gravado antes das rotas serem parametros traz o no MODMATRIX: copia
    // as rotas para os parametros e remove o no. Estado novo nao tem o no e a
    // migracao sai no primeiro if.
    migrateLegacyModMatrix();

// o ficheiro e' reingerido pela mesma via do arrasto: ler, converter, publicar.
// nao ha caminho paralelo.
    const auto tree = parameters_.state;
    const auto path = tree.getProperty("sourcePath").toString();
    if (path.isNotEmpty() && juce::File(path).existsAsFile()) {
        if (ingest(path)) {
            // a regiao so' e' restaurada se o ingest passou. Aplicada antes, corria contra
            // um buffer vazio e o projeto abria mudo sem dar erro.
            const auto start = static_cast<std::uint64_t>(
                juce::jmax<juce::int64>(0, tree.getProperty("byteStart").toString()
                                                     .getLargeIntValue()));
            const auto end = static_cast<std::uint64_t>(
                juce::jmax<juce::int64>(0, tree.getProperty("byteEnd").toString()
                                                   .getLargeIntValue()));
            // regiao guardada invalida neste ficheiro nao e' erro: o ficheiro pode ter
            // mudado desde o save. O ingest ja' deixou material auditavel.
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
    // falso nao e' erro: o host pode entregar qualquer blob, e o contrato do
    // setStateInformation e' ignorar o que nao reconhece.
    static_cast<void>(loadStateXml(*xml));
}

bool PluginProcessor::savePresetToFile(const juce::File& file) {
    auto xml = stateXml();
    if (xml == nullptr) {
        return false;
    }
    // replaceWithText e nao append: um .opcoda anterior tem de sair inteiro, senao
    // o ficheiro ficava com dois documentos.
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

// entrada que o JUCE procura pelo nome, sem namespace. Dentro de opcoda:: linka
// como createPluginFilter@@ e o Standalone nao resolve.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new opcoda::PluginProcessor();
}