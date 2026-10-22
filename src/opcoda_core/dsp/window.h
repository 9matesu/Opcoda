#pragma once

#include <cstdint>

namespace opcoda::dsp {

enum class WindowType : std::uint8_t {
    kHann = 0,
    kGaussian,
    kHamming,
    kBlackman,
};

[[nodiscard]] const char* windowName(WindowType type) noexcept;

// Indice corrompido de choice vira Hann, e nao silencio: a mesma regra dos
// filtros e do LFO, pelo mesmo motivo — tem de soar.
[[nodiscard]] inline WindowType sanitizeWindowType(WindowType type) noexcept {
    return (type <= WindowType::kBlackman) ? type : WindowType::kHann;
}

void fillWindow(WindowType type, float* out, int length) noexcept;

} // namespace opcoda::dsp
