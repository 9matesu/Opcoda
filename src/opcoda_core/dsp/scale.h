#pragma once

#include <cstdint>

namespace opcoda::dsp {

// escalas na ordem do choice da UI.
enum class ScaleType : std::uint8_t {
    kChromatic = 0,
    kMajor,
    kNaturalMinor,
    kHarmonicMinor,
    kDorian,
    kPhrygian,
    kLydian,
    kMixolydian,
    kPentatonicMajor,
    kPentatonicMinor,
    kBlues,
    kWholeTone,
};

inline constexpr int kScaleCount = 12;

// mascara de 12 bits, bit 0 = tonica. chromatic devolve todos os graus.
[[nodiscard]] constexpr std::uint16_t scaleMask(ScaleType scale) noexcept {
    switch (scale) {
        case ScaleType::kMajor: return 0x0AB5;           // 0 2 4 5 7 9 11
        case ScaleType::kNaturalMinor: return 0x05AD;    // 0 2 3 5 7 8 10
        case ScaleType::kHarmonicMinor: return 0x09AD;   // 0 2 3 5 7 8 11
        case ScaleType::kDorian: return 0x06AD;          // 0 2 3 5 7 9 10
        case ScaleType::kPhrygian: return 0x05AB;        // 0 1 3 5 7 8 10
        case ScaleType::kLydian: return 0x0AD5;          // 0 2 4 6 7 9 11
        case ScaleType::kMixolydian: return 0x06B5;      // 0 2 4 5 7 9 10
        case ScaleType::kPentatonicMajor: return 0x0295; // 0 2 4 7 9
        case ScaleType::kPentatonicMinor: return 0x04A9; // 0 3 5 7 10
        case ScaleType::kBlues: return 0x04E9;           // 0 3 5 6 7 10
        case ScaleType::kWholeTone: return 0x0555;       // 0 2 4 6 8 10
        case ScaleType::kChromatic:
        default: return 0x0FFF;
    }
}

[[nodiscard]] constexpr int wrap12(int value) noexcept {
    const int remainder = value % 12;
    return (remainder < 0) ? (remainder + 12) : remainder;
}

// aproxima a nota ao grau mais proximo. empate desce: a cada distancia testa-se
// primeiro o grau abaixo. escala invalida cai em chromatic, como os outros
// choices: estado corrompido tem de soar, nao calar.
[[nodiscard]] constexpr int quantizeToScale(int note, ScaleType scale, int root) noexcept {
    const std::uint16_t mask = scaleMask(scale);
    if (mask == 0x0FFF) {
        return note;
    }
    const int pitchClass = wrap12(note - root);
    if (((mask >> pitchClass) & 1U) != 0U) {
        return note;
    }
    for (int distance = 1; distance <= 6; ++distance) {
        const int down = wrap12(pitchClass - distance);
        if (((mask >> down) & 1U) != 0U) {
            return note - distance;
        }
        const int up = wrap12(pitchClass + distance);
        if (((mask >> up) & 1U) != 0U) {
            return note + distance;
        }
    }
    return note;
}

// semitons relativos a Do4 (MIDI 60). em chromatic e' a diferenca exata.
[[nodiscard]] constexpr double semitonesForNote(int note, ScaleType scale, int root) noexcept {
    return static_cast<double>(quantizeToScale(note, scale, root) - 60);
}

} // namespace opcoda::dsp
