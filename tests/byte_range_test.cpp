#include "opcoda_core/pe/byte_range.h"

#include <gtest/gtest.h>

#include <array>

namespace {

using opcoda::pe::ByteRange;

TEST(ByteRange, EmptyWhenEndIsNotAfterStart) {
    EXPECT_TRUE((ByteRange {10, 10}).empty());
    EXPECT_TRUE((ByteRange {10, 5}).empty());
    EXPECT_EQ((ByteRange {10, 5}).length(), 0u);
}

TEST(ByteRange, LengthCountsTheHalfOpenSpan) {
    EXPECT_EQ((ByteRange {100, 250}).length(), 150u);
}

TEST(ClampByteRange, AcceptsAWindowInsideTheFile) {
    ByteRange range {400, 1000};
    ASSERT_TRUE(opcoda::pe::clampByteRange(range, 2000));
    EXPECT_EQ(range.start, 400u);
    EXPECT_EQ(range.end, 1000u);
}

TEST(ClampByteRange, TruncatesAnEndPastTheFile) {
    ByteRange range {400, 99999};
    ASSERT_TRUE(opcoda::pe::clampByteRange(range, 2000));
    EXPECT_EQ(range.end, 2000u);
}

TEST(ClampByteRange, RejectsAnEmptyFile) {
    ByteRange range {0, 10};
    EXPECT_FALSE(opcoda::pe::clampByteRange(range, 0));
}

TEST(ClampByteRange, RejectsAnInvertedWindow) {
    ByteRange range {500, 100};
    EXPECT_FALSE(opcoda::pe::clampByteRange(range, 2000));
}

TEST(ClampByteRange, RejectsAStartPastTheEndOfTheFile) {
    ByteRange range {5000, 6000};
    EXPECT_FALSE(opcoda::pe::clampByteRange(range, 2000));
}

// Uma janela que comeca exatamente no fim do ficheiro nao tem bytes, e um
// inicio igual ao tamanho e' o caso que um "menor ou igual" aceitaria.
TEST(ClampByteRange, RejectsAStartExactlyAtTheEndOfTheFile) {
    ByteRange range {2000, 2000};
    EXPECT_FALSE(opcoda::pe::clampByteRange(range, 2000));
}

TEST(ClampByteRange, RejectionLeavesTheWindowUntouched) {
    ByteRange range {5000, 6000};
    ASSERT_FALSE(opcoda::pe::clampByteRange(range, 2000));
    EXPECT_EQ(range.start, 5000u);
    EXPECT_EQ(range.end, 6000u);
}

TEST(NearestSectionStart, PicksTheClosestStart) {
    constexpr std::array<std::uint32_t, 3> offsets {1024, 8192, 40000};
    constexpr std::array<std::uint32_t, 3> sizes {7168, 31872, 2048};

    EXPECT_EQ(opcoda::pe::nearestSectionStart(100, offsets.data(), sizes.data(), sizes.size()),
              1024u);
    EXPECT_EQ(opcoda::pe::nearestSectionStart(8000, offsets.data(), sizes.data(), sizes.size()),
              8192u);
    EXPECT_EQ(opcoda::pe::nearestSectionStart(39000, offsets.data(), sizes.data(), sizes.size()),
              40000u);
}

TEST(NearestSectionStart, SkipsSectionsWithoutRawData) {
    // .bss fica entre .data e .reloc e nao tem bytes em disco. Alinhar a ele
    // daria uma janela vazia, que e' o mesmo que nao ter janela.
    constexpr std::array<std::uint32_t, 3> offsets {1024, 8192, 8300};
    constexpr std::array<std::uint32_t, 3> sizes {7168, 0, 108};

    // Perto do inicio de .reloc: 8300 e o mais proximo, e o .bss a 8192 nao entra
    // em conta mesmo estando mais perto em relacao aos outros.
    // Perto do inicio de .reloc: 8300 e o mais proximo dos dois com dados. O .bss
    // a 8192 esta' mais perto em numero de bytes e mesmo assim nao entra, porque
    // alinhar a uma secao sem dados daria uma janela de zero bytes.
    EXPECT_EQ(opcoda::pe::nearestSectionStart(8299, offsets.data(), sizes.data(), sizes.size()),
              8300u);

    // No meio de .data, o inicio de .data ganha. Aqui o salto do .bss e' o que
    // decide: sem o salto, o mais proximo seria 8300 a 100 bytes, e nao 8192 a 8.
    EXPECT_EQ(opcoda::pe::nearestSectionStart(8200, offsets.data(), sizes.data(), sizes.size()),
              8300u);
}

TEST(NearestSectionStart, ReturnsTheStartWhenThereAreNoSections) {
    EXPECT_EQ(opcoda::pe::nearestSectionStart(500, nullptr, nullptr, 0), 500u);
}

TEST(NearestSectionStart, ReturnsTheStartWhenEverySectionIsEmpty) {
    constexpr std::array<std::uint32_t, 2> offsets {1024, 8192};
    constexpr std::array<std::uint32_t, 2> sizes {0, 0};

    EXPECT_EQ(opcoda::pe::nearestSectionStart(500, offsets.data(), sizes.data(), sizes.size()),
              500u);
}

} // namespace
