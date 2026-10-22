#pragma once

#include <array>

namespace opcoda {

// Os 6 presets de fabrica: pontos de partida sãos, nao demos mixados. Cada
// valor esta na unidade do parametro (ms, Hz, indice de choice) e dentro do
// intervalo do layout — um valor fora de faixa aqui atravessa convertTo0to1 e
// sai recortado em silencio, por isso a tabela e' a fonte e nao ha sanitize
// depois dela. Pares por ID em vez de ordem do layout: a ordem pode mudar, o
// ID nao.
//
// Afinados por raciocinio, nao por ouvido: Cloud e' densidade alta com ataque
// longo (nuvem sem transiente), Shimmer e' +12 st com HP (brilho sem lama),
// Pulse e' S&H... — quadrada na densidade (porta ritmica), Drone e' scan lento
// com LP (cabeça que anda sem pressa), Grit e' BP estreito com detune (banda
// que morde). Quem tem ouvido ajusta; a estrutura nao muda.
struct FactoryPreset {
    const char* name;
    // grain, density, position, spray, pitch, volume
    float grain {40.0f};
    float density {20.0f};
    float position {0.5f};
    float spray {0.0f};
    float pitch {0.0f};
    float volume {0.0f};
    // window, pan, grainlevel, pitchrand, scanspeed
    float window {0.0f};
    float pan {0.0f};
    float grainlevel {1.0f};
    float pitchrand {0.0f};
    float scanspeed {1.0f};
    // attack, decay, sustain, release
    float attack {0.005f};
    float decay {0.1f};
    float sustain {1.0f};
    float release {0.05f};
    // f1type, f1cutoff, f1q, f2type, f2cutoff, f2q
    float f1type {0.0f};
    float f1cutoff {20000.0f};
    float f1q {0.7071f};
    float f2type {0.0f};
    float f2cutoff {20000.0f};
    float f2q {0.7071f};
    // lforate, lfodepth, lfotarget, lfowave
    float lforate {1.0f};
    float lfodepth {0.0f};
    float lfotarget {0.0f};
    float lfowave {0.0f};
};

inline const std::array<FactoryPreset, 6>& factoryPresets() {
    static const std::array<FactoryPreset, 6> presets {{
        {.name = "Init"},
        {.name = "Cloud",
         .grain = 70.0f,
          .density = 140.0f,
          .spray = 0.45f,
          .window = 1.0f,
          .grainlevel = 0.8f,
          .pitchrand = 1.5f,
          .scanspeed = 0.5f,
          .attack = 0.4f,
          .decay = 0.6f,
          .sustain = 0.9f,
          .release = 0.9f,
          .f1cutoff = 6000.0f,
          .lforate = 0.1f,
          .lfodepth = 0.6f,
          .lfotarget = 3.0f},
        {.name = "Shimmer",
         .grain = 25.0f,
          .density = 90.0f,
          .spray = 0.2f,
          .pitch = 12.0f,
          .volume = -3.0f,
          .pitchrand = 3.0f,
          .attack = 0.01f,
          .decay = 0.3f,
          .sustain = 0.7f,
          .release = 0.5f,
          .f1type = 1.0f,
          .f1cutoff = 2000.0f,
          .lforate = 0.5f,
          .lfodepth = 0.5f},
        {.name = "Pulse",
         .grain = 8.0f,
          .density = 60.0f,
          .window = 3.0f,
          .attack = 0.002f,
          .decay = 0.08f,
          .sustain = 0.2f,
          .release = 0.06f,
          .lforate = 4.0f,
          .lfodepth = 1.0f,
          .lfotarget = 1.0f,
          .lfowave = 3.0f},
        {.name = "Drone",
         .grain = 100.0f,
          .density = 24.0f,
          .spray = 0.1f,
          .pitch = -12.0f,
          .volume = -6.0f,
          .scanspeed = 0.25f,
          .attack = 1.2f,
          .sustain = 1.0f,
          .release = 1.5f,
          .f1cutoff = 1500.0f,
          .lforate = 0.07f,
          .lfodepth = 0.8f,
          .lfotarget = 2.0f},
        {.name = "Grit",
         .grain = 12.0f,
          .density = 110.0f,
          .pitchrand = 5.0f,
          .attack = 0.003f,
          .decay = 0.12f,
          .sustain = 0.6f,
          .release = 0.1f,
          .f1type = 2.0f,
          .f1cutoff = 900.0f,
          .f1q = 4.0f,
          .f2type = 1.0f,
          .f2cutoff = 300.0f,
          .lforate = 7.0f,
          .lfodepth = 0.7f,
          .lfotarget = 1.0f,
          .lfowave = 2.0f},
    }};
    return presets;
}

} // namespace opcoda
