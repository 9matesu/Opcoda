#include "tsp_bruteforce.h"

#include "tsp_tour.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace opcoda::opt {

bool bruteForceOptimal(const TspInstance& inst, std::vector<int>& out,
                       Cost* optExactCost) noexcept {
    const int n = inst.n;
    if (n < 1 || n > 10) {
        return false;
    }
    if (n == 1) {
        out = identityTour(n);
        if (optExactCost != nullptr) {
            *optExactCost = tourCost(inst, out);
        }
        return true;
    }

    // fixa o no 0 e permuta os restantes. Enumerar os (n-1)! ciclos, espelhos
    // incluidos, continua correto; o limite de n <= 10 mantem o custo de
    // 9! = 362880 percursos dentro do ensaio.
    std::vector<int> rest(n - 1);
    for (int i = 0; i < n - 1; ++i) {
        rest[i] = i + 1;
    }

    std::vector<int> current(n);
    current[0] = 0;
    std::vector<int> bestTour;
    Cost best = std::numeric_limits<Cost>::infinity();

    do {
        for (int i = 0; i < n - 1; ++i) {
            current[static_cast<std::size_t>(i) + 1] = rest[static_cast<std::size_t>(i)];
        }
        const Cost c = tourCost(inst, current);
        if (c < best) {
            best = c;
            bestTour = current;
        }
    } while (std::next_permutation(rest.begin(), rest.end()));

    out = std::move(bestTour);
    if (optExactCost != nullptr) {
        *optExactCost = best;
    }
    return true;
}

} // namespace opcoda::opt
