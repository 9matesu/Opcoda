#pragma once

#include <cmath>

namespace opcoda::dsp {

[[nodiscard]] inline float curveShape(float phase, float curve) noexcept {
    const float p = (phase < 0.0f) ? 0.0f : ((phase > 1.0f) ? 1.0f : phase);
    if (!std::isfinite(curve) || (curve > -1.0e-4f && curve < 1.0e-4f)) {
        return p;
    }
    const double c = (curve < -1.0f) ? -1.0 : ((curve > 1.0f) ? 1.0 : static_cast<double>(curve));
    return static_cast<float>(std::pow(static_cast<double>(p), std::exp2(3.0 * c)));
}

// expoente da curva, para os testes medirem a forma sem repetir a formula.
[[nodiscard]] inline double curveExponent(float curve) noexcept {
    if (!std::isfinite(curve)) {
        return 1.0;
    }
    const double c = (curve < -1.0f) ? -1.0 : ((curve > 1.0f) ? 1.0 : static_cast<double>(curve));
    return std::exp2(3.0 * c);
}

} // namespace opcoda::dsp
