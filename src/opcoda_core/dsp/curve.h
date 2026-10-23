#pragma once

#include <cmath>

namespace opcoda::dsp {

// forma de um estagio do envelope: phase^(2^(3*curve)), com phase 0..1 e curve
// -1..+1. -1 sobe depressa e aproxima devagar, 0 e' rampa linear, +1 fica baixo
// e fecha depressa.
//
// em curva 0 nao ha pow: a funcao devolve a rampa e o resultado fica bit-identico
// ao de antes. os ensaios do ADSR comparam com EXPECT_FLOAT_EQ e 4 ULP.
//
// curva NaN desliga (rampa) e fora de [-1, 1] trunca no limite: 2^300 vira 0 ou
// infinito em silencio.
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
