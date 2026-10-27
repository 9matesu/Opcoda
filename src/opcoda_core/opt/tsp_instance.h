#pragma once

// representacao de uma instancia do PCV (caixeiro viajante) sobre blocos de
// bytes. O custo d(i,j) e' a distancia de Manhattan entre os histogramas de
// 256 valores de byte dos blocos, multiplicada por 100 (escala [0, 200]). A
// matriz e' simetrica por construcao e tem diagonal zero.
//
// Dois modos: FIXO, derivado de uma regiao de um arquivo binario real, e
// ALEATORIO, com matriz inteira sorteada a partir de uma semente registrada.

#include "tsp_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace opcoda::opt {

// instancia pronta para resolver. `at(i, j)` le a matriz linha a linha.
struct TspInstance {
    int n {0};                    // numero de nos (blocos)
    std::vector<Cost> cost;       // n x n, linha por linha
    std::string id;               // identificador reproduzivel ("FIXO:..." ou "ALEATORIO")
    std::uint64_t seed {0};       // semente da construcao (0 nas instancias fixas)
    enum class Kind { kFixed, kRandom } kind {Kind::kFixed};
    std::size_t blockSize {0};    // FIXO: bytes por bloco (salvo o excesso inicial)

    [[nodiscard]] Cost at(int i, int j) const noexcept {
        return cost[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) +
                    static_cast<std::size_t>(j)];
    }
};

// constroi uma instancia FIXA a partir da regiao [start, end) de `data`.
// A regiao e' dividida em `blockCount` blocos de tamanho igual; o resto da
// divisao vai para os primeiros blocos, que ficam com um byte a mais, e
// nenhum bloco fica vazio. Cada bloco vira um histograma de 256 contagens de
// byte, e d(i,j) = soma |p_i(v) - p_j(v)| * 100.
//
// Devolve false e preenche `*error` (se nao for nulo) com um codigo tipado:
// E_NODATA, E_BAD_RANGE, E_OOB, E_BLOCK_COUNT, E_TOO_FEW_BYTES.
[[nodiscard]] bool buildInstanceFromBlocks(const std::uint8_t* data,
                                           std::size_t size,
                                           std::uint64_t start,
                                           std::uint64_t end,
                                           int blockCount,
                                           const std::string& id,
                                           TspInstance& out,
                                           std::string* error);

// gera uma instancia ALEATORIA de n nos: matriz simetrica com custos inteiros
// em [1, 200] e diagonal zero, tirados de std::mt19937_64 semeado por `seed`
// (o gerador e' especificado bit a bit pelo padrao: a matriz reproduz em
// qualquer compilador). O identificador registra a semente.
[[nodiscard]] TspInstance randomSymmetricInstance(int n, std::uint64_t seed);

} // namespace opcoda::opt
