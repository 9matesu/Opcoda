#include "tsp_solvers.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace opcoda::opt {

bool hillClimbing(const TspInstance& inst, const std::vector<int>& initialTour,
                  const HillClimbParams& params, TspResult& out, std::string* error) {
    out = TspResult();

    if (params.maxIterations < 0) {
        return error ? (*error = "E_INVALID_MAX_ITERATIONS", false) : false;
    }
    if (inst.n < 2) {
        return error ? (*error = "E_TOO_FEW_NODES", false) : false;
    }

    std::vector<int> cur;
    if (initialTour.empty()) {
        cur = identityTour(inst.n);
    } else {
        if (!isPermutation(initialTour, inst.n)) {
            return error ? (*error = "E_TOUR_NOT_VALID", false) : false;
        }
        cur = initialTour;
    }
    out.seed = 0; // deterministica: a subida nao sorteia nada

    out.initialCost = tourCost(inst, cur);
    out.finalCost = out.initialCost;
    out.evaluations = 1;
    out.attempts = 1;

    const std::vector<Move> moves = allMoves(inst.n);
    while (out.iterations < static_cast<long>(params.maxIterations)) {
        // melhor-melhora: procura o movimento de menor delta entre todos.
        Cost bestDelta = 0.0;
        Move bestMove;
        bool found = false;
        for (const Move& m : moves) {
            const Cost d = moveDelta(inst, cur, m);
            ++out.evaluations;
            if (!found || d < bestDelta) {
                bestDelta = d;
                bestMove = m;
                found = d < 0.0;
            }
        }
        if (!found) {
            out.stop = StopReason::kNoImprovingMove;
            break;
        }
        applyMove(cur, bestMove);
        ++out.iterations;
    }
    // o laco que termina por limite de passos deixa o criterio padrao,
    // kMaxIterations, que e' o valor inicial de TspResult::stop.

    out.bestTour = cur;
    out.finalCost = tourCost(inst, cur);
    return true;
}

bool hillClimbingRestarts(const TspInstance& inst, std::uint64_t seed,
                          const RestartsParams& params, TspResult& out,
                          std::string* error) {
    out = TspResult();

    if (params.tmax < 1 || params.maxIterationsPerRun < 0) {
        return error ? (*error = "E_INVALID_TMAX", false) : false;
    }
    if (inst.n < 2) {
        return error ? (*error = "E_TOO_FEW_NODES", false) : false;
    }

    Cost bestCost = std::numeric_limits<Cost>::infinity();
    std::vector<int> bestTour;
    Cost firstInitialCost = 0.0;
    long totalIterations = 0;
    long totalEvaluations = 0;

    for (int t = 0; t < params.tmax; ++t) {
        TspRng rng(seed + static_cast<std::uint64_t>(t) + 1u);
        const std::vector<int> init = randomTour(inst.n, rng);
        const Cost initCost = tourCost(inst, init);
        ++totalEvaluations;
        if (t == 0) {
            firstInitialCost = initCost;
        }

        TspResult run;
        if (!hillClimbing(inst, init, HillClimbParams {params.maxIterationsPerRun},
                          run, error)) {
            return false; // erro ja' preenchido pela subida
        }
        totalIterations += run.iterations;
        totalEvaluations += run.evaluations;

        if (run.finalCost < bestCost) {
            bestCost = run.finalCost;
            bestTour = run.bestTour;
        }
    }

    out.seed = seed;
    out.attempts = params.tmax;
    out.stop = StopReason::kAttemptsExhausted;
    out.initialCost = firstInitialCost;
    out.finalCost = bestCost;
    out.bestTour = std::move(bestTour);
    out.iterations = totalIterations;
    out.evaluations = totalEvaluations;
    return true;
}

bool simulatedAnnealing(const TspInstance& inst, std::uint64_t seed,
                        const AnnealingParams& params, TspResult& out,
                        std::string* error) {
    out = TspResult();

    if (params.initialTemperature < 0.0 || params.finalTemperature < 0.0) {
        return error ? (*error = "E_INVALID_TEMPERATURE", false) : false;
    }
    if (params.coolingFactor <= 0.0 || params.coolingFactor >= 1.0) {
        return error ? (*error = "E_INVALID_ALPHA", false) : false;
    }
    if (params.maxIterations < 0) {
        return error ? (*error = "E_INVALID_MAX_ITERATIONS", false) : false;
    }
    if (inst.n < 2) {
        return error ? (*error = "E_TOO_FEW_NODES", false) : false;
    }

    TspRng rng(seed);
    std::vector<int> cur = identityTour(inst.n);
    std::vector<int> bestTour = cur;
    Cost curCost = tourCost(inst, cur);
    Cost bestCost = curCost;
    out.initialCost = curCost;
    out.evaluations = 1;
    out.seed = seed;

    // Metropolis: piora de delta e' aceite com probabilidade exp(-delta/T).
    // O teto no expoente protege o double quando T e' minusculo; exp(-35) ~
    // 6e-16 ja' e' zero na pratica.
    const double maxLag = 35.0;

    Cost temperature = params.initialTemperature;
    while (out.iterations < static_cast<long>(params.maxIterations)) {
        if (temperature <= params.finalTemperature) {
            out.stop = StopReason::kTemperatureFloor;
            break;
        }

        Move m;
        if (inst.n == 2 || rng.uniformInt(0, 1) == 0) {
            // troca de duas posicoes distintas
            int i = rng.uniformInt(0, inst.n - 1);
            int k = rng.uniformInt(0, inst.n - 1);
            while (k == i) {
                k = rng.uniformInt(0, inst.n - 1);
            }
            m = Move {Move::Kind::kSwap, i, k};
        } else {
            // dois-opt: reversao de [i, k], comprimento >= 2. A reversao do
            // ciclo inteiro e' identidade e fica de fora do sorteio.
            int i = rng.uniformInt(0, inst.n - 2);
            int k = rng.uniformInt(i + 1, inst.n - 1);
            if (i == 0 && k == inst.n - 1) {
                k = rng.uniformInt(i + 1, inst.n - 2);
            }
            m = Move {Move::Kind::kTwoOpt, i, k};
        }

        const Cost delta = moveDelta(inst, cur, m);
        ++out.evaluations;
        ++out.iterations;

        bool accept = false;
        if (delta <= 0.0) {
            accept = true; // melhora ou movimento lateral
        } else {
            const double expo = -std::min(delta / temperature, maxLag);
            accept = rng.uniformReal() < std::exp(expo);
        }
        if (accept) {
            applyMove(cur, m);
            curCost += delta;
            if (curCost < bestCost) {
                bestCost = curCost;
                bestTour = cur;
            }
        }
        temperature *= params.coolingFactor;
    }
    // o laco que termina por limite de iteracoes deixa o criterio padrao,
    // kMaxIterations.

    out.bestTour = std::move(bestTour);
    out.finalCost = tourCost(inst, out.bestTour);
    return true;
}

} // namespace opcoda::opt
