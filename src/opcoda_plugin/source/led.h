#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Indicador com brilho.
//
// O brilho e' um gradiente radial que vai do centro opaco para a borda
// transparente. Nao existe BlendMode aditivo no JUCE 8, e nao faz falta: em
// BlendMode normal, a queda de alpha sobre um fundo escuro produz o mesmo
// resultado visual de uma luz que se dissolve no fundo.
//
// Nenhum LED carrega informacao sozinho. Cada instanciacao vem acompanhada de
// um Label, porque cor nao e' suficiente para quem nao distingue verde de
// vermelho, e o criterio 1.4.1 do WCAG 2.2 pede mais que cor.
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

        // Soquete: circulo escuro recuado com borda de 1 px.
        g.setColour(palette::display);
        g.fillEllipse(bounds.reduced(1.0f));
        g.setColour(palette::alpha(palette::chassisBorder, 0.55f));
        g.drawEllipse(bounds.reduced(1.0f), 1.0f);

        if (state_ == State::off) {
            return;
        }

        const auto tint = colourFor(state_);

        // Halo: do centro para fora, opaco a transparente.
        const float haloRadius = radius * 2.4f;
        juce::ColourGradient halo(tint, centre, tint.withAlpha(0.0f),
                                  centre.translated(haloRadius, 0.0f), true);
        g.setGradientFill(halo);
        g.fillEllipse(centre.x - haloRadius, centre.y - haloRadius,
                      haloRadius * 2.0f, haloRadius * 2.0f);

        // Nucleo aceso, opaco.
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