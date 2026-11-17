#pragma once

#include <cstddef>
#include <cstdint>

namespace opcoda::pe {

struct ByteRange {
    std::uint64_t start {0};
    std::uint64_t end {0};

    [[nodiscard]] std::uint64_t length() const noexcept {
        return end > start ? end - start : 0;
    }
    [[nodiscard]] bool empty() const noexcept { return length() == 0; }
};

[[nodiscard]] bool clampByteRange(ByteRange& range, std::uint64_t fileSize) noexcept;

[[nodiscard]] std::uint64_t nearestSectionStart(std::uint64_t start,
                                                 const std::uint32_t* offsets,
                                                 const std::uint32_t* sizes,
                                                 std::size_t count) noexcept;

// true quando `range` coincide exactamente com uma secao com dados brutos.
[[nodiscard]] bool isSectionAligned(const ByteRange& range,
                                    const std::uint32_t* offsets,
                                    const std::uint32_t* sizes,
                                    std::size_t count) noexcept;

[[nodiscard]] ByteRange sectionSnapToggle(const ByteRange& current,
                                          const ByteRange& exact,
                                          std::uint64_t fileSize,
                                          const std::uint32_t* offsets,
                                          const std::uint32_t* sizes,
                                          std::size_t count) noexcept;

} // namespace opcoda::pe
