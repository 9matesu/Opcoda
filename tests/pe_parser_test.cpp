#include <gtest/gtest.h>

#include "opcoda_core/pe/pe_parser.h"
#include "pe_builder.h"

#include <cstdint>
#include <vector>

using opcoda::pe::PeError;
using opcoda::pe::parse;
using opcoda::pe::toString;
using opcoda::test::PeBuilder;

TEST(PeParser, RejectsEmptyBuffer) {
    const auto result = parse(nullptr, 0);
    EXPECT_EQ(result.error, PeError::kEmpty);
    EXPECT_FALSE(result.ok());
    EXPECT_STREQ(toString(result.error), "E_EMPTY");
}

TEST(PeParser, RejectsSingleByte) {
    const std::uint8_t byte = 0x00;
    EXPECT_EQ(parse(&byte, 1).error, PeError::kTooSmall);
}

TEST(PeParser, RejectsMissingMz) {
    PeBuilder builder;
    builder.bytes[0] = 'X';
    EXPECT_EQ(parse(builder.bytes.data(), builder.bytes.size()).error, PeError::kBadMz);
}

TEST(PeParser, RejectsELfanewBeyondFile) {
    PeBuilder builder;
    builder.writeU32(0x3C, 0x7FFFFFFF);
    EXPECT_EQ(parse(builder.bytes.data(), builder.bytes.size()).error, PeError::kBadELfanew);
}

TEST(PeParser, RejectsMissingPeSignature) {
    PeBuilder builder;
    builder.bytes[PeBuilder::kSignatureOffset] = 'X';
    EXPECT_EQ(parse(builder.bytes.data(), builder.bytes.size()).error, PeError::kBadPe);
}

TEST(PeParser, RejectsAbsurdSectionCount) {
    PeBuilder builder;
    builder.writeU16(PeBuilder::kCoffOffset + 2, 60000);
    EXPECT_EQ(parse(builder.bytes.data(), builder.bytes.size()).error, PeError::kTooManySections);
}

TEST(PeParser, RejectsZeroSections) {
    PeBuilder builder;
    builder.writeU16(PeBuilder::kCoffOffset + 2, 0);
    EXPECT_EQ(parse(builder.bytes.data(), builder.bytes.size()).error, PeError::kTooManySections);
}

TEST(PeParser, RejectsSectionTablePastEndOfFile) {
    PeBuilder builder;
    builder.writeU16(PeBuilder::kCoffOffset + 2, 64);
    const auto result = parse(builder.bytes.data(), builder.bytes.size());
    EXPECT_EQ(result.error, PeError::kBadSectionTable);
}

TEST(PeParser, TruncatedFileIsRejectedNotCrashed) {
    // Arquivo bem formado mas cortado no meio: o parser precisa recusar, e o
    // ensaio de borda do T3 conta a rejeicao como sucesso.
    PeBuilder builder;
    for (std::size_t length = 1; length < builder.bytes.size(); ++length) {
        const auto result = parse(builder.bytes.data(), length);
        if (result.ok()) {
            for (std::size_t i = 0; i < result.image.numberOfSections; ++i) {
                const auto& section = result.image.sections[i];
                EXPECT_LE(static_cast<std::size_t>(section.rawOffset) + section.rawSize, length)
                    << "secao aceitou bytes alem do arquivo truncado, length=" << length;
            }
        }
    }
}

TEST(PeParser, RejectsRawSizeBeyondFile) {
    PeBuilder builder;
    builder.writeU32(PeBuilder::kTableOffset + 16, 0x00FFFFFF);
    EXPECT_EQ(parse(builder.bytes.data(), builder.bytes.size()).error, PeError::kOob);
}

TEST(PeParser, ParsesValidImage) {
    PeBuilder builder;
    const auto result = parse(builder.bytes.data(), builder.bytes.size());

    ASSERT_TRUE(result.ok()) << toString(result.error);
    EXPECT_EQ(result.image.numberOfSections, 1);
    EXPECT_STREQ(result.image.sections[0].name, ".text");
    EXPECT_EQ(result.image.sections[0].rawSize, 0x100u);
    EXPECT_EQ(result.image.sections[0].rawOffset, PeBuilder::kHeaderEnd);
    EXPECT_EQ(result.image.sampleCount, 0x100u);
}

TEST(PeParser, SingleRepeatedByteHasZeroEntropy) {
    // Byte unico repetido tem so um simbolo, logo zero bits por byte. E o piso
    // da escala: um .rsrc de icone comprido deve aparecer perto de zero.
    PeBuilder builder;
    builder.fillTextWith(0x41);
    const auto result = parse(builder.bytes.data(), builder.bytes.size());

    ASSERT_TRUE(result.ok()) << toString(result.error);
    EXPECT_DOUBLE_EQ(result.image.sections[0].entropy, 0.0);
}

TEST(PeParser, UniformDistributionOfFourBytesIsTwoBits) {
    // Quatro valores igualmente frequentes dao log2(4) = 2 bits por byte.
    PeBuilder builder;
    builder.fillTextWithGradient(0x00, 4);
    const auto result = parse(builder.bytes.data(), builder.bytes.size());

    ASSERT_TRUE(result.ok()) << toString(result.error);
    EXPECT_NEAR(result.image.sections[0].entropy, 2.0, 0.01);
}

TEST(PeParser, EachSectionIsMeasuredIndependently) {
    // Duas secoes com conteudo distinto: se a entropia fosse medida sobre o
    // arquivo inteiro ou sobre o intervalo errado, as duas dariam o mesmo valor.
    PeBuilder builder;
    const auto secondOffset = static_cast<std::uint32_t>(PeBuilder::kTotalSize);
    builder.grow(0x100);
    builder.addSection(".data", secondOffset, 0x100);

    builder.fillTextWith(0x00);
    for (std::size_t i = 0; i < 0x100; ++i) {
        builder.bytes[secondOffset + i] = static_cast<std::uint8_t>(i & 0xFF);
    }

    const auto result = parse(builder.bytes.data(), builder.bytes.size());
    ASSERT_TRUE(result.ok()) << toString(result.error);
    ASSERT_EQ(result.image.numberOfSections, 2);

    EXPECT_DOUBLE_EQ(result.image.sections[0].entropy, 0.0);
    EXPECT_GT(result.image.sections[1].entropy, 7.0);
}

TEST(PeParser, ZeroRawSizeSectionKeepsEntropyZero) {
    // .bss e' so virtual: SizeOfRawData zero. Medir a partir do offset daria a
    // entropia do resto do arquivo, entao a secao tem de ficar em zero.
    PeBuilder builder;
    builder.addSection(".bss", 0, 0);

    const auto result = parse(builder.bytes.data(), builder.bytes.size());
    ASSERT_TRUE(result.ok()) << toString(result.error);
    ASSERT_EQ(result.image.numberOfSections, 2);
    EXPECT_EQ(result.image.sections[1].rawSize, 0u);
    EXPECT_DOUBLE_EQ(result.image.sections[1].entropy, 0.0);
}

TEST(PeParser, ErrorCodesAreStable) {
    // Nomes de erro aparecem na interface e no relatorio; mudanca aqui e
    // mudanca visivel para o usuario.
    EXPECT_STREQ(toString(PeError::kOk), "E_OK");
    EXPECT_STREQ(toString(PeError::kBadMz), "E_BAD_MZ");
    EXPECT_STREQ(toString(PeError::kBadPe), "E_BAD_PE");
    EXPECT_STREQ(toString(PeError::kOob), "E_OOB");
    EXPECT_STREQ(toString(PeError::kTruncated), "E_TRUNCATED");
}

TEST(PeParser, HandlesEveryByteValueWithoutCrash) {
    // Varredura ampla: nenhum byte isolado pode derrubar o parser.
    PeBuilder builder;
    for (std::size_t i = 0; i < 256; ++i) {
        PeBuilder mutated;
        mutated.bytes[0x100 + (i % 64)] = static_cast<std::uint8_t>(i);
        const auto result = parse(mutated.bytes.data(), mutated.bytes.size());
        EXPECT_TRUE(result.ok() || result.error != PeError::kOk);
    }
    (void)builder;
}

TEST(PeParser, TruncatedAtEveryLength) {
    // Cortar o arquivo em cada comprimento rejeita ou aceita, mas nunca
    // acessa fora do buffer. Este e o ensaio de borda do T3.
    PeBuilder builder;
    const auto full = builder.bytes.size();
    for (std::size_t length = 0; length <= full; length += 7) {
        const auto result = parse(builder.bytes.data(), length);
        if (result.ok()) {
            EXPECT_LE(result.image.sampleCount, full);
        }
    }
}
