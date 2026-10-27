#pragma once

// os tres algoritmos da atividade. Cada um recebe uma semente (ou um percurso
// inicial, quando o caller quer controle total) e devolve um TspResult com o
// custo inicial, custo final, iteracoes, avaliacoes, tentativas e criterio de
// parada.
//
// Avaliacao da funcao objetivo = calculo do custo de uma solucao candidata,
// seja integral (tourCost) ou o delta exato de um movimento. Essa contagem e'
// escrita no CSV e no painel e permite comparar os metodos sem assumir que um
// "passo" de um algoritmo pesa o mesmo que um passo de outro.

#include "tsp_instance.h"
#include "tsp_neighborhood.h"
#include "tsp_rng.h"
#include "tsp_tour.h"
#include "tsp_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace opcoda::opt {

struct HillClimbParams {
    // maximo de passos aceites (movimentos aplicados). 0 = nenhum passo.
    int maxIterations {500};
};

struct RestartsParams {
    int tmax {10};                       // tentativas (reinicios)
    int maxIterationsPerRun {500};       // iteracoes por tentativa
};

struct AnnealingParams {
    double initialTemperature {10.0};    // T0
    double finalTemperature {0.1};       // Tmin
    double coolingFactor {0.995};        // T = alpha * T a cada iteracao
    int maxIterations {20000};           // iteracoes maximas
};

// subida de encosta de melhor-melhora: examina todos os movimentos e aplica o
// que reduz mais o custo. Parada: sem movimento melhor, ou limite de passos.
// O percurso inicial pode ser fornecido; se vier vazio, usa-se o natural.
[[nodiscard]] bool hillClimbing(const TspInstance& inst,
                                const std::vector<int>& initialTour,
                                const HillClimbParams& params,
                                TspResult& out,
                                std::string* error);

// subida de encosta com TMAX tentativas: roda hillClimbing a partir de
// percursos aleatorios distintos (sementes sequenciais seed+1..seed+TMAX) e
// devolve a melhor solucao encontrada. Iteracoes e avaliacoes sao somadas.
[[nodiscard]] bool hillClimbingRestarts(const TspInstance& inst,
                                        std::uint64_t seed,
                                        const RestartsParams& params,
                                        TspResult& out,
                                        std::string* error);

// tempera simulada. Comeca do percurso natural e aceita pioras com
// probabilidade exp(-delta/T); a cada iteracao T = alpha * T. O resultado
// reporta o melhor percurso de toda a execucao (best-of-run).
[[nodiscard]] bool simulatedAnnealing(const TspInstance& inst,
                                      std::uint64_t seed,
                                      const AnnealingParams& params,
                                      TspResult& out,
                                      std::string* error);

} // namespace opcoda::opt
