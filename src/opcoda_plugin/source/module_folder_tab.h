#pragma once

#include "sidebar_button.h"
#include "ui_animation.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Botao de modulo (GRAIN/ENV/FILTER/MOD). Usa exactamente a mesma caixa das
// abas de vista: mesma largura, mesma altura, mesmo glifo, rotulo sempre visto.
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

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override {
        juce::ignoreUnused(down);
        const auto lit = juce::jmax(highlighted ? 1.0f : 0.0f, hover_.hover());
        const auto bounds = getLocalBounds().toFloat();
        const auto icon = getProperties()["uiIcon"].toString();

        sidebar::paintButton(g, bounds, icon, getButtonText(), lit, getToggleState());

        if (hasKeyboardFocus(true)) {
            g.setColour(palette::charcoalDeep);
            g.drawRect(bounds.reduced(1.0f), 3.0f);
            g.setColour(palette::focusRing);
            g.drawRect(bounds.reduced(1.0f), 1.5f);
        }
    }

private:
    PressHover hover_;
};

} // namespace opcoda
