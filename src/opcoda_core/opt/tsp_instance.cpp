#include "tsp_instance.h"

#include <cstring>
#include <random>

namespace opcoda::opt {
namespace {

// distancia L1 entre dois histogramas de 256 posicoes, ja normalizados.
[[nodiscard]] Cost l1Distance(const double* a, const double* b) noexcept {
    Cost d = 0.0;
    for (int v = 0; v < 256; ++v) {
        const double diff = a[v] - b[v];
        d += diff > 0.0 ? diff : -diff;
    }
    return d;
}

} // namespace

bool buildInstanceFromBlocks(const std::uint8_t* data, std::size_t size,
                             std::uint64_t start, std::uint64_t end,
                             int blockCount, const std::string& id,
                             TspInstance& out, std::string* error) {
    out = TspInstance();

    if (data == nullptr) {
        return error ? (*error = "E_NODATA", false) : false;
    }
    if (start >= end) {
        return error ? (*error = "E_BAD_RANGE", false) : false;
    }
    if (end > static_cast<std::uint64_t>(size)) {
        return error ? (*error = "E_OOB", false) : false;
    }
    if (blockCount < 2 || blockCount > 1024) {
        return error ? (*error = "E_BLOCK_COUNT", false) : false;
    }
    const auto region = static_cast<std::size_t>(end - start);
    if (region < static_cast<std::size_t>(blockCount)) {
        return error ? (*error = "E_TOO_FEW_BYTES", false) : false;
    }

    const std::size_t base = region / static_cast<std::size_t>(blockCount);
    const std::size_t remainder = region % static_cast<std::size_t>(blockCount);

    // histograma normalizado de cada bloco. Os blocos ficam iguais em tamanho
    // salvo os primeiros `remainder`, que recebem um byte a mais do resto.
    const auto rows = static_cast<std::size_t>(blockCount);
    std::vector<double> histograms(rows * 256, 0.0);
    for (std::size_t i = 0; i < rows; ++i) {
        const std::size_t off = start + i * base;
        const std::size_t len = base + (i < remainder ? 1 : 0);
        for (std::size_t b = 0; b < len; ++b) {
            ++histograms[i * 256 + data[off + b]];
        }
        for (std::size_t v = 0; v < 256; ++v) {
            histograms[i * 256 + v] /= static_cast<double>(len);
        }
    }

    // d(i,j) = distancia L1 entre histogramas * 100. A simetria vem da L1 e a
    // diagonal sai zero por construcao.
    out.n = static_cast<int>(rows);
    out.cost.assign(rows * rows, 0.0);
    out.id = "FIXO:" + id;
    out.kind = TspInstance::Kind::kFixed;
    out.blockSize = base;

    for (std::size_t i = 0; i < rows; ++i) {
        for (std::size_t j = 0; j < rows; ++j) {
            out.cost[i * rows + j] =
                l1Distance(histograms.data() + i * 256, histograms.data() + j * 256) * 100.0;
        }
    }
    return true;
}

TspInstance randomSymmetricInstance(int n, std::uint64_t seed) {
    TspInstance out;
    out.n = n;
    out.cost.assign(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0.0);
    out.id = "ALEATORIO";
    out.kind = TspInstance::Kind::kRandom;
    out.seed = seed;

    // custos inteiros em [1, 200], simetricos, diagonal zero. mt19937_64 e'
    // definido pelo padrao: a mesma semente da a mesma matriz em qualquer
    // compilador.
    std::mt19937_64 engine(seed);
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const Cost c = static_cast<Cost>(engine() % 200u) + 1.0;
            out.cost[static_cast<std::size_t>(n) * i + j] = c;
            out.cost[static_cast<std::size_t>(n) * j + i] = c;
        }
    }
    return out;
}

} // namespace opcoda::opt
