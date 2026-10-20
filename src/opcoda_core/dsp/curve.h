#pragma once

#include <cmath>

namespace opcoda::dsp {

// Forma de um estagio do envelope.
//
// `phase` corre de 0 a 1 no tempo declarado do estagio, e `curve` vai de -1 a
// +1. O valor e' phase^(2^(3*curve)), que e' a convencao do Serum e a que a
// docs/19 declara:
//
//   -1 -> expoente 1/8   sobe depressa e aproxima devagar (exponencial)
//    0 -> expoente 1     rampa linear (o caminho antigo, exato)
//   +1 -> expoente 8     fica baixo e fecha depressa (logaritmica)
//
// **O sinal estava trocado na primeira versao da F020.** A frase da spec pedia
// "rapido no inicio em -1" e a formula escrita ao lado dela, 2^(-3*curva), dava
// o contrario. Vale a frase: e' o que o knob promete e o que os dois documentos
// descrevem. A formula passou a 2^(+3*curva).
//
// **Em curva 0 nao ha pow.** O motor usa o laco antigo, amostra a amostra, e o
// resultado continua bit-identico ao de antes da F020 — os ensaios do ADSR
// comparam com EXPECT_FLOAT_EQ e 4 ULP, e trocar a aritmetica por pow mudaria
// os ultimos bits de todos eles.
//
// A curva e' saneada: NaN desliga (rampa), e fora de [-1, 1] trunca no limite,
// porque um expoente de 2^300 vira 0 ou infinito em silencio.
[[nodiscard]] inline float curveShape(float phase, float curve) noexcept {
    const float p = (phase < 0.0f) ? 0.0f : ((phase > 1.0f) ? 1.0f : phase);
    if (!std::isfinite(curve) || (curve > -1.0e-4f && curve < 1.0e-4f)) {
        return p;
    }
    const double c = (curve < -1.0f) ? -1.0 : ((curve > 1.0f) ? 1.0 : static_cast<double>(curve));
    return static_cast<float>(std::pow(static_cast<double>(p), std::exp2(3.0 * c)));
}

// Expoente da curva, para os ensaios medirem a forma sem repetir a formula.
[[nodiscard]] inline double curveExponent(float curve) noexcept {
    if (!std::isfinite(curve)) {
        return 1.0;
    }
    const double c = (curve < -1.0f) ? -1.0 : ((curve > 1.0f) ? 1.0 : static_cast<double>(curve));
    return std::exp2(3.0 * c);
}

} // namespace opcoda::dsp
