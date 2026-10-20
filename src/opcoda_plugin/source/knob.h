#pragma once

#include "boxed_label.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace opcoda {

// Um knob: quadrado de cor, rotulo, slider rotativo no meio, valor em baixo.
//
// O corpo creme e a haste sao desenhados pelo LookAndFeel; aqui ficam o rotulo,
// o quadrado de cor da coluna (o swatch do mock) e o valor em mono sobre o
// metal. O quadrado de cor nao carrega informacao sozinho: cada knob tem o nome
// escrito ao lado, que e' o que o criterio 1.4.1 do WCAG exige.
//
// **Os knobs sao juce::Slider de verdade, nao desenho customizado.** Isso nao e'
// padrao: e' o que da foco por teclado, ajuste com setas e AccessibilityHandler
// de graca, e sao exatamente os criterios que a docs/10-acessibilidade-w3c.md
// exige. O desenho do corpo e' so' o fundo: quem opera o controle continua a ser
// o Slider, e e' ele que tem o foco.
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
        title_.setFont(BoxedLabel::sansFont(10.0f, true).withExtraKerningFactor(0.10f));
        title_.setColour(juce::Label::textColourId, palette::labelOnMetal);
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

        // Valor impresso no metal, sem caixa: o mock nao tem valBox, o numero
        // assenta direto na superficie.
        value_.setJustificationType(juce::Justification::centred);
        value_.setFont(BoxedLabel::monoFont(10.0f, true));
        value_.setColour(juce::Label::textColourId, palette::valueOnMetal);
        value_.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(value_);
        format_ = spec.format;

        slider_.addListener(this);
        // O valor inicial vem do parametro, e nao do zero do slider: quem cria
        // o editor depois do estado do host ja entra com o valor carregado.
        showValue(slider_.getValue());

        setSize(72, 94);
    }

    // Quadrado de cor da coluna, do mock. Vem do editor porque a cor e' do
    // modulo e nao do parametro: o knob so' a desenha.
    void setSwatch(juce::Colour colour) {
        swatch_ = colour;
        repaint();
    }

    void resized() override {
        auto area = getLocalBounds();
        title_.setBounds(area.removeFromTop(12));
        area.removeFromTop(2);
        area.removeFromBottom(14);

        // Diametro limitado a 64 px, o w-16 do mock. Sem o teto, o knob crescia
        // ate preencher a celula numa janela alta e virava um disco.
        constexpr int maxDiameter {64};
        const auto diameter = juce::jmax(
            16, juce::jmin(maxDiameter, juce::jmin(area.getWidth(), area.getHeight())));
        slider_.setBounds(area.withSizeKeepingCentre(diameter, diameter));

        value_.setBounds(getLocalBounds().withTop(getHeight() - 14).reduced(2, 0));
    }

    // O corpo e o valor sao filhos; aqui desenha-se so' o quadrado de cor, a
    // esquerda do titulo. A posicao e' medida a partir da largura real do texto
    // em vez de um offset fixo: a 10 px, uma letra de diferenca punha o
    // quadrado por cima da primeira letra.
    void paint(juce::Graphics& g) override {
        if (swatch_.isTransparent() || title_.getWidth() <= 0) {
            return;
        }

        constexpr float swatchSize {8.0f};
        const auto textWidth =
            juce::GlyphArrangement::getStringWidth(title_.getFont(), title_.getText());
        const auto titleCentre = static_cast<float>(title_.getBounds().getCentreX());
        const auto left = titleCentre - textWidth * 0.5f - 6.0f - swatchSize;
        const auto top = static_cast<float>(title_.getBounds().getCentreY())
                         - swatchSize * 0.5f;
        const auto rect = juce::Rectangle<float> {left, top, swatchSize, swatchSize};

        g.setColour(swatch_);
        g.fillRoundedRectangle(rect, 1.5f);
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.drawRoundedRectangle(rect.reduced(0.25f), 1.5f, 1.0f);
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
    juce::Label value_;
    juce::Colour swatch_ {juce::Colours::transparentBlack};
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
    std::function<juce::String(float)> format_;
};

} // namespace opcoda
