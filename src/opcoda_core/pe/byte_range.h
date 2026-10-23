#pragma once

#include <cstddef>
#include <cstdint>

namespace opcoda::pe {

// regiao contigua de bytes, [start, end). e' um par de inteiros e nao um indice
// de secao: o seletor escolhe uma janela arbitraria e pode atravessar o limite
// entre duas secoes.
struct ByteRange {
    std::uint64_t start {0};
    std::uint64_t end {0};

    [[nodiscard]] std::uint64_t length() const noexcept {
        return end > start ? end - start : 0;
    }
    [[nodiscard]] bool empty() const noexcept { return length() == 0; }
};

// valida e corrige a janela contra o tamanho do ficheiro.
//
// false, sem mexer em nada, se o ficheiro esta' vazio, o fim vem antes do inicio
// ou o inicio ja' saiu do ficheiro. sem este recuo a janela ia para a fila e so
// falhava na thread de audio.
//
// o fim e' limitado ao tamanho em vez de rejeitado: "ate ao fim" e' pedido valido.
[[nodiscard]] bool clampByteRange(ByteRange& range, std::uint64_t fileSize) noexcept;

// inicio da secao mais proxima. secoes sem dados brutos, como .bss, sao saltadas:
// alinhar a uma delas daria janela vazia.
[[nodiscard]] std::uint64_t nearestSectionStart(std::uint64_t start,
                                                 const std::uint32_t* offsets,
                                                 const std::uint32_t* sizes,
                                                 std::size_t count) noexcept;

// true quando `range` coincide exactamente com uma secao com dados brutos.
[[nodiscard]] bool isSectionAligned(const ByteRange& range,
                                    const std::uint32_t* offsets,
                                    const std::uint32_t* sizes,
                                    std::size_t count) noexcept;

// regiao que sai de pedir o alinhamento a secao mais proxima.
//
// o alinhamento alterna: se ja' esta' alinhado, devolve `exact`. sem essa volta,
// pedir alinhamento seria operacao sem retorno.
//
// a escolha mora no nucleo porque e' regra sobre o PE e tem teste; o
// PluginProcessor so aplica o resultado.
[[nodiscard]] ByteRange sectionSnapToggle(const ByteRange& current,
                                          const ByteRange& exact,
                                          std::uint64_t fileSize,
                                          const std::uint32_t* offsets,
                                          const std::uint32_t* sizes,
                                          std::size_t count) noexcept;

} // namespace opcoda::pe
