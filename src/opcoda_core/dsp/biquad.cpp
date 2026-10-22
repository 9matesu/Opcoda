#include "opcoda_core/dsp/biquad.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace opcoda::dsp {

const char* filterName(FilterType type) noexcept {
    switch (type) {
        case FilterType::kLowPass: return "Low-pass";
        case FilterType::kHighPass: return "High-pass";
        case FilterType::kBandPass: return "Band-pass";
        case FilterType::kNotch: return "Notch";
    }
    return "Low-pass";
}

FilterType sanitizeFilterType(FilterType type) noexcept {
    return (type <= FilterType::kNotch) ? type : FilterType::kLowPass;
}

BiquadCoeffs makeBiquadCoeffs(FilterType type,
                              float cutoffHz,
                              float q,
                              double sampleRate) noexcept {
    // Saneamento antes da trigonometria: NaN ou fora de faixa nao pode chegar
    // ao sin/cos, porque dali sai coeficiente NaN e o filtro envenena o
    // barramento sem avisar. Q trunca em [0,5, 12]; abaixo de 0,5 o pico some
    // e acima de 12 o anel vira apito.
    const double sr = (std::isfinite(sampleRate) && sampleRate > 0.0) ? sampleRate : 44100.0;
    // Piso duplo: taxa de amostra absurda (sr < 41 Hz) poria nyquist abaixo de
    // 20 e o clamp saia com lo > hi, que e' pre-condicao violada. Hosts reais
    // nunca entregam isso; a linha existe para a UB formal nao existir.
    const double nyquist = std::max(sr * 0.49, 20.0);
    // O NaN cai no min() com Nyquist, e nao em 20000 cru: 20000 acima de
    // Nyquist (sr = 32 kHz, por exemplo) dobrava a frequencia em vez de passar
    // — finito e errado, que a rede de nao-finito nao pegaria.
    const double cutoff = std::min(std::isfinite(cutoffHz)
                                       ? std::clamp(static_cast<double>(cutoffHz), 20.0, nyquist)
                                       : 20000.0,
                                   nyquist);
    const double resonance =
        std::isfinite(q) ? std::clamp(static_cast<double>(q), 0.5, 12.0) : 0.7071;

    const double w0 = 2.0 * std::numbers::pi * cutoff / sr;
    const double cosW = std::cos(w0);
    const double sinW = std::sin(w0);
    const double alpha = sinW / (2.0 * resonance);

    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    switch (sanitizeFilterType(type)) {
        case FilterType::kLowPass:
            b0 = (1.0 - cosW) * 0.5;
            b1 = 1.0 - cosW;
            b2 = (1.0 - cosW) * 0.5;
            break;
        case FilterType::kHighPass:
            b0 = (1.0 + cosW) * 0.5;
            b1 = -(1.0 + cosW);
            b2 = (1.0 + cosW) * 0.5;
            break;
        case FilterType::kBandPass:
            // Pico a 0 dB, e nao ganho unitario na banda: o que interessa e' o
            // que passa no centro, nao a energia total.
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            break;
        case FilterType::kNotch:
            b0 = 1.0;
            b1 = -2.0 * cosW;
            b2 = 1.0;
            break;
    }
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosW;
    const double a2 = 1.0 - alpha;

    BiquadCoeffs coeffs;
    coeffs.b0 = static_cast<float>(b0 / a0);
    coeffs.b1 = static_cast<float>(b1 / a0);
    coeffs.b2 = static_cast<float>(b2 / a0);
    coeffs.a1 = static_cast<float>(a1 / a0);
    coeffs.a2 = static_cast<float>(a2 / a0);

    // Ultima rede: se mesmo assim saiu nao-finito (Q ou sr patologicos que o
    // clamp nao pegou), devolve passagem. Um filtro que vira fio e' melhor do
    // que um filtro que vira NaN.
    if (!std::isfinite(coeffs.b0 + coeffs.b1 + coeffs.b2 + coeffs.a1 + coeffs.a2)) {
        return BiquadCoeffs{};
    }
    return coeffs;
}

void Biquad::reset() noexcept {
    x1_ = 0.0f;
    x2_ = 0.0f;
    y1_ = 0.0f;
    y2_ = 0.0f;
}

float Biquad::process(float x, const BiquadCoeffs& coeffs) noexcept {
    const float y = coeffs.b0 * x + coeffs.b1 * x1_ + coeffs.b2 * x2_ - coeffs.a1 * y1_ -
                    coeffs.a2 * y2_;
    x2_ = x1_;
    x1_ = x;
    y2_ = y1_;
    y1_ = y;
    // Snap a zero abaixo de -300 dBFS, e nos ESTADOS, nao so na saida: com
    // entrada silenciosa a cauda decai exponencialmente e estaciona em
    // subnormal (1e-30), onde cada operacao em x86 sem FTZ custa 10-100x. Oito
    // vozes vezes dois filtros queimariam o p99 exatamente na cauda longa com
    // Q alto. Abaixo de 1e-15 nao ha sinal, so numero — nem ruido termico
    // chega la (-130 dBFS). Zerar so o y nao adiantava: y1_/y2_ guardavam o
    // denormal e a proxima amostra o resuscitava. Nao se usa FTZ via _controlfp
    // porque o flag e' da thread, e em VST3 a thread e' do host.
    if (std::fabs(y1_) < 1e-15f) {
        y1_ = 0.0f;
    }
    if (std::fabs(y2_) < 1e-15f) {
        y2_ = 0.0f;
    }
    return y;
}

} // namespace opcoda::dsp
