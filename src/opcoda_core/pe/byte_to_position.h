#pragma once

#include "opcoda_core/pe/byte_range.h"

#include <cstdint>

namespace opcoda::pe {

// Traducao entre um endereco de byte e a fracao de leitura do motor granular.
//
// A fracao vem do parametro POSITION, um float em [0, 1]. O motor le
//     read = position * (sourceCount - 1)
// em dsp/granular_engine.cpp, e sourceCount e' o comprimento da regiao em
// amostras: pe::toSamples converte um byte numa amostra, entao uma regiao de N
// bytes tem N amostras. Portanto o endereco do byte B dentro de [start, end) e'
//     k = B - start
// e a posicao que o le e' k / (length - 1).
//
// **O ultimo byte nao tem posicao propria.** Com B = end - 1 vem k = length - 1,
// posicao 1.0, e o motor desliga a voz logo a seguir: em granular_engine.cpp a
// condicao e' `index + 1 >= sourceCount`, que com posicao 1.0 e' sempre
// verdadeira. Um clique no ultimo byte seria um clique em silencio. Por isso o
// mapeamento e' meio aberto em cima tambem, e o endereco mais alto que produz
// som e' end - 2.
//
// A regra vive no nucleo e nao no componente da grelha pelo mesmo motivo que a
// ByteRange: o nucleo nao depende de JUCE e e' o que os sanitizers e o
// llvm-cov alcanancam. Duplicar a aritmetica na thread de interface seria a
// forma de ter uma versao sem teste.
[[nodiscard]] double positionForByte(std::uint64_t byte, const ByteRange& region) noexcept;

// O caminho inverso, para o display saber que byte esta a ser lido.
//
// Nao e' a conta ao contrario de uma funcao qualquer. A posicao e' reduzida a
// float antes de multiplicar, porque e' assim que o motor a recebe, e o
// resultado e' arredondado ao byte mais proximo em vez de truncado. Assim o
// endereco que foi clicado volta exato, e o desvio para o indice que o motor
// trunca e' de no maximo meio byte. Truncar dava um cursor sistematicamente um
// byte a esquerda do clique.
//
// E' por isso que o endereco devolvido e' "o byte que se ouve" e nao "o indice
// exato do motor": sao a mesma coisa a menos de meio byte, e e' a segunda que
// o utilizador reconhece. Com regioes grandes a resolucao do parametro e' o
// limite real: um float de 24 bits de mantissa distingue cada byte ate uma
// regiao de 16 MB, e acima disso o cursor salta varios bytes de cada vez. Nao
// ha correcao possivel sem mudar o tipo do parametro.
[[nodiscard]] std::uint64_t byteForPosition(double position, const ByteRange& region) noexcept;

// Quais colunas de uma linha da grelha caem dentro da regiao.
//
// Uma linha tem dezasseis bytes e a regiao pode comecar ou acabar a meio dela.
// A banda desenhada pela grelha e' o rectangulo dessas colunas, e o erro aqui
// e' um rectangulo deslocado ou do tamanho errado: bytes que nao estao na
// regiao marcados como se estivessem.
//
// Vive no nucleo pelo mesmo motivo do resto: e' aritmetica com casos de bordo
// (regiao que comeca em 0x308, regiao que abrange muitas linhas, regiao vazia) e
// dentro de um paint() nao ha onde a testar.
struct RowSpan {
    std::uint64_t firstColumn {0};  // fechada
    std::uint64_t lastColumn {0};   // aberta
    [[nodiscard]] bool empty() const noexcept { return firstColumn >= lastColumn; }
    [[nodiscard]] std::uint64_t count() const noexcept { return empty() ? 0 : lastColumn - firstColumn; }
};

[[nodiscard]] RowSpan regionColumnsInRow(std::uint64_t rowAddress, std::uint64_t rowBytes,
                                         const ByteRange& region) noexcept;

} // namespace opcoda::pe