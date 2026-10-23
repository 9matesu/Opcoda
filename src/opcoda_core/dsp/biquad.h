#pragma once

#include <cstdint>

namespace opcoda::dsp {

// ordem = contrato com o choice da UI: o AudioParameterChoice guarda indice,
// e trocar a ordem aqui muda o som de sessoes salvas.
enum class FilterType : std::uint8_t {
    kLowPass = 0,
    kHighPass,
    kBandPass,
    kNotch,
};

[[nodiscard]] const char* filterName(FilterType type) noexcept;

// tipo fora de faixa vira passa-baixa: o indice vem do choice da UI e um indice
// corrompido tem de soar, nao calar. exposta para o cache de coeficientes do
// motor usar a mesma chave.
[[nodiscard]] FilterType sanitizeFilterType(FilterType type) noexcept;

// ja normalizados por a0 (a0 = 1 implicito). separados do estado de proposito:
// os coeficientes sao iguais para as 8 vozes e saem uma vez por bloco; o estado
// e' que e' por voz.
struct BiquadCoeffs {
    float b0 {1.0f};
    float b1 {0.0f};
    float b2 {0.0f};
    float a1 {0.0f};
    float a2 {0.0f};
};

// rbj cookbook com as guardas que o livro nao tem: cutoff e Q saneados antes da
// trigonometria, cutoff sempre abaixo de nyquist. acima dele o seno muda de
// sinal e o filtro instabiliza em silencio.
[[nodiscard]] BiquadCoeffs makeBiquadCoeffs(FilterType type,
                                           float cutoffHz,
                                           float q,
                                           double sampleRate) noexcept;

// forma direta I. os estados sao amostras puras, entao trocar coeficiente no
// meio do bloco nao gera transiente. na transposta II os estados guardam
// combinacoes de b*, e trocar b no meio deixava transiente.
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
