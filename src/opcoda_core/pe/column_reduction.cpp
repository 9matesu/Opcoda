#include "opcoda_core/pe/column_reduction.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace opcoda::pe {
namespace {

// Byte para amostra, com a mesma conta que byte_to_sample.cpp.
//
// **Duplicada e nao partilhada, e e'  deliberacao.** A funcao de byte_to_sample
// aloca um vetor por chamada, e aqui sao 900 colunas por reconstrucao; um
// caminho sem alocacao tem de ter a aritmetica escrita a mao. A duplicacao esta'
// presa por um ensaio, ByteSampleMapping.IsTheSameAsByteToSample, que compara as
// duas contas byte a byte. Um comentario nao prende nada; um ensaio prende.
constexpr float byteToSample(std::uint8_t byte) noexcept {
    return (static_cast<float>(byte) - 127.5f) / 127.5f;
}

// Shannon de um histograma ja preenchido, com a mesma conta que
// entropy::shannonBitsPerByte: H = - soma de p(xi) log2 p(xi).
double bitsFromHistogram(const std::array<std::uint32_t, 256>& histogram,
                         std::uint64_t observations) noexcept {
    if (observations == 0) {
        return 0.0;
    }

    double sum = 0.0;
    for (const auto count : histogram) {
        if (count == 0) {
            continue;
        }
        const auto p = static_cast<double>(count) / static_cast<double>(observations);
        sum += p * std::log2(p);
    }
    return -sum;
}

// Tecto do passo, e o maior impar abaixo de 256.
//
// O tecto existe pela razao do impar: um passo maior do que a janela do
// histograma pode nao apanhar nenhum exemplar de um valor de byte, e a entropia
// passaria a medir o passo em vez dos dados. Abaixo de 256 e impar, a amostra
// percorre restos diferentes da divisao por 256.
constexpr std::uint64_t kMaxOddStride {255};

} // namespace

bool reduceToColumns(const std::uint8_t* data,
                     std::size_t size,
                     std::uint64_t start,
                     std::uint64_t end,
                     std::uint32_t columns,
                     std::vector<Column>& out) noexcept {
    if (data == nullptr || columns == 0) {
        return false;
    }

    // A soma e' conferida antes de qualquer leitura. `size` e' um size_t e
    // `start`/`end` sao uint64, entao a conversao para uint64 e' exacta em ambos
    // e a comparacao nao pode transbordar.
    if (end <= start) {
        return false;
    }
    if (start >= static_cast<std::uint64_t>(size)) {
        return false;
    }
    if (end > static_cast<std::uint64_t>(size)) {
        return false;
    }

    const auto length = static_cast<std::uint64_t>(end - start);

    // Uma coluna por byte, no maximo. Uma coluna sem byte nenhum nao tem minimo
    // nem maximo nem entropia, e seria um rectangulo vazio no ecra.
    const auto wanted = static_cast<std::uint64_t>(columns);
    const auto actual = static_cast<std::uint32_t>(std::min(wanted, length));
    if (actual == 0) {
        return false;
    }

    std::vector<Column> reduced;
    reduced.resize(actual);

    // A particao usa a divisao com resto, e nao uma divisao float: e' a unica forma
    // de as colunas tesselarem a regiao sem buracos nem sobreposicoes, e um
    // off-by-one aqui marcaria bytes a omissao sem erro visivel.
    const auto columnLength = length / actual;
    const auto remainder = length % actual;
    const auto sampleCap = static_cast<std::uint64_t>(kMaxSamplesPerColumn);

    std::uint64_t cursor = start;

    for (std::uint32_t column = 0; column < actual; ++column) {
        // As primeiras `remainder` colunas recebem um byte a mais. Distribuir pelo fim em
        // vez disso concentraria o desvio numa ponta da curva, e num ficheiro de 12 MB
        // isso sao tres mil bytes de erro so de um lado.
        const auto bytes = columnLength + (column < remainder ? 1u : 0u);

        auto& target = reduced[static_cast<std::size_t>(column)];
        target.byteCount = static_cast<std::uint32_t>(std::min<std::uint64_t>(bytes,
                                                                              0xFFFFFFFFull));

        const auto columnStart = cursor;
        const auto columnEnd = cursor + bytes;

        // **O passo tem de ser impar, e nao e' um detalhe.** O histograma tem 256
        // caixas, e um passo par partilha factores com 256: um passo de 256 numa
        // coluna alinhada caia sempre no mesmo resto da divisao por 256 e lia o
        // mesmo byte em todas as amostras. Numa regiao de bytes ascendentes dava
        // minimo igual a maximo igual a -1 e entropia zero numa coluna que devia
        // estar cheia — e o AddressSanitizer nao apanha nada, porque a leitura
        // estava dentro do buffer.
        //
        // Um passo impar e' primo com 256, entao `inicio + k*passo` percorre
        // restos diferentes da divisao por 256 conforme k cresce. Com colunas
        // pequenas o passo e' 1 e a reducao e' exacta; com colunas grandes o passo
        // e' impar e a amostra ja nao fica presa a um resto.
        auto stride = std::max<std::uint64_t>(1, bytes / sampleCap);
        stride = std::min<std::uint64_t>(stride, kMaxOddStride);
        if ((stride % 2) == 0) {
            ++stride;
        }

        auto minimum = std::numeric_limits<float>::max();
        auto maximum = std::numeric_limits<float>::lowest();
        double sumSquares = 0.0;
        std::array<std::uint32_t, 256> histogram {};
        std::uint64_t observed = 0;

        for (auto offset = columnStart; offset < columnEnd; offset += stride) {
            const auto byte = data[static_cast<std::size_t>(offset)];
            const auto value = byteToSample(byte);

            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
            sumSquares += static_cast<double>(value) * static_cast<double>(value);
            ++histogram[byte];
            ++observed;
        }

        if (observed == 0) {
            // Nao devia acontecer: bytes >= 1 e stride <= bytes. Se acontecer, a
            // particao esta' errada e uma coluna muda e' melhor do que NaN no ecra.
            target = Column {};
            target.byteCount = 0;
            cursor = columnEnd;
            continue;
        }

        target.minimum = minimum;
        target.maximum = maximum;
        target.rms = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(observed)));
        target.entropyBits = static_cast<float>(bitsFromHistogram(histogram, observed));

        cursor = columnEnd;
    }

    out = std::move(reduced);
    return true;
}

} // namespace opcoda::pe
