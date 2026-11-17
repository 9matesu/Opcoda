#pragma once

#include "opcoda_core/pe/byte_range.h"

#include <cstdint>

namespace opcoda::pe {

[[nodiscard]] double positionForByte(std::uint64_t byte, const ByteRange& region) noexcept;

[[nodiscard]] std::uint64_t byteForPosition(double position, const ByteRange& region) noexcept;

struct RowSpan {
    std::uint64_t firstColumn {0};  // fechada
    std::uint64_t lastColumn {0};   // aberta
    [[nodiscard]] bool empty() const noexcept { return firstColumn >= lastColumn; }
    [[nodiscard]] std::uint64_t count() const noexcept { return empty() ? 0 : lastColumn - firstColumn; }
};

[[nodiscard]] RowSpan regionColumnsInRow(std::uint64_t rowAddress, std::uint64_t rowBytes,
                                         const ByteRange& region) noexcept;

} // namespace opcoda::pe