#include "opcoda_core/pe/byte_range.h"

#include <limits>

namespace opcoda::pe {

bool clampByteRange(ByteRange& range, std::uint64_t fileSize) noexcept {
    if (fileSize == 0 || range.end <= range.start || range.start >= fileSize) {
        return false;
    }

    range.end = range.end > fileSize ? fileSize : range.end;
    return !range.empty();
}

std::uint64_t nearestSectionStart(std::uint64_t start,
                                  const std::uint32_t* offsets,
                                  const std::uint32_t* sizes,
                                  std::size_t count) noexcept {
    if (offsets == nullptr || sizes == nullptr) {
        return start;
    }

    std::uint64_t best = start;
    auto bestDistance = std::numeric_limits<std::uint64_t>::max();

    for (std::size_t i = 0; i < count; ++i) {
        if (sizes[i] == 0) {
            continue;
        }

        const auto candidate = static_cast<std::uint64_t>(offsets[i]);
        const auto distance = candidate > start ? candidate - start : start - candidate;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }

    return best;
}

} // namespace opcoda::pe
