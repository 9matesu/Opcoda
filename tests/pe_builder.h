#pragma once

#include "opcoda_core/pe/pe_parser.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

// construtor de PE minimo em memoria, partilhado entre os testes.
// monta so' os campos que o parser le, o que torna a mutacao de qualquer campo
// trivial e o resultado deterministico. num header comum nao duplica o formato
// entre os testes unitarios e o ensaio T1.
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

    // parenteses, e nao chaves: com chaves o vector escolhia o construtor de
    // initializer_list e tentava narrowar kTotalSize para uint8_t.
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

    // acrescenta bytes ao fim do ficheiro e mantem SizeOfImage coerente. sem isto,
    // um teste com duas seccoes escrevia alem do vector e o ASan acusava o teste,
    // nao o parser.
    void grow(std::size_t extraBytes) {
        bytes.resize(bytes.size() + extraBytes, std::uint8_t{0});
        writeU32(kOptionalOffset + 56, static_cast<std::uint32_t>(bytes.size()));
    }

    void addSection(const char* name, std::uint32_t rawOffset, std::uint32_t rawSize) {
        const std::size_t index = numberOfSections++;
        const std::size_t entry = kTableOffset + (index * kEntrySize);
        // ha area reservada depois do cabecalho, mas a verificacao fica explicita:
        // escrever fora do vector transformava o teste num teste invalido, e o ASan
        // acusava a linha errada.
        if (entry + kEntrySize > bytes.size()) {
            throw std::out_of_range("PeBuilder: tabela de secoes excede o buffer");
        }
        // o campo de nome tem 8 bytes no formato PE, mas o literal pode ser menor:
        // copiar sempre 8 leria alem dele. o ASan aponta isso como
        // global-buffer-overflow, e com razao.
        const std::size_t nameLength = std::min(std::strlen(name), std::size_t{7});
        std::memcpy(bytes.data() + entry, name, nameLength);

        writeU32(entry + 16, rawSize);
        writeU32(entry + 20, rawOffset);
        writeU16(kCoffOffset + 2, static_cast<std::uint16_t>(numberOfSections));
    }

    // preenche toda a regiao de .text com um padrao de byte unico e deixa os
    // cabecalhos intactos. usado pelos ensaios de offset DC.
    void fillTextWith(std::uint8_t value) {
        for (std::size_t i = kHeaderEnd; i < kTotalSize; ++i) {
            bytes[i] = value;
        }
    }

    // preenche .text com um gradiente, evitando a media zero perfeita que um byte
    // repetido produziria.
    void fillTextWithGradient(std::uint8_t base, std::uint8_t span) {
        for (std::size_t i = kHeaderEnd; i < kTotalSize; ++i) {
            bytes[i] = static_cast<std::uint8_t>(base + ((i - kHeaderEnd) % span));
        }
    }
};

} // namespace opcoda::test
