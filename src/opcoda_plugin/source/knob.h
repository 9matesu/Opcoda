#pragma once

#include "fonts.h"
#include "look_and_feel.h"
#include "palette.h"
#include "ui_animation.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>

namespace opcoda {

class Knob : public juce::Component, private juce::Slider::Listener, private juce::Timer {
public:
    struct Spec {
        const char* paramId;
        const char* title;       // curto, como no mock
        const char* spokenName;  // nome acessivel, descritivo
        float defaultValue;
        std::function<juce::String(float)> format;
        juce::StringArray choiceLabels {};
        const char* icon {nullptr};
    };

    Knob(juce::AudioProcessorValueTreeState& state, const Spec& spec)
        : attachment_(makeAttachment(state, spec, slider_)) {
        title_ = spec.title;
        getProperties().set("paramId", spec.paramId);
        icon_ = spec.icon != nullptr ? spec.icon : "";
        slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider_.setVelocityBasedMode(false);
        slider_.setDoubleClickReturnValue(true, spec.defaultValue);
        slider_.setName(spec.spokenName);
        slider_.setTooltip(juce::String {spec.spokenName});
        addAndMakeVisible(slider_);

        setName(spec.spokenName);
        setHelpText(spec.spokenName);

        format_ = spec.format;
        choiceLabels_ = spec.choiceLabels;

        if (!choiceLabels_.isEmpty()) {
            const auto choices = choiceLabels_.size();
            if (choices <= 5) {
                slider_.getProperties().set("uiDetents", choices);
            } else {
                hasMenu_ = true;
                menuButton_.setButtonText("");
                menuButton_.setName(juce::String {spec.spokenName} + ": escolher valor");
                menuButton_.setTooltip("Escolher " + juce::String {spec.title} + " na lista");
                menuButton_.getProperties().set("uiIcon", "chevron");
                menuButton_.getProperties().set("uiLook", "icon");
                menuButton_.onClick = [this] { showChoiceMenu(); };
                addAndMakeVisible(menuButton_);
            }
        }

        slider_.addListener(this);

        hover_.attach(slider_, [this](float h, float) {
            slider_.getProperties().set("uiHover", static_cast<double>(h));
            repaint();
        });

        showValue(slider_.getValue());
    }

    ~Knob() override { stopTimer(); }

    void setOnDark(bool on) {
        onDark_ = on;
        repaint();
    }

    void resized() override {
        auto area = getLocalBounds();
        const bool titled = area.getHeight() >= 64;
        if (titled) {
            area.removeFromTop(13);
        }
        area.removeFromTop(1);

        constexpr int maxDiameter {64};
        const auto diameter = std::max(
            18, std::min(maxDiameter, std::min(area.getWidth() - 4, area.getHeight() - 2)));
        disc_ = area.withSizeKeepingCentre(diameter, diameter).toFloat();
        slider_.setBounds(disc_.toNearestInt());

        if (hasMenu_) {
            auto menuArea = disc_.removeFromRight(15).removeFromBottom(15);
            menuButton_.setBounds(menuArea.toNearestInt());
        }
    }

    void paint(juce::Graphics& g) override {
        if (getWidth() <= 0 || getHeight() <= 0 || title_.isEmpty()) {
            return;
        }
        if (getHeight() < 64) {
            return;
        }

        const auto font = type::captionFont();
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, title_, 0.0f, 0.0f);
        const auto textWidth = glyphs.getBoundingBox(0, -1, false).getWidth();

        constexpr float glyphSize {10.0f};
        const float gap = icon_.isEmpty() ? 0.0f : 4.0f;
        const auto total = (icon_.isEmpty() ? 0.0f : glyphSize) + gap + textWidth;
        const auto left = static_cast<float>(getWidth()) * 0.5f - total * 0.5f;
        const auto row = juce::Rectangle<float> {0.0f, 0.0f, static_cast<float>(getWidth()), 13.0f};

        auto textLeft = left;
        if (!icon_.isEmpty()) {
            OpcodaLookAndFeel::paintGlyph(
                g, icon_,
                juce::Rectangle<float> {left, row.getCentreY() - glyphSize * 0.5f, glyphSize,
                                        glyphSize},
                titleInk(), false);
            textLeft += glyphSize + gap;
        }

        g.setColour(titleInk());
        g.setFont(font);
        g.drawText(title_, juce::Rectangle<float> {textLeft, row.getY(), textWidth, row.getHeight()},
                   juce::Justification::centredLeft, false);
    }

    void paintOverChildren(juce::Graphics& g) override {
        if (disc_.getWidth() < 20.0f || valueText_.isEmpty()) {
            return;
        }

        const auto hover = hover_.hover();
        const auto flash = flash_;

        if (hover > 0.01f) {
            g.setColour(palette::alpha(palette::ivory, 0.05f * hover));
            g.fillEllipse(disc_.expanded(1.5f));
        }

        const auto centre = disc_.getCentre();
        const auto fontSize = juce::jlimit(8.0f, type::body, disc_.getWidth() * 0.17f);
        const auto right = hasMenu_ ? 15.0f : 2.0f;
        auto textArea = juce::Rectangle<float> {
            disc_.getX() + 2.0f, centre.y - fontSize * 0.62f,
            disc_.getWidth() - 2.0f - right, fontSize * 1.24f};

        auto ink = valueInk().interpolatedWith(palette::ivory, 0.35f * hover);
        ink = ink.interpolatedWith(palette::accent, 0.55f * flash);
        drawFitted(g, valueText_, textArea, fontSize, ink);
    }

private:
    [[nodiscard]] juce::Colour titleInk() const noexcept {
        return onDark_ ? palette::textOnDarkSub : palette::textSub;
    }
    [[nodiscard]] juce::Colour valueInk() const noexcept {
        return onDark_ ? palette::ivory : palette::textDark;
    }

    static std::unique_ptr<juce::SliderParameterAttachment> makeAttachment(
        juce::AudioProcessorValueTreeState& state, const Spec& spec, juce::Slider& slider) {
        auto* parameter = state.getParameter(spec.paramId);
        jassert(parameter != nullptr);
        return parameter != nullptr
                   ? std::make_unique<juce::SliderParameterAttachment>(*parameter, slider)
                   : nullptr;
    }

    static void drawFitted(juce::Graphics& g,
                           const juce::String& text,
                           juce::Rectangle<float> area,
                           float maxSize,
                           juce::Colour ink) {
        auto size = maxSize;
        auto font = fonts().sans(size);
        const auto widthOf = [&font](const juce::String& value) {
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText(font, value, 0.0f, 0.0f);
            return glyphs.getBoundingBox(0, -1, false).getWidth();
        };
        while (size > 7.5f && widthOf(text) > area.getWidth()) {
            size -= 0.5f;
            font = fonts().sans(size);
        }
        g.setFont(font);
        g.setColour(ink);
        g.drawText(text, area, juce::Justification::centred, false);
    }

    void sliderValueChanged(juce::Slider* changed) override {
        if (changed == &slider_) {
            showValue(slider_.getValue());
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
            juce::PopupMenu::Options().withTargetComponent(&menuButton_).withMinimumWidth(140),
            [this](int result) {
                if (result > 0) {
                    slider_.setValue(static_cast<double>(result - 1), juce::sendNotificationSync);
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

        if (text.isNotEmpty() && text != valueText_) {
            valueText_ = text;
            flash_ = 1.0f;
            startTimerHz(60);
            repaint();
        }
    }

    void timerCallback() override {
        flash_ -= 0.22f;
        if (flash_ <= 0.0f) {
            flash_ = 0.0f;
            stopTimer();
        }
        repaint();
    }

    juce::Slider slider_;
    juce::TextButton menuButton_;
    PressHover hover_;
    bool hasMenu_ {false};
    bool onDark_ {false};
    juce::String title_;
    juce::String icon_;
    juce::String valueText_;
    juce::Rectangle<float> disc_ {};
    float flash_ {0.0f};
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
    std::function<juce::String(float)> format_;
    juce::StringArray choiceLabels_;
};

} // namespace opcoda