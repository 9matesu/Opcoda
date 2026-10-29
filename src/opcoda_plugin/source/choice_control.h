#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace opcoda {

// escolha de parametro que nao e' knob.
//
// **nem tudo e' knob.** um seletor de onda com quatro posicoes e' pior como
// knob: gira-se um disco para escolher entre nomes que ja' cabem em quatro
// botoes com o desenho da onda. aqui a escolha e' um botao por opcao, e o que
// o botao mostra e' o proprio desenho: a forma do LFO, a janela do grao, a
// resposta do filtro.
//
// **o valor vive num slider.** o AudioParameterChoice e' ligado a um
// juce::Slider com SliderParameterAttachment, o mesmo caminho do knob: o host
// automatiza, o preset leva, o undo desfaz e o leitor de tela nomeia. os botoes
// mexem no slider; o slider informa os botoes quando o valor muda por fora.
//
// **quatro modos.** icons: grelha de botoes com glifo. text: grelha de botoes
// com rotulo curto. toggle: uma chave de dois estados. detent: cursor com
// dentes, para as listas compridas.
//
// **sem popup.** a escolha ve-se toda de uma vez; o tooltip acrescenta o nome
// falado de cada opcao.
class ChoiceControl : public juce::Component, private juce::Slider::Listener {
public:
    enum class Mode {
        icons,   // grelha de botoes com glifo
        text,    // grelha de botoes com rotulo curto
        toggle,  // chave de dois estados
        detent,  // cursor com dentes
    };

    struct Item {
        juce::String label {};      // nome falado e tooltip
        juce::String shortLabel {}; // texto curto, quando ha' icone
        const char* icon {nullptr}; // glifo, so' no modo icons
    };

    ChoiceControl(juce::AudioProcessorValueTreeState& state,
                  const juce::String& paramId,
                  const juce::String& title,
                  const juce::String& spokenName,
                  std::vector<Item> items,
                  Mode mode,
                  int columns = 0);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void sliderValueChanged(juce::Slider* changed) override;
    void syncFromValue();
    [[nodiscard]] int currentIndex() const noexcept;
    void choose(int index);

    juce::Slider value_;
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;

    juce::Label title_;
    juce::Label valueLabel_;

    std::vector<std::unique_ptr<juce::Button>> buttons_;
    juce::StringArray labels_;
    std::vector<Item> items_;

    Mode mode_ {Mode::icons};
    int columns_ {0};
};

} // namespace opcoda
