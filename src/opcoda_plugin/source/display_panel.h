#pragma once

#include "palette.h"
#include "ui_tokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Painel do display: a mesma chapa do painel de knobs (raio 6, filete de 1 px) e
// um vidro preto liso por dentro. O vidro e' liso de proposito: um gradiente
// por baixo do ByteDisplay nunca se via -- o componente tapava-o -- e dois
// fundos diferentes no mesmo ecra e' exactamente o que a revisao quer eliminar.
// O rail da sidebar e' a unica superfície clara da coluna, e um fio vertical
// marca o fim dela, alinhado com o painel de parametros.
class DisplayPanel : public juce::Component {
public:
    DisplayPanel() {
        setInterceptsMouseClicks(false, false);
    }

    // 16 de padding + 72 de botao: o rail acaba antes do respiro do conteudo.
    void setRailWidth(int width) {
        if (railWidth_ != width) {
            railWidth_ = width;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto radius = static_cast<float>(tokens::radiusPanel);

        // involucro: a mesma chapa do painel de parametros.
        juce::Path shell;
        shell.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(),
                                  bounds.getHeight(), radius, radius, true, true, true, true);
        g.setColour(palette::faceplateLight);
        g.fillPath(shell);

        // vidro: preto liso, igual ao fundo que o ByteDisplay pinta.
        {
            const juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(shell);
            g.setColour(palette::display);
            g.fillRect(bounds);

            // rail da sidebar: um degrau acima do vidro, com o fio no fim.
            if (railWidth_ > 0) {
                const auto rail = static_cast<float>(railWidth_);
                g.setColour(palette::displayPanel);
                g.fillRect(bounds.withWidth(rail));
                g.setColour(palette::alpha(palette::ivory, 0.07f));
                g.fillRect(juce::Rectangle<float> {rail, bounds.getY() + 16.0f, 1.0f,
                                                   bounds.getHeight() - 32.0f});
            }
        }

        // fio de contorno, como nos outros paineis.
        g.setColour(palette::alpha(palette::ivory, tokens::borderAlpha));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
    }

private:
    int railWidth_ {0};
};

} // namespace opcoda