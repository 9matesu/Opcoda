#pragma once

#include <array>

namespace opcoda {

// dez presets de fabrica. sao pontos de partida, nao demos mixadas. cada valor
// esta na unidade do parametro (ms, hz, indice de choice) e dentro da faixa do
// layout. a tabela e' a fonte; nao ha sanitize depois dela. pares por id, nao
// por ordem do layout: a ordem muda, o id nao.
//
// a ordem dos campos segue a do createParameterLayout, e nao e' decorativa: o
// inicializador designado do C++20 exige os designadores na ordem de declaracao,
// e a lista abaixo fica presa a ela.
//
// afinados por raciocinio, nao por ouvido. cloud = densidade alta e ataque
// longo; shimmer = +12 st com hp; pulse = s&h quadrada na densidade; drone =
// scan lento com lp; grit = bp estreito com detune. quem tem ouvido ajusta.
//
// os quatro ultimos existem por um motivo pratico: os seis primeiros sao
// texturais, e quem abre o instrumento a espera de um pad, um teclado, um baixo
// ou um pluck nao tinha por onde comecar. o que os separa e' a sobreposicao,
// density x grain / 1000: 7,0 graos no pad (corpo continuo), 3,0 no teclado,
// 2,8 no pluck (o grao e' o gesto) e 2,1 no baixo. nos tres tonais o filtro 1
// faz o trabalho, com keytrack e envelope.
//
// o pad nao usa detune aleatorio largo. spread e pitchrand sao desvios por
// grao, nao por voz: 5 st de spread nao engrossa o som, desafina-o contra si
// proprio. a espessura vem da nuvem, do jitter e da largura, que nao mexem na
// afinacao.
struct FactoryPreset {
    const char* name;
    // grain, density, position, spray, pitch, volume
    float grain {40.0f};
    float density {20.0f};
    float position {0.5f};
    float spray {0.0f};
    float pitch {0.0f};
    float volume {0.0f};
    // window, pan, grainlevel, pitchrand
    float window {0.0f};
    float pan {0.0f};
    float grainlevel {1.0f};
    float pitchrand {0.0f};
    // cloudsize (fracao de 500 ms), pitchspread, grainjit
    float cloudsize {0.0f};
    float pitchspread {0.0f};
    float grainjit {0.0f};
    float scanspeed {1.0f};
    // attack, decay, sustain, release
    float attack {0.005f};
    float decay {0.1f};
    float sustain {1.0f};
    float release {0.05f};
    // atkcurve, deccurve, relcurve: -1 cai depressa, +1 fica baixo e fecha no fim
    float atkcurve {0.0f};
    float deccurve {0.0f};
    float relcurve {0.0f};
    // f1type, f1cutoff, f1q, f1slope, f1drive, f1keytrack, f1env
    float f1type {0.0f};
    float f1cutoff {20000.0f};
    float f1q {0.7071f};
    float f1slope {0.0f};
    float f1drive {0.0f};
    float f1keytrack {0.0f};
    float f1env {0.0f};
    // f2type, f2cutoff, f2q
    float f2type {0.0f};
    float f2cutoff {20000.0f};
    float f2q {0.7071f};
    // lforate, lfodepth, lfotarget, lfowave
    float lforate {1.0f};
    float lfodepth {0.0f};
    float lfotarget {0.0f};
    float lfowave {0.0f};
    // width, glide
    float width {1.0f};
    float glide {0.0f};
};

inline const std::array<FactoryPreset, 10>& factoryPresets() {
    static const std::array<FactoryPreset, 10> presets {{
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
        // pad: grao no maximo (100 ms) e 70 graos/s para sete sobrepostos, que e'
        // o que faz corpo e nao chuvisco. a afinacao fica estavel de proposito --
        // spray 1,5 %, spread 0,5 st, pitchrand zero -- e a espessura vem da
        // nuvem de 200 ms, do jitter de 20 % e da largura 1,5. o ataque de 1,5 s
        // com curva +0,35 fica baixo e floresce no fim, que e' o gesto do pad.
        // filtro 1 em 3,2 khz com drive leve, e o filtro 2 em high-pass a 70 hz
        // limpa o rumo que o material binario traz.
        {.name = "Pad",
         .grain = 100.0f,
          .density = 70.0f,
          .spray = 0.015f,
          .volume = -6.0f,
          .window = 1.0f,
          .cloudsize = 0.4f,
          .pitchspread = 0.5f,
          .grainjit = 0.2f,
          .attack = 1.5f,
          .decay = 0.5f,
          .sustain = 1.0f,
          .release = 2.0f,
          .atkcurve = 0.35f,
          .f1cutoff = 3200.0f,
          .f1q = 1.1f,
          .f1drive = 2.0f,
          .f1keytrack = 0.5f,
          .f2type = 1.0f,
          .f2cutoff = 70.0f,
          .lforate = 0.1f,
          .lfodepth = 0.22f,
          .lfotarget = 2.0f,
          .width = 1.5f,
          .glide = 0.15f},
        // teclado: grao curto e denso para o ataque ter corpo, decay de 250 ms com
        // sustain em 25 %, e o corte a abrir no ataque com f1env +0,5 (meio curso
        // de +-5 oitavas). slope de 24 dB porque aqui o filtro e' o timbre.
        {.name = "Keys",
         .grain = 30.0f,
          .density = 100.0f,
          .spray = 0.02f,
          .volume = -3.0f,
          .grainjit = 0.05f,
          .attack = 0.003f,
          .decay = 0.25f,
          .sustain = 0.25f,
          .release = 0.2f,
          .f1cutoff = 1500.0f,
          .f1q = 1.2f,
          .f1slope = 1.0f,
          .f1keytrack = 0.5f,
          .f1env = 0.5f,
          .width = 1.2f},
        // pluck: o envelope e' o som. ataque de 2 ms, decay de 350 ms a zero com
        // curva -0,7 (cai depressa e arrasta a cauda, em vez de rampa), release
        // curto. o corte sai de 700 hz e abre ate' ~6,6 khz com f1env +0,65,
        // fechando junto com o envelope -- o "tchau" do pluck. 140 graos/s de 20 ms
        // dao 2,8 sobrepostos, o suficiente para o ataque ter corpo sem virar pad.
        {.name = "Pluck",
         .grain = 20.0f,
          .density = 140.0f,
          .spray = 0.01f,
          .volume = -3.0f,
          .grainjit = 0.06f,
          .attack = 0.002f,
          .decay = 0.35f,
          .sustain = 0.0f,
          .release = 0.18f,
          .deccurve = -0.7f,
          .relcurve = -0.5f,
          .f1cutoff = 700.0f,
          .f1q = 1.8f,
          .f1slope = 1.0f,
          .f1drive = 5.0f,
          .f1keytrack = 0.8f,
          .f1env = 0.65f,
          .width = 1.15f},
        // baixo: -24 st para descer duas oitavas, filtro quase fechado com
        // keytrack cheio (uma oitava de corte por oitava de nota) e glide de 40 ms.
        // width 0,8 fecha o lado sem colapsar em mono.
        {.name = "Bass",
         .grain = 30.0f,
          .density = 70.0f,
          .pitch = -24.0f,
          .volume = -4.0f,
          .attack = 0.005f,
          .decay = 0.2f,
          .sustain = 0.6f,
          .release = 0.12f,
          .f1cutoff = 400.0f,
          .f1q = 1.5f,
          .f1slope = 1.0f,
          .f1drive = 4.0f,
          .f1keytrack = 1.0f,
          .f1env = 0.2f,
          .width = 0.8f,
          .glide = 0.04f},
    }};
    return presets;
}

} // namespace opcoda
