#pragma once

#include "opcoda_core/pe/pe_parser.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opcoda::pe {

[[nodiscard]] std::vector<float> toSamples(const std::uint8_t* data,
                                           std::size_t offset,
                                           std::size_t count) noexcept;

} // namespace opcoda::pe
