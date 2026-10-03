#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Curva de entropia por janela do binario.
//
// O brilho e' a mesma curva desenhada varias vezes com espessuras decrescentes
// e opacidade decrescente. Nao existe BlendMode aditivo no JUCE 8, e nao e'
// preciso: sobre fundo quase preto, camadas translucidas somam-se de forma
// suficiente para ler como halo.
//
// A escala e' fixa em 0 a 8 bits por byte. Uma escala auto-ajustada faria um
// binario de texto parecer tao ruidoso quanto um comprimido, que e'
// precisamente a distincao que o display existe para mostrar.
class EntropyCurve : public juce::Component {
public:
    explicit EntropyCurve(int resolution = 0)
        : resolution_(resolution) {
        setOpaque(false);
    }

    // Curva ja em bits por byte. Copiada porque o display e' lido a 20 Hz e o
    // vector da fonte muda no ingest; guardar a referencia seria um risco de
    // leitura apos liberacao.
    void setCurve(const std::vector<float>& curve) {
        curve_ = curve;
        rebuildPath();
    }

    void setCursorFraction(float fraction) {
        cursor_ = juce::jlimit(0.0f, 1.0f, fraction);
        repaint();
    }

    [[nodiscard]] float valueAt(float fraction) const noexcept {
        if (curve_.empty()) {
            return 0.0f;
        }
        const auto index = static_cast<std::size_t>(
            juce::jlimit(0.0f, 1.0f, fraction) * static_cast<float>(curve_.size() - 1));
        return curve_[index];
    }

    void paint(juce::Graphics& g) override;
    void resized() override { rebuildPath(); }

private:
    void rebuildPath();

    std::vector<float> curve_;
    juce::Path path_;
    juce::Path filled_;
    int resolution_ {0};
    float cursor_ {0.0f};
};

} // namespace opcoda