#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>

namespace opcoda {

// resposta dos dois filtros, desenhada a partir dos mesmos coeficientes que o
// audio usa.
//
// **nao e' um grafico decorativo.** a curva sai de dsp::makeBiquadCoeffs com o
// tipo, o corte, o Q e o slope ligados aos knobs, e a linha viva usa o corte que
// o motor ja' modulou com keytrack, envelope e lfo. o que se ve e' o que sai.
//
// duas pegas: a do filtro 1 e a do filtro 2. no eixo x o corte, no eixo y o Q --
// o gesto que se faz num filtro, sem passar por quatro knobs.
class FilterView final : public juce::Component {
public:
    explicit FilterView(juce::AudioProcessorValueTreeState& state);

    void paint(juce::Graphics&) override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // corte ja' modulado, publicado pelo audio. zero = ainda nao ha' leitura.
    void setLive(float cutoff1, float cutoff2);

private:
    enum class Handle : std::uint8_t { none, first, second };

    struct Response {
        juce::Rectangle<float> plot;
        float cutoff1 {1000.0f};
        float cutoff2 {20000.0f};
        float q1 {0.7f};
        float q2 {0.7f};
        int type1 {0};
        int type2 {0};
        bool slope24 {false};
        double sampleRate {44100.0};
    };

    [[nodiscard]] Response response() const;
    [[nodiscard]] Handle handleAt(juce::Point<float> point) const;
    [[nodiscard]] float xForHz(float hz, const juce::Rectangle<float>& plot) const;
    [[nodiscard]] float hzForX(float x, const juce::Rectangle<float>& plot) const;
    [[nodiscard]] float yForDb(float db, const juce::Rectangle<float>& plot) const;
    // dB da cadeia (filtro 1 com o slope dele, depois filtro 2) num ponto. os
    // cortes entram por parametro para a linha viva poder usar os modulados.
    [[nodiscard]] static float responseDb(const Response& geometry, float hz, float cutoff1,
                                          float cutoff2);
    [[nodiscard]] float valueOf(const char* id) const;
    void setValue(const char* id, float value);
    void beginGesture(Handle handle);
    void endGesture();

    juce::AudioProcessorValueTreeState& state_;

    Handle active_ {Handle::none};
    Handle hover_ {Handle::none};
    bool gestureOpen_ {false};
    const char* gestureA_ {nullptr};
    const char* gestureB_ {nullptr};

    float startCutoff_ {0.0f};
    float startQ_ {0.0f};

    float liveCutoff1_ {0.0f};
    float liveCutoff2_ {0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterView)
};

} // namespace opcoda
