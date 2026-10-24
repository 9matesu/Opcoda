#pragma once

#include <array>
#include <cstddef>

namespace opcoda::params {

// Valor por omissao de cada parametro, na unidade do parametro: ms, /s, dB,
// hertz ou indice de choice.
//
// **Este e' o unico sitio onde o default mora.** O layout em
// plugin_processor.cpp e os Knob::Spec em plugin_editor.cpp leem daqui. O bug
// que isto fecha e' o do lfodiv: o layout dizia indice 2 ("1/4") e o editor
// dizia 0.0 ("1/1"), e o duplo-clique repunha o valor errado porque eram duas
// tabelas a falar do mesmo ID. Com uma so' tabela, um ID ou um default nao
// podem divergir por esquecimento.
//
// Sem JUCE de proposito: os testes incluem este header no binario puro do
// nucleo, sem carregar o framework.
struct Default {
    const char* id;
    float value;

    // Indice de choice e valor de parametro inteiro. O +0.5 evita o truncamento
    // de 1.9999 por causa do float.
    constexpr int index() const { return static_cast<int>(value + 0.5f); }
};

inline constexpr Default grain {"grain", 40.0f};
inline constexpr Default density {"density", 20.0f};
inline constexpr Default position {"position", 0.5f};
inline constexpr Default spray {"spray", 0.0f};
inline constexpr Default pitch {"pitch", 0.0f};
inline constexpr Default volume {"volume", 0.0f};
inline constexpr Default window {"window", 0.0f};
inline constexpr Default pan {"pan", 0.0f};
inline constexpr Default grainlevel {"grainlevel", 1.0f};
inline constexpr Default pitchrand {"pitchrand", 0.0f};
inline constexpr Default cloudsize {"cloudsize", 0.0f};
inline constexpr Default pitchspread {"pitchspread", 0.0f};
inline constexpr Default grainjit {"grainjit", 0.0f};
inline constexpr Default scanspeed {"scanspeed", 1.0f};
inline constexpr Default attack {"attack", 0.005f};
inline constexpr Default decay {"decay", 0.1f};
inline constexpr Default sustain {"sustain", 1.0f};
inline constexpr Default release {"release", 0.05f};
inline constexpr Default atkcurve {"atkcurve", 0.0f};
inline constexpr Default deccurve {"deccurve", 0.0f};
inline constexpr Default relcurve {"relcurve", 0.0f};
inline constexpr Default f1type {"f1type", 0.0f};
inline constexpr Default f1cutoff {"f1cutoff", 20000.0f};
inline constexpr Default f1q {"f1q", 0.7071f};
inline constexpr Default f1slope {"f1slope", 0.0f};
inline constexpr Default f1drive {"f1drive", 0.0f};
inline constexpr Default f1keytrack {"f1keytrack", 0.0f};
inline constexpr Default f1env {"f1env", 0.0f};
inline constexpr Default f2type {"f2type", 0.0f};
inline constexpr Default f2cutoff {"f2cutoff", 20000.0f};
inline constexpr Default f2q {"f2q", 0.7071f};
inline constexpr Default lforate {"lforate", 1.0f};
inline constexpr Default lfodepth {"lfodepth", 0.0f};
inline constexpr Default lfotarget {"lfotarget", 0.0f};
inline constexpr Default lfowave {"lfowave", 0.0f};
inline constexpr Default scale {"scale", 0.0f};
inline constexpr Default root {"root", 0.0f};
inline constexpr Default glide {"glide", 0.0f};
inline constexpr Default lfosync {"lfosync", 0.0f};
inline constexpr Default lfodiv {"lfodiv", 2.0f};
inline constexpr Default lfosteps {"lfosteps", 16.0f};

inline constexpr std::array<Default, 41> kDefaults {
    grain, density, position, spray, pitch, volume, window, pan, grainlevel,
    pitchrand, cloudsize, pitchspread, grainjit, scanspeed, attack, decay,
    sustain, release, atkcurve, deccurve, relcurve, f1type, f1cutoff, f1q,
    f1slope, f1drive, f1keytrack, f1env, f2type, f2cutoff, f2q, lforate,
    lfodepth, lfotarget, lfowave, scale, root, glide, lfosync, lfodiv,
    lfosteps,
};

} // namespace opcoda::params
