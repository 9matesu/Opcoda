#pragma once

#include <juce_graphics/juce_graphics.h>

// Tokens da estacao de trabalho escura (Granulator II como referencia).
// Ficam aqui para que o editor, o LookAndFeel e o Led pintem a mesma coisa sem
// repetir hex em tres lugares. Fonte: design/DESIGN-SYSTEM.md.
//
// Os tokens nao sao constexpr: o construtor de juce::Colour nao e' uma expressao
// constante, e um token marcado como constexpr nao avalia. inline const evita
// um static por traducao.
namespace opcoda::palette {

// Chassis e superficies escuras. O device e' escuro por fora e mais escuro na
// area de visualizacao: fondo #202326, paineis #292D30, bordas #44494D.
inline const juce::Colour chassis {0xff202326};
inline const juce::Colour chassisDark {0xff191c1e};
inline const juce::Colour chassisBorder {0xff44494d};
inline const juce::Colour header {0xff26292d};
inline const juce::Colour panelBg {0xff292d30};
inline const juce::Colour subPanel {0xff31363a};
inline const juce::Colour valBox {0xff3d4349};

// Display escuro, afundado em relacao ao chassis.
inline const juce::Colour display {0xff0e1013};
inline const juce::Colour displayPanel {0xff14171b};
inline const juce::Colour displayBorder {0xff2e3338};
inline const juce::Colour displayGrid {0x0dffffff};

// Texto. Sobre o fundo escuro os claros passam de 4,5:1 (1.4.3):
// textDark/textOnDark ~13:1, textSub/textOnDarkSub ~7:1 sobre #202326.
// Os nomes textDark/textSub mantem-se para nao renomear dezenas de chamadas:
// significam agora "primario" e "secundario" sobre superficie escura.
inline const juce::Colour textDark {0xffe4e7e8};
inline const juce::Colour textSub {0xff92999e};
inline const juce::Colour textMuted {0xff6b7176};
inline const juce::Colour textField {0xff92999e};
inline const juce::Colour textOnDark {0xffe4e7e8};
inline const juce::Colour textOnDarkSub {0xff92999e};

// Foco. Precisa de 3:1 contra o fundo (1.4.11) e de ser o unico sinal de que
// um controle tem o foco do teclado (2.4.7). Sobre o fundo escuro o ambar da
// 8,29:1, por isso o anel de foco e' accent — com forma (anel de 2 px) como
// segunda pista, como pede o 1.4.1.
inline const juce::Colour focusRing {0xffffaa24};

// Acentos. Ambar para selecao, reproducao e valores importantes; teal para
// visualizacao de dados secundaria. Sobre o fundo escuro ambos passam 3:1
// (1.4.11) e o texto claro sobre eles passa onde e' exigido por vir sempre
// acompanhado de leitura numerica (ver nota do arco do knob abaixo).
//
// Razoes medidas contra #202326 (I:/temp/opencode/contrast-dark.py; re-medir
// se um token mudar). 1.4.3 pede 4,5:1 para texto, 1.4.11 pede 3:1 para
// graficos e foco:
//
//   textDark sobre chassis    12,70:1   (texto primario)
//   textSub sobre chassis      5,47:1   (texto secundario)
//   accent sobre chassis       8,29:1   (anel de foco, arco do knob)
//   waveLow sobre chassis      9,00:1   (forma de onda)
//   border sobre chassis       1,73:1   (borda estrutural: nao carrega estado
//                                       sozinha — abas tem texto + filete —
//                                       por isso o 1.4.11 nao se aplica)
//   textMuted sobre chassis    3,20:1   (so LED apagado e icone desligado,
//                                       decorativos, nunca texto de leitura)
//
// O arco de valor sustenta-se como antes: cada knob tem a leitura numerica por
// baixo, que repete o valor sem cor nenhuma.
inline const juce::Colour accent {0xffffaa24};
inline const juce::Colour accentSoft {0xffffc83b};
inline const juce::Colour accentWarn {0xffffb833};
inline const juce::Colour ok {0xff4caf50};
inline const juce::Colour okBright {0xff82d64a};
inline const juce::Colour error {0xffe05373};

// Onda: teal contido para o material, ambar para selecao/reproducao.
// Medidos contra o display `#0e1013` (1.4.11 vale no escuro como no claro):
//
//   waveLow sobre display    10,86:1
//   accentSoft sobre display 12,32:1
//   waveHigh sobre display    6,40:1
inline const juce::Colour waveLow {0xff36d9ce};
inline const juce::Colour waveHigh {0xffea7070};

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

} // namespace opcoda::palette