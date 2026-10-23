#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opcoda::pe {

double positionForByte(std::uint64_t byte, const ByteRange& region) noexcept {
    const auto length = region.length();

    // regiao com menos de dois bytes nao produz audio: o motor precisa da
    // amostra seguinte para interpolar. zero e' a resposta honesta.
    if (length < 2) {
        return 0.0;
    }

    // end - 2 e' o endereco mais alto com leitura possivel. acima dele a fracao
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

    // a posicao passa por float antes de multiplicar, porque e' assim que o motor
    // a recebe: GranularParams guarda float. refazer a conta em double daria um
    // endereco que o motor nunca le.
    //
    // e o resultado e' arredondado, nao truncado. o motor trunca para escolher a
    // amostra, entao truncar aqui punha o cursor um byte a esquerda do clique.
    // arredondando, o endereco clicado volta exato e o desvio fica em meio byte.
    const auto enginePosition = static_cast<float>(std::clamp(position, 0.0, 1.0));
    const auto lastIndex = static_cast<double>(length - 1);
    const auto read = static_cast<std::uint64_t>(
        std::llround(static_cast<double>(enginePosition) * lastIndex));

    // o mesmo corte do outro lado: posicao 1.0 vinda de automacao tem de dar um
    // endereco que ainda produz som.
    return region.start + std::min<std::uint64_t>(read, length - 2);
}

RowSpan regionColumnsInRow(std::uint64_t rowAddress, std::uint64_t rowBytes,
                           const ByteRange& region) noexcept {
    // linha sem bytes, ou regiao vazia, nao tem colunas. {0, 0} evita a banda
    // com largura zero ou negativa.
    if (rowBytes == 0 || region.empty()) {
        return {};
    }

    // o fim da linha vem de uma soma ja' conferida, para nao transbordar.
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
    // intersecta a regiao. a subtracao com tipos sem sinal e' perigosa, e o clamp
    // torna o resultado honesto em vez de enorme.
    if (first >= rowBytes || last <= first) {
        return {};
    }
    return {std::min(first, rowBytes), std::min(last, rowBytes)};
}

} // namespace opcoda::pe