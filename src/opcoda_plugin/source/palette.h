#pragma once

#include <juce_graphics/juce_graphics.h>

// Tokens do painel. A referencia e' o mock "Binary Granular VST Redesign":
// chassis escuro, duas faixas de metal escovado (header e rack de macros) e o
// dump hexadecimal no centro. Fonte: design/DESIGN-SYSTEM.md.
//
// Os tokens nao sao constexpr: o construtor de juce::Colour nao e' uma expressao
// constante. inline const evita um static por traducao.
namespace opcoda::palette {

// ---- chassis e metal ----
inline const juce::Colour chassis {0xff121418};
inline const juce::Colour frameBorder {0xff2b303c};

inline const juce::Colour metalTop {0xffeae5d7};
inline const juce::Colour metalBase {0xffded8c8};
inline const juce::Colour metalMiddle {0xffd8d1be};
inline const juce::Colour metalBottom {0xffc9c2ad};
inline const juce::Colour metalBorder {0xffa8a18c};

// Superficies de botao escuro sobre o metal, como os botoes LOAD/SAVE do mock.
inline const juce::Colour metalButton {0xff282c35};
inline const juce::Colour amberText {0xfffcd34d};

// ---- chips do header ----
inline const juce::Colour chipBg {0xff1b1e25};
inline const juce::Colour chipBorder {0xff333a48};
inline const juce::Colour tagBg {0xff282d38};
inline const juce::Colour tagBorder {0xff475569};

// ---- texto ----
// Sobre o metal os primeiros passam de 4,5:1. Sobre o escuro, textOnDark e
// textOnDarkSub sao o par slate-300/slate-500 do dump.
inline const juce::Colour textDark {0xff1a1c1e};
inline const juce::Colour textSub {0xff3d3a33};
inline const juce::Colour textMuted {0xff54503f};
inline const juce::Colour subtitleOnMetal {0xff5a5649};
inline const juce::Colour textField {0xff8a8472};
inline const juce::Colour textOnDark {0xffcbd5e1};
inline const juce::Colour textOnDarkSub {0xff64748b};
inline const juce::Colour chipText {0xff94a3b8};
inline const juce::Colour labelOnMetal {0xff353229};
inline const juce::Colour valueOnMetal {0xff2e2a22};

// ---- superficies auxiliares ----
inline const juce::Colour chassisDark {0xff0a0c10};
inline const juce::Colour header {0xffded8c8};
inline const juce::Colour panelBg {0xff2a2d33};
inline const juce::Colour subPanel {0xffc9c2ad};
inline const juce::Colour valBox {0xffdad4c6};
inline const juce::Colour chassisBorder {0xff8f8878};

// ---- display ----
inline const juce::Colour display {0xff0a0c10};
inline const juce::Colour displayPanel {0xff08090b};
inline const juce::Colour displayBorder {0xff252a36};
inline const juce::Colour displayGrid {0x0dffffff};

// ---- acentos ----
inline const juce::Colour accent {0xffff9800};
inline const juce::Colour accentSoft {0xffffc83b};
inline const juce::Colour accentWarn {0xffffb833};
inline const juce::Colour focusRing {0xffffaa24};
inline const juce::Colour ok {0xff10b981};
inline const juce::Colour okBright {0xff39ff14};
inline const juce::Colour error {0xffff3344};
inline const juce::Colour peakRed {0xfff87171};
inline const juce::Colour oledCyan {0xff00e5ff};

inline const juce::Colour posChipBg {0xff121620};
inline const juce::Colour posChipBorder {0xff2b3345};
inline const juce::Colour footerSurface {0xff101216};

inline const juce::Colour waveLow {0xff36d9ce};
inline const juce::Colour waveHigh {0xffea7070};

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

} // namespace opcoda::palette
