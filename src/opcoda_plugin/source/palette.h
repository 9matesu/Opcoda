#pragma once

#include <juce_graphics/juce_graphics.h>

// tokens da estacao de trabalho escura (Granulator II como referencia). ficam
// aqui para o editor, o LookAndFeel e o Led pintarem o mesmo sem repetir hex em
// tres lugares. fonte: design/DESIGN-SYSTEM.md.
//
// nao sao constexpr: o construtor de juce::Colour nao e' expressao constante.
// inline const evita um static por traducao.
namespace opcoda::palette {

// chassis e superficies: fundo #202326, paineis #292D30, bordas #44494D. o
// display e' mais escuro que o chassis.
inline const juce::Colour chassis {0xff202326};
inline const juce::Colour chassisDark {0xff191c1e};
inline const juce::Colour chassisBorder {0xff44494d};
inline const juce::Colour header {0xff26292d};
inline const juce::Colour panelBg {0xff292d30};
inline const juce::Colour subPanel {0xff31363a};
inline const juce::Colour valBox {0xff3d4349};

// display escuro, afundado em relacao ao chassis.
inline const juce::Colour display {0xff0e1013};
inline const juce::Colour displayPanel {0xff14171b};
inline const juce::Colour displayBorder {0xff2e3338};
inline const juce::Colour displayGrid {0x0dffffff};

// texto. sobre o escuro os claros passam 4,5:1 (1.4.3): textDark e textOnDark
// ~13:1, textSub e textOnDarkSub ~7:1 sobre #202326. os nomes textDark/textSub
// ficam para nao renomear dezenas de chamadas: significam primario e secundario.
inline const juce::Colour textDark {0xffe4e7e8};
inline const juce::Colour textSub {0xff92999e};
inline const juce::Colour textMuted {0xff6b7176};
inline const juce::Colour textField {0xff92999e};
inline const juce::Colour textOnDark {0xffe4e7e8};
inline const juce::Colour textOnDarkSub {0xff92999e};

// foco. 3:1 contra o fundo (1.4.11) e unico sinal do foco de teclado (2.4.7).
// o ambar da 8,29:1 sobre o chassis, e o anel de 2 px e' a forma do 1.4.1.
inline const juce::Colour focusRing {0xffffaa24};

// -- acentos --
// ambar para selecao, reproducao e valores; teal para dados secundarios. razoes
// medidas contra #202326 (I:/temp/opencode/contrast-dark.py; re-medir se um token
// mudar):
//
//   textDark 12,70:1   textSub 5,47:1   accent 8,29:1   waveLow 9,00:1
//   border 1,73:1 (estrutural, nao carrega estado sozinha)
//   textMuted 3,20:1 (so LED apagado e icone desligado, nunca texto de leitura)
//
// o arco do knob tem a leitura numerica por baixo, que repete o valor sem cor.
inline const juce::Colour accent {0xffffaa24};
inline const juce::Colour accentSoft {0xffffc83b};
inline const juce::Colour accentWarn {0xffffb833};
inline const juce::Colour ok {0xff4caf50};
inline const juce::Colour okBright {0xff82d64a};
inline const juce::Colour error {0xffe05373};

// onda: teal contido para o material, ambar para selecao/reproducao. sobre o
// display #0e1013: waveLow 10,86:1, accentSoft 12,32:1, waveHigh 6,40:1.
inline const juce::Colour waveLow {0xff36d9ce};
inline const juce::Colour waveHigh {0xffea7070};

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

} // namespace opcoda::palette