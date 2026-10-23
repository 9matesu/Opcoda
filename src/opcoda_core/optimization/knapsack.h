#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opcoda::optimization {

// Metodos de busca local portados do subprojeto C# (OpcodaOtimizacao).
//
// A mochila e' 0/1: cada bloco entra inteiro ou fica de fora. O valor de um
// bloco e' a entropia de Shannon multiplicada pela proporcao de bytes nao
// nulos; o peso e' o tamanho em KiB. A capacidade e' uma fracao do peso total.
enum class Method {
    hillClimb,           // subida de encosta a partir de uma solucao aleatoria
    multiRestart,        // hill climb repetido TMAX vezes, guarda o melhor
    simulatedAnnealing,  // tempera simulada com resfriamento geometrico
};

struct Block {
    std::uint64_t offset {};    // deslocamento absoluto no ficheiro
    std::uint64_t length {};    // bytes do bloco
    double value {};            // entropia * (1 - proporcao de zeros)
    double weightKb {};         // length / 1024
};

struct Problem {
    std::vector<Block> blocks;
    double capacityKb {};
    double totalWeightKb {};
    std::uint64_t start {};
    std::uint64_t end {};
    std::uint64_t blockBytes {};
    bool valid {false};
};

struct Params {
    int restarts {10};
    double initialTemperature {100.0};
    double finalTemperature {1.0};
    double coolingFactor {0.95};
    std::uint32_t seed {42};
    std::uint64_t maxIterations {200000};
};

struct Result {
    Method method {Method::hillClimb};
    std::vector<std::uint8_t> selected;  // um por bloco do problema
    double value {};
    double weightKb {};
    int count {};
    std::uint64_t iterations {};
    double elapsedMs {};
    bool valid {false};
};

// Monta a mochila a partir de uma regiao [start, end) de `data`.
//
// `maxBlocks` limita o numero de itens: se a regiao nao couber em
// `maxBlocks` blocos de `blockBytes`, o bloco efetivo cresce ate caber. Sem
// esse teto, uma regiao de 51 MB viraria 50 mil itens e a vizinhanca por troca
// deixaria de ser interativa.
[[nodiscard]] Problem buildProblem(const std::uint8_t* data, std::size_t size,
                                   std::uint64_t start, std::uint64_t end,
                                   std::uint64_t blockBytes = 1024,
                                   double capacityFraction = 0.5,
                                   std::size_t maxBlocks = 512);

// Corre o metodo escolhido. `problem` tem de vir de buildProblem.
[[nodiscard]] Result optimize(const Problem& problem, Method method,
                              const Params& params = {});

// Concatena, pela ordem dos blocos, os que a solucao marcou. Devolve vazio se
// a solucao nao pertencer ao problema ou se algum bloco sair do ficheiro.
[[nodiscard]] std::vector<std::uint8_t> concatenate(const std::uint8_t* data,
                                                    std::size_t size,
                                                    const Problem& problem,
                                                    const Result& result);

} // namespace opcoda::optimization
