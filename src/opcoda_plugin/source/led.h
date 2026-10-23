#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// indicador com brilho: gradiente radial do centro opaco para a borda
// transparente. nao ha BlendMode aditivo no JUCE 8 e nao faz falta -- em modo
// normal a queda de alpha sobre o escuro le-se como luz.
//
// nenhum LED carrega informacao sozinho: cada um vem com um Label, porque cor nao
// chega a quem nao distingue verde de vermelho (1.4.1).
class Led : public juce::Component {
public:
    enum class State {
        off,    // apagado, sem material carregado
        ready,  // material carregado e pronto
        busy,   // carregando ou trocando
        fault,  // recusa: erro de parser ou de disco
    };

    explicit Led(State initial = State::off) : state_(initial) { setSize(14, 14); }

    void setState(State newState) {
        if (state_ != newState) {
            state_ = newState;
            repaint();
        }
    }

    [[nodiscard]] State state() const noexcept { return state_; }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

        // soquete: circulo escuro recuado com borda de 1 px.
        g.setColour(palette::display);
        g.fillEllipse(bounds.reduced(1.0f));
        g.setColour(palette::alpha(palette::chassisBorder, 0.55f));
        g.drawEllipse(bounds.reduced(1.0f), 1.0f);

        if (state_ == State::off) {
            return;
        }

        const auto tint = colourFor(state_);

        // halo: do centro para fora, opaco a transparente.
        const float haloRadius = radius * 2.4f;
        juce::ColourGradient halo(tint, centre, tint.withAlpha(0.0f),
                                  centre.translated(haloRadius, 0.0f), true);
        g.setGradientFill(halo);
        g.fillEllipse(centre.x - haloRadius, centre.y - haloRadius,
                      haloRadius * 2.0f, haloRadius * 2.0f);

        // nucleo aceso, opaco.
        g.setColour(tint);
        g.fillEllipse(bounds.reduced(radius * 0.42f));
    }

private:
    static juce::Colour colourFor(State state) {
        switch (state) {
            case State::ready: return palette::okBright;
            case State::busy: return palette::accent;
            case State::fault: return palette::error;
            case State::off: break;
        }
        return palette::textMuted;
    }

    State state_;
};

} // namespace opcoda