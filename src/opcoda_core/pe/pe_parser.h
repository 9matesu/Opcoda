#pragma once

#include <cstddef>
#include <cstdint>

namespace opcoda::pe {

enum class PeError : std::uint8_t {
    kOk = 0,
    kEmpty,
    kTooSmall,
    kBadMz,
    kBadELfanew,
    kBadPe,
    kBadOptionalHeader,
    kTooManySections,
    kBadSectionTable,
    kOob,
    kTruncated,
};

const char* toString(PeError e) noexcept;

struct Section {
    char name[9] {};
    std::uint32_t virtualSize {};
    std::uint32_t virtualAddress {};
    std::uint32_t rawSize {};
    std::uint32_t rawOffset {};
    double entropy {};
};

struct PeImage {
    std::uint16_t numberOfSections {};
    std::uint32_t sizeOfHeaders {};
    std::uint32_t sizeOfImage {};
    Section sections[96] {};
    std::size_t sampleCount {};
};

struct PeParseResult {
    PeError error {PeError::kOk};
    PeImage image {};

    [[nodiscard]] bool ok() const noexcept { return error == PeError::kOk; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok(); }
};

[[nodiscard]] PeParseResult parse(const std::uint8_t* data, std::size_t size) noexcept;

[[nodiscard]] PeParseResult parseFile(const char* path) noexcept;

} // namespace opcoda::pe
