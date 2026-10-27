// Executor de experimentos da otimizacao (spec 025). Alvo de console, sem JUCE:
// roda os tres metodos sobre instancias fixas (um binario no disco) e aleatorias
// (geradas a partir de semente), seguindo o protocolo do plan e gravando o CSV
// da monografia.
//
// Usos:
//   build-release\tools\experiment_runner --fixture <file> --out docs/otimizacao/resultados.csv
//   build-release\tools\experiment_runner --fixture <file> --random 8:20260101:16:20260102 --out ...
//   build-release\tools\experiment_runner --fixture <file> --summary --out ...

#include "opcoda_core/opt/tsp_types.h"
#include "opcoda_core/opt/tsp_instance.h"
#include "opcoda_core/opt/tsp_tour.h"
#include "opcoda_core/opt/tsp_solvers.h"
#include "opcoda_core/opt/tsp_bruteforce.h"
#include "opcoda_core/opt/experiment_log.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace opcoda::opt;
using clock = std::chrono::steady_clock;

static void usage() {
    std::cerr
        << "Uso: experiment_runner [opcoes]\n"
        << "  --fixture <file>     binario PE da instancia FIXA\n"
        << "  --random <seed:n>    instancia ALEATORIA de n nos, semente <seed>\n"
        << "                       pode repetir para varias aleatorias\n"
        << "  --methods <hc|hcr|sa>  separados por espaco (padrao: os tres)\n"
        << "  --reps <N>           repeticoes por metodo (padrao: 10)\n"
        << "  --tmax <N>           tentativas da subida com tentativas (padrao: 10)\n"
        << "  --hc_iter <N>        iteracoes da subida (padrao: 500)\n"
        << "  --sa_t0 <f>          temperatura inicial da tempera (padrao: 10)\n"
        << "  --sa_tmin <f>        temperatura final (padrao: 0.1)\n"
        << "  --sa_alpha <f>       fator redutor (padrao: 0.995)\n"
        << "  --sa_iter <N>        iteracoes maximas da tempera (padrao: 20000)\n"
        << "  --out <file>         arquivo CSV de saida\n"
        << "  --summary            mostra estatisticas no console\n"
        << "  --help\n";
}

struct Args {
    std::string fixture;
    std::vector<std::pair<std::uint64_t, int>> randoms;
    std::vector<std::string> methods;
    int reps {10};
    int tmax {10};
    int hc_iter {500};
    double sa_t0 {10.0};
    double sa_tmin {0.1};
    double sa_alpha {0.995};
    int sa_iter {20000};
    std::string out;
    bool summary {false};
};

static std::string makeId(const std::string& kind, const std::string& extra,
                          std::uint64_t seed, int n) {
    std::stringstream s;
    s << kind << ":" << extra << ":" << n << ":s" << seed;
    return s.str();
}

static double runMethod(const std::string& name, const TspInstance& inst,
                        const Args& a, int repetition, TspResult& out,
                        std::string* err) {
    auto start = clock::now();
    if (name == "HC") {
        hillClimbing(inst, {}, {a.hc_iter}, out, err);
    } else if (name == "HCR") {
        hillClimbingRestarts(inst, static_cast<std::uint64_t>(repetition),
                             {a.tmax, a.hc_iter}, out, err);
    } else if (name == "SA") {
        simulatedAnnealing(inst, static_cast<std::uint64_t>(repetition),
                           {a.sa_t0, a.sa_tmin, a.sa_alpha, a.sa_iter}, out, err);
    } else {
        return -1.0;
    }
    auto end = clock::now();
    const auto ms =
        static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                                .count()) /
        1000.0;
    return ms;
}

int main(int argc, char* argv[]) {
    Args a;
    a.methods = {"HC", "HCR", "SA"};

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            usage();
            return 0;
        }
        if (arg == "--fixture" && i + 1 < argc) {
            a.fixture = argv[++i];
        } else if (arg == "--random" && i + 1 < argc) {
            std::string rest = argv[++i];
            std::size_t sep1 = rest.find(':');
            std::size_t sep2 = rest.find(':', sep1 + 1);
            if (sep1 == std::string::npos || sep2 == std::string::npos) {
                std::cerr << " --random precisa de <seed:n>\n";
                return 2;
            }
            std::uint64_t seed;
            int n;
            try {
                seed = static_cast<std::uint64_t>(std::stoull(rest.substr(0, sep1)));
                n = std::stoi(rest.substr(sep2 + 1));
            } catch (...) {
                std::cerr << " --random precisa de <seed:n> validos\n";
                return 2;
            }
            if (n < 2 || n > 64) {
                std::cerr << " --random: n precisa estar entre 2 e 64\n";
                return 2;
            }
            a.randoms.emplace_back(seed, n);
        } else if (arg == "--methods" && i + 1 < argc) {
            std::string rest = argv[++i];
            std::stringstream ss(rest);
            std::string m;
            while (std::getline(ss, m, ' ')) {
                if (!m.empty()) {
                    a.methods.push_back(m);
                }
            }
        } else if (arg == "--reps" && i + 1 < argc) {
            try {
                a.reps = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << " --reps precisa de um inteiro\n";
                return 2;
            }
        } else if (arg == "--tmax" && i + 1 < argc) {
            try {
                a.tmax = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << " --tmax precisa de um inteiro\n";
                return 2;
            }
        } else if (arg == "--hc_iter" && i + 1 < argc) {
            try {
                a.hc_iter = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << " --hc_iter precisa de um inteiro\n";
                return 2;
            }
        } else if (arg == "--sa_t0" && i + 1 < argc) {
            try {
                a.sa_t0 = std::stod(argv[++i]);
            } catch (...) {
                std::cerr << " --sa_t0 precisa de um numero\n";
                return 2;
            }
        } else if (arg == "--sa_tmin" && i + 1 < argc) {
            try {
                a.sa_tmin = std::stod(argv[++i]);
            } catch (...) {
                std::cerr << " --sa_tmin precisa de um numero\n";
                return 2;
            }
        } else if (arg == "--sa_alpha" && i + 1 < argc) {
            try {
                a.sa_alpha = std::stod(argv[++i]);
            } catch (...) {
                std::cerr << " --sa_alpha precisa de um numero\n";
                return 2;
            }
        } else if (arg == "--sa_iter" && i + 1 < argc) {
            try {
                a.sa_iter = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << " --sa_iter precisa de um inteiro\n";
                return 2;
            }
        } else if (arg == "--out" && i + 1 < argc) {
            a.out = argv[++i];
        } else if (arg == "--summary") {
            a.summary = true;
        } else {
            std::cerr << " opcao desconhecida: " << arg << "\n";
            usage();
            return 2;
        }
    }

    if (a.out.empty()) {
        std::cerr << "--out e' obrigatorio\n";
        usage();
        return 2;
    }

    // instancias: fixa (se houver fixture) e aleatorias
    std::vector<std::pair<std::string, TspInstance>> instances;
    std::string err;
    if (!a.fixture.empty()) {
        std::ifstream f(a.fixture, std::ios::binary | std::ios::ate);
        if (!f) {
            std::cerr << " nao leu o fixture: " << a.fixture << "\n";
            return 3;
        }
        const auto size = static_cast<std::size_t>(f.tellg());
        std::vector<std::uint8_t> data(size);
        f.seekg(0);
        if (!f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size))) {
            std::cerr << " nao leu os bytes do fixture\n";
            return 3;
        }
        const std::uint64_t endOffset = static_cast<std::uint64_t>(
            static_cast<std::streamoff>(data.size()) & 0xFFFFFFFFFFFFFFFF);
        std::cerr << " nota: o executor usa a regiao completa do binario ("
                  << size << " bytes -> 16 blocos de "
                  << size / 16 << " bytes)\n";
        TspInstance inst;
        if (!buildInstanceFromBlocks(data.data(), size, 0, static_cast<std::uint64_t>(size),
                                     16, "material-fixo", inst, &err)) {
            std::cerr << " nao construiu instancia FIXA: " << err << "\n";
            return 3;
        }
        instances.emplace_back(inst.id, inst);
    }
    for (auto [seed, n] : a.randoms) {
        instances.emplace_back(makeId("ALEATORIO", "rand", seed, n),
                               randomSymmetricInstance(n, seed));
    }
    if (instances.empty()) {
        std::cerr << " nenhuma instancia fornecida (--fixture e/ou --random)\n";
        usage();
        return 2;
    }

    std::vector<ExperimentRecord> records;
    for (auto& [id, inst] : instances) {
        const int n = inst.n;
        std::uint64_t seed = 0;
        if (inst.kind == TspInstance::Kind::kRandom) {
            seed = inst.seed;
        }
        std::string kind = (inst.kind == TspInstance::Kind::kRandom) ? "ALEATORIO" : "FIXO";

        for (const std::string& method : a.methods) {
            for (int rep = 1; rep <= a.reps; ++rep) {
                TspResult r;
                std::string e;
                const double ms = runMethod(method, inst, a, rep, r, &e);
                if (e.empty() && (ms < 0.0)) {
                    e = "Mtodo invalido";
                }
                if (!e.empty()) {
                    std::cerr << " " << id << " " << method << " rep " << rep << ": " << e
                              << "\n";
                    continue;
                }

                ExperimentRecord rec;
                rec.instanceId = id;
                rec.nodes = n;
                rec.instanceSeed = seed;
                rec.instanceKind = kind;
                rec.method = method;
                rec.parameters = (method == "HCR")
                                     ? ("tmax=" + std::to_string(a.tmax) +
                                        ";iter=" + std::to_string(a.hc_iter))
                                     : ("iter=" + std::to_string(a.hc_iter));
                if (method == "SA") {
                    std::stringstream s;
                    s << std::fixed << std::setprecision(4)
                      << "t0=" << a.sa_t0 << ";tmin=" << a.sa_tmin << ";alpha="
                      << a.sa_alpha << ";iter=" << a.sa_iter;
                    rec.parameters = s.str();
                }
                rec.initialCost = r.initialCost;
                rec.finalCost = r.finalCost;
                rec.iterations = r.iterations;
                rec.evaluations = r.evaluations;
                rec.attempts = r.attempts;
                rec.timeMs = ms;
                rec.seed = r.seed;
                rec.repetition = rep;
                records.push_back(rec);
            }
        }
    }

    if (!writeExperimentCsv(a.out, records, &err)) {
        std::cerr << " nao gravou o CSV: " << err << "\n";
        return 4;
    }
    std::cerr << " gravado " << a.out << " (" << records.size() << " registos)\n";

    if (!a.summary) {
        return 0;
    }

    // --- estatisticas por instância × metodo -------------------------------
    std::vector<std::string> ids;
    for (auto& [id, inst] : instances) {
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());

    // para instancias pequenas, custo exato por enumeracao (uma vez cada)
    std::map<std::string, double> exact;
    for (const auto& id : ids) {
        for (auto& [iid, inst] : instances) {
            if (iid == id && inst.n <= 10) {
                std::vector<int> t;
                Cost c = 0;
                if (bruteForceOptimal(inst, t, &c)) {
                    exact[id] = c;
                }
                break;
            }
        }
    }

    const auto statsFor = [&](const std::string& id, const std::string& method) {
        std::vector<double> costs;
        std::vector<double> times;
        long iterSum = 0, evalSum = 0;
        for (const auto& r : records) {
            if (r.instanceId == id && r.method == method) {
                costs.push_back(r.finalCost);
                times.push_back(r.timeMs);
                iterSum += r.iterations;
                evalSum += r.evaluations;
            }
        }
        return std::make_tuple(costs, times, iterSum, evalSum);
    };

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "=====================================================\n";
    std::cout << "RESUMO: " << records.size() << " registos, " << ids.size()
              << " instancias, " << a.methods.size() << " metodos\n";
    std::cout << "=====================================================\n\n";

    for (const auto& id : ids) {
        auto [costs, times, iterSum, evalSum] = statsFor(id, "SA");
        if (!costs.empty() && !exact.count(id)) {
            std::cout << "[" << id << "] (n=" << std::to_string(costs[0])
                      << "; sem referencia exata, n>10)\n";
        }
        // (o relatorio PDF faz os calculos completos; aqui so' um cabecalho)
    }
    return 0;
}
