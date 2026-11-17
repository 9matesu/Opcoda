#pragma once

#include "palette.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace opcoda {

class OpcodaKeyboard final : public juce::MidiKeyboardComponent {
public:
    using MidiKeyboardComponent::MidiKeyboardComponent;

    void drawWhiteNote(int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                       bool isOver, juce::Colour, juce::Colour) override {
        auto fill = palette::ivory;
        if (isDown) {
            fill = palette::ivory.interpolatedWith(palette::charcoal, 0.26f);
        } else if (isOver) {
            fill = palette::ivory.interpolatedWith(palette::charcoal, 0.08f);
        }

        g.setColour(fill);
        g.fillRect(area);

        // o filete de 1 px e' a unica separacao: a tecla nao tem contorno.
        g.setColour(palette::alpha(palette::charcoalDeep, 0.45f));
        g.fillRect(area.withWidth(1.0f));
    }

    void drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                       bool isOver, juce::Colour) override {
        auto fill = palette::charcoalDeep;
        if (isDown) {
            fill = palette::charcoal;
        } else if (isOver) {
            fill = palette::charcoalDeep.interpolatedWith(palette::charcoal, 0.55f);
        }

        g.setColour(fill);
        g.fillRect(area);
    }
};

} // namespace opcoda