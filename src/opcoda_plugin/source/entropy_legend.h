#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Legenda da entropia: a mesma escala das barras de baixo do ecra. Escuro e
// baixo = material previsivel; claro e alto = material aleatorio.
class EntropyLegend : public juce::Component {
public:
    EntropyLegend() {
        setInterceptsMouseClicks(false, false);
        setHelpText("Entropy per address: dark and short is repetitive, "
                    "bright and tall is random");
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        if (bounds.getWidth() < 20.0f) {
            return;
        }

        g.setFont(fonts().sans(8.0f, false));
        g.setColour(palette::textOnDarkSub);
        g.drawText("ENTROPY",
                   juce::Rectangle<float> {bounds.getX(), bounds.getY(),
                                           bounds.getWidth() * 0.55f, 9.0f},
                   juce::Justification::centredLeft, false);
        g.drawText("0",
                   juce::Rectangle<float> {bounds.getRight() - 52.0f, bounds.getY(), 12.0f,
                                           9.0f},
                   juce::Justification::centredRight, false);
        g.drawText("8 bits",
                   juce::Rectangle<float> {bounds.getRight() - 38.0f, bounds.getY(), 38.0f,
                                           9.0f},
                   juce::Justification::centredLeft, false);

        const auto bar = juce::Rectangle<float> {bounds.getX(), bounds.getY() + 11.0f,
                                                 bounds.getWidth(), 4.0f};
        juce::ColourGradient gradient {juce::Colour {0xff282828}, bar.getX(), bar.getCentreY(),
                                       palette::ivory, bar.getRight(), bar.getCentreY(),
                                       false};
        g.setGradientFill(gradient);
        g.fillRect(bar);
        g.setColour(palette::alpha(palette::ivory, 0.18f));
        g.drawRect(bar, 1.0f);
    }
};

} // namespace opcoda