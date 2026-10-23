#include "opcoda_core/pe/byte_to_position.h"
#include "opcoda_core/pe/byte_range.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace {

using opcoda::pe::ByteRange;
using opcoda::pe::byteForPosition;
using opcoda::pe::positionForByte;

constexpr std::uint64_t kStart {4096};
constexpr std::uint64_t kLength {1000};

// regiao tipica: 1000 bytes a partir de 0x1000.
ByteRange region() { return ByteRange {kStart, kStart + kLength}; }

} // namespace

TEST(PositionForByte, StartOfTheRegionReadsAtZero) {
    EXPECT_DOUBLE_EQ(positionForByte(kStart, region()), 0.0);
}

TEST(PositionForByte, AMiddleByteMapsToItsShareOfTheRegion) {
    // metade da regiao em bytes e' metade da fraccao. sem o -1 no denominador o
    // meio cairia em 0.4995, e o erro so aparecia longe das pontas.
    EXPECT_NEAR(positionForByte(kStart + 499, region()), 499.0 / 999.0, 1e-12);
}

TEST(PositionForByte, TheLastByteNeverMapsToOne) {
    // o caso que justifica o modulo viver no nucleo. posicao 1.0 e' silencio:
    // granular_engine.cpp desliga a voz com `index + 1 >= sourceCount`, o que em
    // 1.0 e' sempre verdade. se o ultimo byte devolvesse 1.0, o clique mais a
    // direita da grelha calava o instrumento sem aviso.
    const auto last = positionForByte(region().end - 1, region());

    EXPECT_LT(last, 1.0);
    EXPECT_NEAR(last, 998.0 / 999.0, 1e-12);
}

TEST(PositionForByte, TheHighestAddressWithSoundIsTheSecondToLast) {
    // end - 2 e' o ultimo endereco que o motor le de facto: ainda ha uma amostra
    // seguinte para interpolar.
    const auto address = byteForPosition(positionForByte(region().end - 2, region()), region());

    EXPECT_EQ(address, region().end - 2);
}

TEST(PositionForByte, NoAddressInTheRegionEverReachesOne) {
    // varre a regiao inteira. um unico 1.0 seria o mesmo defeito mascarado pelo
    // alinhamento.
    for (std::uint64_t i = 0; i < kLength; ++i) {
        const auto position = positionForByte(kStart + i, region());
        ASSERT_GE(position, 0.0) << "endereco " << i;
        ASSERT_LT(position, 1.0) << "endereco " << i;
    }
}

TEST(PositionForByte, AByteBeforeTheRegionIsClampedToItsStart) {
    EXPECT_DOUBLE_EQ(positionForByte(0, region()), 0.0);
    EXPECT_DOUBLE_EQ(positionForByte(kStart - 1, region()), 0.0);
}

TEST(PositionForByte, AByteAfterTheRegionIsClampedToItsEnd) {
    const auto expected = positionForByte(region().end - 2, region());

    EXPECT_DOUBLE_EQ(positionForByte(region().end, region()), expected);
    EXPECT_DOUBLE_EQ(positionForByte(region().end + 100000, region()), expected);
}

TEST(PositionForByte, ARegionTooShortToInterpolateReadsAtZero) {
    // uma regiao de uma amostra nao tem amostra seguinte, e nao ha fraccao que a
    // faca soar. zero, nao divisao por zero.
    const ByteRange single {kStart, kStart + 1};

    EXPECT_DOUBLE_EQ(positionForByte(kStart, single), 0.0);
    EXPECT_DOUBLE_EQ(positionForByte(kStart + 500, single), 0.0);
}

TEST(PositionForByte, AnEmptyRegionReadsAtZero) {
    const ByteRange empty {kStart, kStart};

    EXPECT_DOUBLE_EQ(positionForByte(kStart, empty), 0.0);
    EXPECT_DOUBLE_EQ(positionForByte(kStart + 10, empty), 0.0);
}

TEST(PositionForByte, PositionGrowsWithTheAddress) {
    double previous = -1.0;
    for (std::uint64_t i = 0; i < kLength; ++i) {
        const auto position = positionForByte(kStart + i, region());
        EXPECT_GE(position, previous) << "endereco " << i;
        previous = position;
    }
}

TEST(ByteForPosition, ZeroReadsTheFirstByteOfTheRegion) {
    EXPECT_EQ(byteForPosition(0.0, region()), kStart);
}

TEST(ByteForPosition, OneFallsBackToAnAddressThatStillHasASound) {
    // um POSITION posto a mao em 1.0, ou vindo da automacao do host, nao pode
    // cair no fim do material: o motor desligaria a voz.
    const auto address = byteForPosition(1.0, region());

    EXPECT_LT(address, region().end);
    EXPECT_GE(address, region().start);
    EXPECT_LT(positionForByte(address, region()), 1.0);
}

TEST(ByteForPosition, IsTheExactInverseOfPositionForByte) {
    // fecha exacto em todos os enderecos com leitura possivel. a posicao passa a
    // float e o resultado e' arredondado, o que centra o erro em zero; truncar,
    // como o motor faz, deslocava o cursor em 63 enderecos em mil.
    // o laco vai ate length - 2: o ultimo byte nao fecha, e fecha para o penultimo.
    for (std::uint64_t i = 0; i + 1 < kLength; ++i) {
        const auto address = kStart + i;
        EXPECT_EQ(byteForPosition(positionForByte(address, region()), region()), address)
            << "endereco " << i;
    }
}

TEST(ByteForPosition, TheVeryLastByteOfTheFileSharesItsPositionWithItsPredecessor) {
    // end - 1 nao tem posicao propria: a fraccao seria 1.0 e o motor desligaria a
    // voz. fica entao no mesmo sitio que end - 2. e' a unica perda do caminho
    // inverso, e esta declarada em vez de ser arredondamento casual.
    const auto secondToLast = positionForByte(region().end - 2, region());
    const auto last = positionForByte(region().end - 1, region());

    EXPECT_DOUBLE_EQ(last, secondToLast);
    EXPECT_EQ(byteForPosition(last, region()), region().end - 2);
}

TEST(ByteForPosition, StaysWithinHalfAByteOfTheIndexTheEngineTruncates) {
    // o motor trunca o indice para escolher a amostra. o endereco devolvido e' o
    // mais proximo desse indice, e nao o indice: e' a unica diferenca entre o
    // cursor e o som, e nunca passa de meio byte.
    for (std::uint64_t i = 0; i < kLength; i += 13) {
        const auto position = positionForByte(kStart + i, region());
        const auto engineIndex =
            static_cast<std::uint64_t>(static_cast<float>(position) * 999.0f);
        const auto marked = byteForPosition(position, region()) - kStart;

        ASSERT_LE(marked, engineIndex + 1) << "posicao " << position;
        ASSERT_GE(marked + 1, engineIndex) << "posicao " << position;
    }
}

TEST(ByteForPosition, AnEmptyOrShortRegionReturnsItsStart) {
    EXPECT_EQ(byteForPosition(0.5, ByteRange {kStart, kStart}), kStart);
    EXPECT_EQ(byteForPosition(0.5, ByteRange {kStart, kStart + 1}), kStart);
}

TEST(ByteForPosition, APositionFromTheHostIsClampedBeforeItBecomesAnAddress) {
    // o parametro vem de um float que o host pode empurrar para fora. negativo nao
    // pode transbordar o endereco para o fim do ficheiro.
    EXPECT_EQ(byteForPosition(-3.0, region()), kStart);
    EXPECT_LT(byteForPosition(7.5, region()), region().end);
}

namespace {

constexpr std::uint64_t kColumns {16};

using opcoda::pe::regionColumnsInRow;

} // namespace

TEST(RegionColumnsInRow, AWholeRowInsideTheRegionFillsEveryColumn) {
    const ByteRange region {0x200, 0x400};

    const auto span = regionColumnsInRow(0x300, kColumns, region);

    EXPECT_EQ(span.firstColumn, 0);
    EXPECT_EQ(span.lastColumn, kColumns);
    EXPECT_EQ(span.count(), kColumns);
}

TEST(RegionColumnsInRow, ARegionStartingMidRowStartsAtThatColumn) {
    // o caso que a captura de ecra nao mostra de forma confiavel: a regiao comeca
    // em 0x308, coluna 8 da linha 0x300.
    const ByteRange region {0x308, 0x408};

    const auto span = regionColumnsInRow(0x300, kColumns, region);

    EXPECT_EQ(span.firstColumn, 8);
    EXPECT_EQ(span.lastColumn, kColumns);
    EXPECT_EQ(span.count(), 8);
}

TEST(RegionColumnsInRow, ARegionEndingMidRowStopsAtThatColumn) {
    const ByteRange region {0x200, 0x304};

    const auto span = regionColumnsInRow(0x300, kColumns, region);

    EXPECT_EQ(span.firstColumn, 0);
    EXPECT_EQ(span.lastColumn, 4);
    EXPECT_EQ(span.count(), 4);
}

TEST(RegionColumnsInRow, ARegionEntirelyInsideOneRowCoversExactlyIt) {
    const ByteRange region {0x303, 0x309};

    const auto span = regionColumnsInRow(0x300, kColumns, region);

    EXPECT_EQ(span.firstColumn, 3);
    EXPECT_EQ(span.lastColumn, 9);
    EXPECT_EQ(span.count(), 6);
}

TEST(RegionColumnsInRow, ARowOutsideTheRegionIsEmpty) {
    const ByteRange region {0x400, 0x500};

    EXPECT_TRUE(regionColumnsInRow(0x300, kColumns, region).empty());
    EXPECT_TRUE(regionColumnsInRow(0x500, kColumns, region).empty());
}

TEST(RegionColumnsInRow, TheRowBeforeTheRegionIsEmptyAndTheFirstRowIsNot) {
    const ByteRange region {0x310, 0x400};

    EXPECT_TRUE(regionColumnsInRow(0x300, kColumns, region).empty());
    EXPECT_EQ(regionColumnsInRow(0x310, kColumns, region).firstColumn, 0);
}

TEST(RegionColumnsInRow, AnEmptyRegionHasNoColumnsAnywhere) {
    const ByteRange empty {0x300, 0x300};

    EXPECT_TRUE(regionColumnsInRow(0x300, kColumns, empty).empty());
}

TEST(RegionColumnsInRow, ARowOfZeroBytesHasNoColumns) {
    // grelha estreita ao ponto de a linha nao ter bytes nao pode devolver banda
    // de largura zero, que se desenhava como um risco.
    const ByteRange region {0x300, 0x400};

    EXPECT_TRUE(regionColumnsInRow(0x300, 0, region).empty());
}

TEST(RegionColumnsInRow, ARegionStartingAtTheVeryFirstByteCoversTheWholeRow) {
    const ByteRange region {0x300, 0x400};

    const auto span = regionColumnsInRow(0x300, kColumns, region);

    EXPECT_EQ(span.firstColumn, 0);
    EXPECT_EQ(span.lastColumn, kColumns);
}

TEST(RegionColumnsInRow, TheSpanNeverReachesOutsideTheRow) {
    // a banda e' desenhada com cellRect, que so conhece colunas de 0 a 15. uma
    // regiao maior que a linha tem de ser cortada em 16, senao o rectangulo sai da
    // grelha e pinta o gutter.
    const ByteRange region {0, 0x100000};

    const auto span = regionColumnsInRow(0x300, kColumns, region);

    EXPECT_EQ(span.lastColumn, kColumns);
    EXPECT_LE(span.lastColumn, kColumns);
    EXPECT_LE(span.firstColumn, kColumns);
}

TEST(RegionColumnsInRow, ARowNearTheEndOfTheAddressSpaceIsStillMeasured) {
    const auto near = std::numeric_limits<std::uint64_t>::max() - 0x40;
    const ByteRange region {near, near + 0x40};

    const auto span = regionColumnsInRow(near, kColumns, region);

    EXPECT_FALSE(span.empty());
    EXPECT_EQ(span.firstColumn, 0);
    EXPECT_EQ(span.lastColumn, kColumns);
}

TEST(RegionColumnsInRow, ARowWhoseEndWouldOverflowIsEmptyRatherThanWrapped) {
    // offset + tamanho e' a soma que a constitution manda conferir sem transbordo.
    // aqui transbordaria, e o resultado errado seria uma banda sobre a regiao em
    // vez de nenhuma.
    const auto near = std::numeric_limits<std::uint64_t>::max() - 4;
    const ByteRange region {0, near};

    EXPECT_TRUE(regionColumnsInRow(near, kColumns, region).empty());
}