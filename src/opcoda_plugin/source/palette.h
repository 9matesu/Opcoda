#pragma once

#include <juce_graphics/juce_graphics.h>

// Tokens extraidos do projeto Stitch e registrados em design/DESIGN-SYSTEM.md.
// Ficam aqui para que o editor, o LookAndFeel e o Led pintem a mesma coisa sem
// repetir hex em tres lugares.
//
// Os tokens nao sao constexpr: o construtor de juce::Colour nao e' uma expressao
// constante, e um token marcado como constexpr nao avalia. inline const evita
// um static por traducao.
namespace opcoda::palette {

// Chassis e superficies claras. O device e' claro por fora e escuro na area de
// visualizacao, que e' a convencao do Ableton.
inline const juce::Colour chassis {0xffc2c6c9};
inline const juce::Colour chassisDark {0xffb2b6ba};
inline const juce::Colour chassisBorder {0xff52575c};
inline const juce::Colour header {0xffd2d6d9};
inline const juce::Colour panelBg {0xffb6babd};
inline const juce::Colour subPanel {0xffa8acad};
inline const juce::Colour valBox {0xffd5d8da};

// Display escuro.
inline const juce::Colour display {0xff0e1013};
inline const juce::Colour displayPanel {0xff14181c};
inline const juce::Colour displayBorder {0xff252b31};
inline const juce::Colour displayGrid {0x0dffffff};

// Texto. Sobre superficie clara os tres primeiros passam de 4,5:1, que e' o que
// o criterio 1.4.3 do WCAG 2.2 exige.
inline const juce::Colour textDark {0xff1b1e22};
inline const juce::Colour textSub {0xff383d42};
inline const juce::Colour textMuted {0xff6b7176};
inline const juce::Colour textField {0xff555a60};
inline const juce::Colour textOnDark {0xffc6cacc};
inline const juce::Colour textOnDarkSub {0xff8e98a1};

// Acentos. O laranja e' preenchimento e indicador, nunca portador de texto
// pequeno: sobre o chassis ele da 2,4:1 e reprovaria o contraste.
inline const juce::Colour accent {0xffff9a00};
inline const juce::Colour accentSoft {0xffffc83b};
inline const juce::Colour accentWarn {0xffffb833};
inline const juce::Colour ok {0xff4caf50};
inline const juce::Colour okBright {0xff82d64a};
inline const juce::Colour error {0xffe05373};

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

} // namespace opcoda::palette