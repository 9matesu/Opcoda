#pragma once

#include "animator.h"
#include "fonts.h"
#include "palette.h"

#include "opcoda_core/dsp/lfo.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>

namespace opcoda {

class LfoView final : public juce::Component,
                      private juce::AudioProcessorValueTreeState::Listener {
public:
    explicit LfoView(juce::AudioProcessorValueTreeState& state);
    ~LfoView() override;

    void paint(juce::Graphics&) override;

    // fase em voltas (0..1) e valor bipolar do ultimo bloco.
    void setLive(float phase, float value);

private:
    void parameterChanged(const juce::String&, float) override { repaint(); }

    [[nodiscard]] float valueOf(const char* id) const;
    [[nodiscard]] float rateHz() const;
    [[nodiscard]] juce::Colour inkFor(dsp::LFOTarget target) const noexcept;

    juce::AudioProcessorValueTreeState& state_;

    Eased phaseEased_ {};
    Eased valueEased_ {};
    double lastLiveMs_ {0.0};
    bool liveSeen_ {false};

    float rawPhasePrev_ {0.0f};
    float phaseContinuous_ {0.0f};
    float livePhase_ {0.0f};
    float liveValue_ {0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoView)
};

} // namespace opcoda
