#pragma once

#include <cstdint>

namespace opcoda::dsp {

enum class FilterType : std::uint8_t {
    kLowPass = 0,
    kHighPass,
    kBandPass,
    kNotch,
};

[[nodiscard]] const char* filterName(FilterType type) noexcept;

[[nodiscard]] FilterType sanitizeFilterType(FilterType type) noexcept;

struct BiquadCoeffs {
    float b0 {1.0f};
    float b1 {0.0f};
    float b2 {0.0f};
    float a1 {0.0f};
    float a2 {0.0f};
};

[[nodiscard]] BiquadCoeffs makeBiquadCoeffs(FilterType type,
                                           float cutoffHz,
                                           float q,
                                           double sampleRate) noexcept;

class Biquad {
public:
    void prepare() noexcept { reset(); }
    void reset() noexcept;

    [[nodiscard]] float process(float x, const BiquadCoeffs& coeffs) noexcept;

private:
    float x1_ {0.0f};
    float x2_ {0.0f};
    float y1_ {0.0f};
    float y2_ {0.0f};
};

} // namespace opcoda::dsp
