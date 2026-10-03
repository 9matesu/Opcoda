#pragma once

#include <cstddef>
#include <cstdint>

namespace opcoda::pe {

// Regiao contigua de bytes de um ficheiro, [start, end).
//
// E' um par de inteiros e nao um indice de secao de proposito: uma secao PE e'
// um intervalo roundado e alinhado, enquanto o seletor de bytes escolhe uma
// janela arbitraria, para poder atravessar o limite entre duas secoes.
struct ByteRange {
    std::uint64_t start {0};
    std::uint64_t end {0};

    [[nodiscard]] std::uint64_t length() const noexcept {
        return end > start ? end - start : 0;
    }
    [[nodiscard]] bool empty() const noexcept { return length() == 0; }
};

// Valida e corrige a janela contra o tamanho do ficheiro.
//
// Devolve false, sem mexer em nada, quando a janela nao pode ser usada: ficheiro
// vazio, fim antes do inicio, ou inicio ja a fora do ficheiro. Sem esse recuo a
// janela seria publicada na fila e so falharia depois, na thread de audio, onde
// nao ha' quem recue.
//
// O fim e' limitado ao tamanho do ficheiro em vez de ser rejeitado: um fim
// grande demais e' um pedido valido de "ate ao fim", e trata-lo como erro
// obrigaria o chamador a repetir a aritmetica que esta funcao ja' faz.
[[nodiscard]] bool clampByteRange(ByteRange& range, std::uint64_t fileSize) noexcept;

// Inicio da secao mais proxima de `start`, ignorando secoes sem dados brutos.
//
// Secoes sem dados brutos, como .bss, sao saltadas: alinhar a uma delas daria
// uma janela vazia, que e' o mesmo que nao ter janela.
[[nodiscard]] std::uint64_t nearestSectionStart(std::uint64_t start,
                                                 const std::uint32_t* offsets,
                                                 const std::uint32_t* sizes,
                                                 std::size_t count) noexcept;

// true quando `range` coincide exactamente com uma secao com dados brutos.
[[nodiscard]] bool isSectionAligned(const ByteRange& range,
                                    const std::uint32_t* offsets,
                                    const std::uint32_t* sizes,
                                    std::size_t count) noexcept;

// Regiao que resulta de pedir o alinhamento a secao mais proxima.
//
// **O alinhamento alterna.** Quando `current` ja esta' alinhado, devolve
// `exact`, que e' a regiao exata anterior. Sem esta inversao, pedir o
// alinhamento seria uma operacao sem volta: o utilizador chegava a uma secao
// e nao tinha como sair dela sem reconstruir a selecao byte a byte.
//
// A escolha mora no nucleo e nao no plugin pelo mesmo motivo do resto: e' uma
// regra sobre o PE, e o nucleo e' a parte que tem teste. O `PluginProcessor`
// limita-se a aplicar o resultado.
[[nodiscard]] ByteRange sectionSnapToggle(const ByteRange& current,
                                          const ByteRange& exact,
                                          std::uint64_t fileSize,
                                          const std::uint32_t* offsets,
                                          const std::uint32_t* sizes,
                                          std::size_t count) noexcept;

} // namespace opcoda::pe
