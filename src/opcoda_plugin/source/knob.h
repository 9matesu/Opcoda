#pragma once

#include "assets.h"
#include "boxed_label.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace opcoda {

// Um knob: rotulo em cima, slider rotativo no meio, valor em baixo.
//
// O corpo e' a peca fisica do asset harness, desenhada uma vez e fixa. O que a
// peca nao tem, e que o codigo desenha por cima: o arco de valor, o ponteiro,
// o anel de foco e o texto. A divisao e' deliberada, porque e' ela que mantem
// o que a constitution exige.
//
// **Os knobs sao juce::Slider de verdade, nao desenho customizado.** Isso nao e'
// padrao: e' o que da foco por teclado, ajuste com setas e AccessibilityHandler
// de graca, e sao exatamente os criterios que a docs/10-acessibilidade-w3c.md
// exige. O corpo vem de um PNG, mas o PNG e' so o desenho de fundo: quem opera
// o controle continua a ser o Slider, e e' ele que tem o foco.
//
// O rotulo curto e' o do mock. O nome accessible e' a descricao completa em
// portugues: um nome accessible curto demais, so "SIZE", e' inutil para quem
// navega por leitor de tela.
class Knob : public juce::Component, private juce::Slider::Listener {
public:
    struct Spec {
        const char* paramId;
        const char* title;       // texto curto, como no mock
        const char* spokenName;  // nome accessible, descritivo
        float defaultValue;
        std::function<juce::String(float)> format;
    };

    // Sem parametro de tamanho: o editor posiciona por celula e o knob se ajusta
    // ao espaco que sobra em resized(). Passar um tamanho aqui seria uma segunda
    // fonte da verdade que o layout ja define.
    Knob(juce::AudioProcessorValueTreeState& state, const Spec& spec)
        : attachment_(makeAttachment(state, spec, slider_)) {
        title_.setText(spec.title, juce::dontSendNotification);
        title_.setJustificationType(juce::Justification::centred);
        title_.setFont(BoxedLabel::sansFont(10.0f, true));
        title_.setColour(juce::Label::textColourId, palette::textSub);
        addAndMakeVisible(title_);

        slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        // Linear: os seis parametros usam NormalisableRange linear, e uma curva
        // de resposta seria uma segunda fonte da verdade sem ganho nenhum.
        slider_.setVelocityBasedMode(false);
        slider_.setDoubleClickReturnValue(true, spec.defaultValue);
        slider_.setName(spec.spokenName);
        slider_.setTooltip(juce::String {spec.spokenName});
        addAndMakeVisible(slider_);

        setName(spec.spokenName);
        addAndMakeVisible(value_);
        format_ = spec.format;

        slider_.addListener(this);
        // O valor inicial vem do parametro, e nao do zero do slider: quem cria
        // o editor depois do estado do host ja entra com o valor carregado.
        showValue(slider_.getValue());

        setSize(56, 82);
    }

    void resized() override {
        auto area = getLocalBounds();
        title_.setBounds(area.removeFromTop(13));
        area.removeFromTop(1);
        area.removeFromBottom(18);

        // Diametro limitado. Sem o teto, o knob cresceria ate preencher a celula
        // inteira numa janela alta e viraria um disco em vez de um controle.
        constexpr int maxDiameter {62};
        const auto diameter = juce::jmax(
            16, juce::jmin(maxDiameter, juce::jmin(area.getWidth(), area.getHeight())));
        slider_.setBounds(area.withSizeKeepingCentre(diameter, diameter));

        // Painel fecha o conteudo e o knob e' centrado no que sobra, com teto de
        // diametro. O titulo e a caixa de valor ficam sempre nas pontas, entao
        // encolher a janela encolhe o knob sem empurrar o texto para fora.
        value_.setBounds(getLocalBounds().withTop(getHeight() - 18).reduced(2, 0));
    }

    void paint(juce::Graphics& g) override {
        // Painel do modulo, para o knob nao flotar sobre o chassis liso. O corpo
        // do knob e' desenhado pelo LookAndFeel, que e' quem sabe o angulo.
        g.setColour(palette::panelBg);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 2.0f);
        g.setColour(palette::alpha(palette::chassisBorder, 0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 2.0f, 1.0f);
    }

private:
    // O attachment liga o slider ao parametro e cuida do undo. Ele exige a
    // referencia do parametro, nao a da arvore inteira, porque o undo e' por
    // parametro e nao por estado do plugin.
    static std::unique_ptr<juce::SliderParameterAttachment> makeAttachment(
        juce::AudioProcessorValueTreeState& state, const Spec& spec, juce::Slider& slider) {
        auto* parameter = state.getParameter(spec.paramId);
        jassert(parameter != nullptr);
        return parameter != nullptr
                   ? std::make_unique<juce::SliderParameterAttachment>(*parameter, slider)
                   : nullptr;
    }

    void sliderValueChanged(juce::Slider* changed) override {
        if (changed == &slider_) {
            showValue(slider_.getValue());
            repaint();
        }
    }

    void showValue(double value) {
        if (format_) {
            const auto text = format_(static_cast<float>(value));
            if (text != value_.getText()) {
                value_.setText(text, juce::dontSendNotification);
            }
        }
    }

    juce::Slider slider_;
    juce::Label title_;
    BoxedLabel value_ {palette::valBox, palette::chassisBorder};
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
    std::function<juce::String(float)> format_;
};

} // namespace opcoda
