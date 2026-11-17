#pragma once


#include <array>
#include <cstddef>
#include <cstdint>

namespace opcoda::disasm {

struct Decoded {
    std::uint32_t length {0};
    std::array<char, 80> text {};
    bool hasTarget {false};
    std::int64_t target {0};
};

[[nodiscard]] Decoded decode(const std::uint8_t* data, std::size_t size) noexcept;

} // namespace opcoda::disasm
