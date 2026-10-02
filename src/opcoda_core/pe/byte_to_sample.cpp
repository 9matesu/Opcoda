#include "opcoda_core/pe/byte_to_sample.h"

namespace opcoda::pe {

std::vector<float> toSamples(const std::uint8_t* data,
                             std::size_t offset,
                             std::size_t count) noexcept {
    if (data == nullptr || count == 0) {
        return {};
    }

    std::vector<float> samples(count);
    for (std::size_t i = 0; i < count; ++i) {
        samples[i] = (static_cast<float>(data[offset + i]) - 127.5f) / 127.5f;
    }
    return samples;
}

} // namespace opcoda::pe
