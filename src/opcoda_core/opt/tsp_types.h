#pragma once

// tipos comuns da secao de otimizacao (spec 025). O nucleo nao conhece JUCE e
// nao conhece interface: as funcoes recebem instancias e devolvem resultados
// por valor, sem excecao cruzando a fronteira.

#include <cstdint>
#include <vector>

namespace opcoda::opt {

// custo de uma transicao ou de um percurso. double porque a distancia entre
// histogramas normalizados e' fracionaria; as instancias aleatorias usam
// inteiros dentro dessa escala.
using Cost = double;

// por que o algoritmo parou. A interface mostra o texto de toString() sem
// traduzir de volta: o criterio de parada e' parte do resultado.
enum class StopReason {
    kNoImprovingMove,   // nenhum vizinho melhora o custo (subida de encosta)
    kMaxIterations,     // limite de iteracoes alcancado
    kTemperatureFloor,  // temperatura chegou ao minimo (tempera simulada)
    kAttemptsExhausted, // TMAX tentativas executadas (subida com tentativas)
};

[[nodiscard]] const char* toString(StopReason reason) noexcept;

// resultado de uma execucao. bestTour e' permutacao de 0..n-1; finalCost e' o
// melhor custo encontrado (na tempera, o melhor de sempre, nao o estado final).
struct TspResult {
    std::vector<int> bestTour;
    Cost initialCost {0.0};
    Cost finalCost {0.0};
    long iterations {0};
    long evaluations {0};
    int attempts {1};
    StopReason stop {StopReason::kMaxIterations};
    std::uint64_t seed {0};

    // igualdade completa, usada pelos testes para exigir determinismo.
    [[nodiscard]] bool operator==(const TspResult& other) const noexcept = default;
};

} // namespace opcoda::opt
