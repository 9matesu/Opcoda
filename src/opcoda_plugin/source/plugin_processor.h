#pragma once

#include "opcoda_core/dsp/granular_engine.h"
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

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

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
    };

    [[nodiscard]] const SourceInfo& sourceInfo() const noexcept { return sourceInfo_; }

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

    juce::AudioProcessorValueTreeState parameters_;
};

} // namespace opcoda
