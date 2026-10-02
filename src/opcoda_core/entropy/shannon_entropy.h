#pragma once

#include <cstddef>
#include <cstdint>

namespace opcoda::entropy {

[[nodiscard]] double shannonBitsPerByte(const std::uint8_t* data, std::size_t size) noexcept;

[[nodiscard]] double shannonBitsPerByte(const std::uint8_t* data,
                                        std::size_t size,
                                        std::size_t offset) noexcept;

// Mede exatamente [offset, offset + length). A versao de tres argumentos vai
// ate o fim do arquivo, o que serve para a curva e nao serve para uma secao de
// PE: ali o fim e' rawOffset + rawSize, e o resto do arquivo contaminaria o
// resultado.
[[nodiscard]] double shannonBitsPerByte(const std::uint8_t* data,
                                        std::size_t size,
                                        std::size_t offset,
                                        std::size_t length) noexcept;

[[nodiscard]] std::size_t shannonCurve(const std::uint8_t* data,
                                       std::size_t size,
                                       std::size_t windowBytes,
                                       double* out,
                                       std::size_t outCapacity) noexcept;

} // namespace opcoda::entropy
