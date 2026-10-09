#pragma once

#include <cstdint>

namespace opcoda::dsp {

// Alvo do LFO, na ordem em que a UI vai listar. Como FilterType, a ordem e'
// contrato com o choice: trocar a ordem mudava o som de sessoes salvas.
enum class LFOTarget : std::uint8_t {
    kPitch = 0,
    kDensity,
    kCutoff,
    kPosition,
};

// Forma de onda, idem: a ordem e' contrato.
enum class LFOWave : std::uint8_t {
    kSine = 0,
    kTri,
    kSaw,
    kSquare,
    kSampleHold,
};

[[nodiscard]] const char* lfoTargetName(LFOTarget target) noexcept;
[[nodiscard]] const char* lfoWaveName(LFOWave wave) noexcept;

// Tipos fora de faixa viram o primeiro da lista, e nao erro: chegam como
// indice de choice da UI. Mesma regra do sanitizeFilterType, e pelo mesmo
// motivo — um indice corrompido tem de soar, nao calar.
[[nodiscard]] inline LFOTarget sanitizeLFOTarget(LFOTarget target) noexcept {
    return (target <= LFOTarget::kPosition) ? target : LFOTarget::kPitch;
}

[[nodiscard]] inline LFOWave sanitizeLFOWave(LFOWave wave) noexcept {
    return (wave <= LFOWave::kSampleHold) ? wave : LFOWave::kSine;
}

// O LFO e' funcao pura do tempo absoluto, e nao um oscilador com estado por
// amostra. A fase avanca uma vez por bloco (`advance`), e o valor em qualquer
// instante dentro do bloco sai de `valueAt` com o offset em amostras — exato,
// sem laco por amostra e sem quantizacao em fronteira de bloco.
//
// Por que nao avancar por amostra: o processBlock nao tem laco por amostra
// proprio (os lacos moram em renderVoice e applyGate), e um terceiro laco so
// para o LFO seria O(bloco) com uma soma e um wrap. A forma fechada da o mesmo
// valor com O(1) por leitura, e leitura so ha onde ha spawn ou recalculacao
// de coeficiente.
//
// Determinismo: fase em double a partir de zero, S&H por hash inteiro do
// indice do ciclo — sem RNG com estado, sem parede de clock, sem sin() de
// biblioteca na semente (o sin varia entre libm e quebraria a repetibilidade
// entre compiladores; splitmix64 e' so inteiro e e' bit-identico em toda
// parte).
class LFO {
public:
    void reset() noexcept { phase_ = 0.0; }

    // Avanca a fase em rate*numSamples/sr voltas, SEM wrap. Uma vez por bloco,
    // NO FIM do processBlock (nos dois caminhos de saida, com e sem material —
    // o LFO e' tempo absoluto e marcha mesmo em silencio). Sem wrap de
    // proposito: o indice do ciclo do S&H e' o floor da fase, e com wrap ele
    // seria sempre zero. double aguenta seculos a 20 Hz; waveAt dobra a fracao
    // so para a posicao dentro do ciclo. Com isso, phase_ e' sempre "o inicio
    // do bloco corrente": a continuidade entre blocos (e sob mudanca de taxa)
    // vem de graca, porque so o incremento muda, nunca a fase acumulada.
    void advance(float rateHz, int numSamples, double sampleRate) noexcept;

    // Valor bipolar [-1, 1] no instante `offsetSamples` dentro do bloco
    // corrente: wave(phase_ + rate*offset/sr). Chamar com o offset da voz da o
    // valor exato onde o grao nasce, e nao o valor do inicio do bloco — a
    // 20 Hz com bloco de 256, o inicio e o fim do bloco estao a 0,13 ciclos de
    // distancia, e quantizar no inicio seria degrau audivel no pitch.
    [[nodiscard]] float valueAt(LFOWave wave,
                               float rateHz,
                               int offsetSamples,
                               double sampleRate) const noexcept;

    // Valor no inicio do bloco: para densidade e cutoff, que sao por bloco e
    // nao por grao.
    [[nodiscard]] float blockValue(LFOWave wave) const noexcept;

    // So para teste e debug: fase em voltas, sem wrap. Audio-thread only como
    // o resto do motor — ler da interface seria corrida, e o display nao tem
    // nada que ler aqui (a telemetria e' o caminho dele).
    [[nodiscard]] double phase() const noexcept { return phase_; }

private:
    [[nodiscard]] static float waveAt(LFOWave wave, double phase) noexcept;

    double phase_ {0.0};
};

} // namespace opcoda::dsp
