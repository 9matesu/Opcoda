#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// janela afundada do instrumento: fundo, bisel e profundidade. o conteudo e' dos
// filhos (ByteDisplay, HexGrid).
//
// **sem grelha.** a grelha de 24 px foi o que fazia o ecra parecer um painel web:
// competia com a forma de onda e nao media nada. o que fica e' um vidro escuro com
// gradiente curto, sombra interna e um brilho de 1 px no topo, que dao a
// profundidade sem desenhar linhas por baixo do material.
class DisplayPanel : public juce::Component {
public:
    DisplayPanel() {
        setOpaque(true);
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // vidro: mais claro em cima, quase preto em baixo. o gradiente e' vertical e
        // unico, para o ecra nao ter bandas visiveis.
        juce::ColourGradient glass {
            palette::displayPanel, bounds.getCentreX(), bounds.getY(),
            palette::display, bounds.getCentreX(), bounds.getBottom(), false};
        g.setGradientFill(glass);
        g.fillRect(bounds);

        // brilho de cima, curto: a luz do painel a bater no vidro.
        juce::ColourGradient sheen {
            palette::alpha(palette::ivory, 0.045f), bounds.getCentreX(), bounds.getY(),
            juce::Colours::transparentBlack, bounds.getCentreX(),
            bounds.getY() + bounds.getHeight() * 0.35f, false};
        g.setGradientFill(sheen);
        g.fillRect(bounds.withHeight(bounds.getHeight() * 0.35f));

        // sombra interna: escuro em cima e a' esquerda, luz em baixo e a' direita.
        g.setColour(juce::Colours::black.withAlpha(0.60f));
        g.drawHorizontalLine(1, 0.0f, bounds.getWidth());
        g.drawVerticalLine(1, 0.0f, bounds.getHeight());
        g.setColour(palette::alpha(palette::ivory, 0.05f));
        g.drawHorizontalLine(juce::roundToInt(bounds.getBottom()) - 2, 0.0f,
                             bounds.getWidth());
        g.drawVerticalLine(juce::roundToInt(bounds.getRight()) - 2, 0.0f,
                           bounds.getHeight());

        g.setColour(palette::displayBorder);
        g.drawRect(bounds.reduced(0.5f), 1.0f);
    }
};

} // namespace opcoda
