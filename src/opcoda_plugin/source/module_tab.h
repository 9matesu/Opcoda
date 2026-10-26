#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// aba de modulo: texto puro, no padrao Ableton. sem glifo e sem animacao -- a
// spec de redesign manda "animacao ambiente: nenhuma" e cromo em segundo
// plano; a palavra e o que o leitor de tela diz na mesma.
//
// a activa leva carvao e filete ambar de 2 px: selecao legivel sem depender
// so' de cor. a inactiva e' palavra sobre o chassis, com contorno de 1 px.
class ModuleTab : public juce::Button {
public:
    ModuleTab(const juce::String& title, const juce::String& spokenName)
        : juce::Button(spokenName) {
        setButtonText(title);
        setClickingTogglesState(true);
        setTooltip(spokenName);
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override {
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        const auto on = getToggleState();

        auto fill = on ? palette::charcoal : juce::Colours::transparentBlack;
        if (down) {
            fill = on ? palette::charcoalDeep
                      : palette::alpha(palette::charcoalDeep, 0.16f);
        } else if (highlighted && !on) {
            fill = palette::alpha(palette::charcoalDeep, 0.10f);
        }

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 4.0f);
        if (on) {
            g.setColour(palette::accent);
            g.fillRect(juce::Rectangle<float> {bounds.getX() + 5.0f,
                                               bounds.getBottom() - 3.0f,
                                               bounds.getWidth() - 10.0f, 2.0f});
        } else {
            g.setColour(palette::alpha(palette::faceplateDark, 0.55f));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }

        g.setFont(fonts().sans(10.5f, on));
        g.setColour(on ? palette::ivory : palette::textSub);
        g.drawText(getButtonText(), bounds, juce::Justification::centred, false);

        if (hasKeyboardFocus(true)) {
            g.setColour(palette::charcoalDeep);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 3.0f);
            g.setColour(palette::focusRing);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.5f);
        }
    }
};

} // namespace opcoda
