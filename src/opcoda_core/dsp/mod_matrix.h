#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace opcoda::dsp {

// Fontes e destinos da matriz, na ordem em que o estado as guarda. A ordem e'
// contrato: os inteiros viajam no preset e no .opcoda, e trocar a ordem mudaria
// o som de sessoes salvas.
enum class ModSource : std::uint8_t {
    kNone = 0,
    kLfo,
    kEnvelope,
    kVelocity,
    kKeytrack,
    kModWheel,
    kCc74,
    kCc71,
    kEntropy,
    kCount,
};

enum class ModDest : std::uint8_t {
    kNone = 0,
    kPitch,
    kDensity,
    kPosition,
    kSpray,
    kGrainSize,
    kCutoff1,
    kQ1,
    kCutoff2,
    kQ2,
    kPan,
    kGrainLevel,
    kDrive,
    kCount,
};

inline constexpr int kMaxModRoutes = 8;
// Cada rota viaja em tres inteiros: fonte, destino e amount em milesimos. E' a
// forma que o estado guarda; o APVTS nao ve nada disto como parametro.
inline constexpr int kModRouteWords = 3;

struct ModRoute {
    ModSource source {ModSource::kNone};
    ModDest destination {ModDest::kNone};
    float amount {0.0f};
};

struct ModMatrix {
    std::array<ModRoute, kMaxModRoutes> routes {};
};

// Valores normalizados das fontes no bloco corrente. -1..+1 excepto envelope e
// entropia, que sao unipolares 0..1 — a soma com amount negativo trata as duas
// como "quanto ha", e nao como oscilacao.
struct ModSourceValues {
    float lfo {0.0f};
    float envelope {0.0f};
    float velocity {0.0f};
    float keytrack {0.0f};
    float modWheel {0.0f};
    float cc74 {0.0f};
    float cc71 {0.0f};
    float entropy {0.0f};
};

[[nodiscard]] inline float sourceValue(ModSource source, const ModSourceValues& values) noexcept {
    switch (source) {
        case ModSource::kLfo: return values.lfo;
        case ModSource::kEnvelope: return values.envelope;
        case ModSource::kVelocity: return values.velocity;
        case ModSource::kKeytrack: return values.keytrack;
        case ModSource::kModWheel: return values.modWheel;
        case ModSource::kCc74: return values.cc74;
        case ModSource::kCc71: return values.cc71;
        case ModSource::kEntropy: return values.entropy;
        case ModSource::kNone:
        case ModSource::kCount:
            break;
    }
    return 0.0f;
}

[[nodiscard]] inline ModSource sanitizeModSource(int raw) noexcept {
    return (raw > 0 && raw < static_cast<int>(ModSource::kCount))
               ? static_cast<ModSource>(raw)
               : ModSource::kNone;
}

[[nodiscard]] inline ModDest sanitizeModDest(int raw) noexcept {
    return (raw > 0 && raw < static_cast<int>(ModDest::kCount))
               ? static_cast<ModDest>(raw)
               : ModDest::kNone;
}

[[nodiscard]] inline float sanitizeModAmount(float amount) noexcept {
    return std::isfinite(amount) ? std::clamp(amount, -1.0f, 1.0f) : 0.0f;
}

// Soma das rotas que apontam para `dest`, limitada a [-1, 1]. Duas rotas da
// mesma fonte somam — e' o que permite ligar o LFO ao corte e a densidade ao
// mesmo tempo sem duplicar o LFO.
[[nodiscard]] inline float matrixOffset(const ModMatrix& matrix,
                                       ModDest dest,
                                       const ModSourceValues& values) noexcept {
    float sum = 0.0f;
    for (const auto& route : matrix.routes) {
        if (route.destination != dest || route.source == ModSource::kNone) {
            continue;
        }
        sum += sourceValue(route.source, values) * sanitizeModAmount(route.amount);
    }
    return std::clamp(sum, -1.0f, 1.0f);
}

using ModMatrixArray = std::array<int, kMaxModRoutes * kModRouteWords>;

[[nodiscard]] inline ModMatrixArray toArray(const ModMatrix& matrix) noexcept {
    ModMatrixArray words {};
    for (int i = 0; i < kMaxModRoutes; ++i) {
        const auto& route = matrix.routes[static_cast<std::size_t>(i)];
        const auto base = static_cast<std::size_t>(i * kModRouteWords);
        words[base] = static_cast<int>(route.source);
        words[base + 1] = static_cast<int>(route.destination);
        words[base + 2] = static_cast<int>(std::lround(sanitizeModAmount(route.amount) * 1000.0f));
    }
    return words;
}

// Rota invalida vira rota vazia, e amount fora de faixa e' recortado: estado
// corrompido tem de abrir em silencio, e nao lancar excecao nem modular a esmo.
[[nodiscard]] inline ModMatrix fromArray(const ModMatrixArray& words) noexcept {
    ModMatrix matrix;
    for (int i = 0; i < kMaxModRoutes; ++i) {
        const auto base = static_cast<std::size_t>(i * kModRouteWords);
        ModRoute route;
        route.source = sanitizeModSource(words[base]);
        route.destination = sanitizeModDest(words[base + 1]);
        route.amount = sanitizeModAmount(static_cast<float>(words[base + 2]) / 1000.0f);
        if (route.source == ModSource::kNone || route.destination == ModDest::kNone) {
            route = ModRoute {};
        }
        matrix.routes[static_cast<std::size_t>(i)] = route;
    }
    return matrix;
}

} // namespace opcoda::dsp
