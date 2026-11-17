#pragma once

#include "opcoda_core/pe/pe_parser.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace opcoda::test {

struct PeBuilder {
    static constexpr std::size_t kELfanew = 0x80;
    // assinatura 'PE\0\0' tem 4 bytes, e o COFF_HEADER de 20 vem depois.
    static constexpr std::size_t kSignatureOffset = kELfanew;
    static constexpr std::size_t kCoffOffset = kELfanew + 4;
    static constexpr std::size_t kOptionalOffset = kCoffOffset + 20;
    static constexpr std::size_t kOptionalSize = 96;
    static constexpr std::size_t kTableOffset = kOptionalOffset + kOptionalSize;
    static constexpr std::size_t kEntrySize = 40;
    static constexpr std::size_t kHeaderEnd = 0x200;
    static constexpr std::size_t kTextSize = 0x100;
    static constexpr std::size_t kTotalSize = kHeaderEnd + kTextSize;

    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(kTotalSize, std::uint8_t{0});
    std::size_t numberOfSections {0};

    PeBuilder() {
        bytes[0] = 'M';
        bytes[1] = 'Z';
        writeU32(0x3C, static_cast<std::uint32_t>(kELfanew));

        bytes[kSignatureOffset + 0] = 'P';
        bytes[kSignatureOffset + 1] = 'E';
        writeU16(kCoffOffset + 2, 1);
        writeU16(kCoffOffset + 16, static_cast<std::uint16_t>(kOptionalSize));

        writeU32(kOptionalOffset + 56, static_cast<std::uint32_t>(kTotalSize));
        writeU32(kOptionalOffset + 60, static_cast<std::uint32_t>(kHeaderEnd));

        addSection(".text", static_cast<std::uint32_t>(kHeaderEnd),
                   static_cast<std::uint32_t>(kTextSize));
    }

    void writeU16(std::size_t offset, std::uint16_t value) {
        bytes[offset] = static_cast<std::uint8_t>(value & 0xFF);
        bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    }

    void writeU32(std::size_t offset, std::uint32_t value) {
        bytes[offset] = static_cast<std::uint8_t>(value & 0xFF);
        bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    }

    void grow(std::size_t extraBytes) {
        bytes.resize(bytes.size() + extraBytes, std::uint8_t{0});
        writeU32(kOptionalOffset + 56, static_cast<std::uint32_t>(bytes.size()));
    }

    void addSection(const char* name, std::uint32_t rawOffset, std::uint32_t rawSize) {
        const std::size_t index = numberOfSections++;
        const std::size_t entry = kTableOffset + (index * kEntrySize);
        if (entry + kEntrySize > bytes.size()) {
            throw std::out_of_range("PeBuilder: tabela de secoes excede o buffer");
        }
        const std::size_t nameLength = std::min(std::strlen(name), std::size_t{7});
        std::memcpy(bytes.data() + entry, name, nameLength);

        writeU32(entry + 16, rawSize);
        writeU32(entry + 20, rawOffset);
        writeU16(kCoffOffset + 2, static_cast<std::uint16_t>(numberOfSections));
    }

    void fillTextWith(std::uint8_t value) {
        for (std::size_t i = kHeaderEnd; i < kTotalSize; ++i) {
            bytes[i] = value;
        }
    }

    void fillTextWithGradient(std::uint8_t base, std::uint8_t span) {
        for (std::size_t i = kHeaderEnd; i < kTotalSize; ++i) {
            bytes[i] = static_cast<std::uint8_t>(base + ((i - kHeaderEnd) % span));
        }
    }
};

} // namespace opcoda::test
