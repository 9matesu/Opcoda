#pragma once

#include "opcoda_core/pe/pe_parser.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opcoda::pe {

// Coverte os bytes de uma regiao de secao em amostras normalizadas para [-1, 1].
// Byte 0 vira -1 e byte 255 vira +1, com 127.5 no zero.
//
// A faixa [offset, offset + count) precisa estar dentro do buffer: quem chama
// usa rawOffset e rawSize de um parse bem-sucedido, que ja recortou os limites
// contra o tamanho do arquivo. Nao ha verificacao aqui de proposito, para o
// caminho de audio nao pagar por um branch que o contrato ja garante.
//
// Fica no nucleo, e nao na interface nem no teste: e' a fronteira entre o
// arquivo binario e o motor granular, e o motor consome a saida daqui. O
// ensaio T1 usa a mesma funcao, o que impede as duas implementacoes de
// divergirem.
[[nodiscard]] std::vector<float> toSamples(const std::uint8_t* data,
                                           std::size_t offset,
                                           std::size_t count) noexcept;

} // namespace opcoda::pe
