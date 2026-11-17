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
    EXPECT_NEAR(positionForByte(kStart + 499, region()), 499.0 / 999.0, 1e-12);
}

TEST(PositionForByte, TheLastByteNeverMapsToOne) {
    const auto last = positionForByte(region().end - 1, region());

    EXPECT_LT(last, 1.0);
    EXPECT_NEAR(last, 998.0 / 999.0, 1e-12);
}

TEST(PositionForByte, TheHighestAddressWithSoundIsTheSecondToLast) {
    const auto address = byteForPosition(positionForByte(region().end - 2, region()), region());

    EXPECT_EQ(address, region().end - 2);
}

TEST(PositionForByte, NoAddressInTheRegionEverReachesOne) {
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
    const auto address = byteForPosition(1.0, region());

    EXPECT_LT(address, region().end);
    EXPECT_GE(address, region().start);
    EXPECT_LT(positionForByte(address, region()), 1.0);
}

TEST(ByteForPosition, IsTheExactInverseOfPositionForByte) {
    for (std::uint64_t i = 0; i + 1 < kLength; ++i) {
        const auto address = kStart + i;
        EXPECT_EQ(byteForPosition(positionForByte(address, region()), region()), address)
            << "endereco " << i;
    }
}

TEST(ByteForPosition, TheVeryLastByteOfTheFileSharesItsPositionWithItsPredecessor) {
    const auto secondToLast = positionForByte(region().end - 2, region());
    const auto last = positionForByte(region().end - 1, region());

    EXPECT_DOUBLE_EQ(last, secondToLast);
    EXPECT_EQ(byteForPosition(last, region()), region().end - 2);
}

TEST(ByteForPosition, StaysWithinHalfAByteOfTheIndexTheEngineTruncates) {
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
    const auto near = std::numeric_limits<std::uint64_t>::max() - 4;
    const ByteRange region {0, near};

    EXPECT_TRUE(regionColumnsInRow(near, kColumns, region).empty());
}