#include "entropy_curve.h"

#include <cmath>

namespace opcoda {
namespace {
constexpr float kMaxBitsPerByte = 8.0f;
}

void EntropyCurve::setVisibleWindow(float start, float end) {
    const auto nextStart = juce::jlimit(0.0f, 1.0f, start);
    const auto nextEnd = juce::jlimit(0.0f, 1.0f, end);
    if (nextStart == windowStart_ && nextEnd == windowEnd_) {
        return;
    }

    windowStart_ = nextStart;
    windowEnd_ = nextEnd;
    rebuildPath();
    repaint();
}

float EntropyCurve::valueAt(float fraction) const noexcept {
    if (curve_.empty()) {
        return 0.0f;
    }

    // A fracao e' da janela visivel, nao do ficheiro. Com o cursor a 0.5, isto
    // le o centro da regiao selecionada.
    const auto window = windowEnd_ > windowStart_ ? windowEnd_ - windowStart_ : 1.0f;
    const auto global =
        juce::jlimit(0.0f, 1.0f, windowStart_ + juce::jlimit(0.0f, 1.0f, fraction) * window);
    const auto index = static_cast<std::size_t>(global * static_cast<float>(curve_.size() - 1));
    return curve_[index];
}

void EntropyCurve::rebuildPath() {
    path_.clear();
    filled_.clear();
    if (curve_.size() < 2 || !(windowEnd_ > windowStart_)) {
        return;
    }

    const auto width = static_cast<float>(getWidth());
    const auto height = static_cast<float>(getHeight());
    const auto lastIndex = static_cast<float>(curve_.size() - 1);
    const auto window = windowEnd_ - windowStart_;

    // Apenas os pontos da janela vao para o Path, e nao a curva inteira. Sem
    // isto, a regiao selecionada seria uma faixa fina por cima da mesma curva
    // de sempre.
    auto firstIndex = static_cast<std::size_t>(
        juce::jlimit(0.0f, lastIndex, std::floor(windowStart_ * lastIndex)));
    auto lastWindowIndex = static_cast<std::size_t>(
        juce::jlimit(0.0f, lastIndex, std::ceil(windowEnd_ * lastIndex)));

    // Uma regiao mais estreita do que uma janela da curva nao tem dois pontos
    // para ligar. Incluir o ponto vizinho desenha o segmento em vez de deixar
    // o display vazio, que seria lido como falta de material.
    if (lastWindowIndex == firstIndex) {
        if (firstIndex > 0) {
            --firstIndex;
        } else if (lastWindowIndex < curve_.size() - 1) {
            ++lastWindowIndex;
        }
    }

    for (auto i = firstIndex; i <= lastWindowIndex; ++i) {
        const auto fileFraction = static_cast<float>(i) / lastIndex;
        const auto x = width * ((fileFraction - windowStart_) / window);
        // Entropia alta sobe. A escala vai ate 8 porque um byte tem 8 bits, e
        // um binario comprimido chega perto disso: o topo da barra e' o
        // "aleatorio" e vale a pena ver que o eixo chega la.
        const auto normalised = juce::jlimit(0.0f, 1.0f,
                                            curve_[i] / kMaxBitsPerByte);
        const auto y = height * (1.0f - normalised);

        if (i == firstIndex) {
            path_.startNewSubPath(x, y);
        } else {
            path_.lineTo(x, y);
        }
    }

    // Copia fechada pela base, para o preenchimento sob a curva.
    filled_ = path_;
    filled_.lineTo(width, height);
    filled_.lineTo(0.0f, height);
    filled_.closeSubPath();
}

void EntropyCurve::paint(juce::Graphics& g) {
    if (curve_.size() < 2 || path_.isEmpty()) {
        return;
    }

    const auto bounds = getLocalBounds().toFloat();

    // Area preenchida sob a curva, muito tenue. E' o que separa a curva do
    // fundo sem precisar de contorno fechado.
    if (!filled_.isEmpty()) {
        g.setColour(palette::alpha(palette::accent, 0.10f));
        g.fillPath(filled_);
    }

    // Halo: tres passadas largas e translucidas por baixo da linha.
    struct Halo { float width; float alpha; };
    for (const auto& pass : {Halo {7.0f, 0.07f}, Halo {4.0f, 0.12f}, Halo {2.5f, 0.20f}}) {
        g.setColour(palette::alpha(palette::accent, pass.alpha));
        g.strokePath(path_, juce::PathStrokeType {pass.width,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});
    }

    // A linha em si, opaca.
    g.setColour(palette::accent);
    g.strokePath(path_, juce::PathStrokeType {1.2f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded});

    // Cursor de leitura, na posicao da janela mostrada nas leituras do rodape.
    const auto x = bounds.getWidth() * cursor_;
    g.setColour(palette::alpha(palette::accentSoft, 0.55f));
    g.drawVerticalLine(juce::roundToInt(x), 0.0f, bounds.getHeight());
}

} // namespace opcoda