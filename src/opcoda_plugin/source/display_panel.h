#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

class DisplayPanel : public juce::Component {
public:
    DisplayPanel() {
        setOpaque(true);
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        juce::ColourGradient glass {
            palette::displayPanel, bounds.getCentreX(), bounds.getY(),
            palette::display, bounds.getCentreX(), bounds.getBottom(), false};
        g.setGradientFill(glass);
        g.fillRect(bounds);

        // brilho de cima, curto: a luz do painel a bater no vidro.
        juce::ColourGradient sheen {
            palette::alpha(palette::ivory, 0.045f), bounds.getCentreX(), bounds.getY(),
            juce::Colours::transparentBlack, bounds.getCentreX(),
            bounds.getY() + bounds.getHeight() * 0.35f, false};
        g.setGradientFill(sheen);
        g.fillRect(bounds.withHeight(bounds.getHeight() * 0.35f));


    }
};

} // namespace opcoda
