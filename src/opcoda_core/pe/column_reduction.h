#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opcoda::pe {

struct Column {
    float minimum {0.0f};
    float maximum {0.0f};
    float rms {0.0f};
    float entropyBits {0.0f};

    std::uint64_t byteCount {0};

    [[nodiscard]] bool silent() const noexcept { return minimum >= 0.0f && maximum <= 0.0f; }
};

inline constexpr std::uint32_t kMaxSamplesPerColumn {512};

[[nodiscard]] bool reduceToColumns(const std::uint8_t* data,
                                   std::size_t size,
                                   std::uint64_t start,
                                   std::uint64_t end,
                                   std::uint32_t columns,
                                   std::vector<Column>& out) noexcept;

} // namespace opcoda::pe