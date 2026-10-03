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

TEST(IsSectionAligned, MatchesASectionExactly) {
    constexpr std::array<std::uint32_t, 2> offsets {1024, 8192};
    constexpr std::array<std::uint32_t, 2> sizes {7168, 2048};

    EXPECT_TRUE(opcoda::pe::isSectionAligned({1024, 8192}, offsets.data(), sizes.data(),
                                             offsets.size()));
    EXPECT_TRUE(opcoda::pe::isSectionAligned({8192, 10240}, offsets.data(), sizes.data(),
                                             offsets.size()));
}

TEST(IsSectionAligned, RejectsAWindowThatStartsInsideASection) {
    constexpr std::array<std::uint32_t, 2> offsets {1024, 8192};
    constexpr std::array<std::uint32_t, 2> sizes {7168, 2048};

    // Comeca dentro de .text e acaba no fim dela: nao e' a secao, e' uma janela
    // dentro dela. Confundir os dois faria o alinhamento alternar sem sair.
    EXPECT_FALSE(opcoda::pe::isSectionAligned({2000, 8192}, offsets.data(), sizes.data(),
                                              offsets.size()));
}

TEST(IsSectionAligned, RejectsAnEmptyWindowAndNoSections) {
    constexpr std::array<std::uint32_t, 1> offsets {1024};
    constexpr std::array<std::uint32_t, 1> sizes {7168};

    EXPECT_FALSE(opcoda::pe::isSectionAligned({0, 0}, offsets.data(), sizes.data(),
                                              offsets.size()));
    EXPECT_FALSE(opcoda::pe::isSectionAligned({1024, 8192}, nullptr, nullptr, 0));
}

TEST(SectionSnapToggle, AlignsToTheNearestSectionFromAnExactWindow) {
    constexpr std::array<std::uint32_t, 2> offsets {1024, 8192};
    constexpr std::array<std::uint32_t, 2> sizes {7168, 2048};

    // Janela exacta dentro de .text: alinhar leva ao inicio e ao fim de .text.
    const auto result = opcoda::pe::sectionSnapToggle({2000, 7000}, {2000, 7000}, 10240,
                                                      offsets.data(), sizes.data(),
                                                      offsets.size());
    EXPECT_EQ(result.start, 1024u);
    EXPECT_EQ(result.end, 8192u);
}

TEST(SectionSnapToggle, ReturnsTheExactWindowWhenAlreadyAligned) {
    constexpr std::array<std::uint32_t, 2> offsets {1024, 8192};
    constexpr std::array<std::uint32_t, 2> sizes {7168, 2048};

    // Ja alinhado em .text: volta a janela exacta. E' o que torna o duplo
    // clique reversivel.
    const auto result = opcoda::pe::sectionSnapToggle({1024, 8192}, {2000, 7000}, 10240,
                                                      offsets.data(), sizes.data(),
                                                      offsets.size());
    EXPECT_EQ(result.start, 2000u);
    EXPECT_EQ(result.end, 7000u);
}

// Sem janela exacta guardada nao ha' para onde voltar. Devolve a regiao atual
// em vez de inventar um destino, que seria pior do que nao fazer nada.
TEST(SectionSnapToggle, StaysPutWhenAlignedWithNoExactWindowToReturnTo) {
    constexpr std::array<std::uint32_t, 1> offsets {1024};
    constexpr std::array<std::uint32_t, 1> sizes {7168};

    const auto result = opcoda::pe::sectionSnapToggle({1024, 8192}, {}, 10240,
                                                      offsets.data(), sizes.data(),
                                                      offsets.size());
    EXPECT_EQ(result.start, 1024u);
    EXPECT_EQ(result.end, 8192u);
}

TEST(SectionSnapToggle, SkipsSectionsWithoutRawDataWhenAligning) {
    // .bss a 8192 com tamanho zero: alinhar a ela daria janela vazia.
    constexpr std::array<std::uint32_t, 3> offsets {1024, 8192, 8300};
    constexpr std::array<std::uint32_t, 3> sizes {7168, 0, 108};

    const auto result = opcoda::pe::sectionSnapToggle({8250, 8400}, {8250, 8400}, 10240,
                                                      offsets.data(), sizes.data(),
                                                      offsets.size());
    EXPECT_EQ(result.start, 8300u);
    EXPECT_EQ(result.end, 8408u);
}

// Um ficheiro sem nenhuma secao com dados nao tem para onde alinhar. A janela
// atual e' devolvida, e nao o ficheiro inteiro: alinhar ao fim do ficheiro
// trocaria o material sem o utilizador pedir.
TEST(SectionSnapToggle, KeepsTheWindowWhenThereIsNoSectionToAlignTo) {
    constexpr std::array<std::uint32_t, 2> offsets {0, 0};
    constexpr std::array<std::uint32_t, 2> sizes {0, 0};

    const auto result = opcoda::pe::sectionSnapToggle({2000, 7000}, {}, 10240,
                                                      offsets.data(), sizes.data(),
                                                      offsets.size());
    EXPECT_EQ(result.start, 2000u);
    EXPECT_EQ(result.end, 7000u);
}

TEST(SectionSnapToggle, RejectsAnAlignmentThatWouldLeaveTheFile) {
    // A secao mais proxima comeca depois do fim do ficheiro pedido.
    constexpr std::array<std::uint32_t, 1> offsets {50000};
    constexpr std::array<std::uint32_t, 1> sizes {100};

    const auto result = opcoda::pe::sectionSnapToggle({100, 200}, {}, 10240,
                                                      offsets.data(), sizes.data(),
                                                      offsets.size());
    EXPECT_EQ(result.start, 100u);
    EXPECT_EQ(result.end, 200u);
}

} // namespace
