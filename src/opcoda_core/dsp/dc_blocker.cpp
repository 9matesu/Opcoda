#include "opcoda_core/dsp/dc_blocker.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace opcoda::dsp {

void DcBlocker::setSampleRate(double sampleRate) noexcept {
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 44100.0;
}

void DcBlocker::reset() noexcept {
    x1_ = 0.0f;
    y1_ = 0.0f;
}

void DcBlocker::processBlock(float* data, int numSamples) noexcept {
    if (data == nullptr) {
        return;
    }
    for (int i = 0; i < numSamples; ++i) {
        data[i] = process(data[i]);
    }
}

double DcBlocker::cutoffHz() const noexcept {
    const double r = static_cast<double>(r_);
    if (r <= 0.0 || r >= 1.0) {
        return 0.0;
    }
    return -sampleRate_ * std::log(r) / (2.0 * std::numbers::pi);
}

double DcBlocker::gainDbAt(double frequencyHz) const noexcept {
    const double r = static_cast<double>(r_);
    const double w = (2.0 * std::numbers::pi * frequencyHz) / sampleRate_;
    const double numMag = 2.0 * std::abs(std::sin(w * 0.5));
    const double denMag = std::sqrt(1.0 + r * r - 2.0 * r * std::cos(w));
    if (denMag <= 0.0) {
        return 0.0;
    }
    return 20.0 * std::log10(std::max(numMag / denMag, 1e-300));
}

double DcBlocker::dcGainDb() const noexcept {
    return gainDbAt(0.0);
}

} // namespace opcoda::dsp
