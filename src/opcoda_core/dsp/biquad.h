#pragma once

#include <cstdint>

namespace opcoda::dsp {

// Tipo de filtro, na mesma ordem em que a UI vai listar: passa-baixa,
// passa-alta, passa-faixa, rejeita-faixa. A ordem e' contrato com o choice da
// UI — um AudioParameterChoice guarda indice, e trocar a ordem aqui mudava o
// som de sessoes salvas.
enum class FilterType : std::uint8_t {
    kLowPass = 0,
    kHighPass,
    kBandPass,
    kNotch,
};

[[nodiscard]] const char* filterName(FilterType type) noexcept;

// Tipo fora de faixa vira passa-baixa, e nao erro: o tipo chega como indice
// de choice da UI, e um indice corrompido tem de soar, nao calar. Exposta
// para o motor usar a mesma na chave do cache de coeficientes — sem ela, um
// indice invalido recalculava a cada bloco em vez de uma vez.
[[nodiscard]] FilterType sanitizeFilterType(FilterType type) noexcept;

// Coeficientes ja normalizados por a0 (a0 = 1 implicito). Separados do estado
// de proposito: os coeficientes sao iguais para as 8 vozes e calculados uma
// vez por bloco, e o estado e' por voz. Copiar 5 floats por voz por bloco
// seria trabalho que nao muda nada.
struct BiquadCoeffs {
    float b0 {1.0f};
    float b1 {0.0f};
    float b2 {0.0f};
    float a1 {0.0f};
    float a2 {0.0f};
};

// RBJ cookbook, com as guardas que o livro nao tem: cutoff e Q saneados antes
// de entrar na trigonometria, e cutoff sempre abaixo de Nyquist — acima dele
// o seno muda de sinal e o filtro instabiliza em silencio.
[[nodiscard]] BiquadCoeffs makeBiquadCoeffs(FilterType type,
                                           float cutoffHz,
                                           float q,
                                           double sampleRate) noexcept;

// Biquad em Forma Direta I: o estado (x1, x2, y1, y2) e' por voz, e os
// coeficientes chegam por argumento a cada amostra. DFI e nao Transposta II
// porque os coeficientes mudam por bloco (automacao de cutoff): na Transposta
// os estados guardam combinacoes de b*, e trocar b no meio deixa transiente;
// na DFI os estados sao amostras puras, e trocar coeficiente e' continuo.
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
