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

// Frame escuro. O device e' metal claro nas faixas de controle (header e painel
// de macro knobs) e escuro em todo o resto, que e' a leitura do mock
// "Binary Granular VST Redesign": #121418 de fundo com borda #2b303c.
inline const juce::Colour chassis {0xff121418};
inline const juce::Colour frameBorder {0xff2b303c};

// Metal escovado das faixas de controle. O mock usa um gradiente vertical
// (#eae5d7 -> #d8d1be -> #c9c2ad); a base plana cobre o caso de um fill unico e
// a borda separa o metal do escuro.
inline const juce::Colour metalTop {0xffeae5d7};
inline const juce::Colour metalBase {0xffded8c8};
inline const juce::Colour metalMiddle {0xffd8d1be};
inline const juce::Colour metalBottom {0xffc9c2ad};
inline const juce::Colour metalBorder {0xffa8a18c};

// Superficies de botao escuro sobre o metal, como os botoes LOAD/SAVE do mock:
// #282c35 com texto ambar.
inline const juce::Colour metalButton {0xff282c35};
inline const juce::Colour amberText {0xfffcd34d};

// Chip do ficheiro no header: superficie escura #1b1e25 com borda #333a48, do
// bloco de ficheiro do mock. Fica sobre o metal claro sem virar uma caixa de
// metal, que era o que a leitura anterior fazia.
inline const juce::Colour chipBg {0xff1b1e25};
inline const juce::Colour chipBorder {0xff333a48};
inline const juce::Colour tagBg {0xff282d38};
inline const juce::Colour tagBorder {0xff475569};

// Rotulo sobre metal. O #353229 do mock e' mais quente que textSub e e' o que
// faz o titulo do knob pertencer a peca e nao ao fundo.
inline const juce::Colour labelOnMetal {0xff353229};

// Valor do knob impresso no metal, sem caixa: #2e2a22 do mock.
inline const juce::Colour valueOnMetal {0xff2e2a22};

// Divisorias dos chips e contorno do LED: um cinza neutro que se le sobre o
// metal e sobre o escuro, sem gritar em nenhum dos dois.
inline const juce::Colour chassisBorder {0xff8f8878};

// Chip da faixa de arquivo: metal um tom abaixo do header, como os chips
// #c9c2ad do mock.
inline const juce::Colour subPanel {0xffc9c2ad};

// Caixa de valor dos knobs: metal clareado com a borda das divisorias, o
// equivalente a valBox #d5d8da do mock anterior trazido para o metal.
inline const juce::Colour valBox {0xffdad4c6};

// Display escuro. O dump hexadecimal e' a zona mais escura do mock (#08090b),
// com grade e moldura em azul fendado.
inline const juce::Colour display {0xff0a0c10};
inline const juce::Colour displayPanel {0xff08090b};
inline const juce::Colour displayBorder {0xff252a36};
inline const juce::Colour displayGrid {0x0dffffff};

// Texto. Sobre o metal os tres primeiros passam de 4,5:1, que e' o que o
// criterio 1.4.3 do WCAG 2.2 exige. Sobre o escuro, o par textOnDark/textOnDarkSub
// e' o par slate-300/slate-500 do dump hex do mock.
inline const juce::Colour textDark {0xff1a1c1e};
inline const juce::Colour textSub {0xff3d3a33};
inline const juce::Colour textMuted {0xff54503f};
// Subtitulo do wordmark no mock: #5a5649, mais quente e mais claro que textSub.
inline const juce::Colour subtitleOnMetal {0xff5a5649};
inline const juce::Colour textField {0xff8a8472};
inline const juce::Colour textOnDark {0xffcbd5e1};
inline const juce::Colour textOnDarkSub {0xff64748b};
// slate-400 do mock: tamanho e tag do chip de ficheiro.
inline const juce::Colour chipText {0xff94a3b8};

// Acentos. O ambar e' preenchimento e indicador, nunca portador de texto
// pequeno sobre o metal: sobre #ded8c8 ele da 2:1 e reprovaria o contraste.
// Sobre o display escuro da 9,8:1, e e' la que ele escreve.
inline const juce::Colour accent {0xffff9800};
inline const juce::Colour accentSoft {0xffffc83b};
inline const juce::Colour accentWarn {0xffffb833};
inline const juce::Colour ok {0xff10b981};
inline const juce::Colour okBright {0xff39ff14};
inline const juce::Colour error {0xffff3344};

// Ciano do chip POS do mock (#00e5ff). E' a unica leitura em ciano do painel e
// por isso nao compete com o ambar do estado nem com o verde do SR.
inline const juce::Colour oledCyan {0xff00e5ff};

// Chip POS: fundo #121620 com borda #2b3345, do rodape de estado do mock.
inline const juce::Colour posChipBg {0xff121620};
inline const juce::Colour posChipBorder {0xff2b3345};

// Pico em red-400 do mock (#f87171). O error #ff3344 fica para as recusas do
// parser, onde o vermelho tem de ser mais forte do que uma leitura de nivel.
inline const juce::Colour peakRed {0xfff87171};

// Faixa do rodape de saida: um tom acima do chassis escuro. A moldura do topo
// e' a frameBorder, para a faixa ler como a mesma familia das outras
// superficies.
inline const juce::Colour footerSurface {0xff101216};

inline juce::Colour alpha(juce::Colour colour, float opacity) {
    return colour.withAlpha(opacity);
}

} // namespace opcoda::palette
