#pragma once

// registro de um experimento e escrita CSV. Nao depende de JUCE, o que permite
// usar o mesmo formato na interface, no executor de console e nos testes.
//
// As colunas seguem o protocolo da spec 025 (14 no total):
//
// instance_id,n,instance_seed,instance_kind,method,parameters,initial_cost,
// final_cost,iterations,evaluations,attempts,time_ms,seed,repetition
//
// instance_seed e' a semente da instancia aleatoria (0 para instancias fixas).
// seed e' a semente do algoritmo, repetition o numero da repeticao (1..R) e
// attempts o TMAX quando aplicavel. Os campos nunca contem virgula, por isso
// nao ha quoting.

#include "tsp_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace opcoda::opt {

struct ExperimentRecord {
    std::string instanceId;
    int nodes {0};
    std::uint64_t instanceSeed {0};
    std::string instanceKind; // "FIXO" ou "ALEATORIO"
    std::string method;       // "HC", "HCR", "SA"
    std::string parameters;
    double initialCost {0.0};
    double finalCost {0.0};
    long iterations {0};
    long evaluations {0};
    int attempts {1};
    double timeMs {0.0};
    std::uint64_t seed {0};
    int repetition {0};
};

[[nodiscard]] std::string experimentCsvHeader() noexcept;

[[nodiscard]] std::string experimentCsvRow(const ExperimentRecord& r) noexcept;

// escreve `header + rows` em `path`. false em erro (diretorio, permissao, etc.),
// com mensagem em error.
[[nodiscard]] bool writeExperimentCsv(const std::string& path,
                                      const std::vector<ExperimentRecord>& records,
                                      std::string* error);

} // namespace opcoda::opt
