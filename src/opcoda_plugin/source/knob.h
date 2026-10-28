#pragma once

#include "fonts.h"
#include "look_and_feel.h"
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
// **parametros de escolha levam dropdown.** girar um knob de quatro a doze
// posicoes e' pior que escolher numa lista: o caret ao lado do valor abre o menu
// com os rotulos reais, e o slider continua a responder a roda e as setas para
// quem prefere o gesto continuo.
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
        // glifo desenhado ao lado do titulo. a 10 px diz o que o knob faz antes
        // de o nome se ler: e' a mesma lingua dos icones dos modulos.
        const char* icon {nullptr};
    };

    // sem parametro de tamanho: o editor posiciona por celula e o knob ajusta-se
    // em resized(). um tamanho aqui seria uma segunda fonte da verdade.
    Knob(juce::AudioProcessorValueTreeState& state, const Spec& spec)
        : attachment_(makeAttachment(state, spec, slider_)) {
        title_ = spec.title;
        icon_ = spec.icon != nullptr ? spec.icon : "";
        value_.setJustificationType(juce::Justification::centred);
        value_.setFont(fonts().sans(13.0f));
        value_.setColour(juce::Label::textColourId, palette::textDark);
        value_.setInterceptsMouseClicks(false, false);

        slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider_.setVelocityBasedMode(false);
        slider_.setDoubleClickReturnValue(true, spec.defaultValue);
        slider_.setName(spec.spokenName);
        slider_.setTooltip(juce::String {spec.spokenName});
        addAndMakeVisible(slider_);

        setName(spec.spokenName);
        addAndMakeVisible(value_);

        format_ = spec.format;
        choiceLabels_ = spec.choiceLabels;

        if (!choiceLabels_.isEmpty()) {
            menuButton_.setButtonText("");
            menuButton_.setName(juce::String {spec.spokenName} + ": escolher valor");
            menuButton_.setTooltip("Escolher " + juce::String {spec.title} + " na lista");
            menuButton_.getProperties().set("uiIcon", "chevron");
            menuButton_.getProperties().set("uiLook", "icon");
            menuButton_.onClick = [this] { showChoiceMenu(); };
            addAndMakeVisible(menuButton_);
        }

        slider_.addListener(this);
        // valor inicial vem do parametro e nao do zero do slider: quem cria o
        // editor depois do estado do host ja entra com o valor carregado.
        showValue(slider_.getValue());

        setSize(100, 140);
    }

    // teclas de chapa clara e carvao escura: os rotulos do knob trocam de tinta
    // quando o knob vive no mestre, que e' uma tira escura. textSub sobre carvao
    // seria texto invisivel.
    void setOnDark(bool on) {
        titleColour_ = on ? palette::textOnDarkSub : palette::textSub;
        value_.setColour(juce::Label::textColourId,
                         on ? palette::ivory : palette::textDark);
        repaint();
    }

    void resized() override {
        auto area = getLocalBounds();
        const bool tight = getHeight() < 112;
        const int titleHeight = tight ? 14 : 17;
        area.removeFromTop(titleHeight);
        area.removeFromTop(1);

        const int valueHeight = tight ? 18 : 22;
        auto valueRow = area.removeFromBottom(valueHeight);
        value_.setBounds(valueRow.reduced(2, 0));

        if (choiceLabels_.isEmpty()) {
            value_.setBounds(valueRow.reduced(2, 0));
        } else {
            // o caret ocupa a ponta direita do valor; o texto recua para nao
            // ficar debaixo dele.
            menuButton_.setBounds(valueRow.removeFromRight(18).withSizeKeepingCentre(16, 16));
            value_.setBounds(valueRow.reduced(2, 0));
        }

        // teto de diametro: 96 px de slider, que dao disco visual de ~91 px. acima
        // disso o arco encosta na celula e a escala perde a leitura.
        constexpr int maxDiameter {96};
        const auto diameter = juce::jmax(
            16, juce::jmin(maxDiameter, juce::jmin(area.getWidth(), area.getHeight())));
        slider_.setBounds(area.withSizeKeepingCentre(diameter, diameter));
    }

    void paint(juce::Graphics& g) override {
        if (getWidth() <= 0 || getHeight() <= 0 || title_.isEmpty()) {
            return;
        }

        const bool tight = getHeight() < 112;
        const int titleHeight = tight ? 14 : 17;
        const auto font = fonts().sans(tight ? 10.5f : 12.0f, true)
                              .withExtraKerningFactor(0.10f);

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, title_, 0.0f, 0.0f);
        const auto textWidth = glyphs.getBoundingBox(0, -1, false).getWidth();

        const float glyphSize = tight ? 10.0f : 12.0f;
        const float gap = icon_.isEmpty() ? 0.0f : 4.0f;
        const auto total = (icon_.isEmpty() ? 0.0f : glyphSize) + gap + textWidth;
        const auto left = static_cast<float>(getWidth()) * 0.5f - total * 0.5f;
        const auto row = juce::Rectangle<float> {
            0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(titleHeight)};

        auto textLeft = left;
        if (!icon_.isEmpty()) {
            OpcodaLookAndFeel::paintGlyph(
                g, icon_,
                juce::Rectangle<float> {left, row.getCentreY() - glyphSize * 0.5f,
                                        glyphSize, glyphSize},
                titleColour_, false);
            textLeft += glyphSize + gap;
        }

        g.setColour(titleColour_);
        g.setFont(font);
        g.drawText(title_, juce::Rectangle<float> {textLeft, row.getY(), textWidth,
                                                   row.getHeight()},
                   juce::Justification::centredLeft, false);
    }

private:
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

    void showChoiceMenu() {
        if (choiceLabels_.isEmpty()) {
            return;
        }

        const auto current = juce::jlimit(0, choiceLabels_.size() - 1,
                                           juce::roundToInt(slider_.getValue()));
        juce::PopupMenu menu;
        for (int i = 0; i < choiceLabels_.size(); ++i) {
            menu.addItem(i + 1, choiceLabels_[i], true, i == current);
        }

        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&menuButton_)
                .withMinimumWidth(140),
            [this](int result) {
                if (result > 0) {
                    // sendNotificationSync e nao setValue: o attachment escreve
                    // no parametro e o host fica a saber, como no gesto do knob.
                    slider_.setValue(static_cast<double>(result - 1),
                                     juce::sendNotificationSync);
                }
            });
    }

    void showValue(double value) {
        juce::String text;
        if (!choiceLabels_.isEmpty()) {
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
    juce::Label value_;
    juce::TextButton menuButton_;
    juce::String title_;
    juce::String icon_;
    juce::Colour titleColour_ {palette::textSub};
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
    std::function<juce::String(float)> format_;
    juce::StringArray choiceLabels_;
};

} // namespace opcoda
