#pragma once

// otimo exato por enumeracao. Adequado apenas a instancias pequenas (n <= 10:
// fixa-se o primeiro no e permutam-se os restantes, (n-1)! ciclos). Usado para
// validar os metodos heuristicos nos testes e para dar referencia exata ao
// resumo do executor.

#include "tsp_instance.h"
#include "tsp_types.h"

#include <vector>

namespace opcoda::opt {

// enumera todos os percursos que comecam em 0; devolve true quando n <= 10 e
// preenche `out` com o melhor percurso. O custo exato vai para `*optExactCost`
// quando o ponteiro nao for nulo.
[[nodiscard]] bool bruteForceOptimal(const TspInstance& inst,
                                     std::vector<int>& out,
                                     Cost* optExactCost = nullptr) noexcept;

} // namespace opcoda::opt
