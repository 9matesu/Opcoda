#include "opcoda_core/dsp/lfo.h"

#include <cmath>
#include <numbers>

namespace opcoda::dsp {

const char* lfoTargetName(LFOTarget target) noexcept {
    switch (target) {
        case LFOTarget::kPitch: return "Pitch";
        case LFOTarget::kDensity: return "Density";
        case LFOTarget::kCutoff: return "Cutoff";
        case LFOTarget::kPosition: return "Position";
    }
    return "Pitch";
}

const char* lfoWaveName(LFOWave wave) noexcept {
    switch (wave) {
        case LFOWave::kSine: return "Sine";
        case LFOWave::kTri: return "Tri";
        case LFOWave::kSaw: return "Saw";
        case LFOWave::kSquare: return "Square";
        case LFOWave::kSampleHold: return "S&H";
    }
    return "Sine";
}

namespace {

// splitmix64: hash inteiro deterministico para o S&H. O ciclo N tem sempre o
// mesmo valor, em qualquer compilador e sem estado — sin() de biblioteca
// variava entre libm e quebrava a repetibilidade que o T1/T2 assumem.
std::uint64_t splitmix64(std::uint64_t x) noexcept {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

} // namespace

float LFO::waveAt(LFOWave wave, double phase) noexcept {
    // Fase em [0, 1). O wrap e' do advance/valueAt; aqui nao se confia: fracao
    // fora disso dobra a forma em vez de indexar lixo.
    const double p = phase - std::floor(phase);

    switch (wave) {
        case LFOWave::kSine:
            return static_cast<float>(std::sin(2.0 * std::numbers::pi * p));
        case LFOWave::kTri: return static_cast<float>(4.0 * std::abs(p - 0.5) - 1.0);
        case LFOWave::kSaw: return static_cast<float>(2.0 * p - 1.0);
        case LFOWave::kSquare: return p < 0.5 ? 1.0f : -1.0f;
        case LFOWave::kSampleHold: {
            // Indice do ciclo, nao da amostra: o valor segura o ciclo inteiro
            // e so muda na virada, que e' a definicao de sample-and-hold. Os
            // 53 bits de cima viram [0, 1), depois bipolar.
            const auto cycle = static_cast<std::uint64_t>(std::floor(phase));
            constexpr double kToUnit = 1.0 / 9007199254740992.0;
            return static_cast<float>(static_cast<double>(splitmix64(cycle) >> 11) * kToUnit *
                                      2.0 -
                                      1.0);
        }
    }
    return 0.0f;
}

void LFO::advance(float rateHz, int numSamples, double sampleRate) noexcept {
    if (!(sampleRate > 0.0) || numSamples <= 0 || !(rateHz >= 0.0f)) {
        return;
    }
    phase_ += static_cast<double>(rateHz) * static_cast<double>(numSamples) / sampleRate;
}

float LFO::valueAt(LFOWave wave,
                   float rateHz,
                   int offsetSamples,
                   double sampleRate) const noexcept {
    if (!(sampleRate > 0.0) || !(rateHz >= 0.0f)) {
        return waveAt(wave, phase_);
    }
    const double at = phase_ + static_cast<double>(rateHz) *
                                    static_cast<double>(offsetSamples >= 0 ? offsetSamples : 0) /
                                    sampleRate;
    return waveAt(wave, at);
}

float LFO::blockValue(LFOWave wave) const noexcept {
    return waveAt(wave, phase_);
}

} // namespace opcoda::dsp
