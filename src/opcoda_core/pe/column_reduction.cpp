#include "opcoda_core/pe/column_reduction.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace opcoda::pe {
namespace {

constexpr float byteToSample(std::uint8_t byte) noexcept {
    return (static_cast<float>(byte) - 127.5f) / 127.5f;
}

double bitsFromHistogram(const std::array<std::uint32_t, 256>& histogram,
                         std::uint64_t observations) noexcept {
    if (observations == 0) {
        return 0.0;
    }

    double sum = 0.0;
    for (const auto count : histogram) {
        if (count == 0) {
            continue;
        }
        const auto p = static_cast<double>(count) / static_cast<double>(observations);
        sum += p * std::log2(p);
    }
    return -sum;
}

constexpr std::uint64_t kMaxColumns {65536};

} // namespace

bool reduceToColumns(const std::uint8_t* data,
                     std::size_t size,
                     std::uint64_t start,
                     std::uint64_t end,
                     std::uint32_t columns,
                     std::vector<Column>& out) noexcept {
    if (data == nullptr || columns == 0) {
        return false;
    }

    if (end <= start) {
        return false;
    }
    if (start >= static_cast<std::uint64_t>(size)) {
        return false;
    }
    if (end > static_cast<std::uint64_t>(size)) {
        return false;
    }

    const auto length = static_cast<std::uint64_t>(end - start);

    // uma coluna por byte, no maximo. coluna sem byte seria um rectangulo vazio.
    const auto wanted = static_cast<std::uint64_t>(columns);
    const auto capped = std::min(wanted, kMaxColumns);
    const auto actual = static_cast<std::uint32_t>(std::min(capped, length));
    if (actual == 0) {
        return false;
    }

    std::vector<Column> reduced;
    reduced.resize(actual);

    const auto columnLength = length / actual;
    const auto remainder = length % actual;
    const auto sampleCap = static_cast<std::uint64_t>(kMaxSamplesPerColumn);

    std::uint64_t cursor = start;

    for (std::uint32_t column = 0; column < actual; ++column) {
        const auto bytes = columnLength + (column < remainder ? 1u : 0u);

        auto& target = reduced[static_cast<std::size_t>(column)];

        target.byteCount = bytes;

        const auto columnStart = cursor;
        const auto columnEnd = cursor + bytes;

        auto stride = std::max<std::uint64_t>(1, (bytes + sampleCap - 1) / sampleCap);
        if ((stride % 2) == 0) {
            ++stride;
        }

        auto minimum = std::numeric_limits<float>::max();
        auto maximum = std::numeric_limits<float>::lowest();
        double sumSquares = 0.0;
        std::array<std::uint32_t, 256> histogram {};
        std::uint64_t observed = 0;

        for (auto offset = columnStart; offset < columnEnd; offset += stride) {
            const auto byte = data[static_cast<std::size_t>(offset)];
            const auto value = byteToSample(byte);

            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
            sumSquares += static_cast<double>(value) * static_cast<double>(value);
            ++histogram[byte];
            ++observed;
        }

        if (observed == 0) {
            target = Column {};
            target.byteCount = 0;
            cursor = columnEnd;
            continue;
        }

        target.minimum = minimum;
        target.maximum = maximum;
        target.rms = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(observed)));
        target.entropyBits = static_cast<float>(bitsFromHistogram(histogram, observed));

        cursor = columnEnd;
    }

    out = std::move(reduced);
    return true;
}

} // namespace opcoda::pe
