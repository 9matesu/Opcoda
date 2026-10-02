#include "opcoda_core/pe/pe_parser.h"

#include "opcoda_core/entropy/shannon_entropy.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace opcoda::pe {
namespace {

constexpr std::size_t kDosSignatureOffset = 0;
constexpr std::size_t kELfanewOffset = 0x3C;
constexpr std::size_t kPeSignatureSize = 4;

constexpr std::size_t kCoffNumberOfSectionsOffset = 2;
constexpr std::size_t kCoffSizeOfOptionalHeaderOffset = 16;
constexpr std::size_t kOptionalSizeOfHeadersOffset = 60;
constexpr std::size_t kOptionalSizeOfImageOffset = 56;
constexpr std::size_t kSectionTableEntrySize = 40;
constexpr std::size_t kCoffHeaderSize = 20;
constexpr std::size_t kPe32OptionalHeaderSize = 96;
constexpr std::size_t kMinSizeForAnyPe = 64;
constexpr std::uint16_t kMaxSections = 96;

std::uint16_t readU16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t readU32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

bool inBounds(std::size_t offset, std::size_t length, std::size_t total) noexcept {
    if (offset > total) {
        return false;
    }
    return length <= total - offset;
}

} // namespace

const char* toString(PeError e) noexcept {
    switch (e) {
        case PeError::kOk: return "E_OK";
        case PeError::kEmpty: return "E_EMPTY";
        case PeError::kTooSmall: return "E_TOO_SMALL";
        case PeError::kBadMz: return "E_BAD_MZ";
        case PeError::kBadELfanew: return "E_BAD_E_LFANEW";
        case PeError::kBadPe: return "E_BAD_PE";
        case PeError::kBadOptionalHeader: return "E_BAD_OPTIONAL_HEADER";
        case PeError::kTooManySections: return "E_TOO_MANY_SECTIONS";
        case PeError::kBadSectionTable: return "E_BAD_SECTION_TABLE";
        case PeError::kOob: return "E_OOB";
        case PeError::kTruncated: return "E_TRUNCATED";
    }
    return "E_UNKNOWN";
}

PeParseResult parse(const std::uint8_t* data, std::size_t size) noexcept {
    PeParseResult result;
    if (data == nullptr || size == 0) {
        result.error = PeError::kEmpty;
        return result;
    }
    if (size < kMinSizeForAnyPe) {
        result.error = PeError::kTooSmall;
        return result;
    }
    if (data[kDosSignatureOffset] != 'M' || data[kDosSignatureOffset + 1] != 'Z') {
        result.error = PeError::kBadMz;
        return result;
    }

    const auto eLfanew = static_cast<std::size_t>(readU32(data + kELfanewOffset));
    if (!inBounds(eLfanew, kPeSignatureSize + kCoffHeaderSize, size)) {
        result.error = PeError::kBadELfanew;
        return result;
    }

    const std::uint8_t* signature = data + eLfanew;
    if (signature[0] != 'P' || signature[1] != 'E' ||
        signature[2] != 0 || signature[3] != 0) {
        result.error = PeError::kBadPe;
        return result;
    }

    const std::uint8_t* coff = signature + kPeSignatureSize;

    const auto numberOfSections = readU16(coff + kCoffNumberOfSectionsOffset);
    if (numberOfSections == 0 || numberOfSections > kMaxSections) {
        result.error = PeError::kTooManySections;
        return result;
    }

    const auto sizeOfOptionalHeader = readU16(coff + kCoffSizeOfOptionalHeaderOffset);
    if (sizeOfOptionalHeader < kPe32OptionalHeaderSize) {
        result.error = PeError::kBadOptionalHeader;
        return result;
    }

    const std::size_t optionalOffset = static_cast<std::size_t>(coff - data) + kCoffHeaderSize;
    if (!inBounds(optionalOffset, sizeOfOptionalHeader, size)) {
        result.error = PeError::kBadOptionalHeader;
        return result;
    }

    const std::size_t tableOffset = optionalOffset + sizeOfOptionalHeader;
    const std::size_t tableBytes = static_cast<std::size_t>(numberOfSections) * kSectionTableEntrySize;
    if (!inBounds(tableOffset, tableBytes, size)) {
        result.error = PeError::kBadSectionTable;
        return result;
    }

    PeImage& image = result.image;
    image.numberOfSections = numberOfSections;
    image.sizeOfHeaders = readU32(data + optionalOffset + kOptionalSizeOfHeadersOffset);
    image.sizeOfImage = readU32(data + optionalOffset + kOptionalSizeOfImageOffset);

    std::size_t totalRaw = 0;
    for (std::size_t i = 0; i < numberOfSections; ++i) {
        const std::uint8_t* entry = data + tableOffset + (i * kSectionTableEntrySize);
        Section& section = image.sections[i];

        std::memcpy(section.name, entry, 8);
        section.name[8] = '\0';
        section.virtualSize = readU32(entry + 8);
        section.virtualAddress = readU32(entry + 12);
        section.rawOffset = readU32(entry + 20);

        const std::uint32_t declaredRawSize = readU32(entry + 16);
        const std::size_t available = section.rawOffset < size ? size - section.rawOffset : 0;
        section.rawSize = static_cast<std::uint32_t>(
            std::min<std::size_t>(declaredRawSize, available));

        if (declaredRawSize > section.rawSize) {
            result.error = PeError::kOob;
            return result;
        }

        // A secao tem um fim proprio, rawOffset + rawSize, e medir ate o fim
        // do arquivo pegaria o material das secoes seguintes. Uma secao sem
        // dados brutos, como .bss, fica com entropia zero.
        section.entropy = entropy::shannonBitsPerByte(
            data, size, section.rawOffset, section.rawSize);

        totalRaw += section.rawSize;
    }

    image.sampleCount = totalRaw;
    return result;
}

PeParseResult parseFile(const char* path) noexcept {
    PeParseResult result;
    if (path == nullptr) {
        result.error = PeError::kEmpty;
        return result;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        result.error = PeError::kEmpty;
        return result;
    }

    const std::streamoff end = file.tellg();
    if (end <= 0) {
        result.error = PeError::kEmpty;
        return result;
    }

    const auto size = static_cast<std::size_t>(end);
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(size);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
        result.error = PeError::kTruncated;
        return result;
    }

    return parse(bytes.data(), size);
}

} // namespace opcoda::pe
