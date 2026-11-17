#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

namespace opcoda::palette {

enum class Theme { dark, classic };

struct Tokens {
    juce::Colour chassis, chassisDark, chassisBorder, header, faceplate, faceplateLight,
        faceplateDark, charcoal, charcoalDeep, panelBg, subPanel, valBox;
    juce::Colour toolbarTop, toolbarBottom, hair, knobTrack, waveBody, regionFill;
    juce::Colour knobTop, knobBottom, knobEdge, brass, rust, sage, ivory;
    juce::Colour display, displayPanel, displayBorder, displayGrid;
    juce::Colour textDark, textSub, textMuted, textField, textOnDark, textOnDarkSub;
    juce::Colour focusRing;
    juce::Colour accent, accentSoft, accentWarn, ok, okBright, error, errorLight;
    juce::Colour waveLow, waveHigh;
    juce::Colour meterBed, meterGreen, meterAmber, meterRed;
    juce::Colour rail;
};

inline Tokens darkTokens() {
    Tokens t {};
    t.chassis = juce::Colour {0xff1b1b1d};
    t.chassisDark = juce::Colour {0xff111111};
    t.chassisBorder = juce::Colour {0xff37373a};
    t.header = juce::Colour {0xff2a2a2c};
    t.faceplate = juce::Colour {0xff202022};
    t.faceplateLight = juce::Colour {0xff262628};
    t.faceplateDark = juce::Colour {0xff161618};
    t.charcoal = juce::Colour {0xff2c2c2e};
    t.charcoalDeep = juce::Colour {0xff161618};
    t.panelBg = juce::Colour {0xff202022};
    t.subPanel = juce::Colour {0xff262628};
    t.valBox = juce::Colour {0xff2c2c2e};

    t.toolbarTop = juce::Colour {0xff333335};
    t.toolbarBottom = juce::Colour {0xff2a2a2c};
    t.hair = juce::Colour {0x12ffffff};
    t.knobTrack = juce::Colour {0x38ffffff};
    t.waveBody = juce::Colour {0xffc9c9cb};
    t.regionFill = juce::Colour {0x0fffffff};

    t.knobTop = juce::Colour {0xff2c2c2e};
    t.knobBottom = juce::Colour {0xff161618};
    t.knobEdge = juce::Colour {0xff37373a};
    t.brass = juce::Colour {0xff9a9a9e};
    t.rust = juce::Colour {0xffe8e8e8};
    t.sage = juce::Colour {0xff6f6f74};
    t.ivory = juce::Colour {0xffdcdcde};

    t.display = juce::Colour {0xff0c0c0d};
    t.displayPanel = juce::Colour {0xff161618};
    t.displayBorder = juce::Colour {0xff37373a};
    t.displayGrid = juce::Colour {0x0dffffff};

    t.textDark = juce::Colour {0xffdcdcde};
    t.textSub = juce::Colour {0xff9a9a9e};
    t.textMuted = juce::Colour {0xff6f6f74};
    t.textField = juce::Colour {0xff9a9a9e};
    t.textOnDark = juce::Colour {0xffdcdcde};
    t.textOnDarkSub = juce::Colour {0xff9a9a9e};

    t.focusRing = juce::Colour {0xffe8e8e8};

    t.accent = juce::Colour {0xffe8e8e8};
    t.accentSoft = juce::Colour {0xfff0f0f0};
    t.accentWarn = juce::Colour {0xffffffff};
    t.ok = juce::Colour {0xff9a9a9e};
    t.okBright = juce::Colour {0xffdcdcde};
    t.error = juce::Colour {0xffffffff};
    t.errorLight = juce::Colour {0xffffffff};

    t.waveLow = juce::Colour {0xff8c8c8e};
    t.waveHigh = juce::Colour {0xffe8e8e8};

    t.meterBed = juce::Colour {0xff161618};
    t.meterGreen = juce::Colour {0xff9a9a9e};
    t.meterAmber = juce::Colour {0xffc4c4c4};
    t.meterRed = juce::Colour {0xffffffff};

    t.rail = juce::Colour {0xff161618};
    return t;
}

inline Tokens classicTokens() {
    Tokens t {};
    t.chassis = juce::Colour {0xff171512};
    t.chassisDark = juce::Colour {0xff0d0c0a};
    t.chassisBorder = juce::Colour {0xff6b5f4c};
    t.header = juce::Colour {0xffd6c9ae};
    t.faceplate = juce::Colour {0xffd6c9ae};
    t.faceplateLight = juce::Colour {0xffe4d9c3};
    t.faceplateDark = juce::Colour {0xff9e8e73};
    t.charcoal = juce::Colour {0xff211d18};
    t.charcoalDeep = juce::Colour {0xff100e0c};
    t.panelBg = juce::Colour {0xff26231e};
    t.subPanel = juce::Colour {0xff302c25};
    t.valBox = juce::Colour {0xff38332b};

    t.toolbarTop = t.faceplateLight;
    t.toolbarBottom = t.faceplate;
    t.hair = juce::Colour {0x12ffffff};
    t.knobTrack = juce::Colour {0x38ffffff};
    t.waveBody = t.sage;
    t.regionFill = juce::Colour {0x0fffffff};

    t.knobTop = juce::Colour {0xff3b352c};
    t.knobBottom = juce::Colour {0xff161310};
    t.knobEdge = juce::Colour {0xff7d7059};
    t.brass = juce::Colour {0xffc09552};
    t.rust = juce::Colour {0xffe2661c};
    t.sage = juce::Colour {0xff7e9a63};
    t.ivory = juce::Colour {0xffeee4cf};

    t.display = juce::Colour {0xff000000};
    t.displayPanel = juce::Colour {0xff070706};
    t.displayBorder = juce::Colour {0xff2b251d};
    t.displayGrid = juce::Colour {0x14ffd9a0};

    t.textDark = juce::Colour {0xff1c1913};
    t.textSub = juce::Colour {0xff4a4237};
    t.textMuted = juce::Colour {0xff6c6353};
    t.textField = juce::Colour {0xff453f35};
    t.textOnDark = juce::Colour {0xffe9dfc9};
    t.textOnDarkSub = juce::Colour {0xffb3a794};

    t.focusRing = juce::Colour {0xfff2b34d};

    t.accent = juce::Colour {0xfff2740d};
    t.accentSoft = juce::Colour {0xffffc061};
    t.accentWarn = juce::Colour {0xffe3a33d};
    t.ok = juce::Colour {0xff7f9a64};
    t.okBright = juce::Colour {0xffb9d18a};
    t.error = juce::Colour {0xffc05a43};
    t.errorLight = juce::Colour {0xffd3705a};

    t.waveLow = juce::Colour {0xffa9c09a};
    t.waveHigh = juce::Colour {0xffe0703a};

    t.meterBed = juce::Colour {0xff080706};
    t.meterGreen = juce::Colour {0xff6fae5c};
    t.meterAmber = juce::Colour {0xffd9a63c};
    t.meterRed = juce::Colour {0xffd4563a};

    t.rail = juce::Colour {0xff1d1a16};
    return t;
}

inline Tokens active {darkTokens()};
inline Tokens previous {darkTokens()};
inline Theme currentTheme {Theme::dark};

inline juce::Colour& chassis = active.chassis;
inline juce::Colour& chassisDark = active.chassisDark;
inline juce::Colour& chassisBorder = active.chassisBorder;
inline juce::Colour& header = active.header;
inline juce::Colour& faceplate = active.faceplate;
inline juce::Colour& faceplateLight = active.faceplateLight;
inline juce::Colour& faceplateDark = active.faceplateDark;
inline juce::Colour& charcoal = active.charcoal;
inline juce::Colour& charcoalDeep = active.charcoalDeep;
inline juce::Colour& panelBg = active.panelBg;
inline juce::Colour& subPanel = active.subPanel;
inline juce::Colour& valBox = active.valBox;
inline juce::Colour& toolbarTop = active.toolbarTop;
inline juce::Colour& toolbarBottom = active.toolbarBottom;
inline juce::Colour& hair = active.hair;
inline juce::Colour& knobTrack = active.knobTrack;
inline juce::Colour& waveBody = active.waveBody;
inline juce::Colour& regionFill = active.regionFill;

inline juce::Colour& knobTop = active.knobTop;
inline juce::Colour& knobBottom = active.knobBottom;
inline juce::Colour& knobEdge = active.knobEdge;
inline juce::Colour& brass = active.brass;
inline juce::Colour& rust = active.rust;
inline juce::Colour& sage = active.sage;
inline juce::Colour& ivory = active.ivory;

inline juce::Colour& display = active.display;
inline juce::Colour& displayPanel = active.displayPanel;
inline juce::Colour& displayBorder = active.displayBorder;
inline juce::Colour& displayGrid = active.displayGrid;

inline juce::Colour& textDark = active.textDark;
inline juce::Colour& textSub = active.textSub;
inline juce::Colour& textMuted = active.textMuted;
inline juce::Colour& textField = active.textField;
inline juce::Colour& textOnDark = active.textOnDark;
inline juce::Colour& textOnDarkSub = active.textOnDarkSub;

inline juce::Colour& focusRing = active.focusRing;

inline juce::Colour& accent = active.accent;
inline juce::Colour& accentSoft = active.accentSoft;
inline juce::Colour& accentWarn = active.accentWarn;
inline juce::Colour& ok = active.ok;
inline juce::Colour& okBright = active.okBright;
inline juce::Colour& error = active.error;
inline juce::Colour& errorLight = active.errorLight;

inline juce::Colour& waveLow = active.waveLow;
inline juce::Colour& waveHigh = active.waveHigh;

inline juce::Colour& meterBed = active.meterBed;
inline juce::Colour& meterGreen = active.meterGreen;
inline juce::Colour& meterAmber = active.meterAmber;
inline juce::Colour& meterRed = active.meterRed;

inline juce::Colour& rail = active.rail;

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

void setTheme(Theme theme);

Theme loadSavedTheme();
void saveTheme(Theme theme);

juce::Colour translate(juce::Colour colour);

void applyToTree(juce::Component& root);

} // namespace opcoda::palette
