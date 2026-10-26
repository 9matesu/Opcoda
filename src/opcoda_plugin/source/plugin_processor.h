#pragma once

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/rt/note_tracker.h"
#include "opcoda_core/rt/spsc_ring.h"
#include "opcoda_core/rt/transport.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace opcoda {

// a fila carrega ponteiro cru, nunca shared_ptr: SpscRing exige tipo trivial
// no slot, e soltar a ultima referencia na thread de audio pode rodar
// operator delete ali dentro. A memoria e' da interface, em owned_, e so' sai
// depois de o audio devolver o ponteiro pela fila de retorno.
using SampleBuffer = std::shared_ptr<const std::vector<float>>;
// o que viaja pela fila: amostras e a curva de entropia ja' reduzida.
// a curva vem pronta, medida na interface; dentro do callback custava uma
// passagem pela regiao a cada publicacao.
// struct trivial: dois ponteiros, nada mais.
struct PublishedMaterial {
    const std::vector<float>* samples {nullptr};
    const float* entropyCurve {nullptr};
};

using SamplePtr = const std::vector<float>*;

// AudioProcessor com editor minimo: carrega um binario e produz audio.
// o visual completo entra na etapa F008.
class PluginProcessor : public juce::AudioProcessor {
public:
    PluginProcessor();

    const juce::String getName() const override { return JucePlugin_Name; }
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // aceita MIDI porque e' instrumento: sem isso o host nao entrega eventos.
    // nao produz MIDI e nao e' efeito de MIDI, por isso as duas abaixo sao falsas.
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // cauda = maior grao: e' o tempo que um grao ja' nascido ainda tem para
    // acabar depois de o gate fechar.
    // devolver 0 valia sem transporte, quando a nota acabava com o gate. Com o
    // botao, o STOP fecha o gate e deixa ate' 100 ms de graos em voo; sem cauda
    // declarada o host corta-os a meio.
    double getTailLengthSeconds() const override {
        return static_cast<double>(dsp::GranularEngine::kMaxGrainMs) / 1000.0;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

// estado: parametros, ficheiro carregado e regiao.
// copyState e replaceState da APVTS tratam dos parametros; sem eles o projeto
// reabre nos defaults e a automatizacao do host some. O binario vai na mesma
// arvore Value, e o .opcoda e' esse XML em ficheiro: stateXml/loadStateXml,
// um formato so'.
void getStateInformation(juce::MemoryBlock& destData) override;
void setStateInformation(const void* data, int sizeInBytes) override;

// presets de fabrica (F016): os pontos de partida de factory_presets.h. So' na
// thread de interface, com undo do host a cada setValueNotifyingHost.
void applyFactoryPreset(int index);

// XML do estado completo, nullptr se a arvore nao serializar. Quem guarda
// decide o contentor; o conteudo e' um so'.
[[nodiscard]] std::unique_ptr<juce::XmlElement> stateXml();
// aplica um XML como o do stateXml. false sem tocar em nada se nao for estado
// opcoda: sessao de outro plugin nao cala este.
bool loadStateXml(const juce::XmlElement& xml);
bool savePresetToFile(const juce::File& file);
bool loadPresetFromFile(const juce::File& file);

    // thread de interface. Le o ficheiro, converte e publica na fila; false
    // preenche lastError.
    bool ingest(const juce::String& path);

    [[nodiscard]] juce::String lastError() const noexcept { return lastError_; }
    [[nodiscard]] juce::String sourceName() const noexcept { return sourceName_; }

    // caminho do binario activo. quem desfaz compara-o para saber se basta repor
    // parametros ou se tem de recarregar o material.
    [[nodiscard]] const juce::String& sourcePath() const noexcept { return sourcePath_; }

    // metadados do material carregado, para a faixa de arquivo do editor. Escrito
    // no ingest e lido na interface: mesma thread, nenhum campo precisa de atomico.
    struct SourceInfo {
        juce::String name;
        juce::String formatTag;  // "[64-bit PE]", igual ao mock
        std::int64_t sizeBytes {0};

        // uma linha por secao, com entropia em bits por byte. O display desenha as abas
        // proporcionais ao tamanho.
        struct SectionInfo {
            juce::String name;
            std::uint32_t rawOffset {0};
            std::uint32_t rawSize {0};
            double entropy {0.0};
        };

std::vector<SectionInfo> sections;
    };

// -- seletor de bytes --
// escolhe que regiao do binario alimenta o motor. Janela contigua [start, end),
// nao indice de secao. A struct e' do nucleo, em pe/byte_range.h: a regra de
// validacao tem teste la'.
    using ByteRange = pe::ByteRange;

    // regiao corrente. Comeca na primeira secao com dados e o seletor move-a.
    [[nodiscard]] const ByteRange& byteRange() const noexcept { return byteRange_; }

    // move o seletor. false sem mexer em nada se o intervalo for vazio, invertido
    // ou sair do ficheiro.
    // validar aqui evita publicar na fila um buffer que so' falharia no callback.
    bool setByteRange(std::uint64_t start, std::uint64_t end, bool rememberExactRange = true);

// alterna entre a regiao exacta e a secao PE mais proxima. E' o duplo clique e
// o Enter no seletor.
// alterna em vez de alinhar sempre: sem a volta, quem entrasse numa secao nao
// tinha como voltar ao byte.
// a decisao e' pe::sectionSnapToggle, no nucleo, que tem teste. Aqui so' se
// juntam as secoes e se publica.
bool snapByteRangeToSection();

// true quando a regiao esta' alinhada numa secao com dados brutos.
[[nodiscard]] bool byteRangeIsSectionAligned() const noexcept;

    // telemetria do callback, lida pelo editor a 20 Hz. Sao atomicos soltos e nao
    // uma struct com lock: nao ha snapshot consistente entre campos, o que para
    // pico e contagem de vozes nao pesa.
    struct Telemetry {
        std::atomic<float> peakDb {-100.0f};
        // pico por canal: o medidor do canto inferior direito e' stereo, como o
        // mestre de uma mesa. peakDb continua a ser o maximo dos dois, para quem
        // so' quer o tecto.
        std::atomic<float> peakLeftDb {-100.0f};
        std::atomic<float> peakRightDb {-100.0f};
        std::atomic<int> activeVoices {0};
        std::atomic<bool> sounding {false};
        std::atomic<float> sampleRate {48000.0f};

        // transporte em tres campos: onde esta' a cabeca (do callback, por bloco) e
        // quanto tempo dura a volta (da interface).
        std::atomic<float> playheadFraction {0.0f};
        std::atomic<double> playDurationSeconds {0.0};
        std::atomic<bool> playing {false};
        // HOLD congela a cabeca sem fechar o gate: os graos continuam a nascer
        // na posicao actual. e' o latch do transporte, publicado para o botao.
        std::atomic<bool> held {false};

        // envelope e filtros, para os visualizadores dos modulos ENV e FILTER: o
        // ponto do ADSR e a resposta mostram o que o audio esta' a fazer, nao o
        // que o knob pediu. o corte vem ja' com keytrack, envelope e lfo.
        std::atomic<float> envelopeLevel {0.0f};
        std::atomic<float> envelopeStage {0.0f};
        std::atomic<int> envelopePhase {0};
        std::atomic<float> cutoff1Hz {0.0f};
        std::atomic<float> cutoff2Hz {0.0f};
    };

    [[nodiscard]] const Telemetry& telemetry() const noexcept { return telemetry_; }

    // teclas fisicamente premidas, em dois uint64 (128 notas): o teclado do
    // editor acende o que chega do MIDI do host, nao so' o que se toca com o
    // rato. dois atomicos e nao um de 128 bits, que nao existe; a leitura pode
    // apanhar as duas metades de instantes diferentes e isso nao se ve.
    struct PressedNotes {
        std::uint64_t low {0};
        std::uint64_t high {0};
    };

    [[nodiscard]] PressedNotes pressedNotes() const noexcept {
        return {pressedLow_.load(std::memory_order_relaxed),
                pressedHigh_.load(std::memory_order_relaxed)};
    }

    // onde esta' cada grao: o callback publica uma vez por bloco, o editor le a
    // 60 Hz e desenha sobre a forma de onda.
    // nota: ler aqui e' seguro; chamar processBlock da interface nao, porque esse
    // e' o caminho que escreve no motor.
    [[nodiscard]] const dsp::GrainTelemetry& grainTelemetry() const noexcept {
        return engine_.telemetry();
    }

    // transporte de audicao: percorre a regiao sem nota MIDI, para ouvir o material
    // sem teclado. Os metodos abaixo sao da interface, que nunca chama advance();
    // isso acontece dentro de processBlock.
    [[nodiscard]] bool isTransportPlaying() const noexcept { return transport_.isPlaying(); }

    // verdadeiro quando a regiao produz som. Com menos de tres bytes nao ha posicao
    // que o motor leia, e o botao fica desativado.
    [[nodiscard]] bool hasPlayableRegion() const noexcept { return transport_.hasRegion(); }

    void startTransport() noexcept { transport_.play(); }
    void stopTransport() noexcept { transport_.stop(); }
    void toggleTransport() noexcept {
        if (transport_.isPlaying()) {
            transport_.stop();
        } else {
            transport_.play();
        }
    }

    // HOLD: congela a cabeca do transporte sem fechar o gate. o motor continua a
    // disparar graos, mas sempre na mesma posicao — o material fica em suspensao
    // ate' se largar o latch. atomic porque o botao e' da interface e a leitura e'
    // do callback.
    void setTransportHold(bool hold) noexcept {
        transportHold_.store(hold, std::memory_order_relaxed);
    }

    [[nodiscard]] bool isTransportHeld() const noexcept {
        return transportHold_.load(std::memory_order_relaxed);
    }

    // teclado virtual e teclas do computador. A interface empurra o evento e o
    // callback consome na fila: entra pelo mesmo NoteTracker que o MIDI do host,
    // sem passar pelo MidiBuffer e sem alocar no audio.
    void pushKeyboardNote(int noteNumber, float velocity, bool on) noexcept;

    // ancora de busca do transporte, vinda do parametro POSITION e so' quando o
    // utilizador o move. Escrita a cada quadro pelo editor, a cabeca nunca saia do
    // ponto de ancoragem.
    void seekTransportTo(float fraction) noexcept { transport_.seekToFraction(fraction); }

    [[nodiscard]] float transportPosition() const noexcept {
        return transport_.positionFraction();
    }

    [[nodiscard]] double transportDurationSeconds() const noexcept {
        return transport_.durationSeconds();
    }

    // verdadeiro so' quando sabemos que o host esta' parado. E' o inverso de "o
    // host esta' a tocar", e a distincao e' o ponto.
    // o motor so' existe enquanto o host chama processBlock, e host parado nao
    // chama: aceitar o toque e nao produzir nada e' pior do que recusar, e por isso
    // a interface desativa o botao.
    // getPosition() devolve nullopt em host sem informacao de tempo, e ai' a
    // resposta e' false: desconhecido nao e' parado. Tratar como parado deixava o
    // standalone sem dispositivo de audio com o botao travado para sempre.
    [[nodiscard]] bool hostTransportIsKnownToBeStopped() const noexcept {
        const auto* head = getPlayHead();
        if (head == nullptr) {
            // sem host nao ha quem mande: false evita o botao travado num teste.
            return false;
        }

        // JUCE 8 usa getPosition(), que devolve Optional. CurrentPositionInfo com
        // isPlaying esta' depreciada; aviso depreciado parte o build com /WX.
        const auto position = head->getPosition();
        if (!position.hasValue()) {
            return false; // host sem tempo: desconhecido, nao parado
        }
        return !position->getIsPlaying();
    }

    [[nodiscard]] const SourceInfo& sourceInfo() const noexcept { return sourceInfo_; }

    // bytes crus do ficheiro, para a grelha mostrar os bytes. Guardados no ingest
    // para nao haver I/O de disco a cada redesenho.
    // o vector nao muda de endereco quando o ingest troca o conteudo, por isso a
    // grelha guarda o ponteiro e nao uma copia.
    // so' a interface escreve e le. O audio nunca ve' isto: so' o buffer
    // convertido, pela fila.
    [[nodiscard]] const std::vector<std::uint8_t>& sourceBytes() const noexcept {
        return sourceBytes_;
    }

    // aplica na interface os ultimos CCs recebidos.
    // escrever no parametro da thread de audio foi descartado: setValueNotifyingHost
    // notifica listeners, e o SliderParameterAttachment e' um deles — acesso
    // cruzado ao GUI a partir do callback.
    // custo: a latencia do timer do editor, uns 50 ms. Em troca o parametro segue a
    // fonte unica da verdade, com o host notificado e o knob atras.
    void applyPendingControllerChanges();

    // drena o teclado virtual no callback, antes de currentParams(), para a nota
    // deste bloco ja' entrar na altura.
    void drainKeyboardNotes() noexcept;

    // -- F020: matriz de modulacao --
    // desde 24/10/2026 cada rota e' parametro (m1src/m1dst/m1amt ... m8*): o host
    // automatiza, o preset leva e a vista MATRIX escreve por attachment. O audio
    // le os 24 raws por bloco, por ponteiro pre-resolvido, sem alocar.
    [[nodiscard]] dsp::ModMatrix readModMatrix() const noexcept;

    // projeto gravado antes das rotas serem parametros traz o no MODMATRIX.
    // copia as rotas para os parametros e remove o no. Roda na thread de
    // mensagem, depois de replaceState.
    void migrateLegacyModMatrix();

    // le BPM e ppq do host uma vez por bloco e guarda em atomicos. O nucleo nao
    // conhece o host; estes dois doubles sao tudo o que atravessa.
    // sem transporte (Standalone) fica 120 e -1, que o motor le como "sem ppq".
    void updateHostTransport() noexcept;
    // live_ e' publicado pelo audio e lido pela interface. Sem o atomico a leitura
    // seria uma corrida com a troca de material, que o compilador nao flagra: os
    // ponteiros sao do mesmo tipo.
    [[nodiscard]] bool hasSource() const noexcept {
        return live_.load(std::memory_order_acquire) != nullptr;
    }

    // layout dos parametros expostos ao host para automacao. O estado e' lido uma
    // vez por bloco e copiado para o motor, que nao guarda referencia: e' o que
    // deixa o caminho de audio fora do estado do host.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    [[nodiscard]] dsp::GranularParams currentParams() const noexcept;

    [[nodiscard]] juce::AudioProcessorValueTreeState& parameters() noexcept { return parameters_; }
    [[nodiscard]] const juce::AudioProcessorValueTreeState& parameters() const noexcept { return parameters_; }

    // thread de interface, periodico: so' agora o ponteiro devolvido pelo audio
    // pode ser liberado.
    void releaseReturnedBuffers();

private:
    // ponto unico de entrada da amostra no audio. Troca o material ativo pelo
    // recem-chegado e devolve o antigo a' interface, a unica que libera memoria.
    void drainIncomingQueue() noexcept;

    // ponteiros raw das 24 celulas da matriz, resolvidos uma vez na construcao.
    // ler getRawParameterValue por id no bloco construiria juce::String no audio.
    struct RouteRaw {
        const std::atomic<float>* source {nullptr};
        const std::atomic<float>* destination {nullptr};
        const std::atomic<float>* amount {nullptr};
    };
    std::array<RouteRaw, dsp::kMaxModRoutes> routeRaw_ {};

    // le o MidiBuffer do bloco e atualiza o NoteTracker. Roda no audio, sem alocar
    // nem bloquear: o MidiBuffer ja' traz os contadores resolvidos.
    void readNotes(const juce::MidiBuffer& midi) noexcept;

    // publica o bitmask de teclas premidas. roda no audio, so' quando entraram
    // eventos: um bloco sem MIDI nao tem o que mudar.
    void publishPressedNotes() noexcept;

    void publishTelemetry(const juce::AudioBuffer<float>& buffer) noexcept;

    // copia crua do ficheiro, para o seletor converter qualquer janela sem voltar
    // ao disco. O accessor publico e' o sourceBytes(); este e' o vector.
    std::vector<std::uint8_t> sourceBytes_;

    // converte a janela corrente e publica na fila. false sem mexer em nada se a
    // janela nao produz audio.
    bool publishByteRange();

    opcoda::dsp::GranularEngine engine_;

    // duas filas em sentidos opostos, ponteiro cru: a interface publica material
    // novo e recolhe o que o audio ja' terminou de usar.
    rt::SpscRing<PublishedMaterial, 4> incoming_;
    rt::SpscRing<PublishedMaterial, 4> returned_;

    // teclado virtual. Capacidade 64: um bloco de 2048 a 48 kHz dura 43 ms, e nesse
    // tempo nao se carrega em 64 teclas. Cheia, descarta — como as filas sem trava.
    rt::SpscRing<rt::NoteTracker::Event, 64> keyboardNotes_;

    // tempo do host: escrito por bloco pelo callback e lido por currentParams() no
    // mesmo bloco. Relaxado chega, nao ordena nada entre os dois.
    std::atomic<double> hostBpm_ {120.0};
    std::atomic<double> hostPpq_ {-1.0};

    // donos de toda a memoria de amostra. So' a interface escreve, e so' remove
    // depois de o audio devolver o buffer.
    std::vector<SampleBuffer> owned_;

    // curva de entropia de cada buffer, no mesmo indice de owned_, e viaja com o
    // ponteiro na fila.
    std::vector<std::vector<float>> entropyCurves_;

    std::atomic<SamplePtr> live_ {nullptr};
    juce::String lastError_;
    juce::String sourceName_;
    SourceInfo sourceInfo_;
    ByteRange byteRange_;

    // inicio exacto, sem alinhamento, da ultima regiao escolhida. E' o destino do
    // duplo clique com a regiao alinhada, e o que torna a operacao reversivel.
    ByteRange lastExactRange_ {};

    // caminho do binario ativo, para o estado do host. Distinto de sourceName_, que
    // e' so' o nome do ficheiro para a interface.
    juce::String sourcePath_;

    // notas premidas e CCs mapeados. Vive no audio porque e' estado do audio e o
    // host so' entrega MIDI durante processBlock.
    // a regra esta' em rt::NoteTracker, no nucleo e sem JUCE, para ter teste. Aqui
    // so' se traduz MidiMessage em Event.
    rt::NoteTracker notes_;

    // transporte de audicao, ao lado da engine e nao dentro: a engine responde "que
    // som produzir agora", o transporte "de onde comecar". O motor recebe a
    // posicao ja' resolvida e nao sabe que existe transporte.
    rt::Transport transport_;

    // latch do transporte. escrito pela interface, lido no callback.
    std::atomic<bool> transportHold_ {false};

    // teclas premidas, escrito pelo audio e lido pela interface.
    std::atomic<std::uint64_t> pressedLow_ {0};
    std::atomic<std::uint64_t> pressedHigh_ {0};

    Telemetry telemetry_;

    static constexpr int kCcSustain = 64;
    static constexpr int kCcModWheel = 1;  // roda de modulacao, fonte da matriz
    static constexpr int kCcDensity = 74;  // Brightness, mapeia em DENSITY
    static constexpr int kCcPosition = 71; // Resonance, mapeia em POSITION
    static constexpr int kDensityController = 0;
    static constexpr int kPositionController = 1;
    static constexpr int kModWheelController = 2;

    static constexpr const char* kModMatrixNode = "MODMATRIX";

    juce::AudioProcessorValueTreeState parameters_;
};

} // namespace opcoda
