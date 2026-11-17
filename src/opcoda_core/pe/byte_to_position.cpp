#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opcoda::pe {

double positionForByte(std::uint64_t byte, const ByteRange& region) noexcept {
    const auto length = region.length();

    if (length < 2) {
        return 0.0;
    }

    const auto highest = region.end - 2;
    const auto address = std::clamp<std::uint64_t>(byte, region.start, highest);

    return static_cast<double>(address - region.start) / static_cast<double>(length - 1);
}

std::uint64_t byteForPosition(double position, const ByteRange& region) noexcept {
    const auto length = region.length();
    if (length < 2) {
        return region.start;
    }

    const auto enginePosition = static_cast<float>(std::clamp(position, 0.0, 1.0));
    const auto lastIndex = static_cast<double>(length - 1);
    const auto read = static_cast<std::uint64_t>(
        std::llround(static_cast<double>(enginePosition) * lastIndex));

    return region.start + std::min<std::uint64_t>(read, length - 2);
}

RowSpan regionColumnsInRow(std::uint64_t rowAddress, std::uint64_t rowBytes,
                           const ByteRange& region) noexcept {
    if (rowBytes == 0 || region.empty()) {
        return {};
    }

    // o fim da linha vem de uma soma ja' conferida, para nao transbordar.
    if (rowAddress > std::numeric_limits<std::uint64_t>::max() - rowBytes) {
        return {};
    }
    const auto rowEnd = rowAddress + rowBytes;

    if (rowEnd <= region.start || rowAddress >= region.end) {
        return {};
    }

    const auto first = (region.start > rowAddress) ? region.start - rowAddress : 0;
    const auto last = (region.end < rowEnd) ? region.end - rowAddress : rowBytes;

    if (first >= rowBytes || last <= first) {
        return {};
    }
    return {std::min(first, rowBytes), std::min(last, rowBytes)};
}

} // namespace opcoda::pe