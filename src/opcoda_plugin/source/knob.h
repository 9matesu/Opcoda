#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>

namespace opcoda {

// knob: rotulo em cima, slider rotativo no meio, valor em baixo.
//
// o corpo e' desenhado pelo LookAndFeel, uma vez e fixo. arco de valor, ponteiro,
// anel de foco e texto sao desenhados por cima, porque dependem do valor e do
// foco.
//
// **os knobs sao juce::Slider de verdade, nao desenho customizado.** e' o que da
// foco por teclado, ajuste com setas e AccessibilityHandler, os criterios de
// docs/10-acessibilidade-w3c.md. o corpo e' so o fundo: quem opera e tem o foco
// continua a ser o Slider.
//
// o rotulo curto e' o do mock. o nome acessivel e' a descricao completa em
// portugues: "SIZE" sozinho e' inutil para quem navega por leitor de tela.
class Knob : public juce::Component, private juce::Slider::Listener {
public:
    struct Spec {
        const char* paramId;
        const char* title;       // curto, como no mock
        const char* spokenName;  // nome acessivel, descritivo
        float defaultValue;
        std::function<juce::String(float)> format;
        // so para choices: rotulos na ordem do AudioParameterChoice. com a lista,
        // o valor mostra o rotulo em vez do numero. o attachment ja poe o
        // intervalo 0..N-1 com passo 1, por isso aqui so falta o texto.
        juce::StringArray choiceLabels {};
    };

    // sem parametro de tamanho: o editor posiciona por celula e o knob ajusta-se
    // em resized(). um tamanho aqui seria uma segunda fonte da verdade.
    Knob(juce::AudioProcessorValueTreeState& state, const Spec& spec)
        : attachment_(makeAttachment(state, spec, slider_)) {
        title_.setText(spec.title, juce::dontSendNotification);
        title_.setJustificationType(juce::Justification::centred);
        title_.setFont(fonts().sans(9.0f, true));
        title_.setColour(juce::Label::textColourId, palette::textSub);
        addAndMakeVisible(title_);

        // valor em texto puro: a moldura valBox era ruido repetido seis vezes.
        value_.setJustificationType(juce::Justification::centred);
        value_.setFont(fonts().sans(10.0f));
        value_.setColour(juce::Label::textColourId, palette::textDark);
        value_.setInterceptsMouseClicks(false, false);

        slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        // os seis parametros usam range linear: uma curva de resposta seria outra
        // fonte da verdade sem ganho.
        slider_.setVelocityBasedMode(false);
        slider_.setDoubleClickReturnValue(true, spec.defaultValue);
        slider_.setName(spec.spokenName);
        slider_.setTooltip(juce::String {spec.spokenName});
        addAndMakeVisible(slider_);

        setName(spec.spokenName);
        addAndMakeVisible(value_);
        format_ = spec.format;
        choiceLabels_ = spec.choiceLabels;

        slider_.addListener(this);
        // valor inicial vem do parametro e nao do zero do slider: quem cria o
        // editor depois do estado do host ja entra com o valor carregado.
        showValue(slider_.getValue());

        setSize(56, 82);
    }

    void resized() override {
        auto area = getLocalBounds();
        title_.setBounds(area.removeFromTop(13));
        area.removeFromTop(1);
        area.removeFromBottom(18);

        // teto de diametro: sem ele o knob cresce ate encher a celula numa janela
        // alta e vira um disco.
        constexpr int maxDiameter {62};
        const auto diameter = juce::jmax(
            16, juce::jmin(maxDiameter, juce::jmin(area.getWidth(), area.getHeight())));
        slider_.setBounds(area.withSizeKeepingCentre(diameter, diameter));

        // titulo e valor ficam sempre nas pontas: encolher a janela encolhe o
        // knob sem empurrar o texto para fora.
        value_.setBounds(getLocalBounds().withTop(getHeight() - 18).reduced(2, 0));
    }

    void paint(juce::Graphics& g) override {
        // sem cartao: o knob mora direto no chassis, sem caixa nem borda. rotulo,
        // disco e valor nao precisam de moldura; o corpo do disco vem do
        // LookAndFeel.
        static_cast<void>(g);
    }

private:
    // o attachment liga o slider ao parametro e trata do undo. precisa da
    // referencia do parametro, e nao da arvore: o undo e' por parametro.
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
        juce::String text;
        if (!choiceLabels_.isEmpty()) {
            // isfinite antes do roundToInt: float fora de faixa para int e' UB
            // formal, e NaN nao passa pelo jlimit. NaN mostra o primeiro rotulo
            // em vez de um acesso selvagem.
            const auto index = std::isfinite(value)
                                   ? juce::jlimit(0, choiceLabels_.size() - 1,
                                                  juce::roundToInt(value))
                                   : 0;
            text = choiceLabels_[index];
        } else if (format_) {
            text = format_(static_cast<float>(value));
        }
        if (text.isNotEmpty() && text != value_.getText()) {
            value_.setText(text, juce::dontSendNotification);
        }
    }

    juce::Slider slider_;
    juce::Label title_;
    juce::Label value_;
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
    std::function<juce::String(float)> format_;
    juce::StringArray choiceLabels_;
};

} // namespace opcoda
