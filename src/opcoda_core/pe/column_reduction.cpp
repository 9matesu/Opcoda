#include "opcoda_core/pe/column_reduction.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace opcoda::pe {
namespace {

// byte para amostra, mesma conta de byte_to_sample.cpp.
//
// duplicada de proposito: aquela aloca um vetor por chamada e aqui sao 900
// colunas por reconstrucao. o ensaio ByteSampleMapping.IsTheSameAsByteToSample
// compara as duas contas byte a byte. comentario nao prende nada, ensaio prende.
constexpr float byteToSample(std::uint8_t byte) noexcept {
    return (static_cast<float>(byte) - 127.5f) / 127.5f;
}

// Shannon de um histograma cheio: H = -soma p log2 p, igual a
// entropy::shannonBitsPerByte.
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

// tecto de colunas. `columns` chega de fora e a funcao e' noexcept: um resize que
// lance bad_alloc chama std::terminate e derruba o host. recusa antes de alocar,
// como o parser faz com kMaxSections.
//
// 65536 e' muito acima do display: columnCountForWidth() vai ate 8192.
constexpr std::uint64_t kMaxColumns {65536};

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

    // a soma e' conferida antes de qualquer leitura. size_t e uint64 convertem
    // sem perda, entao a comparacao nao transborda.
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

    // uma coluna por byte, no maximo. coluna sem byte seria um rectangulo vazio.
    const auto wanted = static_cast<std::uint64_t>(columns);
    const auto capped = std::min(wanted, kMaxColumns);
    const auto actual = static_cast<std::uint32_t>(std::min(capped, length));
    if (actual == 0) {
        return false;
    }

    std::vector<Column> reduced;
    reduced.resize(actual);

    // divisao com resto, e nao float: e' a unica forma de as colunas tesselarem a
    // regiao sem buraco nem sobreposicao. um off-by-one marcaria bytes a omissao.
    const auto columnLength = length / actual;
    const auto remainder = length % actual;
    const auto sampleCap = static_cast<std::uint64_t>(kMaxSamplesPerColumn);

    std::uint64_t cursor = start;

    for (std::uint32_t column = 0; column < actual; ++column) {
        // as primeiras `remainder` colunas levam um byte a mais. distribuir pelo
        // fim concentraria o desvio numa ponta da curva.
        const auto bytes = columnLength + (column < remainder ? 1u : 0u);

        auto& target = reduced[static_cast<std::size_t>(column)];

        // oito bytes e nao quatro: uma coluna pode cobrir mais de 4 G, e um uint32
        // truncado em silencio quebrava a soma das contagens.
        target.byteCount = bytes;

        const auto columnStart = cursor;
        const auto columnEnd = cursor + bytes;

        // o passo e' levantado, nunca abaixado. o alvo e' amostras por coluna:
        // um passo fixo faria o custo crescer com o ficheiro.
        //
        // sobe forcado a impar. 256 = 2^8, logo gcd(impar, 256) = 1, e
        // inicio + k*passo percorre restos diferentes da divisao por 256. com
        // passo par, uma coluna de bytes ascendentes lia sempre o mesmo byte e
        // dava min = max = -1 com entropia zero.
        auto stride = std::max<std::uint64_t>(1, (bytes + sampleCap - 1) / sampleCap);
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
            // nao devia acontecer: bytes >= 1 e stride <= bytes. se acontecer, a
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
