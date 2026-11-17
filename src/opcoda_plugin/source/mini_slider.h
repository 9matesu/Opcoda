#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace opcoda {

class MiniSlider final : public juce::Component, private juce::Slider::Listener {
    class SilentSlider final : public juce::Slider {
    public:
        void paint(juce::Graphics&) override {}
    };

public:
    struct Spec {
        const char* paramId;
        const char* title;
        const char* spokenName;
        float defaultValue;
        std::function<juce::String(float)> format;
    };

    MiniSlider(juce::AudioProcessorValueTreeState& state, const Spec& spec)
        : attachment_(makeAttachment(state, spec.paramId, slider_)) {
        title_ = spec.title;
        format_ = spec.format;

        slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider_.setDoubleClickReturnValue(true, spec.defaultValue);
        slider_.setName(spec.spokenName);
        slider_.setTooltip(juce::String {spec.spokenName});
        addAndMakeVisible(slider_);
        slider_.addListener(this);

        setName(spec.spokenName);
        setHelpText(spec.spokenName);
        showValue(slider_.getValue());
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        constexpr float kLabelWidth {52.0f};
        constexpr float kValueWidth {46.0f};
        constexpr float kTrackHeight {3.0f};

        g.setFont(type::captionFont());
        g.setColour(palette::textOnDarkSub);
        g.drawText(title_,
                   juce::Rectangle<float> {bounds.getX(), bounds.getY(), kLabelWidth,
                                           bounds.getHeight()},
                   juce::Justification::centredLeft, false);

        const auto track = juce::Rectangle<float> {bounds.getX() + kLabelWidth,
                                                   bounds.getCentreY() - kTrackHeight * 0.5f,
                                                   bounds.getWidth() - kLabelWidth - kValueWidth,
                                                   kTrackHeight};
        g.setColour(palette::alpha(palette::ivory, 0.12f));
        g.fillRoundedRectangle(track, kTrackHeight * 0.5f);

        const auto level = slider_.valueToProportionOfLength(slider_.getValue());
        const auto fill =
            juce::jmax(kTrackHeight, track.getWidth() * static_cast<float>(level));
        g.setColour(palette::accent);
        g.fillRoundedRectangle(track.withWidth(fill), kTrackHeight * 0.5f);

        g.setColour(palette::textOnDark);
        g.drawText(valueText_,
                   juce::Rectangle<float> {track.getRight() + 4.0f, bounds.getY(),
                                           kValueWidth - 4.0f, bounds.getHeight()},
                   juce::Justification::centredRight, false);
    }

    void resized() override {
        constexpr float kLabelWidth {52.0f};
        constexpr float kValueWidth {46.0f};
        const auto hit = getLocalBounds().toFloat().reduced(0.0f, 2.0f);
        const auto track = juce::Rectangle<float> {hit.getX() + kLabelWidth, hit.getY(),
                                                   hit.getWidth() - kLabelWidth - kValueWidth,
                                                   hit.getHeight()};
        slider_.setBounds(track.toNearestInt());
    }

private:
    static std::unique_ptr<juce::SliderParameterAttachment> makeAttachment(
        juce::AudioProcessorValueTreeState& state, const char* paramId,
        juce::Slider& slider) {
        auto* parameter = state.getParameter(paramId);
        jassert(parameter != nullptr);
        return parameter != nullptr
                   ? std::make_unique<juce::SliderParameterAttachment>(*parameter, slider)
                   : nullptr;
    }

    void sliderValueChanged(juce::Slider*) override { showValue(slider_.getValue()); }

    void showValue(double value) {
        const auto text = format_(static_cast<float>(value));
        if (text.isNotEmpty() && text != valueText_) {
            valueText_ = text;
            repaint();
        }
    }

    SilentSlider slider_;
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;
    std::function<juce::String(float)> format_;
    juce::String title_;
    juce::String valueText_;
};

} // namespace opcoda