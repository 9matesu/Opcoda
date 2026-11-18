#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

namespace opcoda::palette {

enum class Theme { dark, classic, light, amber, cyan };

struct Tokens {
    // Material do chassi, do mais fundo ao mais claro. Cada degrau vale 3 a 6
    // niveis: a hierarquia le-se sem contorno e dois paineis vizinhos nunca
    // parecem materiais diferentes.
    juce::Colour chassis, chassisDark, chassisBorder, header, faceplate, faceplateLight,
        faceplateDark, charcoal, charcoalDeep, subPanel;
    juce::Colour toolbarTop, toolbarBottom, hair, knobTrack;
    juce::Colour knobEdge, brass, rust, sage, ivory;
    juce::Colour display, displayPanel, displayBorder, displayGrid;
    juce::Colour textDark, textSub, textMuted, textField, textOnDark, textOnDarkSub;
    juce::Colour focusRing;
    juce::Colour accent, accentSoft, accentWarn, ok, okBright, error, errorLight;
    juce::Colour meterBed, meterGreen, meterAmber, meterRed;
};

inline Tokens darkTokens() {
    Tokens t {};
    // a janela e o 0x0a; a sombra do chassi e o 5 degraus abaixo.
    t.chassis = juce::Colour {0xff0a0a0a};
    t.chassisDark = juce::Colour {0xff050505};
    t.chassisBorder = juce::Colour {0xff2a2a2a};
    t.header = juce::Colour {0xff141414};
    // corpo atras dos paineis: um degrau acima da janela, nunca preto.
    t.faceplate = juce::Colour {0xff0e0e0e};
    t.faceplateLight = juce::Colour {0xff181818}; // painel primario
    t.faceplateDark = juce::Colour {0xff080808};  // filetes e separadores
    t.charcoal = juce::Colour {0xff1c1c1c};       // controlo e teclado
    t.charcoalDeep = juce::Colour {0xff080808};   // encaixes, celulas, teclas pretas
    t.subPanel = juce::Colour {0xff141414};       // painel secundario (master)

    t.toolbarTop = juce::Colour {0xff202020};
    t.toolbarBottom = juce::Colour {0xff161616};
    t.hair = juce::Colour {0x14ffffff};
    t.knobTrack = juce::Colour {0x44ffffff};

    t.knobEdge = juce::Colour {0xff2e2e2e};
    t.brass = juce::Colour {0xff9a9a9e};
    t.rust = juce::Colour {0xfff2f2f2};
    t.sage = juce::Colour {0xff6f6f74};
    t.ivory = juce::Colour {0xfff2f2f2};

    t.display = juce::Colour {0xff000000};
    t.displayPanel = juce::Colour {0xff0a0a0a};
    t.displayBorder = juce::Colour {0xff2a2a2a};
    t.displayGrid = juce::Colour {0x10ffffff};

    t.textDark = juce::Colour {0xfff2f2f2};
    t.textSub = juce::Colour {0xffb0b0b0};
    t.textMuted = juce::Colour {0xff808080};
    t.textField = juce::Colour {0xffb0b0b0};
    t.textOnDark = juce::Colour {0xfff2f2f2};
    t.textOnDarkSub = juce::Colour {0xffb0b0b0};

    t.focusRing = juce::Colour {0xfff2f2f2};

    t.accent = juce::Colour {0xfff2f2f2};
    t.accentSoft = juce::Colour {0xffffffff};
    t.accentWarn = juce::Colour {0xffffffff};
    t.ok = juce::Colour {0xffb0b0b0};
    t.okBright = juce::Colour {0xfff2f2f2};
    t.error = juce::Colour {0xffe8323c};
    t.errorLight = juce::Colour {0xffe8323c};

    t.meterBed = juce::Colour {0xff0a0a0a};
    t.meterGreen = juce::Colour {0xffb0b0b0};
    t.meterAmber = juce::Colour {0xffd0d0d0};
    t.meterRed = juce::Colour {0xffe8323c};

    return t;
}

inline Tokens classicTokens() {
    Tokens t {};
    t.chassis = juce::Colour {0xff171512};
    t.chassisDark = juce::Colour {0xff0d0c0a};
    t.chassisBorder = juce::Colour {0xff6b5f4c};
    t.header = juce::Colour {0xffd6c9ae};
    t.faceplate = juce::Colour {0xffcfc2a8};
    t.faceplateLight = juce::Colour {0xffe4d9c3};
    t.faceplateDark = juce::Colour {0xff9e8e73};
    t.charcoal = juce::Colour {0xff211d18};
    t.charcoalDeep = juce::Colour {0xff100e0c};
    t.subPanel = juce::Colour {0xffc9bca2};

    t.toolbarTop = t.faceplateLight;
    t.toolbarBottom = t.faceplate;
    t.hair = juce::Colour {0x12ffffff};
    t.knobTrack = juce::Colour {0x38ffffff};

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

    t.meterBed = juce::Colour {0xff080706};
    t.meterGreen = juce::Colour {0xff6fae5c};
    t.meterAmber = juce::Colour {0xffd9a63c};
    t.meterRed = juce::Colour {0xffd4563a};

    return t;
}

inline Tokens lightTokens() {
    Tokens t {};
    // modo claro: chassis de papel, paineis claros, texto escuro. O ecra fica
    // escuro em todos os temas -- e' onde a onda e' desenhada a branco.
    t.chassis = juce::Colour {0xffded9d0};
    t.chassisDark = juce::Colour {0xffc8c2b8};
    t.chassisBorder = juce::Colour {0xffaaa398};
    t.header = juce::Colour {0xffefebe3};
    t.faceplate = juce::Colour {0xffe6e1d7};
    t.faceplateLight = juce::Colour {0xfff7f4ee};
    t.faceplateDark = juce::Colour {0xffc9c3b7};
    t.charcoal = juce::Colour {0xff2b2b2b};
    t.charcoalDeep = juce::Colour {0xff121212};
    t.subPanel = juce::Colour {0xffdcd6cb};
    t.toolbarTop = t.faceplateLight;
    t.toolbarBottom = t.faceplate;
    t.hair = juce::Colour {0x18000000};
    t.knobTrack = juce::Colour {0x38000000};
    t.knobEdge = juce::Colour {0xff8a8478};
    t.brass = juce::Colour {0xff8a6a2f};
    t.rust = juce::Colour {0xffc05a2a};
    t.sage = juce::Colour {0xff5f7a4a};
    t.ivory = juce::Colour {0xfff7f4ee};

    t.display = juce::Colour {0xff000000};
    t.displayPanel = juce::Colour {0xff0a0a0a};
    t.displayBorder = juce::Colour {0xff2a2a2a};
    t.displayGrid = juce::Colour {0x10ffffff};

    t.textDark = juce::Colour {0xff1c1c1c};
    t.textSub = juce::Colour {0xff4a4a4a};
    t.textMuted = juce::Colour {0xff6c6c6c};
    t.textField = juce::Colour {0xff3a3a3a};
    t.textOnDark = juce::Colour {0xfff2f2f2};
    t.textOnDarkSub = juce::Colour {0xffb0b0b0};

    t.focusRing = juce::Colour {0xff3b6cff};
    // azul cobalto: acento forte, legivel sobre o papel e sobre o vidro.
    t.accent = juce::Colour {0xff2f5fe0};
    t.accentSoft = juce::Colour {0xff8fb0ff};
    t.accentWarn = juce::Colour {0xffc05a2a};
    t.ok = juce::Colour {0xff3f7a4a};
    t.okBright = juce::Colour {0xff5fa06a};
    t.error = juce::Colour {0xffc0392b};
    t.errorLight = juce::Colour {0xffd35a4a};

    t.meterBed = juce::Colour {0xffd8d3c9};
    t.meterGreen = juce::Colour {0xff3f7a4a};
    t.meterAmber = juce::Colour {0xffb07a2a};
    t.meterRed = juce::Colour {0xffc0392b};
    return t;
}

inline Tokens amberTokens() {
    Tokens t = darkTokens();
    // grafite com ambar: o mesmo material, outra luz.
    t.faceplateLight = juce::Colour {0xff1c1712};
    t.faceplate = juce::Colour {0xff100d0a};
    t.subPanel = juce::Colour {0xff15110d};
    t.accent = juce::Colour {0xffffa726};
    t.accentSoft = juce::Colour {0xffffd08a};
    t.accentWarn = juce::Colour {0xffff7043};
    t.brass = juce::Colour {0xffffb74d};
    t.focusRing = t.accentSoft;
    return t;
}

inline Tokens cyanTokens() {
    Tokens t = darkTokens();
    // grafite com ciano: a mesma maquina, outra temperatura.
    t.faceplateLight = juce::Colour {0xff121719};
    t.faceplate = juce::Colour {0xff0b0f11};
    t.subPanel = juce::Colour {0xff0e1416};
    t.accent = juce::Colour {0xff35d0e0};
    t.accentSoft = juce::Colour {0xff9ae6f0};
    t.accentWarn = juce::Colour {0xff4fc3f7};
    t.brass = juce::Colour {0xff7fdfe6};
    t.focusRing = t.accentSoft;
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
inline juce::Colour& subPanel = active.subPanel;
inline juce::Colour& toolbarTop = active.toolbarTop;
inline juce::Colour& toolbarBottom = active.toolbarBottom;
inline juce::Colour& hair = active.hair;
inline juce::Colour& knobTrack = active.knobTrack;

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

inline juce::Colour& meterBed = active.meterBed;
inline juce::Colour& meterGreen = active.meterGreen;
inline juce::Colour& meterAmber = active.meterAmber;
inline juce::Colour& meterRed = active.meterRed;

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

void setTheme(Theme theme);

Theme loadSavedTheme();
void saveTheme(Theme theme);

juce::Colour translate(juce::Colour colour);

void applyToTree(juce::Component& root);

} // namespace opcoda::palette
