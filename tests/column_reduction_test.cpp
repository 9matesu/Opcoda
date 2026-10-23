// reducao que alimenta a forma de onda e a curva de entropia.
// o que estes ensaios protegem e' a particao: uma coluna que salta um byte ou le
// o mesmo byte duas vezes da uma curva quase certa, errada num sitio so.
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

// mesma coisa com std::function, para padroes escritos em linha. um template
// evitava a conversao de lambda, mas aqui o objectivo e' medir custo.
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

// a conversao byte->amostra do desenho tem de bater com a do motor. este ensaio
// segura a duplicacao: column_reduction.cpp repete a conta porque toSamples aloca
// um vetor por chamada, e aqui sao 900 colunas por reconstrucao.
TEST(ColumnReduction, ByteSampleMappingIsTheSameAsByteToSample) {
    const auto bytes = pattern(4096, ascending);

    // `offset + 256 <= 4096` e' obrigatorio: toSamples nao verifica limites, quem
    // chama recorta-os. violar isto poe o ASan a apontar para a funcao errada.
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

// com colunas de um byte a reducao e' exacta. sem isto, um envelope errado
// passava despercebido.
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
    // byte 0x42 da -0,4961: a regiao tem som, so nao varia.
    // mede-se a altura do envelope, nao se a coluna e' muda. uma constante em -1
    // dava altura zero e nao silencio, e confundir os dois escondia o defeito.
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

// fr-018: as colunas tesselam a regiao. nem um byte saltado, nem um lido duas
// vezes.
TEST(ColumnReduction, PartialLastColumnPartitionsTheRegionExactly) {
    // 1000 e 256 nao se dividem: e' onde uma divisao float erraria o indice ao
    // longo da segunda metade.
    // nota: o nome antigo dizia "ReadsEveryByteExactlyOnce" e prometia demais. com
    // passo maior que 1 as colunas leem uma amostra, nao cada byte. o invariante
    // e' de particao, nao de leitura.
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

            // nenhuma coluna passa a regiao, nem excede as vizinhas em mais de
            // uma unidade de resto.
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
    // quatro bytes e novecentas colunas dao quatro colunas de um byte. coluna sem
    // byte nao tem envelope e seria um rectangulo vazio no ecra.
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

    // nenhuma destas chamadas pode ler fora do buffer. e' a disciplina do
    // principio III que o parser segue.
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
    // uma coluna, nao oito. com oito colunas cada uma recebe 32 valores e a
    // entropia e' log2(32) = 5. 8 so com a regiao inteira numa coluna: a primeira
    // versao pedia 5 e o ensaio e' que estava errado.
    const auto bytes = pattern(256, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 1, columns));
    ASSERT_EQ(columns.size(), 1u);
    EXPECT_EQ(columns[0].byteCount, 256u);
    EXPECT_NEAR(columns[0].entropyBits, 8.0f, 1e-4f);
}

TEST(ColumnReduction, EntropyScalesWithTheNumberOfDistinctValues) {
    // 32 valores distintos e uniformes por coluna dao log2(32) = 5 bits. fixa que
    // a escala e' a de Shannon e nao uma constante inventada.
    const auto bytes = pattern(256, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 8, columns));
    ASSERT_EQ(columns.size(), 8u);

    for (const auto& column : columns) {
        EXPECT_EQ(column.byteCount, 32u);
        EXPECT_NEAR(column.entropyBits, 5.0f, 1e-4f);
    }
}

// material de alta entropia e' o caso real: o texto de um .exe nao e' padrao
// repetido. por isso o ensaio do tecto de amostras usa isto e nao um padrao
// sintetico.
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
    // 4 MB em 256 colunas: 16 384 bytes por coluna contra um tecto de 512
    // amostras. o passo fica em 33, logo umas 497 leituras por coluna.
    const auto size = std::size_t {4} * 1024 * 1024;
    const auto bytes = pseudoRandom(size);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, size, 256, columns));
    ASSERT_EQ(columns.size(), 256u);

    for (const auto& column : columns) {
        // a contagem e' da coluna inteira, nao das amostras: e' o que diz ao
        // desenho que a coluna e' maior do que o ecra.
        EXPECT_EQ(column.byteCount, static_cast<std::uint32_t>(size / 256));

        // 7,4 e nao 7,9, por estatistica. a entropia de uma amostra de N bytes
        // uniformes fica abaixo de 8 por um vieso de ~255/(2*N*ln2). com N ~ 497
        // o vieso medido e' 0,51 e o valor ronda 7,48.
        // o que interessa e' ler alto, e nao 5,8 como com o tecto de 64 amostras.
        EXPECT_GT(column.entropyBits, 7.4f);
        EXPECT_LT(column.minimum, -0.9f);
        EXPECT_GT(column.maximum, 0.9f);
    }
}

// o passo tem de ser impar.
// com passo par que partilhe factores com 256 lia sempre o mesmo resto, e uma
// coluna alinhada de bytes ascendentes dava min = max = -1 e entropia zero.
// nenhum sanitizer apanha: a leitura estava dentro do buffer.
TEST(ColumnReduction, SamplingStepIsOddSoItCannotLockToOneByteValue) {
    // 4 MB em 256 colunas: 16 384 bytes por coluna. com o tecto de 512 o passo
    // e' 32, forcado a 33.
    const auto size = std::size_t {4} * 1024 * 1024;
    const auto bytes = pattern(size, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, size, 256, columns));
    ASSERT_EQ(columns.size(), 256u);

    for (const auto& column : columns) {
        EXPECT_EQ(column.byteCount, static_cast<std::uint64_t>(size / 256));

        // com passo par, `inicio + k*passo` cobre so 8 restos da divisao por 256 —
        // 8 restos com passo 32 — e a entropia caia para perto de 3.
        EXPECT_GT(column.entropyBits, 7.4f)
            << "o passo prendeu-se a um resto da divisao por 256";
        EXPECT_LT(column.minimum, -0.9f);
        EXPECT_GT(column.maximum, 0.9f);
    }
}

// o tecto de 512 amostras tem de valer tambem em colunas enormes. o defeito
// antigo limitava o passo a 255, e limitar o passo BAIXAVA-o, logo MULTIPLICAVA
// as amostras: acima de 130 560 bytes por coluna o custo crescia com o ficheiro.
TEST(ColumnReduction, SampleCountStaysCappedOnEnormousColumns) {
    // uma coluna com 268 MB: com o defeito seriam 1 052 689 amostras, 2 056
    // vezes o tecto.
    const auto bytes = std::vector<std::uint8_t>(std::size_t {1} << 22, 0x42);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 1, columns));
    ASSERT_EQ(columns.size(), 1u);
    EXPECT_EQ(columns[0].byteCount, bytes.size());

    // com regiao constante a entropia e' zero e o envelope e' o valor do byte. o
    // ensaio mede contagem de amostras, nao medida.
    for (const auto count : {std::uint32_t {1}, std::uint32_t {16},
                            static_cast<std::uint32_t>(bytes.size())}) {
        std::vector<Column> probe;
        const auto sub = static_cast<std::uint64_t>(count);
        ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, sub, 1, probe));
        EXPECT_EQ(probe.size(), 1u) << "count=" << count;
        EXPECT_EQ(probe[0].byteCount, sub) << "count=" << count;
    }
}

// o tecto de colunas existe pelo mesmo motivo que kMaxSections no parser: um
// `resize` que lance dentro de uma funcao noexcept chama std::terminate.
TEST(ColumnReduction, ColumnCountIsCappedRatherThanAllocated) {
    const auto bytes = pattern(4096, ascending);

    // 4 294 967 295 colunas para 4096 bytes. sem tecto alocava a conta completa
    // antes de aparar ao numero de bytes.
    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(),
                                0xFFFFFFFFu, columns));
    EXPECT_EQ(columns.size(), 4096u);
    EXPECT_EQ(totalBytes(columns), 4096u);
}

// `size == 0` com ponteiro nao nulo e `end == start` com valor nao nulo: casos
// que dependiam de guardas diferentes e a suite nao fixava.
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
    // ultima coluna de uma regiao que nao divide: o passo tem de sair do
    // comprimento DELA, e nao do comprimento medio, senao o ultimo grupo lia para
    // dentro da regiao seguinte.
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

// 64 bytes e' a menor regiao que o editor aceita, 1,3 ms. a reducao tem de
// correr nela sem NaN nem indice a mais.
TEST(ColumnReduction, SmallestRegionTheEditorAccepts) {
    const auto bytes = pattern(64, alternating);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, 64, 900, columns));
    EXPECT_EQ(columns.size(), 64u);
    EXPECT_EQ(totalBytes(columns), 64u);

    // 900 colunas pedidas e 64 bytes dao 64 colunas de UM byte: minimo = maximo,
    // o valor desse byte, e nao um envelope de dois bytes. a primeira versao pedia
    // -1 e +1 em todas e falhava, porque um byte so nao tem os dois.
    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto expected = (static_cast<float>(bytes[i]) - 127.5f) / 127.5f;
        EXPECT_NEAR(columns[i].minimum, expected, 1e-6f) << "coluna " << i;
        EXPECT_NEAR(columns[i].maximum, expected, 1e-6f) << "coluna " << i;
        EXPECT_NEAR(columns[i].rms, std::abs(expected), 1e-6f) << "coluna " << i;
        EXPECT_EQ(columns[i].byteCount, 1u) << "coluna " << i;
    }
}

// a regiao e' a unidade. uma curva de entropia que mede o ficheiro inteiro ao
// lado de uma regiao estreita mente sobre o que esta a soar.
TEST(ColumnReduction, OnlyTheRegionIsMeasured) {
    // ficheiro de 8192 em que so os ultimos 64 bytes sao a regiao. se a reducao
    // medisse o ficheiro, a primeira coluna vinha com entropia alta.
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
