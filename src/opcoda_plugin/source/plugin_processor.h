#pragma once

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/rt/note_tracker.h"
#include "opcoda_core/rt/spsc_ring.h"
#include "opcoda_core/rt/transport.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <vector>

namespace opcoda {

// A fila carrega ponteiro cru, nunca shared_ptr. Sao duas razoes que apontam
// para a mesma escolha: SpscRing exige tipo trivial para poder copiar o slot
// sem contagem de referencia, e decrementar um contador na thread de audio
// pode rodar o operator delete ali dentro.
//
// A memoria e' da thread de interface, que a mantem viva em owned_ e so a
// libera depois que a thread de audio devolveu o ponteiro pela fila de
// retorno.
using SampleBuffer = std::shared_ptr<const std::vector<float>>;
// O que viaja pela fila: as amostras E a curva de entropia ja reduzida.
//
// **A curva viaja porque nao pode ser calculada na thread de audio.** O motor
// modulava a dispersao dos graos pela entropia local, e media-a dentro de
// processBlock: O(regiao) por troca de material, sem alocar nada — o histograma e'
// de pilha — e portanto invisivel para o guard de alocacao. Com o transporte,
// arrastar a regiao republica o material a cada evento de rato, e cada publicacao
// virava uma passagem completa na thread de audio.
//
// A struct tem de continuar trivial para a SpscRing: dois ponteiros, nada mais.
struct PublishedMaterial {
    const std::vector<float>* samples {nullptr};
    const float* entropyCurve {nullptr};
};

using SamplePtr = const std::vector<float>*;

// AudioProcessor com editor minimo: a forma que carrega um binario e produz
// audio. O visual completo entra na etapa F008.
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

    // Aceita MIDI porque e' um instrumento: sem isso o host nao entrega
    // eventos. Nao produz MIDI e nao e' efeito de MIDI, entao as duas funcoes
    // abaixo continuam falsas.
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // A cauda e' o maior grao, porque e' o tempo que um grao que ja nasceu ainda
    // tem para acabar depois de o gate fechar.
    //
    // **Devolver 0 era verdade antes do transporte e passou a ser mentira.** Sem
    // transporte so havia notas, e uma nota acabava com o gate. Com o botao, o gate
    // fecha no STOP e ha ate' 100 ms de graos em voo — e o host, avisado de que nao
    // ha cauda, corta-os. Carregar STOP sem uma tecla premida passava a dar um
    // corte duro a meio de um grao, e a rampa de 5 ms do gate protecte precisamente
    // o sitio que o corte apagava.
    double getTailLengthSeconds() const override {
        return static_cast<double>(dsp::GranularEngine::kMaxGrainMs) / 1000.0;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

// Estado: os seis parametros e o ficheiro carregado.
//
// copyState e replaceState da APVTS tratam dos parametros, incluindo os que o
// host exponentiale ja' tinha empurrado para dentro com setValueNotifyingHost.
// Sem isso, reabrir um projeto no Ableton trazia os knobs de volta no valor
// default e a automatizacao do usuario sumia.
//
// A parte do binario e' uma arvore Value na APVTS, e nao um campo solto, pelo
// mesmo motivo: o host pede o estado quando o projeto e' aberto, e o caminho
// que ele percorre e' o mesmo dos parametros.
void getStateInformation(juce::MemoryBlock& destData) override;
void setStateInformation(const void* data, int sizeInBytes) override;

    // Chamado na thread de interface. Le o arquivo, converte e publica na fila.
    // Devolve false e preenche lastError em caso de recusa.
    bool ingest(const juce::String& path);

    [[nodiscard]] juce::String lastError() const noexcept { return lastError_; }
    [[nodiscard]] juce::String sourceName() const noexcept { return sourceName_; }

    // Metadados do material carregado para a faixa de arquivo do editor. Tudo
    // e' publicado no ingest e lido na thread de interface, entao nenhum campo
    // precisa de atomico: as duas pontas sao a mesma thread.
    struct SourceInfo {
        juce::String name;
        juce::String formatTag;  // "[64-bit PE]", como no mock
        std::int64_t sizeBytes {0};

        // Uma linha por secao, com a entropia em bits por byte. Publicado para
        // o display, que precisa desenhar as abas proporcionais ao tamanho.
        struct SectionInfo {
            juce::String name;
            std::uint32_t rawOffset {0};
            std::uint32_t rawSize {0};
            double entropy {0.0};
        };

std::vector<SectionInfo> sections;
    };

// Seletor de bytes: escolhe qual regiao do binario alimenta o motor.
    //
    // A janela e' uma regiao contigua [start, end) e nao um indice de secao.
    // A struct e' a do nucleo, em opcoda_core/pe/byte_range.h, porque a regra de
    // validacao tem teste la e nao aqui.
    using ByteRange = pe::ByteRange;

    // Regiao corrente. Comeca na primeira secao com dados e e' movida pelo
    // seletor.
    [[nodiscard]] const ByteRange& byteRange() const noexcept { return byteRange_; }

    // Move o seletor. Devolve false e nao mexe em nada se o intervalo nao for
    // valido: vazio, invertido, ou a sair do ficheiro.
    //
    // A restricao e' do nucleo, nao da interface: e' o motor que recusa ler fora
    // do buffer, e valida-lo aqui evita publicar na fila um buffer invalido que
    // so falharia depois, na thread de audio.
    bool setByteRange(std::uint64_t start, std::uint64_t end, bool rememberExactRange = true);

// Alterna entre a regiao exacta e a secao PE mais proxima.
//
// E' o que o duplo clique e a tecla Enter fazem no seletor. **Alterna** em vez
// de alinhar sempre: sem a volta, pedir o alinhamento seria uma operacao sem
// saida, e quem entrasse numa secao nao saberia como voltar ao byte.
//
// A decisao vive em pe::sectionSnapToggle, no nucleo, porque a regra tem teste
// la. Este metodo junta as secoes do PE e publica o resultado.
bool snapByteRangeToSection();

// true quando a regiao atual esta' alinhada numa secao com dados brutos.
[[nodiscard]] bool byteRangeIsSectionAligned() const noexcept;

    // Telemetria publicada pelo callback e lida pelo editor a 20 Hz.
    //
    // Sao atomicos soltos em vez de uma struct protegida por lock: sao cinco
    // valores de leitura, e o custo de um CriticalSection por bloco seria o
    // oposto do que este projeto persegue. Nao ha snapshot consistente entre os
    // campos, e para uma leitura de pico e contagem de vozes isso e' irrelevante
    // e vale a pena dizer explicitamente em vez de fingir que e' atómico.
    struct Telemetry {
        std::atomic<float> peakDb {-1.0f};
        std::atomic<int> activeVoices {0};
        std::atomic<bool> sounding {false};
        std::atomic<float> sampleRate {48000.0f};

        // Transporte. Sao tres campos e nao um, porque o display precisa de duas
        // coisas diferentes: *onde* esta a cabeca e *quanto tempo* dura a volta.
        // publishedPosition vem da thread de audio a cada bloco, e as outras duas
        // sao estado da interface lido de lado.
        std::atomic<float> playheadFraction {0.0f};
        std::atomic<double> playDurationSeconds {0.0};
        std::atomic<bool> playing {false};
    };

    [[nodiscard]] const Telemetry& telemetry() const noexcept { return telemetry_; }

    // Transporte de audicao: percorre a regiao sem nota MIDI, para se ouvir o
    // material sem teclado. E' o que torna a forma de onda navegavel audivel, e
    // nao so visivel.
    //
    // Os tres metodos sao a interface publica do transporte, e sao chamados da
    // thread de interface. A thread de audio nunca os chama: ela so' chama
    // advance(), e isso acontece dentro de processBlock.
    [[nodiscard]] bool isTransportPlaying() const noexcept { return transport_.isPlaying(); }

    // Verdadeiro quando a regiao actual produz som. Com menos de tres bytes nao ha
    // posicao que o motor leia, e o botao de reproducao tem de ficar desativado em
    // vez de aceitar o toque e produzir um drone de uma amostra.
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

    // Ancora de busca do transporte. O valor vem do parametro POSITION, e so
    // quando o utilizador o move: se o editor escrevesse isto a cada quadro, o
    // transporte voltava ao ponto de ancoragem sessenta vezes por segundo e a
    // cabeca nunca passava dele.
    void seekTransportTo(float fraction) noexcept { transport_.seekToFraction(fraction); }

    [[nodiscard]] float transportPosition() const noexcept {
        return transport_.positionFraction();
    }

    [[nodiscard]] double transportDurationSeconds() const noexcept {
        return transport_.durationSeconds();
    }

    // Verdadeiro quando o host esta' a correr o transporte.
    //
    // **Sem isto o botao de reproducao mente.** O motor so' existe enquanto o host
    // chama processBlock, e um host parado nao chama. Um botao que aceita o toque
    // nesse estado e nao produz nada e' pior do que um botao que recusa o toque, e
    // por isso a interface desativa-o e diz porquê.
    //
    // getPlayHead() pode ser nulo: e' o que acontece num teste sem host, e o
    //nullptr e' resposta valida e nao um erro.
    // **Verdadeiro apenas quando sabemos que o host esta' parado.** E o inverso de
    // "o host esta' a tocar", e a distincao e' o ponto.
    //
    // Sem isto o botao de reproducao mente. O motor so' existe enquanto o host
    // chama processBlock, e um host parado nao chama: um botao que aceita o toque
    // nesse estado e nao produz nada e' pior do que um botao que recusa o toque.
    //
    // A distincao esta' no que acontece quando o host nao diz nada. getPosition()
    // devolve nullopt num host que nao fornece informacao de tempo, e nesse caso a
    // resposta e' `false` — nao sabemos que parou. Tratar o desconhecido como
    // parado desabilitaria o botao para sempre em qualquer host que nao de tempo,
    // e o Standalone sem dispositivo de audio e' um deles.
    [[nodiscard]] bool hostTransportIsKnownToBeStopped() const noexcept {
        const auto* head = getPlayHead();
        if (head == nullptr) {
            // Sem host nenhum nao ha quem mande. Num teste, isto e' o que evita
            // que o botao fique travado.
            return false;
        }

        // A API actual do JUCE 8 e' getPosition(), que devolve um Optional.
        // CurrentPositionInfo com isPlaying foi depreciada, e por isso nao e'
        // usada: um aviso de depreciacao num /WX parte o build, e vale a pena que
        // o portao aponte o uso de API velha.
        const auto position = head->getPosition();
        if (!position.hasValue()) {
            return false; // host sem informacao de tempo: desconhecido, nao parado
        }
        return !position->getIsPlaying();
    }

    [[nodiscard]] const SourceInfo& sourceInfo() const noexcept { return sourceInfo_; }

    // Bytes crus do ficheiro carregado, para a grelha mostrar os bytes.
    //
    // E' a mesma razao que fez o ingest guardar o ficheiro em vez de o ler do
    // disco outra vez: a alternativa e' I/O na thread de interface a cada
    // redesenho. O vector nao muda de endereco quando o ingest atribui outro
    // conteudo, por isso a grelha guarda um ponteiro para ele e nao uma copia.
    //
    // Pertence a thread de interface, que e' a unica que escreve. A thread de
    // audio nunca a le: so ve o buffer ja convertido, pela fila.
    [[nodiscard]] const std::vector<std::uint8_t>& sourceBytes() const noexcept {
        return sourceBytes_;
    }

    // Aplica na thread de interface os ultimos CCs recebidos.
    //
    // O caminho alternativo, escrever no parametro direto da thread de audio,
    // foi descartado de proposito. setValueNotifyingHost notifica os
    // listeners, e o SliderParameterAttachment e' um deles: seria acesso
    // cruzado ao GUI a partir do callback, e a GUI a partir do callback e'
    // exatamente a classe de defeito que o guard de alocacao nao apanha.
    //
    // O custo e' a latencia do timer do editor, cerca de 50 ms. Em troca, o
    // parametro continua a ser a fonte unica da verdade: o host e' notificado,
    // o knob segue, e o estado do projeto fica certo sem caminho paralelo.
    void applyPendingControllerChanges();

    // live_ e' publicado pela thread de audio e consultado pela de interface.
    // Sem o atomico a leitura do editor seria uma corrida com a troca de
    // material, o que o compilador nao pode flagar porque os ponteiros sao do
    // mesmo tipo.
    [[nodiscard]] bool hasSource() const noexcept {
        return live_.load(std::memory_order_acquire) != nullptr;
    }

    // Os seis parametros do MVP (Tabela 8 do artigo), expostos ao host para
    // automacao. O estado e' lido uma vez por bloco e copiado para a estrutura
    // do motor, que nao guarda referencia: e o que mantem o caminho de audio
    // livre de acesso concorrente a estado do host.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    [[nodiscard]] dsp::GranularParams currentParams() const noexcept;

    [[nodiscard]] juce::AudioProcessorValueTreeState& parameters() noexcept { return parameters_; }
    [[nodiscard]] const juce::AudioProcessorValueTreeState& parameters() const noexcept { return parameters_; }

    // Chamado na thread de interface, periodicamente: so agora o ponteiro
    // devolvido pela thread de audio pode ser liberado com seguranca.
    void releaseReturnedBuffers();

private:
    // Ponto unico de entrada da amostra na thread de audio. Troca o material
    // ativo pelo recem-chegado e devolve o antigo para a interface, que e' a
    // unica que pode liberar a memoria.
    void drainIncomingQueue() noexcept;

    // Le o MidiBuffer do bloco e atualiza a contagem de notas. Roda na thread
    // de audio e nao aloca nem bloqueia: um MidiBuffer e' so uma lista de
    // mensagens com contadores ja resolvidos.
void readNotes(const juce::MidiBuffer& midi) noexcept;

    void publishTelemetry(const juce::AudioBuffer<float>& buffer) noexcept;

    // Bytes crus do ficheiro carregado, para o seletor poder converter qualquer
    // janela sem voltar a ler o disco. O accessor publico e' o sourceBytes()
    // acima; este e' o vector.
    std::vector<std::uint8_t> sourceBytes_;

    // Converte a janela corrente e publica na fila. Devolve false sem mexer em
    // nada se a janela nao produz audio.
    bool publishByteRange();

    opcoda::dsp::GranularEngine engine_;

    // Duas filas em sentidos opostos, com ponteiro cru. A thread de interface
    // publica o material novo e recolhe o ponteiro que a thread de audio ja
    // terminou de usar.
    rt::SpscRing<PublishedMaterial, 4> incoming_;
    rt::SpscRing<PublishedMaterial, 4> returned_;

    // Donos de toda a memoria de amostra. So a thread de interface escreve
    // aqui, e so remove um buffer depois que a thread de audio o devolveu.
    std::vector<SampleBuffer> owned_;

    // A curva de entropiareduzida que acompanha cada buffer. Vive ao lado de
    // owned_ com o mesmo indice, e viaja com o ponteiro na fila.
    std::vector<std::vector<float>> entropyCurves_;

    std::atomic<SamplePtr> live_ {nullptr};
    juce::String lastError_;
    juce::String sourceName_;
    SourceInfo sourceInfo_;
    ByteRange byteRange_;

    // Inicio exato, sem alinhamento, da ultima regiao escolhida pelo utilizador.
    // E' o destino do duplo clique quando a regiao esta' alinhada numa secao, e
    // o que torna a operacao reversivel.
    ByteRange lastExactRange_ {};

    // Caminho do binario ativo, para o estado do host. Distinto de sourceName_,
    // que e' so o nome do ficheiro para a interface.
    juce::String sourcePath_;

    // Notas premidas e CCs mapeados. Vive na thread de audio porque e' estado do
    // audio, e o host so entrega MIDI durante processBlock.
    //
    // A regra esta' em rt::NoteTracker, no nucleo e sem JUCE, para ter testes.
    // Aqui so se traduz MidiMessage em Event, que e' a parte fina.
    rt::NoteTracker notes_;

    // Transporte de audicao. Vive ao lado da engine e nao dentro dela porque sao
    // duas perguntas diferentes: a engine pergunta "que som produzir agora" e o
    // transporte pergunta "de onde comecar". O motor recebe a posicao ja
    // resolvida, e continua a nao saber que existe um transporte.
    rt::Transport transport_;

    Telemetry telemetry_;

    static constexpr int kCcSustain = 64;
    static constexpr int kCcDensity = 74;  // Brightness: DENSITY
    static constexpr int kCcPosition = 71; // Resonance: POSITION
    static constexpr int kDensityController = 0;
    static constexpr int kPositionController = 1;

    juce::AudioProcessorValueTreeState parameters_;
};

} // namespace opcoda
