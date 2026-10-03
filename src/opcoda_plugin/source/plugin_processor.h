#pragma once

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/rt/note_tracker.h"
#include "opcoda_core/rt/spsc_ring.h"

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
    double getTailLengthSeconds() const override { return 0.0; }

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

        // Curva de entropia por janela, ja normalizada em [0, 8]. Vem do mesmo
        // shannonCurve do ensaio T1, entao o que se ve e o que se mediu.
        std::vector<float> entropyCurve;
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
    bool setByteRange(std::uint64_t start, std::uint64_t end);

// Alinha o inicio da regiao ao inicio da secao mais proxima, e estende o fim
    // ate ao fim dessa secao.
    //
    // E' o que o duplo clique no seletor faz. O parametro `start` e' o inicio
    // pedido; o metodo escolhe a secao, porque o alignamento por section e' uma
    // pergunta sobre o PE e nao sobre aritmetica de bytes.
    bool snapByteRangeToSection(std::uint64_t start);

    // true quando a regiao atual esta' alinhada numa secao. O duplo clique
    // usa isto para alternar entre o byte exato e o alinhado, em vez de fazer
    // uma operacao sem volta.
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
    };

    [[nodiscard]] const Telemetry& telemetry() const noexcept { return telemetry_; }

    [[nodiscard]] const SourceInfo& sourceInfo() const noexcept { return sourceInfo_; }

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

    // Curva de entropia por janela, normalizada em [0, 8]. Roda na thread de
    // interface, no ingest, e nao no caminho de audio.
    static std::vector<float> buildEntropyCurve(const std::uint8_t* data,
                                                std::size_t size);

    void publishTelemetry(const juce::AudioBuffer<float>& buffer) noexcept;

    // Bytes crus do ficheiro carregado, para o seletor poder converter
    // qualquer janela sem voltar a ler o disco.
    //
    // E' memoria que ja era alocada no ingest e que ficava de fora a seguir. O
    // custo e' o tamanho do ficheiro, e a alternativa — reler do disco a cada
    // movimento do seletor — pinge I/O na thread de interface e torna o
    // arraste lento. Guardar e' a troca mais barata.
    //
    // Pertence a thread de interface, que e' a unica que escreve nela. A thread
    // de audio nunca a le: so ve o buffer ja convertido, pela fila.
    std::vector<std::uint8_t> sourceBytes_;

    // Converte a janela corrente e publica na fila. Devolve false sem mexer em
    // nada se a janela nao produz audio.
    bool publishByteRange();

    opcoda::dsp::GranularEngine engine_;

    // Duas filas em sentidos opostos, com ponteiro cru. A thread de interface
    // publica o material novo e recolhe o ponteiro que a thread de audio ja
    // terminou de usar.
    rt::SpscRing<SamplePtr, 4> incoming_;
    rt::SpscRing<SamplePtr, 4> returned_;

    // Donos de toda a memoria de amostra. So a thread de interface escreve
    // aqui, e so remove um buffer depois que a thread de audio o devolveu.
    std::vector<SampleBuffer> owned_;

    std::atomic<SamplePtr> live_ {nullptr};
    juce::String lastError_;
    juce::String sourceName_;
    SourceInfo sourceInfo_;
    ByteRange byteRange_;

    // Inicio em bytes exatos, sem alinhamento. E' o destino do duplo clique
    // quando a regiao esta' alinhada numa secao, e o que torna a operacao
    // reversivel.
    std::uint64_t lastExactStart_ {0};

    // Caminho do binario ativo, para o estado do host. Distinto de sourceName_,
    // que e' so o nome do ficheiro para a interface.
    juce::String sourcePath_;

    // Notas premidas e CCs mapeados. Vive na thread de audio porque e' estado do
    // audio, e o host so entrega MIDI durante processBlock.
    //
    // A regra esta' em rt::NoteTracker, no nucleo e sem JUCE, para ter testes.
    // Aqui so se traduz MidiMessage em Event, que e' a parte fina.
    rt::NoteTracker notes_;
    Telemetry telemetry_;

    static constexpr int kCcSustain = 64;
    static constexpr int kCcDensity = 74;  // Brightness: DENSITY
    static constexpr int kCcPosition = 71; // Resonance: POSITION
    static constexpr int kDensityController = 0;
    static constexpr int kPositionController = 1;

    juce::AudioProcessorValueTreeState parameters_;
};

} // namespace opcoda
