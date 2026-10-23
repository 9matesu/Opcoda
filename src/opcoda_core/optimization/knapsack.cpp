#include "opcoda_core/optimization/knapsack.h"

#include "opcoda_core/entropy/shannon_entropy.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <random>
#include <utility>

namespace opcoda::optimization {
namespace {

constexpr double kEps = 1e-9;

struct State {
    std::vector<std::uint8_t> selected;
    double value {};
    double weightKb {};
    int count {};
};

State evaluate(const Problem& problem, const std::vector<std::uint8_t>& selected) {
    State state;
    state.selected = selected;
    for (std::size_t i = 0; i < problem.blocks.size(); ++i) {
        if (selected[i] != 0) {
            state.value += problem.blocks[i].value;
            state.weightKb += problem.blocks[i].weightKb;
            ++state.count;
        }
    }
    return state;
}

// Repara uma solucao acima da capacidade tirando os itens de pior valor por
// unidade de peso. Itens de peso zero nunca sao tirados por esta ordenacao.
void repair(const Problem& problem, State& state) {
    if (state.weightKb <= problem.capacityKb + kEps) {
        return;
    }

    std::vector<std::size_t> chosen;
    chosen.reserve(static_cast<std::size_t>(state.count));
    for (std::size_t i = 0; i < state.selected.size(); ++i) {
        if (state.selected[i] != 0) {
            chosen.push_back(i);
        }
    }

    const auto ratio = [&problem](std::size_t i) {
        const auto weight = problem.blocks[i].weightKb;
        if (weight <= 0.0) {
            return std::numeric_limits<double>::infinity();
        }
        return problem.blocks[i].value / weight;
    };

    std::sort(chosen.begin(), chosen.end(), [&](std::size_t a, std::size_t b) {
        const auto ra = ratio(a);
        const auto rb = ratio(b);
        if (ra != rb) {
            return ra < rb;
        }
        return a < b;
    });

    for (const auto i : chosen) {
        if (state.weightKb <= problem.capacityKb + kEps) {
            break;
        }
        state.selected[i] = 0;
        state.value -= problem.blocks[i].value;
        state.weightKb -= problem.blocks[i].weightKb;
        --state.count;
    }

    if (state.weightKb > problem.capacityKb + kEps) {
        state.selected.assign(state.selected.size(), 0);
        state.value = 0.0;
        state.weightKb = 0.0;
        state.count = 0;
    }
}

State randomInitial(const Problem& problem, std::mt19937& rng) {
    std::uniform_int_distribution<int> coin(0, 1);
    std::vector<std::uint8_t> selected(problem.blocks.size(), 0);
    for (auto& bit : selected) {
        bit = static_cast<std::uint8_t>(coin(rng));
    }
    auto state = evaluate(problem, selected);
    repair(problem, state);
    return state;
}

bool better(const State& a, const State& b) {
    return a.value > b.value + kEps;
}

State hillClimb(const Problem& problem, std::mt19937& rng, std::uint64_t& iterations) {
    State current = randomInitial(problem, rng);
    const auto n = problem.blocks.size();

    // A vizinhanca e' avaliada por delta, e nao por copia: com 512 blocos, uma
    // copia por troca sao 262 mil vectores por iteracao. A copia so acontece
    // quando um vizinho melhora, e o repair fica reservado ao arranque aleatorio
    // e aos vizinhos da tempera.
    for (;;) {
        State best = current;

        for (std::size_t i = 0; i < n; ++i) {
            ++iterations;
            if (current.selected[i] != 0) {
                continue;
            }
            const auto& block = problem.blocks[i];
            const auto weight = current.weightKb + block.weightKb;
            if (weight > problem.capacityKb + kEps) {
                continue;
            }
            const auto value = current.value + block.value;
            if (value > best.value + kEps) {
                best = current;
                best.selected[i] = 1;
                best.value = value;
                best.weightKb = weight;
                best.count = current.count + 1;
            }
        }

        for (std::size_t i = 0; i < n; ++i) {
            ++iterations;
            if (current.selected[i] == 0) {
                continue;
            }
            const auto& block = problem.blocks[i];
            const auto value = current.value - block.value;
            if (value > best.value + kEps) {
                best = current;
                best.selected[i] = 0;
                best.value = value;
                best.weightKb = current.weightKb - block.weightKb;
                best.count = current.count - 1;
            }
        }

        for (std::size_t i = 0; i < n; ++i) {
            ++iterations;
            if (current.selected[i] == 0) {
                continue;
            }
            const auto& inside = problem.blocks[i];
            for (std::size_t j = 0; j < n; ++j) {
                ++iterations;
                if (current.selected[j] != 0) {
                    continue;
                }
                const auto& outside = problem.blocks[j];
                const auto weight = current.weightKb - inside.weightKb + outside.weightKb;
                if (weight > problem.capacityKb + kEps) {
                    continue;
                }
                const auto value = current.value - inside.value + outside.value;
                if (value > best.value + kEps) {
                    best = current;
                    best.selected[i] = 0;
                    best.selected[j] = 1;
                    best.value = value;
                    best.weightKb = weight;
                    best.count = current.count;
                }
            }
        }

        if (!better(best, current)) {
            break;
        }
        current = std::move(best);
    }

    return current;
}

State randomNeighbor(const Problem& problem, const State& current, std::mt19937& rng) {
    const auto n = problem.blocks.size();
    std::uniform_int_distribution<std::size_t> pick(0, n - 1);
    std::uniform_int_distribution<int> kind(0, 2);
    auto candidate = current.selected;
    const int k = kind(rng);

    if (k == 0) {
        for (std::size_t attempt = 0; attempt < n; ++attempt) {
            const auto i = pick(rng);
            if (candidate[i] == 0) {
                candidate[i] = 1;
                break;
            }
        }
    } else if (k == 1) {
        for (std::size_t attempt = 0; attempt < n; ++attempt) {
            const auto i = pick(rng);
            if (candidate[i] != 0) {
                candidate[i] = 0;
                break;
            }
        }
    } else {
        std::size_t inside = n;
        std::size_t outside = n;
        for (std::size_t attempt = 0; attempt < n; ++attempt) {
            const auto i = pick(rng);
            if (candidate[i] != 0) {
                inside = i;
                break;
            }
        }
        for (std::size_t attempt = 0; attempt < n; ++attempt) {
            const auto j = pick(rng);
            if (candidate[j] == 0) {
                outside = j;
                break;
            }
        }
        if (inside != n && outside != n) {
            candidate[inside] = 0;
            candidate[outside] = 1;
        }
    }

    auto state = evaluate(problem, candidate);
    repair(problem, state);
    return state;
}

State simulatedAnnealing(const Problem& problem, const Params& params,
                         std::mt19937& rng, std::uint64_t& iterations) {
    State current = randomInitial(problem, rng);
    State best = current;
    double temperature = params.initialTemperature;
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    while (temperature >= params.finalTemperature &&
           iterations < params.maxIterations) {
        auto candidate = randomNeighbor(problem, current, rng);
        const double delta = candidate.value - current.value;
        if (delta > 0.0 || unit(rng) < std::exp(delta / temperature)) {
            current = std::move(candidate);
            if (better(current, best)) {
                best = current;
            }
        }
        temperature *= params.coolingFactor;
        ++iterations;
        if (!(temperature > 0.0)) {
            break;
        }
    }

    return best;
}

Params sanitize(Params params) {
    if (params.restarts < 1) {
        params.restarts = 1;
    }
    if (params.restarts > 100) {
        params.restarts = 100;
    }
    if (!std::isfinite(params.initialTemperature) || params.initialTemperature <= 0.0) {
        params.initialTemperature = 100.0;
    }
    if (!std::isfinite(params.finalTemperature) || params.finalTemperature <= 0.0) {
        params.finalTemperature = 1.0;
    }
    if (params.finalTemperature > params.initialTemperature) {
        params.finalTemperature = params.initialTemperature;
    }
    if (!std::isfinite(params.coolingFactor) || params.coolingFactor <= 0.0 ||
        params.coolingFactor >= 1.0) {
        params.coolingFactor = 0.95;
    }
    if (params.maxIterations == 0) {
        params.maxIterations = 1;
    }
    if (params.maxIterations > 1000000) {
        params.maxIterations = 1000000;
    }
    return params;
}

} // namespace

Problem buildProblem(const std::uint8_t* data, std::size_t size,
                     std::uint64_t start, std::uint64_t end,
                     std::uint64_t blockBytes, double capacityFraction,
                     std::size_t maxBlocks) {
    Problem problem;
    problem.start = start;
    problem.end = end;
    problem.blockBytes = blockBytes;

    if (data == nullptr || size == 0 || start >= end || end > size ||
        blockBytes == 0 || maxBlocks == 0 || !std::isfinite(capacityFraction)) {
        return problem;
    }

    const auto region = end - start;
    auto effective = blockBytes;
    const auto maxBlocks64 = static_cast<std::uint64_t>(maxBlocks);
    const auto maxBytes = maxBlocks64 > std::numeric_limits<std::uint64_t>::max() / blockBytes
        ? std::numeric_limits<std::uint64_t>::max()
        : maxBlocks64 * blockBytes;
    if (region > maxBytes) {
        effective = region / maxBlocks64 + (region % maxBlocks64 != 0 ? 1 : 0);
    }
    if (effective == 0) {
        return problem;
    }

    problem.blockBytes = effective;
    const auto count = region / effective + (region % effective != 0 ? 1 : 0);
    problem.blocks.reserve(static_cast<std::size_t>(count));

    double totalWeight = 0.0;
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto offset = start + i * effective;
        const auto length = std::min(effective, end - offset);
        const auto* pointer = data + static_cast<std::size_t>(offset);
        const auto n = static_cast<std::size_t>(length);

        std::size_t zeros = 0;
        for (std::size_t b = 0; b < n; ++b) {
            if (pointer[b] == 0) {
                ++zeros;
            }
        }
        const auto zeroRatio = n == 0
            ? 0.0
            : static_cast<double>(zeros) / static_cast<double>(n);
        const auto entropy = entropy::shannonBitsPerByte(pointer, n);

        Block block;
        block.offset = offset;
        block.length = length;
        block.value = entropy * (1.0 - zeroRatio);
        block.weightKb = static_cast<double>(length) / 1024.0;
        totalWeight += block.weightKb;
        problem.blocks.push_back(block);
    }

    problem.totalWeightKb = totalWeight;
    problem.capacityKb = totalWeight * std::clamp(capacityFraction, 0.0, 1.0);
    problem.valid = true;
    return problem;
}

Result optimize(const Problem& problem, Method method, const Params& params) {
    Result result;
    result.method = method;
    result.valid = false;
    if (!problem.valid) {
        return result;
    }

    result.selected.assign(problem.blocks.size(), 0);
    result.valid = true;
    if (problem.blocks.empty()) {
        return result;
    }

    const auto clean = sanitize(params);
    std::mt19937 rng(clean.seed);
    std::uint64_t iterations = 0;
    const auto begin = std::chrono::steady_clock::now();

    State best;
    switch (method) {
        case Method::hillClimb:
            best = hillClimb(problem, rng, iterations);
            break;
        case Method::multiRestart:
            best = evaluate(problem, result.selected);
            for (int t = 0; t < clean.restarts; ++t) {
                auto state = hillClimb(problem, rng, iterations);
                if (better(state, best)) {
                    best = std::move(state);
                }
            }
            break;
        case Method::simulatedAnnealing:
            best = simulatedAnnealing(problem, clean, rng, iterations);
            break;
    }

    const auto finish = std::chrono::steady_clock::now();
    result.selected = std::move(best.selected);
    result.value = best.value;
    result.weightKb = best.weightKb;
    result.count = best.count;
    result.iterations = iterations;
    result.elapsedMs =
        std::chrono::duration<double, std::milli>(finish - begin).count();
    return result;
}

std::vector<std::uint8_t> concatenate(const std::uint8_t* data, std::size_t size,
                                      const Problem& problem, const Result& result) {
    std::vector<std::uint8_t> output;
    if (data == nullptr || !problem.valid || !result.valid ||
        result.selected.size() != problem.blocks.size()) {
        return output;
    }

    std::uint64_t total = 0;
    for (std::size_t i = 0; i < problem.blocks.size(); ++i) {
        if (result.selected[i] == 0) {
            continue;
        }
        const auto& block = problem.blocks[i];
        if (block.offset > size || block.length > size - block.offset ||
            block.length > std::numeric_limits<std::uint64_t>::max() - total) {
            return {};
        }
        total += block.length;
    }
    if (total == 0 || total > size) {
        return {};
    }

    output.reserve(static_cast<std::size_t>(total));
    for (std::size_t i = 0; i < problem.blocks.size(); ++i) {
        if (result.selected[i] == 0) {
            continue;
        }
        const auto& block = problem.blocks[i];
        const auto* pointer = data + static_cast<std::size_t>(block.offset);
        output.insert(output.end(), pointer,
                      pointer + static_cast<std::size_t>(block.length));
    }
    return output;
}

} // namespace opcoda::optimization
