#pragma once

#include "fonts.h"
#include "palette.h"

#include "opcoda_core/dsp/curve.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>

namespace opcoda {

class EnvelopeView final : public juce::Component,
                           private juce::AudioProcessorValueTreeState::Listener {
public:
    explicit EnvelopeView(juce::AudioProcessorValueTreeState& state);
    ~EnvelopeView() override;

    void paint(juce::Graphics&) override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // leitura do audio, chamada pelo editor a 60 Hz.
    void setLive(float level, float stage, int phase);

private:
    void parameterChanged(const juce::String&, float) override { repaint(); }

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
