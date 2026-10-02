#pragma once

namespace opcoda::dsp {

class Limiter {
public:
    static constexpr float kCeilingDb = -1.0f;

    void setSampleRate(double sampleRate) noexcept;
    void prepare(double ceilingDb = kCeilingDb) noexcept;
    void reset() noexcept;

    void setAttackMs(double attackMs) noexcept;
    void setReleaseMs(double releaseMs) noexcept;

    [[nodiscard]] float process(float sample) noexcept;

    void processBlock(float* data, int numSamples) noexcept;

    [[nodiscard]] float ceiling() const noexcept { return ceiling_; }
    [[nodiscard]] float currentGain() const noexcept { return gain_; }

private:
    float ceiling_ {0.891f};
    float gain_ {1.0f};
    float attackCoef_ {0.0f};
    float releaseCoef_ {0.0f};
    double sampleRate_ {44100.0};
};

} // namespace opcoda::dsp
