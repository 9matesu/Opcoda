#pragma once

#include <cmath>
#include <cstdint>

namespace opcoda::dsp {

enum class LFOTarget : std::uint8_t {
    kPitch = 0,
    kDensity,
    kCutoff,
    kPosition,
};

// idem: a ordem e' contrato.
enum class LFOWave : std::uint8_t {
    kSine = 0,
    kTri,
    kSaw,
    kSquare,
    kSampleHold,
};

[[nodiscard]] const char* lfoTargetName(LFOTarget target) noexcept;
[[nodiscard]] const char* lfoWaveName(LFOWave wave) noexcept;

[[nodiscard]] inline LFOTarget sanitizeLFOTarget(LFOTarget target) noexcept {
    return (target <= LFOTarget::kPosition) ? target : LFOTarget::kPitch;
}

[[nodiscard]] inline LFOWave sanitizeLFOWave(LFOWave wave) noexcept {
    return (wave <= LFOWave::kSampleHold) ? wave : LFOWave::kSine;
}

inline constexpr int kLFODivCount = 12;

[[nodiscard]] inline int sanitizeLFODiv(int div) noexcept {
    return (div >= 0 && div < kLFODivCount) ? div : 2; // 2 = 1/4, o default do parametro
}

[[nodiscard]] inline const char* lfoDivName(int div) noexcept {
    switch (sanitizeLFODiv(div)) {
        case 0: return "1/1";
        case 1: return "1/2";
        case 2: return "1/4";
        case 3: return "1/8";
        case 4: return "1/16";
        case 5: return "1/32";
        case 6: return "1/64";
        case 7: return "1/4.";
        case 8: return "1/8.";
        case 9: return "1/4T";
        case 10: return "1/8T";
        default: return "1/16T";
    }
}

// ciclos por batida. 1/8 da 2 ciclos, que a 120 bpm sao 4 Hz.
[[nodiscard]] inline double cyclesPerBeat(int div) noexcept {
    switch (sanitizeLFODiv(div)) {
        case 0: return 0.25;
        case 1: return 0.5;
        case 2: return 1.0;
        case 3: return 2.0;
        case 4: return 4.0;
        case 5: return 8.0;
        case 6: return 16.0;
        case 7: return 1.0 / 1.5;  // 1/4 pontuada
        case 8: return 1.0 / 0.75; // 1/8 pontuada
        case 9: return 1.5;        // 1/4 tercina
        case 10: return 3.0;       // 1/8 tercina
        default: return 6.0;       // 1/16 tercina
    }
}

[[nodiscard]] inline double sanitizeBpm(double bpm) noexcept {
    return (std::isfinite(bpm) && bpm > 0.0 && bpm <= 1000.0) ? bpm : 120.0;
}

[[nodiscard]] inline double syncedRateHz(int div, double bpm) noexcept {
    return cyclesPerBeat(div) * sanitizeBpm(bpm) / 60.0;
}

[[nodiscard]] inline int sanitizeLFOSteps(int steps) noexcept {
    return (steps < 1) ? 1 : ((steps > 16) ? 16 : steps);
}

[[nodiscard]] inline double quantizeLFOPhase(double phase, int steps) noexcept {
    const int n = sanitizeLFOSteps(steps);
    if (n >= 16) {
        return phase;
    }
    return std::floor(phase * static_cast<double>(n)) / static_cast<double>(n);
}

class LFO {
public:
    void reset() noexcept { phase_ = 0.0; }

    void setPhase(double phase) noexcept { phase_ = phase; }

    void advance(float rateHz, int numSamples, double sampleRate) noexcept;

    [[nodiscard]] float valueAt(LFOWave wave,
                               float rateHz,
                               int offsetSamples,
                               double sampleRate,
                               int steps = 16) const noexcept;

    // valor no inicio do bloco: densidade e cutoff, que sao por bloco e nao por grao.
    [[nodiscard]] float blockValue(LFOWave wave, int steps = 16) const noexcept;

    [[nodiscard]] double phase() const noexcept { return phase_; }

    [[nodiscard]] static float shapeAt(LFOWave wave, double phase) noexcept {
        return waveAt(wave, phase);
    }

private:
    [[nodiscard]] static float waveAt(LFOWave wave, double phase) noexcept;

    double phase_ {0.0};
};

} // namespace opcoda::dsp
