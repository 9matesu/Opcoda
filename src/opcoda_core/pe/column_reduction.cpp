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

// Tecto de colunas.
//
// Existe pelo mesmo motivo que kMaxSections no parser: `columns` chega de uma
// chamada e a funcao e' `noexcept`, e um `resize` que lance bad_alloc dentro de uma
// funcao noexcept chama std::terminate e derruba o host. O parser recusa acima do
// teto **antes** de alocar, e aqui e' igual.
//
// 65536 e' muito acima do que o display produz: columnCountForWidth() vai ate 8192
// colunas, e o zoom chega a multiplicar isso por 8.
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
    const auto capped = std::min(wanted, kMaxColumns);
    const auto actual = static_cast<std::uint32_t>(std::min(capped, length));
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

        // **Oito bytes, e nao quatro.** Uma coluna pode cobrir mais de 4 G num
        // ficheiro enorme com poucas colunas, e um uint32 truncado em silencio
        // quebrava o invariante de que a soma das contagens da o comprimento da
        // regiao — que e' o invariante que o ensaio usa para provar a particao.
        target.byteCount = bytes;

        const auto columnStart = cursor;
        const auto columnEnd = cursor + bytes;

        // **O passo e' levantado, nunca abaixado.**
        //
        // O objectivo e' um numero de amostras por coluna, e nao um numero de passo.
        // Um passo de 255 num tecto de 512 amostras daria 512 amostras de uma coluna
        // de 130 KB e 1 052 689 de uma coluna de 268 MB: o custo cresceria com o
        // ficheiro, que e' exactamente o que o tecto existe para impedir.
        //
        // O passo sobe, forcado a impar. Impar porque 256 = 2^8 e logo
        // gcd(impar, 256) = 1, entao `inicio + k*passo` percorre restos diferentes da
        // divisao por 256 conforme k cresce. Com passo par que partilhe factores com
        // 256, uma coluna alinhada de bytes ascendentes lia sempre o mesmo byte e dava
        // min = max = -1 com entropia zero, numa coluna cheia.
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
