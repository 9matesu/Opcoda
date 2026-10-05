#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opcoda::pe {

double positionForByte(std::uint64_t byte, const ByteRange& region) noexcept {
    const auto length = region.length();

    // Uma regiao com menos de dois bytes nao produz audio: o motor precisa de
    // pelo menos uma amostra seguinte para interpolar, e o buffer de uma so
    // amostra nunca chega a encher esse par. Zero e' a resposta honesta, e nao
    // uma fracao inventada.
    if (length < 2) {
        return 0.0;
    }

    // end - 2 e' o endereco mais alto com leitura possivel. Acima dele a fracao
    // seria 1.0, e o motor desliga a voz nessa fracao.
    const auto highest = region.end - 2;
    const auto address = std::clamp<std::uint64_t>(byte, region.start, highest);

    return static_cast<double>(address - region.start) / static_cast<double>(length - 1);
}

std::uint64_t byteForPosition(double position, const ByteRange& region) noexcept {
    const auto length = region.length();
    if (length < 2) {
        return region.start;
    }

    // A posicao e' reduzida a float antes de multiplicar, porque e' assim que o
    // motor a recebe: em granular_engine.cpp ela vem de GranularParams, que e'
    // float. Refazer a conta em double daria um endereco que o motor nunca le.
    //
    // E o resultado e' arredondado, nao truncado. O motor trunca para escolher
    // a amostra, e truncar aqui punha o cursor sistematicamente um byte a
    // esquerda do que foi clicado: um teste apanhou 63 enderecos em mil com o
    // cursor deslocado. Arredondando o erro fica centrado em zero, o endereco
    // clicado volta exato, e o desvio do indice que o motor trunca e' de no
    // maximo meio byte.
    const auto enginePosition = static_cast<float>(std::clamp(position, 0.0, 1.0));
    const auto lastIndex = static_cast<double>(length - 1);
    const auto read = static_cast<std::uint64_t>(
        std::llround(static_cast<double>(enginePosition) * lastIndex));

    // O mesmo corte do outro lado. Uma fracao de 1.0 que chegue aqui, vinda de
    // um POSITION posto a mao ou de automacao, tem de dar um endereco que ainda
    // produz som e nao o fim do material.
    return region.start + std::min<std::uint64_t>(read, length - 2);
}

RowSpan regionColumnsInRow(std::uint64_t rowAddress, std::uint64_t rowBytes,
                           const ByteRange& region) noexcept {
    // Uma linha sem bytes, ou uma regiao vazia, nao tem colunas. Devolver
    // {0, 0} e' o que impede a banda de ser desenhada com largura zero ou
    // negativa.
    if (rowBytes == 0 || region.empty()) {
        return {};
    }

    // A soma transborda antes de a comparacao, e o fim da linha e' calculado a
    // partir de uma subtracao para nao passar por start + rowBytes.
    if (rowAddress > std::numeric_limits<std::uint64_t>::max() - rowBytes) {
        return {};
    }
    const auto rowEnd = rowAddress + rowBytes;

    if (rowEnd <= region.start || rowAddress >= region.end) {
        return {};
    }

    const auto first = (region.start > rowAddress) ? region.start - rowAddress : 0;
    const auto last = (region.end < rowEnd) ? region.end - rowAddress : rowBytes;

    // first e' no maximo rowBytes e last e' no minimo rowBytes, porque a linha
    // intersecta a regiao. Ainda assim, a subtraqua com tipos sem sinal e'
    // perigosa e o clamp abaixo torna o resultado honesto em vez de enorme.
    if (first >= rowBytes || last <= first) {
        return {};
    }
    return {std::min(first, rowBytes), std::min(last, rowBytes)};
}

} // namespace opcoda::pe