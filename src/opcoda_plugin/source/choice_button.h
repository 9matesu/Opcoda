#pragma once

#include "fonts.h"
#include "knob.h"
#include "look_and_feel.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// parametro de escolha como tecla, nao como knob.
//
// **girar um knob de duas a doze posicoes e' pior do que ler a lista.** o valor
// atual fica escrito e o clique abre o menu com os rotulos reais. o titulo com
// glifo fica em cima, como no knob, e o valor vive numa fenda baixa e larga no
// centro da celula -- campo de escolha, nao disco. e' ainda um juce::Button:
// foco, teclado e o AccessibilityHandler vem de graca, como no knob que ele
// substitui.
class ChoiceButton : public juce::Button {
public:
    ChoiceButton(juce::AudioProcessorValueTreeState& state, const Knob::Spec& spec)
        : juce::Button(spec.spokenName),
          title_(spec.title),
          icon_(spec.icon != nullptr ? spec.icon : ""),
          labels_(spec.choiceLabels),
          parameter_(state.getParameter(spec.paramId)) {
        setName(spec.spokenName);
        setTooltip(juce::String {spec.spokenName} + ". Clique para escolher.");
        setWantsKeyboardFocus(true);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);

        jassert(parameter_ != nullptr);
        if (parameter_ != nullptr) {
            attachment_ = std::make_unique<juce::ParameterAttachment>(
                *parameter_,
                [this](float normalised) { showNormalised(normalised); }, nullptr);
        }
        showNormalised(parameter_ != nullptr ? parameter_->getValue() : 0.0f);

        onClick = [this] { showMenu(); };
    }

    void setOnDark(bool on) {
        // no painel claro a fenda e' carvao, a mesma lingua das teclas de vista
        // e da caixa de preset; numa baia escura ela inverte para um banho de
        // creme. o texto acompanha.
        titleColour_ = on ? palette::textOnDarkSub : palette::textSub;
        textColour_ = palette::ivory;
        slotFill_ = on ? palette::alpha(palette::ivory, 0.10f) : palette::charcoal;
        slotEdge_ = on ? palette::alpha(palette::ivory, 0.26f)
                       : palette::alpha(palette::knobEdge, 0.55f);
        onDark_ = on;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool over, bool down) override {
        const auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty()) {
            return;
        }

        const bool tight = getHeight() < 112;
        const int titleHeight = tight ? 14 : 17;

        // titulo e glifo: a mesma leitura do knob.
        const auto font = fonts().sans(tight ? 10.5f : 12.0f, true)
                              .withExtraKerningFactor(0.10f);
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, title_, 0.0f, 0.0f);
        const auto textWidth = glyphs.getBoundingBox(0, -1, false).getWidth();
        const float glyphSize = tight ? 10.0f : 12.0f;
        const float gap = icon_.isEmpty() ? 0.0f : 4.0f;
        const auto total = (icon_.isEmpty() ? 0.0f : glyphSize) + gap + textWidth;
        auto textLeft = bounds.getCentreX() - total * 0.5f;

        if (!icon_.isEmpty()) {
            OpcodaLookAndFeel::paintGlyph(
                g, icon_,
                juce::Rectangle<float> {textLeft,
                                        static_cast<float>(titleHeight) * 0.5f -
                                            glyphSize * 0.5f,
                                        glyphSize, glyphSize},
                titleColour_, false);
            textLeft += glyphSize + gap;
        }
        g.setColour(titleColour_);
        g.setFont(font);
        g.drawText(title_,
                   juce::Rectangle<float> {textLeft, 0.0f, textWidth,
                                           static_cast<float>(titleHeight)},
                   juce::Justification::centredLeft, false);

        // corpo: uma fenda de escolha, baixa e larga. o azulejo quadrado que aqui
        // estava (do tamanho do disco do knob) lia-se como um knob sem escala;
        // um campo horizontal diz "da' para escolher" antes de o caret se ver.
        constexpr float kSlotHeight {30.0f};
        const float slotWidth = juce::jmin(bounds.getWidth() - 4.0f, 104.0f);
        const auto slot =
            juce::Rectangle<float> {slotWidth, kSlotHeight}.withCentre(
                juce::Point<float> {
                    bounds.getCentreX(),
                    static_cast<float>(titleHeight) +
                        (bounds.getHeight() - static_cast<float>(titleHeight)) * 0.5f});

        auto fill = slotFill_;
        auto edge = slotEdge_;
        if (down) {
            fill = onDark_ ? palette::alpha(palette::ivory, 0.18f) : palette::charcoalDeep;
        } else if (over) {
            fill = onDark_ ? palette::alpha(palette::ivory, 0.16f)
                           : palette::charcoal.interpolatedWith(palette::ivory, 0.10f);
            edge = palette::alpha(palette::accent, 0.70f);
        }

        g.setColour(fill);
        g.fillRoundedRectangle(slot, 3.0f);
        g.setColour(edge);
        g.drawRoundedRectangle(slot, 3.0f, 1.0f);
        // realce de 1 px no topo, como nos outros botoes da casa.
        g.setColour(palette::alpha(palette::ivory, onDark_ ? 0.28f : 0.12f));
        g.drawHorizontalLine(juce::roundToInt(slot.getY() + 1.0f),
                             slot.getX() + 4.0f, slot.getRight() - 4.0f);

        auto textArea = slot;
        const bool narrow = slot.getWidth() < 72.0f;
        const auto caret = textArea.removeFromRight(narrow ? 9.0f : 11.0f)
                               .withSizeKeepingCentre(narrow ? 8.0f : 9.0f,
                                                      narrow ? 8.0f : 9.0f);
        textArea.reduce(3.5f, 0.0f);
        g.setColour(textColour_);
        g.setFont(fonts().sans(narrow ? 9.5f : 11.5f));
        g.drawFittedText(value_, textArea.toNearestInt(),
                         juce::Justification::centred, 1, 0.62f);
        OpcodaLookAndFeel::paintGlyph(
            g, "chevron", caret,
            palette::alpha(textColour_, over ? 1.0f : 0.72f), false);

        if (hasKeyboardFocus(true)) {
            g.setColour(palette::charcoalDeep);
            g.drawRoundedRectangle(slot.reduced(1.0f), 2.5f, 3.0f);
            g.setColour(palette::focusRing);
            g.drawRoundedRectangle(slot.reduced(1.0f), 2.5f, 1.5f);
        }
    }

private:
    void showNormalised(float normalised) {
        auto index = 0;
        if (parameter_ != nullptr) {
            index = juce::jlimit(
                0, juce::jmax(0, labels_.size() - 1),
                juce::roundToInt(parameter_->convertFrom0to1(normalised)));
        }
        value_ = labels_.isEmpty() ? juce::String {} : labels_[index];
        setButtonText(value_);
        repaint();
    }

    void showMenu() {
        if (parameter_ == nullptr || labels_.isEmpty()) {
            return;
        }

        const auto current = juce::jlimit(
            0, labels_.size() - 1,
            juce::roundToInt(parameter_->convertFrom0to1(parameter_->getValue())));
        juce::PopupMenu menu;
        for (int i = 0; i < labels_.size(); ++i) {
            menu.addItem(i + 1, labels_[i], true, i == current);
        }

        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(150),
            [this](int result) {
                if (result > 0 && parameter_ != nullptr) {
                    parameter_->setValueNotifyingHost(
                        parameter_->convertTo0to1(static_cast<float>(result - 1)));
                }
            });
    }

    juce::String title_;
    juce::String icon_;
    juce::StringArray labels_;
    juce::String value_;
    juce::RangedAudioParameter* parameter_ {nullptr};
    std::unique_ptr<juce::ParameterAttachment> attachment_;
    juce::Colour titleColour_ {palette::textSub};
    juce::Colour textColour_ {palette::ivory};
    juce::Colour slotFill_ {palette::charcoal};
    juce::Colour slotEdge_ {palette::alpha(palette::knobEdge, 0.55f)};
    bool onDark_ {false};
};

} // namespace opcoda
