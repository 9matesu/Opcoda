#include "plugin_processor.h"

#include "opcoda_core/pe/byte_to_sample.h"
#include "opcoda_core/pe/pe_parser.h"

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
    lastError_.clear();
    return true;
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

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    // A troca de material e o unico ponto de cruzamento entre as threads, e
    // acontece por troca de ponteiro, sem alocar e sem esperar.
    drainIncomingQueue();

    const auto numSamples = buffer.getNumSamples();
    engine_.processBlock(buffer.getWritePointer(0),
                         buffer.getWritePointer(1),
                         numSamples,
                         currentParams());
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