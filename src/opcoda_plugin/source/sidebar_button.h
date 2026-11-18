#pragma once

#include "fonts.h"
#include "look_and_feel.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda::sidebar {

// Uma coluna de botoes: largura fixa, um abaixo do outro, sem gap. O glifo e o
// rotulo estao sempre visiveis -- nada de expandir com hover, nada de texto
// escondido. A mesma caixa serve as vistas e os modulos, para a interface ter
// uma linguagem so.
inline constexpr int kWidth {72};
inline constexpr int kHeight {32};
inline constexpr int kInset {8};
inline constexpr int kGlyphBox {28};

inline void paintButton(juce::Graphics& g,
                        juce::Rectangle<float> bounds,
                        const juce::String& icon,
                        const juce::String& label,
                        float hover,
                        bool selected,
                        bool onDark = false) {
    const auto box = bounds.reduced(1.0f);
    const auto lit = juce::jlimit(0.0f, 1.0f, hover);

    // Sem caixa no repouso: a coluna e' uma lista continua, nao uma pilha de
    // cartoes a flutuar. O hover poe um banho subtil; o estado activo poe um
    // banho mais claro e um filete de 2 px na esquerda -- o acento e' da coluna,
    // nao do botao, e por isso o item activo parece pertencer a' sidebar.
    if (selected) {
        g.setColour(palette::alpha(palette::ivory, 0.10f));
        g.fillRect(box);
        g.setColour(palette::ivory);
        g.fillRect(juce::Rectangle<float> {box.getX(), box.getY(), 2.0f, box.getHeight()});
    } else if (lit > 0.0f) {
        g.setColour(palette::alpha(palette::ivory, (onDark ? 0.07f : 0.05f) * lit));
        g.fillRect(box);
    }

    // tinta: marfim no activo, cinza claro no repouso. Sem inverter para
    // carvao -- a coluna e' escura de ponta a ponta.
    const auto ink = selected ? palette::ivory
                              : palette::alpha(palette::textOnDarkSub, 0.92f);

    // glifo a esquerda, numa caixa quadrada colada a' borda.
    const auto glyphBox = juce::Rectangle<float> {
        box.getX(), box.getY(), static_cast<float>(kGlyphBox), box.getHeight()};
    if (icon.isNotEmpty()) {
        OpcodaLookAndFeel::paintGlyph(g, icon, glyphBox.reduced(3.0f), ink, selected);
    }

    // rotulo a direita, sempre visivel. Encolhe para caber: nunca transborda.
    g.setColour(ink);
    g.setFont(fonts().sans(8.0f, Fonts::Weight::semibold).withExtraKerningFactor(0.05f));
    g.drawFittedText(label,
                     juce::Rectangle<int> {
                         juce::roundToInt(glyphBox.getRight()),
                         juce::roundToInt(box.getY()),
                         juce::roundToInt(box.getWidth()) - kGlyphBox,
                         juce::roundToInt(box.getHeight())},
                     juce::Justification::centredLeft, 1, 0.55f);
}

} // namespace opcoda::sidebar
