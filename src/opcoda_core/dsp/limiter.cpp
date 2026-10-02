#include "opcoda_core/dsp/limiter.h"

#include <cmath>

namespace opcoda::dsp {
namespace {

float timeCoefficient(double timeMs, double sampleRate) noexcept {
    if (timeMs <= 0.0 || sampleRate <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(std::exp(-1.0 / ((timeMs * 0.001) * sampleRate)));
}

} // namespace

void Limiter::setSampleRate(double sampleRate) noexcept {
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 44100.0;
    setAttackMs(5.0);
    setReleaseMs(50.0);
}

void Limiter::prepare(double ceilingDb) noexcept {
    ceiling_ = static_cast<float>(std::pow(10.0, ceilingDb / 20.0));
    reset();
}

void Limiter::reset() noexcept {
    gain_ = 1.0f;
}

void Limiter::setAttackMs(double attackMs) noexcept {
    attackCoef_ = timeCoefficient(attackMs, sampleRate_);
}

void Limiter::setReleaseMs(double releaseMs) noexcept {
    releaseCoef_ = timeCoefficient(releaseMs, sampleRate_);
}

float Limiter::process(float sample) noexcept {
    const float magnitude = std::abs(sample);
    float target = 1.0f;
    if (magnitude > ceiling_ && magnitude > 0.0f) {
        target = ceiling_ / magnitude;
    }

    const float coefficient = (target < gain_) ? attackCoef_ : releaseCoef_;
    gain_ = target + coefficient * (gain_ - target);

    // O ganho suavizado sozinho deixa passar acima do teto na amostra em que o
    // ataque ainda nao convergiu. Sem este clamp o limitador nao limita, e e' o
    // teto que o ensaio T1 mede. Custo: alguns dB de distorcao no ataque.
    float output = sample * gain_;
    if (output > ceiling_) {
        output = ceiling_;
    } else if (output < -ceiling_) {
        output = -ceiling_;
    }
    return output;
}

void Limiter::processBlock(float* data, int numSamples) noexcept {
    if (data == nullptr) {
        return;
    }
    for (int i = 0; i < numSamples; ++i) {
        data[i] = process(data[i]);
    }
}

} // namespace opcoda::dsp
