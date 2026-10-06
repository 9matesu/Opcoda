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

// Foco. Precisa de 3:1 contra o fundo (1.4.11) e de ser o unico sinal de que
// um controle tem o foco do teclado (2.4.7). Sobra entao apenas uma cor: o
// accent da 1,24:1 sobre o chassis, reprovado. textDark da 9,73:1 e' a escolha.
inline const juce::Colour focusRing {0xff1b1e22};

// Acentos. O laranja e' preenchimento e indicador, nunca portador de texto
// pequeno: sobre o chassis ele da 1,24:1 e reprovaria o contraste.
//
// As razoes medidas, para nao estimar de olhos. O 1.4.11 do WCAG 2.2 exige 3:1
// para um grafico que precise de contraste para ser entendido, e o laranja fica
// abaixo disso em toda superficie clara do chassis:
//
//   accent sobre chassis      1,24:1   (anel de foco, se fosse accent)
//   accent sobre valBox       1,49:1   (aro do knob, pior ponto do arco)
//   accent sobre chassisDark  1,04:1   (aro do knob, ponta escura do gradiente)
//   accent sobre subPanel     1,08:1   (face do knob)
//
// O arco de valor ainda se sustenta porque cada knob tem a leitura numerica por
// baixo, que repete o valor sem cor nenhuma. O anel de foco nao tem essa
// segunda via: e' o unico sinal de que o controle tem o foco do teclado, e por
// isso ele usa focusRing, e nao accent.
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