// A reducao que alimenta a forma de onda e a curva de entropia.
//
// A propriedade que estes ensaios protectem nao e' o desenho: e' a particao. Uma
// coluna que salta um byte ou le o mesmo byte duas vezes produz uma curva que
// parece certa e so esta errada num sitio, e nenhum captura de ecra apanha isso.
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

// Preenche um byte de cada vez, para os padroes serem legiveis no ensaio.
std::vector<std::uint8_t> pattern(std::size_t size, std::uint8_t (*next)(std::size_t)) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = next(i);
    }
    return bytes;
}

// Mesma coisa com o parametro em std::function, para os ensaios que precisam de
// um padrao escrito em linha. Uma sobrecarga com template seria mais elegante e
// traria uma conversao de lambda para ponteiro a mais por chamada, num ficheiro
// cujo objectivo e' medir custo.
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

// A conversao byte->amostra do desenho tem de ser a mesma que a do motor.
//
// **Este ensaio e' o que segura a duplicacao.** column_reduction.cpp repete a
// conta de byte_to_sample.cpp porque a funcao do nucleo aloca um vetor por
// chamada e aqui sao 900 colunas por reconstrucao. Dois numeros que precisam de
// concordar e nao sao partilhados so concordam enquanto ninguem mexer em nenhum
// dos dois.
TEST(ColumnReduction, ByteSampleMappingIsTheSameAsByteToSample) {
    const auto bytes = pattern(4096, ascending);

    // `offset + 256 <= 4096` e' obrigatorio: toSamples nao verifica limites, e a
    // razao esta' escrita no cabecavel — quem chama tem de recortar os limites
    // contra um parse bem-sucedido. Esta e' a unica funcao do nucleo que aceita
    // ler fora do buffer por desenho, e um ensaio que a violasse levaria o
    // AddressSanitizer a apontar para a funcao errada.
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

// Com colunas de um byte a reducao e' exacta, e e' por isso que este ensaio tem
// valor: sem ele, qualquer valor errado no envelope passaria despercebido.
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
    // Byte 0x42 da -0,4961 e nao 0: a regiao tem som, so nao varia.
    //
    // **O que se verifica e' a altura do envelope, e nao que a coluna seja muda.**
    // Uma coluna constante na regiao de -1 daria min = max = -1, que e' um
    // envelope de altura zero mas NAO e' silencio. Colapsar os dois e' que seria o
    // defeito, e um ensaio que confundisse as duas coisas passaria com uma reducao
    // errada.
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
    // Metade abaixo de 127.5 e metade acima: o envelope tem de abranger zero.
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

// A propriedade que o FR-018 exige: as colunas tesselam a regiao. Nem um byte
// saltado, nem um byte lido duas vezes.
TEST(ColumnReduction, PartialLastColumnReadsEveryByteExactlyOnce) {
    // 1000 e 256 nao se dividem: e' o caso em que uma divisao float erraria o
    // indice ao longo de toda a segunda metade.
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

            // Nenhuma coluna pode ter mais bytes do que a regiao, e nenhuma pode
            // ter mais do que as vizinhas mais uma unidade de resto.
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
    // Quatro bytes e novecentas colunas dao quatro colunas de um byte, e nao
    // novecentas com seiscentas vazias: uma coluna sem byte nao tem envelope e
    // seria um rectangulo vazio no ecra.
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

    // Nenhuma destas chamadas pode ler fora do buffer. Esta e' a mesma disciplina
    // do principio III que o parser segue, e a razao de a funcao estar no nucleo.
    EXPECT_FALSE(reduceToColumns(nullptr, 256, 0, 256, 8, columns));
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 0, 0, 8, columns));       // vazia
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 100, 50, 8, columns));    // invertida
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 256, 512, 8, columns));   // comeca no fim
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 0, 257, 8, columns));     // passa o fim
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 0, 256, 0, columns));     // sem colunas

    // `out` fica intacto em todas as recusas: quem chama decide com o valor de
    // retorno e nao com o estado do vetor.
    columns = sentinel;
    EXPECT_FALSE(reduceToColumns(bytes.data(), 256, 0, 257, 8, columns));
    EXPECT_EQ(columns.size(), sentinel.size());
}

TEST(ColumnReduction, OffsetsAndLengthsAtTheBufferEdges) {
    const auto bytes = pattern(1000, ascending);
    std::vector<Column> columns;

    // A regiao nao e' o ficheiro: comeca e acaba em qualquer sitio.
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

// As duas pontas da escala, que sao o que fixa o eixo da curva.
TEST(ColumnReduction, EntropyOfConstantBytesIsZero) {
    const auto bytes = pattern(4096, constant);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 8, columns));
    for (const auto& column : columns) {
        EXPECT_NEAR(column.entropyBits, 0.0f, 1e-5f);
    }
}

TEST(ColumnReduction, EntropyOfOneColumnOverAllByteValuesIsEight) {
    // **Uma coluna, nao oito.** Com 256 bytes divididos em oito colunas, cada
    // coluna recebe 32 valores e a entropia e' log2(32) = 5, nao 8. A primeira
    // versao deste ensaio pedia 5 e foi o ensaio que estava errado, nao a conta:
    // 8 e' a entropia de 256 valores uniformes, e isso exige a regiao inteira
    // numa coluna so.
    const auto bytes = pattern(256, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 1, columns));
    ASSERT_EQ(columns.size(), 1u);
    EXPECT_EQ(columns[0].byteCount, 256u);
    EXPECT_NEAR(columns[0].entropyBits, 8.0f, 1e-4f);
}

TEST(ColumnReduction, EntropyScalesWithTheNumberOfDistinctValues) {
    // Cada coluna de 32 bytes de um padrao ascendente tem 32 valores distintos e
    // uniformes, e log2(32) = 5 bits. E' a mesma conta vista de outro lado, e
    // fixa que a escala e' mesmo a de Shannon e nao uma constante inventada.
    const auto bytes = pattern(256, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, bytes.size(), 8, columns));
    ASSERT_EQ(columns.size(), 8u);

    for (const auto& column : columns) {
        EXPECT_EQ(column.byteCount, 32u);
        EXPECT_NEAR(column.entropyBits, 5.0f, 1e-4f);
    }
}

// Material de alta entropia e' o caso real: o texto de um .exe nao e' padrao
// repetido. E' por isso que o ensaio do tecto de amostras o usa em vez de um
// padrao sintetico.
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
    // 4 MB com 256 colunas: 16 384 bytes por coluna, contra um tecto de 64
    // amostras. Sao 256 leituras por coluna em vez de 16 384.
    const auto size = std::size_t {4} * 1024 * 1024;
    const auto bytes = pseudoRandom(size);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, size, 256, columns));
    ASSERT_EQ(columns.size(), 256u);

    for (const auto& column : columns) {
        // A contagem e' a da coluna inteira, e nao a das amostras: e' o que
        // permite ao desenho saber que a coluna e' maior do que o ecra.
        EXPECT_EQ(column.byteCount, static_cast<std::uint32_t>(size / 256));

        // **7,4 e nao 7,9, e a razao e' estatistica e nao de implementacao.** A
        // entropia de uma amostra de N bytes uniformes de 256 valores fica abaixo
        // de 8 por um vieso de cerca de 255/(2*N*ln2). Com N = 16384/33 = 497
        // amostras o vieso medido e' de 0,51, e o valor fica perto de 7,48.
        //
        // A propriedade que interessa e' que material de alta entropia leia alto e
        // nao em 5,8 como lia com o tecto de 64 amostras. Esse numero e' o
        // limite log2(64), e foi assim que o tecto de 512 foi escolhido.
        EXPECT_GT(column.entropyBits, 7.4f);
        EXPECT_LT(column.minimum, -0.9f);
        EXPECT_GT(column.maximum, 0.9f);
    }
}

// O passo tem de ser impar, e este e' o ensaio dessa propriedade.
//
// Sem ela, um passo par que partilhe factores com 256 lia sempre o mesmo resto da
// divisao por 256, e uma coluna alinhada de bytes ascendentes dava min = max = -1
// com entropia zero. Nenhum sanitizer apanha isso: a leitura estava dentro do
// buffer e era correcta byte a byte.
TEST(ColumnReduction, SamplingStepIsOddSoItCannotLockToOneByteValue) {
    // 4 MB em 256 colunas: 16 384 bytes por coluna, passo de 33, ~497 amostras.
    // A coluna esta' alinhada, que e' a condicao em que um passo par se prende.
    const auto size = std::size_t {4} * 1024 * 1024;
    const auto bytes = pattern(size, ascending);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, size, 256, columns));
    ASSERT_EQ(columns.size(), 256u);

    for (const auto& column : columns) {
        EXPECT_EQ(column.byteCount, static_cast<std::uint32_t>(size / 256));

        // Com passo par, `inicio + k*passo` percorre um subconjunto de restos da
        // divisao por 256 — 8 restos com passo 32 — e a entropia lia perto de 3.
        // Com passo impar, leem-se quase todos os 256 valores.
        EXPECT_GT(column.entropyBits, 7.4f)
            << "o passo prendeu-se a um resto da divisao por 256";
        EXPECT_LT(column.minimum, -0.9f);
        EXPECT_GT(column.maximum, 0.9f);
    }
}


TEST(ColumnReduction, SamplingStepNeverReadsPastTheColumn) {
    // A ultima coluna de uma regiao que nao divide: o passo tem de ser calculado
    // com o comprimento DELA e nao com o comprimento medio, ou o ultimo grupo
    // lia para dentro da regiao seguinte.
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

// Uma regiao de 64 bytes e' a menor que o editor aceita, e 1,3 ms de duracao. A
// reducao tem de funcionar nela sem NaN nem indice a mais.
TEST(ColumnReduction, SmallestRegionTheEditorAccepts) {
    const auto bytes = pattern(64, alternating);

    std::vector<Column> columns;
    ASSERT_TRUE(reduceToColumns(bytes.data(), bytes.size(), 0, 64, 900, columns));
    EXPECT_EQ(columns.size(), 64u);
    EXPECT_EQ(totalBytes(columns), 64u);

    // Com 900 colunas pedidas e 64 bytes, saem 64 colunas de UM byte. Cada uma tem
    // logo minimo = maximo, que e' o valor desse byte — e nao um envelope de dois
    // bytes como numa regiao maior. A primeira versao deste ensaio pedia -1 e +1 em
    // todas as colunas e falhava porque um byte so nao tem os dois.
    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto expected = (static_cast<float>(bytes[i]) - 127.5f) / 127.5f;
        EXPECT_NEAR(columns[i].minimum, expected, 1e-6f) << "coluna " << i;
        EXPECT_NEAR(columns[i].maximum, expected, 1e-6f) << "coluna " << i;
        EXPECT_NEAR(columns[i].rms, std::abs(expected), 1e-6f) << "coluna " << i;
        EXPECT_EQ(columns[i].byteCount, 1u) << "coluna " << i;
    }
}

// A regiao tem de ser a unidade. Uma curva de entropia que mede o ficheiro inteiro
// ao lado de uma regiao estreita mente sobre o que esta a soar.
TEST(ColumnReduction, OnlyTheRegionIsMeasured) {
    // Um ficheiro de 8192 em que so os ultimos 64 bytes sao os da regiao. Se a
    // reducao medisse o ficheiro inteiro, a primeira coluna teria entropia alta.
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
