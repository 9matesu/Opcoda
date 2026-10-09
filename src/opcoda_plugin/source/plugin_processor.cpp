#include "plugin_processor.h"

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
    // getRawParameterValue da o ponteiro para o valor guardavel, e o valor e'
    // o que o utilizador ve: nao e' normalizado. Confundir os dois espacos
    // entrega ao motor 0.4 em vez de 40 e nao da erro nenhum — e um bug que so
    // aparece como som esquisito.
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
    // O transporte precisa da taxa de amostragem para saber quantos bytes por
    // amostra consome, e e' a unica vez que a recebe: depois do primeiro bloco o
    // host pode mudar a taxa sem avisar, e quem continua a ter a verdade e' o
    // metodo de prepare.
    transport_.prepare(sampleRate);
}

void PluginProcessor::releaseResources() {
    engine_.reset();
    // O transporte e' reposto pela via do pedido, nao por escrito directo: quem
    // escreve em position_ e' advance(), e so' advance' escreve.
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

    // Os bytes crus ficam para o seletor de bytes converter qualquer janela sem
    // voltar a ler o disco.
    sourceBytes_ = std::move(bytes);

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

    // A curva de entropia do ficheiro inteiro foi embora com o componente que a
    // desenhava. O rodape passou a medir a entropia da janela de 256 bytes onde
    // esta a cabeca de leitura, que e' o que interessa e custa um histograma por
    // tique em vez de 320 no ingest.

    // O seletor arranca na primeira secao com dados, e nao em zero: o cabecalho
    // PE nao produz audio reconhecivel, e comecar la faria o instrumento soar
    // errado logo apos o primeiro arrasto.
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

    // A curva e as secoes estao publicadas acima; falta o audio. Se a primeira
    // janela nao produzir audio, o ingest e' um sucesso sem som, que e' o mesmo
    // que recusa so sem o codigo de erro.
    return publishByteRange();
}

bool PluginProcessor::setByteRange(std::uint64_t start, std::uint64_t end,
                                   bool rememberExactRange) {
    // A janela e' validada no nucleo, e nao aqui: e' a unica forma de a regra ter
    // teste, porque o nucleo nao depende de JUCE e este ficheiro depende.
    pe::ByteRange next {start, end};
    if (!pe::clampByteRange(next, static_cast<std::uint64_t>(sourceBytes_.size()))) {
        return false;
    }

    // Pedir a mesma janela nao republica material: republicar interromperia o
    // audio para trocar por uma copia igual, sem ganho nenhum.
    if (next.start == byteRange_.start && next.end == byteRange_.end) {
        return true;
    }

    // Falha nao mexe no estado: a thread de audio continua com o material
    // anterior em vez de ficar sem fonte.
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

    // O nucleo decide, porque a regra tem teste la e nao aqui. Aqui so se junta
    // a lista de secoes do PE ao formato que a funcao espera, e se publica o
    // resultado.
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
        // Nada a mudar. Republicar material identico interromperia o audio para
        // trocar por uma copia igual.
        return true;
    }

    if (!setByteRange(next.start, next.end, false)) {
        return false;
    }

    // Ao alinhar, a regiao anterior passa a ser a exacta a que se pode voltar.
    // Ao voltar a exacta, a exacta continua a ser a mesma, e nao a regiao
    // alinhada: senao o segundo duplo clique saltava para o sitio errado.
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

    // A curva de entropia e' calculada AQUI, na thread de interface, e nao no
    // motor. Sao 256 pontos da regiao, e o custo e' irrelevante fora do callback —
    // enquanto dentro do callback era uma passagem completa por troca de material.
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
        // Sem colunas a curva fica a zeros e o motor usa o valor medio, que e' o
        // mesmo que a curva desativada daria.
    }

    // Se a fila de entrada esta cheia, a thread de audio ainda nao consumiu a
    // publicacao anterior. Recusar e melhor do que crescer: a memoria e' do
    // produtor, e uma alocacao ali custaria o contrato de tempo real.
    owned_.push_back(std::move(buffer));
    entropyCurves_.push_back(std::move(curve));

    const PublishedMaterial material {owned_.back().get(), entropyCurves_.back().data()};
    if (!incoming_.push(material)) {
        owned_.pop_back();
        entropyCurves_.pop_back();
        lastError_ = "E_BUSY";
        return false;
    }

    // O transporte so sabe o comprimento da regiao quando lha dizemos. Sem
    // esta linha a duracao de uma volta seria a do material anterior, e o
    // primeiro transporte depois de carregar um ficheiro arrastaria a duracao do
    // ficheiro que estava la antes.
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
            // Devolve o ponteiro antigo antes de trocar, para que a interface
            // nunca libere memoria que a thread de audio ainda esta lendo.
            returned_.push({previous, nullptr});
        }
        live_.store(incoming.samples, std::memory_order_release);
        // **So' copia o ponteiro da curva.** A medicao foi feita na interface; aqui
        // ha um copy_n de no maximo 1024 floats e nada mais. A versao anterior
        // media a entropia do material inteiro dentro deste metodo, que e' chamado
        // de processBlock.
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
            // A curva segue o buffer. Esvaziar so' as amostras deixaria o indice
            // desalinhado e a proxima publicacao escreveria a curva no sitio errado.
            if (index < entropyCurves_.size()) {
                entropyCurves_.erase(entropyCurves_.begin() +
                                     static_cast<std::ptrdiff_t>(index));
            }
        }
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

    // Transporte e gate. A ordem e' o que impede som sem posicao ou posicao sem
    // som: o advance vem primeiro, e o mesmo booleano local decide a posicao que
    // entra nos params e o gate que entra no motor.
    //
    // **Um booleano local e nao duas leituras do atomico.** Se o utilizador carregar
    // em stop entre as duas, a posicao e' de um estado e o gate de outro, e o
    // resultado e' audio a sair de uma regiao que o transporte ja largou.
    const auto playing = transport_.isPlaying();

    auto params = currentParams();
    if (playing) {
        params.position = transport_.advance(numSamples, playing);
    }

    // O OR com as notas e' o que faz o botao funcionar sem MIDI: o motor so'
    // precisa de gate aberto para produzir, e gate aberto sem nota e' exactamente
    // o que o transporte e'. O ADSR com defaults (attack 5 ms) continua a valer,
    // porque setSounding e' a mesma porta que as notas usam — e e' chamada a
    // cada bloco, por isso o disparo e' na aresta e nao no nivel.
    engine_.setSounding(notes_.sounding() || playing);
    engine_.processBlock(buffer.getWritePointer(0),
                         buffer.getWritePointer(1),
                         numSamples,
                         params);

    publishTelemetry(buffer);
}

// Publica o pico do bloco, as vozes ativas e a posicao do transporte. Roda depois
// do motor e antes de sair: a leitura e' sobre o bloco que acabou de ser escrito.
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

    // O `sounding` e' o mesmo OU que o motor recebeu, e nao notes_.sounding(): o
    // rodape e' a resposta a "esta a sair som", e durante o transporte a resposta
    // e' sim mesmo sem nota nenhuma.
    const auto audible = notes_.sounding() || transport_.isPlaying();
    telemetry_.sounding.store(audible, std::memory_order_relaxed);
    telemetry_.playing.store(transport_.isPlaying(), std::memory_order_relaxed);
    telemetry_.playheadFraction.store(transport_.positionFraction(),
                                      std::memory_order_relaxed);
}


void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters_.copyState();

// O binario carregado vai para o estado porque sem ele o projeto abre com
    // os parametros certos e o som errado, que e' pior do que nao abrir nada.
    state.setProperty("sourcePath", sourcePath_, nullptr);

    // A regiao tambem vai, pelo mesmo motivo: sem ela o projeto abre a tocar
    // outra parte do ficheiro, e o utilizador nao sabe porque. Fica como
    // propriedade e nao como PARAM, para nao virar um setimo parametro
    // automatizavel: a Tabela 8 tem seis e o ensaio T4 mede seis.
    state.setProperty("byteStart", static_cast<std::int64_t>(byteRange_.start), nullptr);
    state.setProperty("byteEnd", static_cast<std::int64_t>(byteRange_.end), nullptr);

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
    const auto tree = parameters_.state;
    const auto path = tree.getProperty("sourcePath").toString();
    if (path.isNotEmpty() && juce::File(path).existsAsFile()) {
        if (ingest(path)) {
            // A regiao so e' restaurada se o ingest passou. Aplicar antes
            // faria setByteRange Working contra um buffer vazio, e o projeto
            // abriria mudo sem dar erro.
            const auto start = static_cast<std::uint64_t>(
                juce::jmax<juce::int64>(0, tree.getProperty("byteStart").toString()
                                                     .getLargeIntValue()));
            const auto end = static_cast<std::uint64_t>(
                juce::jmax<juce::int64>(0, tree.getProperty("byteEnd").toString()
                                                   .getLargeIntValue()));
            // Uma regiao guardada invalida neste ficheiro nao e' erro: o ficheiro pode
            // ter mudado desde que o projeto foi guardado. O ingest ja deixou
            // material auditavel, entao segue com ele.
            if (end > start) {
                static_cast<void>(setByteRange(start, end));
            }
        }
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