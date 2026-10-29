#pragma once

#include <cmath>
#include <cstdint>

namespace opcoda::dsp {

// alvo do lfo, na ordem do choice da UI. como FilterType, a ordem e' contrato:
// trocar muda o som de sessoes salvas.
enum class LFOTarget : std::uint8_t {
    kPitch = 0,
    kDensity,
    kCutoff,
    kPosition,
};

// idem: a ordem e' contrato.
enum class LFOWave : std::uint8_t {
    kSine = 0,
    kTri,
    kSaw,
    kSquare,
    kSampleHold,
};

[[nodiscard]] const char* lfoTargetName(LFOTarget target) noexcept;
[[nodiscard]] const char* lfoWaveName(LFOWave wave) noexcept;

// fora de faixa vira o primeiro da lista. chega como indice de choice, e indice
// corrompido tem de soar, nao calar. mesma regra do sanitizeFilterType.
[[nodiscard]] inline LFOTarget sanitizeLFOTarget(LFOTarget target) noexcept {
    return (target <= LFOTarget::kPosition) ? target : LFOTarget::kPitch;
}

[[nodiscard]] inline LFOWave sanitizeLFOWave(LFOWave wave) noexcept {
    return (wave <= LFOWave::kSampleHold) ? wave : LFOWave::kSine;
}

// divisao musical do lfo sincronizado, na ordem do choice. a ordem e' contrato.
//
// a divisao diz quantas notas daquele valor cabem numa batida, e e' isso que
// cyclesPerBeat devolve: 1/4 = 1 ciclo por batida, 1/8 = 2, 1/16 = 4. pontuada
// multiplica o comprimento por 1,5, tercina por 2/3. 1/1 tem 0,25 porque um
// compasso 4/4 leva quatro batidas.
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

// ciclos por batida. 1/8 da 2 ciclos, que a 120 bpm sao 4 Hz.
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

// bpm invalido cai para 120: sem transporte o lfo tem de continuar a oscilar,
// e nao parar nem dividir por zero.
[[nodiscard]] inline double sanitizeBpm(double bpm) noexcept {
    return (std::isfinite(bpm) && bpm > 0.0 && bpm <= 1000.0) ? bpm : 120.0;
}

[[nodiscard]] inline double syncedRateHz(int div, double bpm) noexcept {
    return cyclesPerBeat(div) * sanitizeBpm(bpm) / 60.0;
}

// a forma e' amostrada em `steps` patamares por ciclo: floor(fase * passos) / passos.
// 16 ja' e' "sem quantizacao", a diferenca para a onda continua e' inaudivel.
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

// lfo e' funcao pura do tempo absoluto, nao um oscilador com estado por amostra.
// a fase avanca uma vez por bloco e o valor em qualquer instante do bloco sai de
// valueAt com o offset em amostras.
//
// sem wrap na fase: o indice do ciclo do S&H e' o floor dela. S&H por splitmix64
// do indice, inteiro puro — bit-identico entre compiladores, ao contrario do sin
// de libm.
class LFO {
public:
    void reset() noexcept { phase_ = 0.0; }

    // fase derivada do transporte, quando o modo sync esta' ligado. avancar
    // depois do set e' inofensivo: o proximo bloco volta a derivar.
    void setPhase(double phase) noexcept { phase_ = phase; }

    // avanca a fase em rate*numSamples/sr voltas, SEM wrap. uma vez por bloco, no
    // fim do processBlock, e nos dois caminhos de saida: o lfo e' tempo absoluto e
    // marcha mesmo em silencio. sem wrap de proposito, senao o indice do ciclo do
    // S&H seria sempre zero. phase_ e' sempre o inicio do bloco corrente.
    void advance(float rateHz, int numSamples, double sampleRate) noexcept;

    // valor bipolar no instante offsetSamples dentro do bloco corrente. passar o
    // offset da voz da o valor onde o grao nasce, e nao o do inicio do bloco:
    // a 20 Hz com bloco de 256, os dois estao a 0,13 ciclos de distancia.
    [[nodiscard]] float valueAt(LFOWave wave,
                               float rateHz,
                               int offsetSamples,
                               double sampleRate,
                               int steps = 16) const noexcept;

    // valor no inicio do bloco: densidade e cutoff, que sao por bloco e nao por grao.
    [[nodiscard]] float blockValue(LFOWave wave, int steps = 16) const noexcept;

    // so para teste e debug: fase em voltas, sem wrap. thread de audio, como o
    // resto do motor. o display le a telemetria.
    [[nodiscard]] double phase() const noexcept { return phase_; }

    // a forma em funcao da fase, para quem desenha a curva (a vista MOD) sem
    // duplicar a matematica das ondas no lado da interface.
    [[nodiscard]] static float shapeAt(LFOWave wave, double phase) noexcept {
        return waveAt(wave, phase);
    }

private:
    [[nodiscard]] static float waveAt(LFOWave wave, double phase) noexcept;

    double phase_ {0.0};
};

} // namespace opcoda::dsp
