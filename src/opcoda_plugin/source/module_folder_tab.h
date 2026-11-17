#pragma once

#include "fonts.h"
#include "look_and_feel.h"
#include "palette.h"
#include "ui_animation.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

class ModuleFolderTab : public juce::Button {
public:
    ModuleFolderTab(const char* icon, const juce::String& spokenName)
        : juce::Button(spokenName) {
        setName(spokenName);
        setTooltip(spokenName);
        getProperties().set("uiIcon", icon);
        setClickingTogglesState(true);
        hover_.attach(*this, [this](float, float) { repaint(); });
    }

    void setVertical(bool vertical) {
        vertical_ = vertical;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override {
        const auto bounds =
            vertical_ ? getLocalBounds().toFloat().withSizeKeepingCentre(
                            static_cast<float>(getWidth()),
                            juce::jmin(30.0f, static_cast<float>(getHeight()) - 8.0f))
                      : getLocalBounds().toFloat().reduced(1.0f, 0.0f);
        const auto on = getToggleState();
        const auto lit = juce::jmax(highlighted ? 1.0f : 0.0f, hover_.hover());
        juce::ignoreUnused(down);

        if (lit > 0.01f) {
            g.setColour(palette::alpha(palette::ivory, 0.05f * lit));
            g.fillRoundedRectangle(bounds, 3.0f);
        }

        const auto icon = getProperties()["uiIcon"].toString();
        const auto ink = on ? palette::accentSoft
                            : (lit > 0.01f ? palette::textDark : palette::textSub);
        if (icon.isNotEmpty() && OpcodaLookAndFeel::paintGlyph(g, icon, bounds, ink, on)) {
            // o glifo ja' esta' desenhado.
        } else {
            g.setFont(nameFont());
            g.setColour(ink);
            g.drawText(getButtonText(), bounds, juce::Justification::centred, false);
        }

        if (hasKeyboardFocus(true)) {
            g.setColour(palette::charcoalDeep);
            g.drawRect(bounds.reduced(1.0f), 3.0f);
            g.setColour(palette::focusRing);
            g.drawRect(bounds.reduced(1.0f), 1.5f);
        }
    }

private:
    bool vertical_ {false};

    static juce::Font nameFont() {
        return fonts().sans(9.0f, true).withExtraKerningFactor(0.09f);
    }

    PressHover hover_;
};

} // namespace opcoda