#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace opcoda {

// vista MATRIX: as oito rotas da matriz de modulacao, uma por linha.
//
// cada celula e' um parametro do APVTS (m1src, m1dst, m1amt ... m8amt): o combo
// da fonte, o combo do destino e a barra do amount sao componentes de verdade,
// com attachment, e por isso o host automatiza, o preset leva e o leitor de
// tela nomeia. A vista nao guarda estado nenhum: escreve nos mesmos parametros
// que o audio le por bloco.
//
// os indices seguem dsp::ModSource e dsp::ModDest; os rotulos vem do proprio
// parametro, que e' a unica fonte da verdade sobre a ordem.
class ModMatrixView : public juce::Component {
public:
    ModMatrixView();

    // liga as celulas aos parametros. uma vez so: os attachments guardam a
    // referencia do estado e a ordem dos items vem dos choices.
    void attach(juce::AudioProcessorValueTreeState& state);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Row {
        // o ComboBox do JUCE tem construtor explicito: o nome vem aqui e o nome
        // de verdade, com o numero da rota, entra em attach().
        juce::ComboBox source_ {"route source"};
        juce::ComboBox destination_ {"route destination"};
        juce::Slider amount_;
        std::unique_ptr<juce::ComboBoxParameterAttachment> sourceAttachment_ {};
        std::unique_ptr<juce::ComboBoxParameterAttachment> destinationAttachment_ {};
        std::unique_ptr<juce::SliderParameterAttachment> amountAttachment_ {};
    };

    void layoutRows();

    // geometria das linhas, guardada para o paint desenhar o indice.
    std::array<juce::Rectangle<int>, 8> rowBounds_ {};
    bool attached_ {false};
    std::array<Row, 8> rows_ {};
};

} // namespace opcoda
