#pragma once

#include "opcoda_core/pe/pe_parser.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opcoda::pe {

// bytes de uma regiao de secao em amostras [-1, 1]: byte 0 = -1, 255 = +1,
// 127.5 no zero.
//
// [offset, offset + count) tem de estar dentro do buffer: quem chama passa
// rawOffset e rawSize de um parse bom, que ja' recortou contra o tamanho do
// ficheiro. sem verificacao aqui de proposito: o caminho de audio nao paga por
// um branch que o contrato garante.
//
// a fronteira entre o binario e o motor. o ensaio T1 usa a mesma funcao, e as
// duas implementacoes nao podem divergir.
[[nodiscard]] std::vector<float> toSamples(const std::uint8_t* data,
                                           std::size_t offset,
                                           std::size_t count) noexcept;

} // namespace opcoda::pe
