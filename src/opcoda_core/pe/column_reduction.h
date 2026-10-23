#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opcoda::pe {

// reduz um intervalo de bytes a colunas, uma por pixel do display.
//
// min e max sao o envelope desenhado. rms e' o nucleo: sem ele, bytes aleatorios
// e bytes constantes parecem iguais. entropia em bits por byte e' a curva do OE7.
//
// fica no nucleo porque e' aritmetica com casos de bordo (comprimento que nao
// divide o numero de colunas), e dentro de um paint() nao ha onde testar.
struct Column {
    float minimum {0.0f};
    float maximum {0.0f};
    float rms {0.0f};
    float entropyBits {0.0f};

    // bytes que a coluna representa. 64 bits porque uma coluna pode cobrir mais
    // de 4 G num ficheiro enorme com poucas colunas. a soma das contagens da o
    // comprimento da regiao, e e' esse o invariante que o ensaio usa.
    std::uint64_t byteCount {0};

    [[nodiscard]] bool silent() const noexcept { return minimum >= 0.0f && maximum <= 0.0f; }
};

// tecto de amostras por coluna.
//
// sem ele, 12 MB viram 12 milhoes de operacoes por reconstrucao, e a reconstrucao
// acontece sempre que a regiao muda, ou seja a cada quadro de arrasto. com o
// tecto o custo nao depende do tamanho do ficheiro.
//
// o valor tem de ser maior que 256: a entropia de N bytes uniformes aproxima-se
// de log2(N), nao de 8. com 64 amostras a curva nunca passaria de 6 bits num
// material que vale 7. com 512, log2(512) = 9, acima do topo da escala.
//
// 512 e' o equilibrio: 900 colunas vezes 512 sao ~2 ms de reconstrucao, contra
// os 250 ms que 12 MB custariam sem tecto.
inline constexpr std::uint32_t kMaxSamplesPerColumn {512};

// reduz [start, end) a no maximo `columns` colunas.
//
// false sem mexer em `out` quando o intervalo nao e' usavel: data nula, vazio ou
// invertido, ou fim a sair do buffer. offset + tamanho em aritmetica que nao
// transborda e' o principio III da constitution.
//
// o numero de colunas pode sair menor que o pedido: com quatro bytes e novecentas
// colunas sao quatro colunas, nao novecentas de meio byte. nao ha coluna sem
// byte, e um resize que lance dentro de noexcept derruba o host.
// out.size() e' a verdade. aloca em `out`, na thread de interface.
[[nodiscard]] bool reduceToColumns(const std::uint8_t* data,
                                   std::size_t size,
                                   std::uint64_t start,
                                   std::uint64_t end,
                                   std::uint32_t columns,
                                   std::vector<Column>& out) noexcept;

} // namespace opcoda::pe