#pragma once

#include "opcoda_core/pe/byte_range.h"

#include <cstdint>

namespace opcoda::pe {

// traducao entre endereco de byte e fracao de leitura do motor.
//
// a fracao vem do parametro POSITION, float em [0, 1]. o motor le
// read = position * (sourceCount - 1), e a regiao de N bytes tem N amostras,
// logo a posicao do byte B dentro de [start, end) e' (B - start) / (length - 1).
//
// o ultimo byte nao tem posicao propria: com B = end - 1 a posicao e' 1.0 e a
// voz desliga logo a seguir. por isso o mapeamento e' meio aberto em cima e o
// endereco mais alto que produz som e' end - 2.
//
// mora no nucleo porque nao depende de JUCE e e' o que os sanitizers alcancam.
// duplicar a aritmetica na thread de interface seria uma versao sem teste.
[[nodiscard]] double positionForByte(std::uint64_t byte, const ByteRange& region) noexcept;

// caminho inverso, para o display saber que byte esta' sendo lido.
//
// a posicao passa por float antes de multiplicar, porque e' assim que o motor a
// recebe, e o resultado e' arredondado ao byte mais proximo em vez de truncado.
// truncar punha o cursor um byte a esquerda do clique, sempre.
//
// com regioes grandes a resolucao do float e' o limite real: 24 bits de mantissa
// distinguem cada byte ate 16 MB, acima disso o cursor salta varios. nao ha
// correcao sem mudar o tipo do parametro.
[[nodiscard]] std::uint64_t byteForPosition(double position, const ByteRange& region) noexcept;

// colunas de uma linha da grelha que caem dentro da regiao.
//
// a linha tem dezasseis bytes e a regiao pode comecar ou acabar a meio. o erro
// aqui e' um rectangulo deslocado: bytes fora da regiao marcados como dentro.
//
// mora no nucleo porque e' aritmetica com casos de bordo, e dentro de um paint()
// nao ha onde a testar.
struct RowSpan {
    std::uint64_t firstColumn {0};  // fechada
    std::uint64_t lastColumn {0};   // aberta
    [[nodiscard]] bool empty() const noexcept { return firstColumn >= lastColumn; }
    [[nodiscard]] std::uint64_t count() const noexcept { return empty() ? 0 : lastColumn - firstColumn; }
};

[[nodiscard]] RowSpan regionColumnsInRow(std::uint64_t rowAddress, std::uint64_t rowBytes,
                                         const ByteRange& region) noexcept;

} // namespace opcoda::pe