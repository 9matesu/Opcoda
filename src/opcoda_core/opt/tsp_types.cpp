#include "tsp_types.h"

namespace opcoda::opt {
const char* toString(StopReason reason) noexcept {
    switch (reason) {
    case StopReason::kNoImprovingMove: return "Sem movimento melhor";
    case StopReason::kMaxIterations: return "Limite de iteracoes atingido";
    case StopReason::kTemperatureFloor: return "Temperatura minima alcancada";
    case StopReason::kAttemptsExhausted: return "Tentativas esgotadas";
    default: return "Desconhecido";
    }
}
} // namespace opcoda::opt
