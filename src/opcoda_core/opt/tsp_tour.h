#pragma once

// percurso: permutacao dos nos, fechada em ciclo.
//
// O custo vale
//
//     C(pi) = soma d(pi_i, pi_((i+1) mod n))   para i = 0..n-1
//
// e a transicao final fecha o ciclo: do ultimo no volta ao primeiro.

#include "tsp_instance.h"
#include "tsp_rng.h"
#include "tsp_types.h"

#include <vector>

namespace opcoda::opt {

// um percurso valido e' uma permutacao de 0..n-1: cada no aparece exatamente
// uma vez, logo o ciclo visita todos os nos sem repetir.
[[nodiscard]] bool isPermutation(const std::vector<int>& tour, int n) noexcept;

// custo fechado do percurso, incluindo a transicao de volta ao primeiro no.
[[nodiscard]] Cost tourCost(const TspInstance& inst, const std::vector<int>& tour) noexcept;

// percurso "natural": 0, 1, 2, ..., n-1 (a ordem em que os blocos entram).
[[nodiscard]] std::vector<int> identityTour(int n);

// percurso aleatorio, sorteado por `rng`.
[[nodiscard]] std::vector<int> randomTour(int n, TspRng& rng);

} // namespace opcoda::opt
