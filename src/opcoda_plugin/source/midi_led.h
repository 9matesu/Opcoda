#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Ponto de actividade MIDI: acende quando entra um evento, apaga sozinho.
// Sem glow: disco chapado, so muda o preenchimento.
class MidiLed : public juce::Component {
public:
    MidiLed() {
        setInterceptsMouseClicks(false, false);
        setSize(9, 9);
    }

    // 0 = apagado, 1 = aceso. A interface chama com um valor que decai.
    void setLit(float amount) {
        const auto clamped = juce::jlimit(0.0f, 1.0f, amount);
        if (std::abs(clamped - lit_) > 0.01f) {
            lit_ = clamped;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        const auto dot = getLocalBounds().toFloat().reduced(1.5f);

        // corpo apagado
        g.setColour(palette::alpha(palette::ivory, 0.16f));
        g.fillEllipse(dot);
        g.setColour(palette::alpha(palette::ivory, 0.28f));
        g.drawEllipse(dot, 1.0f);

        // preenchimento aceso
        if (lit_ > 0.01f) {
            g.setColour(palette::alpha(midiGreen_, lit_));
            g.fillEllipse(dot.reduced(1.0f));
        }
    }

private:
    juce::Colour midiGreen_ {0xff3ce86a};
    float lit_ {0.0f};
};

} // namespace opcoda