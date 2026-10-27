#pragma once

// movimentos de vizinhanca para o PCV. Os dois movimentos manipulam posições
// de um percurso (permutacao) e nunca o invalidam:
//
// * troca: inverte dois nodos em duas posições dadas.
// * dois-opt: inverte a ordem de um segmento do percurso.
//
// moveDelta calcula a variacao do custo FECHADO com aritmetica O(1) sobre
// as arestas cortadas pelo movimento. O valor e' exacto e e' conferido em
// teste contra o recálculo integral.

#include "tsp_instance.h"
#include "tsp_types.h"

#include <cstdint>
#include <vector>

namespace opcoda::opt {

struct Move {
    enum class Kind { kSwap, kTwoOpt } kind {Kind::kSwap};
    // kSwap: posições i e k a trocar (i != k)
    // kTwoOpt: segmentos tour[i..k] a inverter (i < k)
    int i {0};
    int k {0};
};

// aplica o movimento no percurso.
void applyMove(std::vector<int>& tour, const Move& m) noexcept;

// variacao do custo fechado ao aplicar m, SEM alterar o percurso.
[[nodiscard]] Cost moveDelta(const TspInstance& inst, const std::vector<int>& tour,
                             const Move& m) noexcept;

// gera todos os movimentos validos de um percurso de n nos: trocas (i,k)
// com i < k e reversões (i,k) com 2 <= (k-i+1) <= n-1.
//
// A reversão de todo o ciclo (i=0, k=n-1) é identidade para uma métrica
// simétrica e e' omitida.
[[nodiscard]] std::vector<Move> allMoves(int n);

} // namespace opcoda::opt
