#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <utility>
#include <vector>

namespace opcoda::palette {
namespace {

juce::File themeFile() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Opcoda")
        .getChildFile("theme.txt");
}

std::vector<std::pair<juce::Colour, juce::Colour>> tokenPairs() {
    return {
        {previous.chassis, active.chassis},
        {previous.chassisDark, active.chassisDark},
        {previous.chassisBorder, active.chassisBorder},
        {previous.header, active.header},
        {previous.faceplate, active.faceplate},
        {previous.faceplateLight, active.faceplateLight},
        {previous.faceplateDark, active.faceplateDark},
        {previous.charcoal, active.charcoal},
        {previous.charcoalDeep, active.charcoalDeep},
        {previous.panelBg, active.panelBg},
        {previous.subPanel, active.subPanel},
        {previous.valBox, active.valBox},
        {previous.toolbarTop, active.toolbarTop},
        {previous.toolbarBottom, active.toolbarBottom},
        {previous.hair, active.hair},
        {previous.knobTrack, active.knobTrack},
        {previous.waveBody, active.waveBody},
        {previous.regionFill, active.regionFill},
        {previous.knobTop, active.knobTop},
        {previous.knobBottom, active.knobBottom},
        {previous.knobEdge, active.knobEdge},
        {previous.brass, active.brass},
        {previous.rust, active.rust},
        {previous.sage, active.sage},
        {previous.ivory, active.ivory},
        {previous.display, active.display},
        {previous.displayPanel, active.displayPanel},
        {previous.displayBorder, active.displayBorder},
        {previous.displayGrid, active.displayGrid},
        {previous.textDark, active.textDark},
        {previous.textSub, active.textSub},
        {previous.textMuted, active.textMuted},
        {previous.textField, active.textField},
        {previous.textOnDark, active.textOnDark},
        {previous.textOnDarkSub, active.textOnDarkSub},
        {previous.focusRing, active.focusRing},
        {previous.accent, active.accent},
        {previous.accentSoft, active.accentSoft},
        {previous.accentWarn, active.accentWarn},
        {previous.ok, active.ok},
        {previous.okBright, active.okBright},
        {previous.error, active.error},
        {previous.errorLight, active.errorLight},
        {previous.waveLow, active.waveLow},
        {previous.waveHigh, active.waveHigh},
        {previous.meterBed, active.meterBed},
        {previous.meterGreen, active.meterGreen},
        {previous.meterAmber, active.meterAmber},
        {previous.meterRed, active.meterRed},
        {previous.rail, active.rail},
    };
}

void translateComponent(juce::Component& component) {
    static constexpr int colourIds[] = {
        juce::Label::textColourId,
        juce::Label::textWhenEditingColourId,
        juce::Label::backgroundColourId,
        juce::Label::outlineColourId,
        juce::ComboBox::backgroundColourId,
        juce::ComboBox::textColourId,
        juce::ComboBox::outlineColourId,
        juce::ComboBox::buttonColourId,
        juce::ComboBox::arrowColourId,
        juce::ComboBox::focusedOutlineColourId,
        juce::TextButton::buttonColourId,
        juce::TextButton::buttonOnColourId,
        juce::TextButton::textColourOffId,
        juce::TextButton::textColourOnId,
    };

    for (const int id : colourIds) {
        if (component.isColourSpecified(id)) {
            component.setColour(id, translate(component.findColour(id)));
        }
    }

    for (int i = 0; i < component.getNumChildComponents(); ++i) {
        translateComponent(*component.getChildComponent(i));
    }
}

} // namespace

void setTheme(Theme theme) {
    if (currentTheme == theme) {
        return;
    }

    previous = active;
    currentTheme = theme;
    active = theme == Theme::classic ? classicTokens() : darkTokens();
}

Theme loadSavedTheme() {
    const auto file = themeFile();
    if (!file.existsAsFile()) {
        return Theme::dark;
    }

    const auto value = file.loadFileAsString().trim().toLowerCase();
    return value == "classic" ? Theme::classic : Theme::dark;
}

void saveTheme(Theme theme) {
    auto file = themeFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText(theme == Theme::classic ? "classic" : "dark");
}

juce::Colour translate(juce::Colour colour) {
    for (const auto& [from, to] : tokenPairs()) {
        if (from == colour) {
            return to;
        }
    }
    return colour;
}

void applyToTree(juce::Component& root) {
    translateComponent(root);
    root.repaint();
}

} // namespace opcoda::palette