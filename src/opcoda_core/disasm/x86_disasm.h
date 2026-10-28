#pragma once

// decodificador minimo de x86-64 para a coluna ASM da grelha de bytes.
//
// **o que ele promete e o que nao promete.** o nucleo inteiro, o controlo de
// fluxo e os movimentos SSE mais comuns saem com o mnemonico certo e o
// comprimento certo; AVX (VEX/EVEX) e os escapes 0F 38/0F 3A caem em "db" e a
// leitura volta a sincronizar no proximo opcode que reconhece. E' orientacao
// ao olho, nao desmontador de referencia.
//
// sem JUCE, sem excecao, sem alocacao: o texto sai num buffer fixo e o
// comprimento nunca passa de 15 nem le para alem do buffer. E' a regra do
// nucleo (constituto I) e e' o que deixa a coluna desenhar dentro do paint.

#include <array>
#include <cstddef>
#include <cstdint>

namespace opcoda::disasm {

// instrucao decodificada. length 0 significa "os dados acabaram a meio da
// instrucao": quem lista para a'i. hasTarget marca ramo relativo (call, jmp,
// jcc, loop): o deslocamento esta' em target, relativo ao fim da instrucao, e
// quem tem o endereco base e' que soma.
struct Decoded {
    std::uint32_t length {0};
    std::array<char, 80> text {};
    bool hasTarget {false};
    std::int64_t target {0};
};

[[nodiscard]] Decoded decode(const std::uint8_t* data, std::size_t size) noexcept;

} // namespace opcoda::disasm
