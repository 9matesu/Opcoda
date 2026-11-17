#include <gtest/gtest.h>

#include "opcoda_core/pe/byte_to_sample.h"
#include "opcoda_core/pe/column_reduction.h"

#include <cmath>
#include <functional>
#include <numeric>
#include <vector>

using opcoda::pe::Column;
using opcoda::pe::reduceToColumns;

namespace {

// preenche um byte de cada vez, para o padrao ficar legivel.
std::vector<std::uint8_t> pattern(std::size_t size, std::uint8_t (*next)(std::size_t)) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = next(i);
    }
    return bytes;
}

std::vector<std::uint8_t> pattern(std::size_t size,
                                  const std::function<std::uint8_t(std::size_t)>& next) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = next(i);
    }
    return bytes;
}

std::uint8_t alternating(std::size_t index) {
    return (index % 2 == 0) ? 0x00 : 0xFF;
}

std::uint8_t constant(std::size_t) { return 0x42; }

std::uint8_t ascending(std::size_t index) {
    return static_cast<std::uint8_t>(index % 256);
}

std::uint64_t totalBytes(const std::vector<Column>& columns) {
    return std::accumulate(columns.begin(), columns.end(), std::uint64_t {0},
                           [](std::uint64_t sum, const Column& column) {
                               return sum + column.byteCount;
                           });
}

} // namespace

TEST(ColumnReduction, ByteSampleMappingIsTheSameAsByteToSample) {
    const auto bytes = pattern(4096, ascending);

    for (std::size_t offset = 0; offset + 256 <= bytes.size(); offset += 137) {
        const auto reference = opcoda::pe::toSamples(bytes.data(), offset, 256);

        std::vector<Column> columns;
        ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), offset,
                                   offset + reference.size(), 256, columns));
        ASSERT_EQ(columns.size(), 256u);

        for (std::size_t i = 0; i < columns.size(); ++i) {
            const auto expected = reference[i];
            EXPECT_NEAR(columns[i].minimum, expected, 1e-6f) << "coluna " << i;
            EXPECT_NEAR(columns[i].maximum, expected, 1e-6f) << "coluna " << i;
        }
    }
}

TEST(ColumnReduction, KnownPatternGivesExactEnvelope) {
    const auto bytes = pattern(256, alternating);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 8, columns));
    ASSERT_EQ(columns.size(), 8u);

    for (const auto& column : columns) {
        EXPECT_NEAR(column.minimum, -1.0f, 1e-6f);
        EXPECT_NEAR(column.maximum, 1.0f, 1e-6f);
    }
}

TEST(ColumnReduction, ConstantRegionCollapsesToAZeroHeightEnvelope) {
    const auto bytes = pattern(256, constant);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 4, columns));
    ASSERT_EQ(columns.size(), 4u);

    const auto expected = (0x42 - 127.5f) / 127.5f;
    EXPECT_LT(expected, 0.0f) << "o fixture tem de ficar abaixo do zero, senao nao prova nada";

    for (const auto& column : columns) {
        EXPECT_NEAR(column.minimum, expected, 1e-6f);
        EXPECT_NEAR(column.maximum, expected, 1e-6f);
        EXPECT_NEAR(column.maximum - column.minimum, 0.0f, 1e-6f);
        EXPECT_NEAR(column.rms, std::abs(expected), 1e-6f);
        EXPECT_FALSE(column.silent()) << "constante abaixo de zero nao e' silencio";
    }
}

TEST(ColumnReduction, ZeroCrossingRegionIsNotSilent) {
    // metade abaixo de 127.5, metade acima: o envelope tem de abranger zero.
    const auto bytes = pattern(64, [](std::size_t index) {
        return (index % 2 == 0) ? 0x00 : 0xFF;
    });

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 4, columns));
    for (const auto& column : columns) {
        EXPECT_FALSE(column.silent());
        EXPECT_LT(column.minimum, 0.0f);
        EXPECT_GT(column.maximum, 0.0f);
    }
}

TEST(ColumnReduction, PartialLastColumnPartitionsTheRegionExactly) {
    for (const auto length : {std::uint64_t {3}, std::uint64_t {7}, std::uint64_t {1000},
                              std::uint64_t {255}, std::uint64_t {65537}}) {
        for (const auto columns : {1u, 2u, 7u, 64u, 256u, 1000u, 4096u}) {
            const auto bytes = pattern(static_cast<std::size_t>(length), ascending);

            std::vector<Column> reduced;
            ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, length, columns, reduced));

            EXPECT_EQ(totalBytes(reduced), length)
                << "length=" << length << " columns=" << columns;
            for (const auto& column : reduced) {
                EXPECT_GT(column.byteCount, 0u) << "uma coluna sem byte e' um buraco na curva";
            }

            const auto longest = std::max_element(
                reduced.begin(), reduced.end(),
                [](const Column& a, const Column& b) { return a.byteCount < b.byteCount; });
            const auto shortest = std::min_element(
                reduced.begin(), reduced.end(),
                [](const Column& a, const Column& b) { return a.byteCount < b.byteCount; });
            ASSERT_LE(longest->byteCount - shortest->byteCount, 1u)
                << "length=" << length << " columns=" << columns;
        }
    }
}

TEST(ColumnReduction, ColumnsNeverExceedTheByteCount) {
    const auto bytes = pattern(4, alternating);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 900, columns));
    EXPECT_EQ(columns.size(), 4u);
    EXPECT_EQ(totalBytes(columns), 4u);
}

TEST(ColumnReduction, RejectsRangesOutsideTheBuffer) {
    const auto bytes = pattern(256, ascending);
    std::vector<Column> columns;
    const auto sentinel = std::vector<Column> {Column {}, Column {}};

    EXPECT_FALSE(reduceToColumns(nullptr, 256, 0, 256, 8, columns));
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 0, 0, 8, columns));       // vazia
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 100, 50, 8, columns));    // invertida
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 256, 512, 8, columns));   // comeca no fim
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 0, 257, 8, columns));     // passa o fim
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 0, 256, 0, columns));     // sem colunas

    // `out` fica intacto na recusa: quem chama decide pelo retorno, nao pelo vetor.
    columns = sentinel;
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 0, 257, 8, columns));
    EXPECT_EQ(columns.size(), sentinel.size());
}

TEST(ColumnReduction, OffsetsAndLengthsAtTheBufferEdges) {
    const auto bytes = pattern(1000, ascending);
    std::vector<Column> columns;

    // a regiao nao e' o ficheiro: comeca e acaba onde calhar.
    for (const auto start : {std::uint64_t {0}, std::uint64_t {1}, std::uint64_t {17},
                             std::uint64_t {743}}) {
        for (const auto length : {std::uint64_t {1}, std::uint64_t {2}, std::uint64_t {3},
                                  std::uint64_t {257}}) {
            if (start + length > bytes.size()) {
                continue;
            }
            ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), start, start + length, 16,
                                        columns));
            EXPECT_EQ(totalBytes(columns), length) << "start=" << start << " length=" << length;
        }
    }
}

// as duas pontas da escala, que fixam o eixo da curva.
TEST(ColumnReduction, EntropyOfConstantBytesIsZero) {
    const auto bytes = pattern(4096, constant);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 8, columns));
    for (const auto& column : columns) {
        EXPECT_NEAR(column.entropyBits, 0.0f, 1e-5f);
    }
}

TEST(ColumnReduction, EntropyOfOneColumnOverAllByteValuesIsEight) {
    const auto bytes = pattern(256, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 1, columns));
    ASSERT_EQ(columns.size(), 1u);
    EXPECT_EQ(columns[0].byteCount, 256u);
    EXPECT_NEAR(columns[0].entropyBits, 8.0f, 1e-4f);
}

TEST(ColumnReduction, EntropyScalesWithTheNumberOfDistinctValues) {
    const auto bytes = pattern(256, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 8, columns));
    ASSERT_EQ(columns.size(), 8u);

    for (const auto& column : columns) {
        EXPECT_EQ(column.byteCount, 32u);
        EXPECT_NEAR(column.entropyBits, 5.0f, 1e-4f);
    }
}

std::vector<std::uint8_t> pseudoRandom(std::size_t size) {
    std::vector<std::uint8_t> bytes(size);
    std::uint64_t state = 0x9E3779B97F4A7C15ull;
    for (std::size_t i = 0; i < size; ++i) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        bytes[i] = static_cast<std::uint8_t>((state * 0x2545F4914F6CDD1Dull) >> 33);
    }
    return bytes;
}

TEST(ColumnReduction, SamplingIsCappedButStillTracksRealMaterial) {
    const auto size = std::size_t {4} * 1024 * 1024;
    const auto bytes = pseudoRandom(size);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, size, 256, columns));
    ASSERT_EQ(columns.size(), 256u);

    for (const auto& column : columns) {
        EXPECT_EQ(column.byteCount, static_cast<std::uint32_t>(size / 256));

        EXPECT_GT(column.entropyBits, 7.4f);
        EXPECT_LT(column.minimum, -0.9f);
        EXPECT_GT(column.maximum, 0.9f);
    }
}

TEST(ColumnReduction, SamplingStepIsOddSoItCannotLockToOneByteValue) {
    const auto size = std::size_t {4} * 1024 * 1024;
    const auto bytes = pattern(size, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, size, 256, columns));
    ASSERT_EQ(columns.size(), 256u);

    for (const auto& column : columns) {
        EXPECT_EQ(column.byteCount, static_cast<std::uint64_t>(size / 256));

        EXPECT_GT(column.entropyBits, 7.4f)
            << "o passo prendeu-se a um resto da divisao por 256";
        EXPECT_LT(column.minimum, -0.9f);
        EXPECT_GT(column.maximum, 0.9f);
    }
}

TEST(ColumnReduction, SampleCountStaysCappedOnEnormousColumns) {
    const auto bytes = std::vector<std::uint8_t>(std::size_t {1} << 22, 0x42);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 1, columns));
    ASSERT_EQ(columns.size(), 1u);
    EXPECT_EQ(columns[0].byteCount, bytes.size());

    for (const auto count : {std::uint32_t {1}, std::uint32_t {16},
                            static_cast<std::uint32_t>(bytes.size())}) {
        std::vector<Column> probe;
        const auto sub = static_cast<std::uint64_t>(count);
        ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, sub, 1, probe));
        EXPECT_EQ(probe.size(), 1u) << "count=" << count;
        EXPECT_EQ(probe[0].byteCount, sub) << "count=" << count;
    }
}

TEST(ColumnReduction, ColumnCountIsCappedRatherThanAllocated) {
    const auto bytes = pattern(4096, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(),
                                0xFFFFFFFFu, columns));
    EXPECT_EQ(columns.size(), 4096u);
    EXPECT_EQ(totalBytes(columns), 4096u);
}

TEST(ColumnReduction, DegenerateBuffersAreRefused) {
    const std::uint8_t probe {0};
    std::vector<Column> columns;

    EXPECT_FALSE(reduceToColumns(&probe, 0, 0, 0, 8, columns));
    EXPECT_FALSE(reduceToColumns(&probe, 0, 0, 1, 8, columns));
    EXPECT_FALSE(reduceToColumns(&probe, 10, 5, 5, 8, columns));
    EXPECT_FALSE(reduceToColumns(&probe, 10, 10, 10, 8, columns));
    EXPECT_TRUE(columns.empty());
}


TEST(ColumnReduction, SamplingStepNeverReadsPastTheColumn) {
    const auto size = std::size_t {300000};
    const auto bytes = pattern(size, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, size, 7, columns));
    EXPECT_EQ(totalBytes(columns), static_cast<std::uint64_t>(size));

    for (const auto& column : columns) {
        EXPECT_GE(column.minimum, -1.0f);
        EXPECT_LE(column.maximum, 1.0f);
        EXPECT_TRUE(std::isfinite(column.rms));
        EXPECT_GE(column.entropyBits, 0.0f);
        EXPECT_LE(column.entropyBits, 8.0f);
    }
}

TEST(ColumnReduction, SmallestRegionTheEditorAccepts) {
    const auto bytes = pattern(64, alternating);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, 64, 900, columns));
    EXPECT_EQ(columns.size(), 64u);
    EXPECT_EQ(totalBytes(columns), 64u);

    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto expected = (static_cast<float>(bytes[i]) - 127.5f) / 127.5f;
        EXPECT_NEAR(columns[i].minimum, expected, 1e-6f) << "coluna " << i;
        EXPECT_NEAR(columns[i].maximum, expected, 1e-6f) << "coluna " << i;
        EXPECT_NEAR(columns[i].rms, std::abs(expected), 1e-6f) << "coluna " << i;
        EXPECT_EQ(columns[i].byteCount, 1u) << "coluna " << i;
    }
}

TEST(ColumnReduction, OnlyTheRegionIsMeasured) {
    auto bytes = pattern(8192, constant);
    for (std::size_t i = 8192 - 64; i < 8192; ++i) {
        bytes[i] = alternating(i);
    }

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 8192 - 64, 8192, 8, columns));
    ASSERT_EQ(columns.size(), 8u);

    for (const auto& column : columns) {
        EXPECT_GT(column.entropyBits, 0.9f)
            << "a regiao e' alternada e nao constante; se der zero, mediu o ficheiro";
    }
}
