#include "opcoda_core/entropy/shannon_entropy.h"

#include <array>
#include <cmath>

namespace opcoda::entropy {

double shannonBitsPerByte(const std::uint8_t* data, std::size_t size) noexcept {
    return shannonBitsPerByte(data, size, 0);
}

double shannonBitsPerByte(const std::uint8_t* data,
                          std::size_t size,
                          std::size_t offset) noexcept {
    if (data == nullptr || offset >= size) {
        return 0.0;
    }
    return shannonBitsPerByte(data, size, offset, size - offset);
}

double shannonBitsPerByte(const std::uint8_t* data,
                          std::size_t size,
                          std::size_t offset,
                          std::size_t length) noexcept {
    if (data == nullptr || length == 0 || offset > size || length > size - offset) {
        return 0.0;
    }

    std::array<std::uint32_t, 256> histogram {};

    for (std::size_t i = 0; i < length; ++i) {
        ++histogram[data[offset + i]];
    }

    const double n = static_cast<double>(length);
    double sum = 0.0;
    for (const auto occurrences : histogram) {
        if (occurrences == 0) {
            continue;
        }
        const double p = static_cast<double>(occurrences) / n;
        sum += p * std::log2(p);
    }
    return -sum;
}

std::size_t shannonCurve(const std::uint8_t* data,
                         std::size_t size,
                         std::size_t windowBytes,
                         double* out,
                         std::size_t outCapacity) noexcept {
    if (data == nullptr || out == nullptr || windowBytes == 0 || outCapacity == 0) {
        return 0;
    }

    std::size_t written = 0;
    for (std::size_t offset = 0; offset < size && written < outCapacity; offset += windowBytes) {
        out[written++] = shannonBitsPerByte(data, size, offset);
    }
    return written;
}

} // namespace opcoda::entropy
