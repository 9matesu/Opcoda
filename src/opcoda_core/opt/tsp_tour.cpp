#include "tsp_tour.h"

namespace opcoda::opt {
namespace {

[[nodiscard]] bool hasValue(const std::vector<int>& tour, int value, int n) noexcept {
    if (value < 0 || value >= n) {
        return false;
    }
    for (int v : tour) {
        if (v == value) {
            return true;
        }
    }
    return false;
}

} // namespace

bool isPermutation(const std::vector<int>& tour, int n) noexcept {
    if (n < 0 || static_cast<int>(tour.size()) != n) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        if (!hasValue(tour, i, n)) {
            return false;
        }
    }
    return true;
}

Cost tourCost(const TspInstance& inst, const std::vector<int>& tour) noexcept {
    const int n = inst.n;
    if (n < 1 || static_cast<int>(tour.size()) < n) {
        return 0.0;
    }
    if (n == 1) {
        return inst.at(tour[0], tour[0]);
    }

    Cost total = 0.0;
    for (int i = 0; i < n; ++i) {
        total += inst.at(tour[i], tour[(i + 1) % n]);
    }
    return total;
}

std::vector<int> identityTour(int n) {
    std::vector<int> tour(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        tour[i] = i;
    }
    return tour;
}

std::vector<int> randomTour(int n, TspRng& rng) {
    return rng.shuffle(identityTour(n));
}

} // namespace opcoda::opt
