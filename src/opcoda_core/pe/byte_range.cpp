#include "opcoda_core/pe/byte_range.h"

#include <limits>

namespace opcoda::pe {

bool clampByteRange(ByteRange& range, std::uint64_t fileSize) noexcept {
    if (fileSize == 0 || range.end <= range.start || range.start >= fileSize) {
        return false;
    }

    range.end = range.end > fileSize ? fileSize : range.end;
    return !range.empty();
}

std::uint64_t nearestSectionStart(std::uint64_t start,
                                  const std::uint32_t* offsets,
                                  const std::uint32_t* sizes,
                                  std::size_t count) noexcept {
    if (offsets == nullptr || sizes == nullptr) {
        return start;
    }

    // sem secao com dados, o melhor inicio e' o proprio `start`. o chamador
    // trata disso em vez de inventar um destino.
    std::uint64_t best = start;
    auto bestDistance = std::numeric_limits<std::uint64_t>::max();

    for (std::size_t i = 0; i < count; ++i) {
        if (sizes[i] == 0) {
            continue;
        }

        const auto candidate = static_cast<std::uint64_t>(offsets[i]);
        const auto distance = candidate > start ? candidate - start : start - candidate;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }

    return best;
}

bool isSectionAligned(const ByteRange& range,
                      const std::uint32_t* offsets,
                      const std::uint32_t* sizes,
                      std::size_t count) noexcept {
    if (offsets == nullptr || sizes == nullptr || range.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < count; ++i) {
        if (sizes[i] == 0) {
            continue;
        }

        const auto start = static_cast<std::uint64_t>(offsets[i]);
        if (range.start == start && range.end == start + static_cast<std::uint64_t>(sizes[i])) {
            return true;
        }
    }
    return false;
}

ByteRange sectionSnapToggle(const ByteRange& current,
                            const ByteRange& exact,
                            std::uint64_t fileSize,
                            const std::uint32_t* offsets,
                            const std::uint32_t* sizes,
                            std::size_t count) noexcept {
    // ja alinhado: volta a regiao exata. `exact` vazio significa que nunca
    // houve escolha exata, e nao ha' para onde voltar.
    if (isSectionAligned(current, offsets, sizes, count)) {
        return exact.empty() ? current : exact;
    }

    const auto start = nearestSectionStart(current.start, offsets, sizes, count);

    // nearestSectionStart devolveu o proprio inicio: nao ha secao com dados.
    // devolver a regiao atual e' mais honesto do que trocar o material do
    // utilizador, e alinhar ao fim do ficheiro seria pior.
    if (start == current.start) {
        return current;
    }

    // a janela alinhada vai ate ao fim da secao.
    auto end = fileSize;
    for (std::size_t i = 0; i < count; ++i) {
        if (sizes[i] == 0) {
            continue;
        }

        const auto candidate = static_cast<std::uint64_t>(offsets[i]);
        if (candidate == start) {
            end = candidate + static_cast<std::uint64_t>(sizes[i]);
            break;
        }
    }

    ByteRange aligned {start, end};
    if (!clampByteRange(aligned, fileSize)) {
        return current;
    }
    return aligned;
}

} // namespace opcoda::pe
