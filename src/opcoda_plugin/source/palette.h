#pragma once

#include <juce_graphics/juce_graphics.h>

// tokens da chapa: grafite por fora, creme envelhecido por dentro, carvao nas
// janelas. o display e' a unica superficie que nao muda de temperatura.
//
// nao sao constexpr: o construtor de juce::Colour nao e' expressao constante.
// inline const evita um static por traducao.
namespace opcoda::palette {

// -- materiais --
// chassis externo e chapa. o chassis aparece nas costuras e no teclado; a
// faceplate e' o corpo do instrumento.
inline const juce::Colour chassis {0xff171512};
inline const juce::Colour chassisDark {0xff0d0c0a};
inline const juce::Colour chassisBorder {0xff6b5f4c};
inline const juce::Colour header {0xffd6c9ae};
inline const juce::Colour faceplate {0xffd6c9ae};
inline const juce::Colour faceplateLight {0xffe4d9c3};
inline const juce::Colour faceplateDark {0xff9e8e73};
inline const juce::Colour charcoal {0xff211d18};
inline const juce::Colour charcoalDeep {0xff100e0c};
inline const juce::Colour panelBg {0xff26231e};
inline const juce::Colour subPanel {0xff302c25};
inline const juce::Colour valBox {0xff38332b};

// knob: corpo escuro com aro quente. o topo e' mais claro que a base.
inline const juce::Colour knobTop {0xff3b352c};
inline const juce::Colour knobBottom {0xff161310};
inline const juce::Colour knobEdge {0xff7d7059};
inline const juce::Colour brass {0xffc09552};
// o arco do valor e' laranja: e' a leitura mais forte do painel.
inline const juce::Colour rust {0xffe2661c};
inline const juce::Colour sage {0xff7e9a63};
inline const juce::Colour ivory {0xffeee4cf};

// display afundado. o fundo e' preto: o material e' a unica coisa que brilha.
// grid leva um toque de ambar para nao parecer web.
inline const juce::Colour display {0xff000000};
inline const juce::Colour displayPanel {0xff070706};
inline const juce::Colour displayBorder {0xff2b251d};
inline const juce::Colour displayGrid {0x14ffd9a0};

// tinta. textDark e' tinta sobre creme; textOnDark e' creme sobre carvao.
// textSub fica acima de 4,5:1 no creme e textOnDarkSub no display.
inline const juce::Colour textDark {0xff1c1913};
inline const juce::Colour textSub {0xff4a4237};
inline const juce::Colour textMuted {0xff6c6353};
inline const juce::Colour textField {0xff453f35};
inline const juce::Colour textOnDark {0xffe9dfc9};
inline const juce::Colour textOnDarkSub {0xffb3a794};

// foco. o anel duplo do LookAndFeel garante 3:1 nos dois fundos.
inline const juce::Colour focusRing {0xfff2b34d};

// -- acentos --
// laranja queimado para selecao, transporte e valores; verde-palido para sinal.
// o texto numerico repete o estado sem depender da cor.
inline const juce::Colour accent {0xfff2740d};
inline const juce::Colour accentSoft {0xffffc061};
inline const juce::Colour accentWarn {0xffe3a33d};
inline const juce::Colour ok {0xff7f9a64};
inline const juce::Colour okBright {0xffb9d18a};
inline const juce::Colour error {0xffc05a43};
inline const juce::Colour errorLight {0xffd3705a};

// onda: verde-palido no material previsivel, laranja no quente, ferrugem no
// aleatorio. as tres cores tambem aparecem na silhueta de entropia.
inline const juce::Colour waveLow {0xffa9c09a};
inline const juce::Colour waveHigh {0xffe0703a};

// -- medidor de saida --
// escada de mixer: verde no corpo, ambar perto do topo, vermelho no tecto. a
// calha e' quase preta para os segmentos apagados nao virarem grelha.
inline const juce::Colour meterBed {0xff080706};
inline const juce::Colour meterGreen {0xff6fae5c};
inline const juce::Colour meterAmber {0xffd9a63c};
inline const juce::Colour meterRed {0xffd4563a};

// barra de ferramentas lateral do ecra: carvao unico, para os icones de vista
// e zoom lerem como uma tira de teclas do aparelho.
inline const juce::Colour rail {0xff1d1a16};

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

} // namespace opcoda::palette
