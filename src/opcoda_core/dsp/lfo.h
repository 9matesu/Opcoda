#pragma once

#include <cmath>
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

// Divisao musical do LFO sincronizado, na ordem do choice da UI — a ordem e'
// contrato, como a das formas: trocar mudava o som de sessoes salvas.
//
// A divisao diz quantas notas daquele valor cabem numa batida, e e' isso que
// cyclesPerBeat devolve: 1/4 = 1 ciclo por batida, 1/8 = 2, 1/16 = 4. Pontuada
// multiplica o comprimento por 1,5 (menos ciclos por batida); tercina por 2/3
// (mais ciclos). 1/1 tem 0,25 ciclos por batida porque um compasso 4/4 leva
// quatro batidas.
inline constexpr int kLFODivCount = 12;

[[nodiscard]] inline int sanitizeLFODiv(int div) noexcept {
    return (div >= 0 && div < kLFODivCount) ? div : 2; // 2 = 1/4, o default do parametro
}

[[nodiscard]] inline const char* lfoDivName(int div) noexcept {
    switch (sanitizeLFODiv(div)) {
        case 0: return "1/1";
        case 1: return "1/2";
        case 2: return "1/4";
        case 3: return "1/8";
        case 4: return "1/16";
        case 5: return "1/32";
        case 6: return "1/64";
        case 7: return "1/4.";
        case 8: return "1/8.";
        case 9: return "1/4T";
        case 10: return "1/8T";
        default: return "1/16T";
    }
}

// Ciclos por batida. 1/8 -> 2 ciclos por batida -> 4 Hz a 120 BPM, porque a
// 120 BPM uma batida dura 0,5 s.
[[nodiscard]] inline double cyclesPerBeat(int div) noexcept {
    switch (sanitizeLFODiv(div)) {
        case 0: return 0.25;
        case 1: return 0.5;
        case 2: return 1.0;
        case 3: return 2.0;
        case 4: return 4.0;
        case 5: return 8.0;
        case 6: return 16.0;
        case 7: return 1.0 / 1.5;  // 1/4 pontuada
        case 8: return 1.0 / 0.75; // 1/8 pontuada
        case 9: return 1.5;        // 1/4 tercina
        case 10: return 3.0;       // 1/8 tercina
        default: return 6.0;       // 1/16 tercina
    }
}

// BPM invalido cai para 120: sem transporte o LFO tem de continuar a oscilar,
// e nao parar nem dividir por zero.
[[nodiscard]] inline double sanitizeBpm(double bpm) noexcept {
    return (std::isfinite(bpm) && bpm > 0.0 && bpm <= 1000.0) ? bpm : 120.0;
}

[[nodiscard]] inline double syncedRateHz(int div, double bpm) noexcept {
    return cyclesPerBeat(div) * sanitizeBpm(bpm) / 60.0;
}

// Degraus: a forma e' amostrada em `steps` patamares por ciclo, pela formula
// floor(fase * passos) / passos. 16 e' o valor "sem quantizacao", e nao um
// degrau de 16: acima disso a diferenca para a onda continua e' inaudivel.
[[nodiscard]] inline int sanitizeLFOSteps(int steps) noexcept {
    return (steps < 1) ? 1 : ((steps > 16) ? 16 : steps);
}

[[nodiscard]] inline double quantizeLFOPhase(double phase, int steps) noexcept {
    const int n = sanitizeLFOSteps(steps);
    if (n >= 16) {
        return phase;
    }
    return std::floor(phase * static_cast<double>(n)) / static_cast<double>(n);
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

    // Fase derivada do transporte. O motor chama isto quando o modo Sync esta'
    // ligado; a fase passa a ser funcao do ppq, e nao do tempo acumulado.
    // Avancar depois do set e' inofensivo: o proximo bloco volta a derivar.
    void setPhase(double phase) noexcept { phase_ = phase; }

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
                               double sampleRate,
                               int steps = 16) const noexcept;

    // Valor no inicio do bloco: para densidade e cutoff, que sao por bloco e
    // nao por grao.
    [[nodiscard]] float blockValue(LFOWave wave, int steps = 16) const noexcept;

    // So para teste e debug: fase em voltas, sem wrap. Audio-thread only como
    // o resto do motor — ler da interface seria corrida, e o display nao tem
    // nada que ler aqui (a telemetria e' o caminho dele).
    [[nodiscard]] double phase() const noexcept { return phase_; }

private:
    [[nodiscard]] static float waveAt(LFOWave wave, double phase) noexcept;

    double phase_ {0.0};
};

} // namespace opcoda::dsp
