#pragma once

#include <cstddef>
#include <cstdint>

namespace opcoda::entropy {

[[nodiscard]] double shannonBitsPerByte(const std::uint8_t* data, std::size_t size) noexcept;

[[nodiscard]] double shannonBitsPerByte(const std::uint8_t* data,
                                        std::size_t size,
                                        std::size_t offset) noexcept;

// mede exactamente [offset, offset + length). a versao de tres argumentos vai ate
// ao fim do ficheiro, o que serve para a curva; para uma secao de PE o fim e'
// rawOffset + rawSize, e o resto do ficheiro contaminaria o resultado.
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
