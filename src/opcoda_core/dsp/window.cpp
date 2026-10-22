#include "opcoda_core/dsp/window.h"

#include <cmath>
#include <numbers>

namespace opcoda::dsp {

const char* windowName(WindowType type) noexcept {
    switch (type) {
        case WindowType::kHann: return "Hann";
        case WindowType::kGaussian: return "Gaussian";
        case WindowType::kHamming: return "Hamming";
        case WindowType::kBlackman: return "Blackman";
    }
    return "unknown";
}

void fillWindow(WindowType type, float* out, int length) noexcept {
    if (out == nullptr || length <= 0) {
        return;
    }
    if (length == 1) {
        out[0] = 1.0f;
        return;
    }

    const double n = static_cast<double>(length - 1);
    const double twoPi = 2.0 * std::numbers::pi;

    for (int i = 0; i < length; ++i) {
        const double t = static_cast<double>(i) / n;
        double value = 0.0;

        switch (type) {
            case WindowType::kHann:
                value = 0.5 * (1.0 - std::cos(twoPi * t));
                break;
            case WindowType::kGaussian: {
                const double centred = t - 0.5;
                value = std::exp(-0.5 * std::pow(centred / 0.25, 2.0));
                break;
            }
            case WindowType::kHamming:
                value = 0.54 - 0.46 * std::cos(twoPi * t);
                break;
            case WindowType::kBlackman:
                value = 0.42 - 0.5 * std::cos(twoPi * t) +
                        0.08 * std::cos(2.0 * twoPi * t);
                break;
        }
        out[i] = static_cast<float>(value);
    }
}

} // namespace opcoda::dsp
