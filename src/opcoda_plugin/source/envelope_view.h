#pragma once

#include "fonts.h"
#include "palette.h"

#include "opcoda_core/dsp/curve.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>

namespace opcoda {

// envelope ADSR desenhado: a forma que o motor aplica ao mix, com o ponto vivo
// em cima do que esta' a soar.
//
// **os knobs passam a secundarios.** um ADSR tem quatro numeros e uma forma; ler
// a forma nos numeros e' o que se fazia, e obrigava a imaginar. aqui a forma e' o
// objeto e os numeros sao o ajuste fino.
//
// **reactivo ao clique.** tres pegas de canto mudam os tempos e o sustain, e tres
// pegas a meio dos segmentos dobram a curva de cada um -- os mesmos sete
// parametros que os knobs mostram, sem um knob a mais.
//
// **sincronizado.** o ponto vivo vem do audio (nivel, fase e progresso do
// estagio), nao de um temporizador da interface: mostra o envelope que esta' a
// soar, com o mesmo atraso de um bloco que o resto do display.
class EnvelopeView final : public juce::Component {
public:
    explicit EnvelopeView(juce::AudioProcessorValueTreeState& state);

    void paint(juce::Graphics&) override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // leitura do audio, chamada pelo editor a 60 Hz.
    void setLive(float level, float stage, int phase);

private:
    // pegas: os tres cantos e o meio de cada segmento.
    enum class Handle : std::uint8_t {
        none,
        attack,
        decay,
        release,
        curveAttack,
        curveDecay,
        curveRelease,
    };

    struct Shape {
        juce::Rectangle<float> plot;
        float attackX {0.0f};
        float decayX {0.0f};
        float releaseX {0.0f};
        float sustainY {0.0f};
        float attackCurve {0.0f};
        float decayCurve {0.0f};
        float releaseCurve {0.0f};
        float sustain {1.0f};
    };

    [[nodiscard]] Shape shape() const;
    [[nodiscard]] Handle handleAt(juce::Point<float> point) const;
    [[nodiscard]] juce::Point<float> livePoint(const Shape& geometry) const;
    [[nodiscard]] float valueOf(const char* id) const;
    void setValue(const char* id, float value);
    void beginGesture(Handle handle);
    void endGesture();
    [[nodiscard]] static juce::String labelFor(Handle handle);

    juce::AudioProcessorValueTreeState& state_;

    Handle active_ {Handle::none};
    Handle hover_ {Handle::none};
    bool gestureOpen_ {false};
    const char* gestureA_ {nullptr};
    const char* gestureB_ {nullptr};

    float startTime_ {0.0f};
    float startSustain_ {0.0f};
    float startCurve_ {0.0f};

    float liveLevel_ {0.0f};
    float liveStage_ {0.0f};
    int livePhase_ {0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeView)
};

} // namespace opcoda
