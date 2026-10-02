#pragma once

#include "opcoda_core/dsp/dc_blocker.h"
#include "opcoda_core/dsp/limiter.h"
#include "opcoda_core/dsp/window.h"

#include <array>
#include <cstdint>

namespace opcoda::dsp {

struct GranularParams {
    float grainSizeMs {40.0f};
    float densityGrainsPerSec {20.0f};
    float position {0.0f};
    float spray {0.0f};
    float pitchSemitones {0.0f};
    float volumeDb {0.0f};
    float pan {0.0f};
    WindowType window {WindowType::kHann};
};

struct Grain {
    bool active {false};
    double position {};
    double readStep {};
    int remaining {};
    int windowIndex {};
    int writeIndex {0};
    float gain {};
    float panLeft {};
    float panRight {};
};

class GranularEngine {
public:
    static constexpr int kMaxVoices = 8;
    static constexpr int kMaxGrainSamples = 48000;
    static constexpr float kMaxGrainMs = 100.0f;
    static constexpr float kMinGrainMs = 1.0f;

    void prepare(double sampleRate, int maximumBlockSize) noexcept;
    void reset() noexcept;

    void setSampleRate(double sampleRate) noexcept { sampleRate_ = sampleRate; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    void setSource(const float* samples, std::size_t count) noexcept;

    [[nodiscard]] bool hasSource() const noexcept { return source_ != nullptr; }
    [[nodiscard]] std::size_t sourceSize() const noexcept { return sourceCount_; }

    void processBlock(float* left, float* right, int numSamples,
                      const GranularParams& params) noexcept;

    [[nodiscard]] int activeVoiceCount() const noexcept;
    [[nodiscard]] int lastActiveVoices() const noexcept { return lastActiveVoices_; }

private:
    void rebuildWindow(const GranularParams& params) noexcept;
    void startGrain(int voice, const GranularParams& params, double entropy) noexcept;

    static constexpr std::size_t kMaxEntropyPoints = 1024;

    double sampleRate_ {44100.0};

    const float* source_ {nullptr};
    std::size_t sourceCount_ {0};

    std::array<Grain, kMaxVoices> voices_ {};

    std::array<float, kMaxGrainSamples> windowA_ {};
    std::array<float, kMaxGrainSamples> windowB_ {};
    const float* activeWindow_ {nullptr};
    int activeWindowLength_ {0};
    WindowType activeWindowType_ {WindowType::kHann};

    double spawnAccumulator_ {0.0};

    std::array<float, kMaxEntropyPoints> entropyCurve_ {};
    std::size_t entropyPointCount_ {0};

    DcBlocker dcBlockerLeft_ {};
    DcBlocker dcBlockerRight_ {};
    Limiter limiterLeft_ {};
    Limiter limiterRight_ {};

    int lastActiveVoices_ {0};
};

} // namespace opcoda::dsp
