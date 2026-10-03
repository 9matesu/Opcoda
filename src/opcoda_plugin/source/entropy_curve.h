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

    // Janela visivel da curva, como fracao do ficheiro inteiro. O motor ouve a
    // regiao selecionada, por isso a curva tambem mostra a regiao selecionada:
    // uma curva sempre do ficheiro inteiro mentiria sobre o que esta a soar.
    void setVisibleWindow(float start, float end);

    void setCursorFraction(float fraction) {
        cursor_ = juce::jlimit(0.0f, 1.0f, fraction);
        repaint();
    }

    // Valor na posicao do cursor, dentro da janela visivel. Com o cursor a 0.5,
    // devolve o centro da regiao selecionada, e nao o centro do ficheiro.
    [[nodiscard]] float valueAt(float fraction) const noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override { rebuildPath(); }

private:
    void rebuildPath();

    std::vector<float> curve_;
    juce::Path path_;
    juce::Path filled_;
    int resolution_ {0};
    float cursor_ {0.0f};
    float windowStart_ {0.0f};
    float windowEnd_ {1.0f};
};

} // namespace opcoda