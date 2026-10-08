#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Area escura do device. E' onde mora toda a energia de brilho, porque e' a unica
// regiao com contraste suficiente para glow: sobre o chassis claro, um halo
// laranja some.
//
// Nesta etapa pinta fundo, grade e a moldura. As abas de secao e a curva de
// entropia entram na etapa seguinte.
class DisplayPanel : public juce::Component {
public:
    DisplayPanel() {
        setOpaque(true);
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);

        g.setColour(palette::display);
        g.fillRect(bounds);

        // Grade de 24 px do mock. A linha vertical e' a mais presente, para
        // marcar o tempo sem competir com o conteudo.
        if (cachedWidth_ != bounds.getWidth() || cachedHeight_ != bounds.getHeight()) {
            rebuildGrid(bounds.getWidth(), bounds.getHeight());
        }

        g.setColour(palette::displayGrid);
        g.strokePath(gridPath_, juce::PathStrokeType {1.0f});

        // Linha do zero, no meio da altura e mais forte que a grade.
        // drawHorizontalLine toma int, e bounds e' float.
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawHorizontalLine(juce::roundToInt(bounds.getCentreY()), 0.0f, bounds.getWidth());

        // Brilho interno da moldura: duas linhas brancas cada vez mais fracas
        // para dentro. E' o que afunda o ecra em relacao ao chassi — luz que vem
        // de dentro do device, nao de cima dele.
        //
        // Branco e sem acento de proposito: qualquer laranja aqui teria de passar
        // o 1.4.11, e brilho decorativo nao precisa de cor para se ler como
        // profundidade.
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawRect(bounds.reduced(1.0f), 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.03f));
        g.drawRect(bounds.reduced(2.0f), 1.0f);

        g.setColour(palette::displayBorder);
        g.drawRect(bounds, 1.0f);
    }

private:
    void rebuildGrid(float width, float height) {
        gridPath_.clear();
        constexpr float step = 24.0f;
        for (float x = step; x < width; x += step) {
            gridPath_.startNewSubPath(x, 0.0f);
            gridPath_.lineTo(x, height);
        }
        for (float y = step; y < height; y += step) {
            gridPath_.startNewSubPath(0.0f, y);
            gridPath_.lineTo(width, y);
        }
        cachedWidth_ = width;
        cachedHeight_ = height;
    }

    juce::Path gridPath_;
    float cachedWidth_ {-1.0f};
    float cachedHeight_ {-1.0f};
};

} // namespace opcoda