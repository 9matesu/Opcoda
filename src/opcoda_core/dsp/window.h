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

void fillWindow(WindowType type, float* out, int length) noexcept;

} // namespace opcoda::dsp
