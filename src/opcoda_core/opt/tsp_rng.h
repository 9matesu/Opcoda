#pragma once

// gerador de numeros pseudo-aleatorios deterministico para os experimentos.
// std::mt19937_64 e' definido bit a bit pelo padrao C++: a mesma semente
// produz a mesma sequencia em qualquer compilador, o que torna o experimento
// reproduzivel fora desta maquina. A semente acompanha o registro no CSV.

#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace opcoda::opt {

class TspRng {
public:
    explicit TspRng(std::uint64_t seed) : engine_(seed), seed_(seed) {}

    // inteiro uniforme em [lo, hi], inclusive nas duas pontas. O produto pelo
    // tamanho do intervalo herda o enviesamento do double, aceitavel aqui: o
    // sorteio precisa ser reproduzivel, nao perfeito.
    [[nodiscard]] int uniformInt(int lo, int hi) {
        if (hi <= lo) {
            return lo;
        }
        const int span = hi - lo + 1;
        int j = lo + static_cast<int>(uniformReal() * static_cast<double>(span));
        if (j > hi) {
            j = hi; // o arredondamento do double pode tocar a borda superior
        }
        return j;
    }

    // real uniforme em [0, 1).
    [[nodiscard]] double uniformReal() { return dist_(engine_); }

    // Fisher-Yates sobre uma copia do vetor — usado para percursos aleatorios.
    [[nodiscard]] std::vector<int> shuffle(std::vector<int> v) {
        const int n = static_cast<int>(v.size());
        for (int i = n - 1; i > 0; --i) {
            std::swap(v[static_cast<std::size_t>(i)],
                      v[static_cast<std::size_t>(uniformInt(0, i))]);
        }
        return v;
    }

    // semente da construcao; nao avanca o estado do gerador.
    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }

private:
    std::mt19937_64 engine_;
    std::uint64_t seed_;
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
};

} // namespace opcoda::opt
