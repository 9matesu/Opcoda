#include "tsp_neighborhood.h"

#include <utility>

namespace opcoda::opt {

void applyMove(std::vector<int>& tour, const Move& m) noexcept {
    if (m.kind == Move::Kind::kSwap) {
        std::swap(tour[static_cast<std::size_t>(m.i)],
                  tour[static_cast<std::size_t>(m.k)]);
        return;
    }
    // dois-opt: inverte tour[m.i .. m.k], inclusive.
    auto a = tour.begin() + m.i;
    auto b = tour.begin() + m.k;
    while (a < b) {
        std::swap(*a, *b);
        ++a;
        --b;
    }
}

Cost moveDelta(const TspInstance& inst, const std::vector<int>& tour,
               const Move& m) noexcept {
    const int n = inst.n;
    if (n < 2) {
        return 0.0;
    }

    if (m.kind == Move::Kind::kTwoOpt) {
        if (m.i >= m.k) {
            return 0.0;
        }
        // inverter o ciclo inteiro (i = 0, k = n - 1) e' identidade numa
        // metrica simetrica: nenhuma aresta muda.
        if (m.i == 0 && m.k == n - 1) {
            return 0.0;
        }
        const int before = (m.i - 1 + n) % n;
        const int after = (m.k + 1) % n;
        const Cost removed =
            inst.at(tour[static_cast<std::size_t>(before)],
                    tour[static_cast<std::size_t>(m.i)]) +
            inst.at(tour[static_cast<std::size_t>(m.k)],
                    tour[static_cast<std::size_t>(after)]);
        const Cost added =
            inst.at(tour[static_cast<std::size_t>(before)],
                    tour[static_cast<std::size_t>(m.k)]) +
            inst.at(tour[static_cast<std::size_t>(m.i)],
                    tour[static_cast<std::size_t>(after)]);
        return added - removed;
    }

    // troca: o delta exato somando as arestas incidentes as posicoes i e k,
    // deduplicando a aresta entre elas quando sao vizinhas no ciclo (inclusive
    // no par 0 / n-1, que a formula ingenua contava tres vezes). O percurso
    // depois da troca nao precisa ser materializado: a posicao i passa a valer
    // tour[k] e a k passa a valer tour[i].
    const int p = m.i;
    const int q = m.k;
    if (p == q) {
        return 0.0;
    }
    const auto prev = [&](int pos) { return (pos - 1 + n) % n; };
    const auto next = [&](int pos) { return (pos + 1) % n; };

    std::pair<int, int> edges[4];
    int count = 0;
    const auto addEdge = [&](int a, int b) {
        if (a > b) {
            std::swap(a, b);
        }
        for (int e = 0; e < count; ++e) {
            if (edges[e].first == a && edges[e].second == b) {
                return;
            }
        }
        edges[count] = {a, b};
        ++count;
    };
    addEdge(prev(p), p);
    addEdge(p, next(p));
    addEdge(prev(q), q);
    addEdge(q, next(q));

    const auto valueAfter = [&](int pos) -> int {
        if (pos == p) {
            return tour[static_cast<std::size_t>(q)];
        }
        if (pos == q) {
            return tour[static_cast<std::size_t>(p)];
        }
        return tour[static_cast<std::size_t>(pos)];
    };

    Cost oldSum = 0.0;
    Cost newSum = 0.0;
    for (int e = 0; e < count; ++e) {
        const int a = edges[e].first;
        const int b = edges[e].second;
        oldSum += inst.at(tour[static_cast<std::size_t>(a)],
                          tour[static_cast<std::size_t>(b)]);
        newSum += inst.at(valueAfter(a), valueAfter(b));
    }
    return newSum - oldSum;
}

std::vector<Move> allMoves(int n) {
    std::vector<Move> moves;
    if (n < 2) {
        return moves;
    }
    moves.reserve(static_cast<std::size_t>(n) * (static_cast<std::size_t>(n) - 1));
    for (int i = 0; i < n; ++i) {
        for (int k = i + 1; k < n; ++k) {
            moves.push_back(Move {Move::Kind::kSwap, i, k});
            // a inversao do ciclo inteiro (i = 0, k = n - 1) e' identidade
            // para metrica simetrica e fica de fora.
            if (!(i == 0 && k == n - 1)) {
                moves.push_back(Move {Move::Kind::kTwoOpt, i, k});
            }
        }
    }
    return moves;
}

} // namespace opcoda::opt
