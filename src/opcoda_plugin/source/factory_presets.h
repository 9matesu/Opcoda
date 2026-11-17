#pragma once

#include <array>

namespace opcoda {

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
        {.name = "Haze",
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
        {.name = "Pad",
         .grain = 100.0f,
          .density = 70.0f,
          .spray = 0.015f,
          .volume = -6.0f,
          .window = 1.0f,
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
        {.name = "Keys",
         .grain = 30.0f,
          .density = 100.0f,
          .spray = 0.02f,
          .volume = -3.0f,
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
        {.name = "Pluck",
         .grain = 20.0f,
          .density = 140.0f,
          .spray = 0.01f,
          .volume = -3.0f,
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
