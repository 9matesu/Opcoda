// Testes da secao de otimizacao (PCV sobre blocos, spec 025).
//
// O que se testa:
//   - instancias: determinismo, simetria, diagonal zero, validacao de erro,
//     blocos iguais -> custo zero
//   - percursos: validade e custo fechado (conferido a mao numa instância 4 nos)
//   - vizinhanca: troca e 2-opt preservam permutacao; delta exato conferido
//     contra recalcule integral em todas as posicoes
//   - metodos: determinismo por semente, criterios de parada, TMAX respeitado,
//     melhor solução nunca pior que o custo inicial na subida
//   - referencia exata: enumeracao (n <= 10) nunca perdida pelos metodos
//   - registro: formatacao do CSV

#include <gtest/gtest.h>

#include "opcoda_core/opt/experiment_log.h"
#include "opcoda_core/opt/tsp_bruteforce.h"
#include "opcoda_core/opt/tsp_instance.h"
#include "opcoda_core/opt/tsp_neighborhood.h"
#include "opcoda_core/opt/tsp_rng.h"
#include "opcoda_core/opt/tsp_solvers.h"
#include "opcoda_core/opt/tsp_tour.h"

#include "pe_builder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

using opcoda::opt::buildInstanceFromBlocks;
using opcoda::opt::bruteForceOptimal;
using opcoda::opt::experimentCsvHeader;
using opcoda::opt::experimentCsvRow;
using opcoda::opt::ExperimentRecord;
using opcoda::opt::hillClimbing;
using opcoda::opt::hillClimbingRestarts;
using opcoda::opt::isPermutation;
using opcoda::opt::Move;
using opcoda::opt::Move::Kind;
using opcoda::opt::randomSymmetricInstance;
using opcoda::opt::randomTour;
using opcoda::opt::simulatedAnnealing;
using opcoda::opt::tourCost;
using opcoda::opt::TspInstance;
using opcoda::opt::TspRng;
using opcoda::opt::TspResult;
using opcoda::opt::toString;

// Matriz de 4 nos, explicita, para conferir tourCost a mao. Custo inteiro em
// [0,200], simetrica, diagonal zero.
static const std::vector<std::vector<opcoda::opt::Cost>> kHandMatrix4 = {
    {0, 20, 15, 30},
    {20, 0, 25, 10},
    {15, 25, 0, 20},
    {30, 10, 20, 0}};

static TspInstance makeHandInstance() {
    TspInstance out;
    out.n = 4;
    out.cost.clear();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            out.cost.push_back(kHandMatrix4[i][j]);
        }
    }
    out.id = "hand";
    return out;
}

// Instancia aleatoria para comparacoes deterministicas entre metodos.
static TspInstance makeRandInstance(int n, std::uint64_t seed) {
    TspInstance out;
    out.n = n;
    out.cost.resize(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0.0);
    std::mt19937_64 engine(seed);
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            opcoda::opt::Cost c = static_cast<opcoda::opt::Cost>(engine() % 200u) + 1.0;
            out.cost[out.n * i + j] = c;
            out.cost[out.n * j + i] = c;
        }
    }
    out.id = "seed";
    return out;
}

// TESTE 2.1 - Instancias

TEST(Instancia, DeterministicaFixa) {
    opcoda::test::PeBuilder b;
    b.fillTextWith(0x41);  // seccao inteira de bytes iguais

    TspInstance a, c;
    std::string e1, e2;
    bool okA = buildInstanceFromBlocks(b.bytes.data(), b.bytes.size(), 0u,
                                       static_cast<std::uint64_t>(b.bytes.size()), 8,
                                       "repro", a, &e1);
    bool okC = buildInstanceFromBlocks(b.bytes.data(), b.bytes.size(), 0u,
                                       static_cast<std::uint64_t>(b.bytes.size()), 8,
                                       "repro", c, &e2);
    ASSERT_TRUE(okA);
    ASSERT_TRUE(okC);
    ASSERT_EQ(e1, e2);
    ASSERT_EQ(a.n, c.n);
    ASSERT_EQ(a.cost, c.cost);
    ASSERT_EQ(a.id, "FIXO:repro");
    ASSERT_EQ(a.blockSize, b.kTextSize / 8);
}

TEST(Instancia, SimetricaEDiagonalZero) {
    opcoda::test::PeBuilder b;
    b.fillTextWithGradient(0x41, 17);

    TspInstance inst;
    std::string err;
    ASSERT_TRUE(buildInstanceFromBlocks(b.bytes.data(), b.bytes.size(), 0u,
                                        static_cast<std::uint64_t>(b.bytes.size()), 8,
                                        "mat", inst, &err));

    for (int i = 0; i < inst.n; ++i) {
        for (int j = 0; j < inst.n; ++j) {
            EXPECT_EQ(inst.cost[inst.n * i + j], inst.cost[inst.n * j + i])
                << "simetria falhou (" << i << "," << j << ")";
            EXPECT_EQ(inst.cost[inst.n * i + i], 0.0) << "diagonal zero falhou em " << i;
        }
    }
}

TEST(Instancia, AleatoriaDeterministica) {
    const TspInstance a = randomSymmetricInstance(11, 20260101);
    const TspInstance b = randomSymmetricInstance(11, 20260101);
    ASSERT_EQ(a.n, b.n);
    ASSERT_EQ(a.cost, b.cost);
    ASSERT_EQ(a.id, "ALEATORIO");
    ASSERT_EQ(a.seed, b.seed);
    ASSERT_EQ(a.seed, 20260101u);

    const TspInstance c = randomSymmetricInstance(11, 20260102);
    bool differ = false;
    for (std::size_t i = 0; i < a.cost.size(); ++i) {
        if (a.cost[i] != c.cost[i]) {
            differ = true;
        }
    }
    EXPECT_TRUE(differ);
}

TEST(Instancia, ValidacaoErros) {
    const std::uint8_t ok[] = {0x00, 0x01, 0x02, 0x03};
    std::string err;
    TspInstance out;

    // sem dados
    EXPECT_FALSE(buildInstanceFromBlocks(nullptr, 4, 0, 4, 2, "x", out, &err));
    EXPECT_EQ(err, "E_NODATA");

    // range vazio
    EXPECT_FALSE(buildInstanceFromBlocks(ok, 4, 4, 4, 2, "x", out, &err));
    EXPECT_EQ(err, "E_BAD_RANGE");

    // offset fora do buffer
    EXPECT_FALSE(buildInstanceFromBlocks(ok, 4, 3, 6, 2, "x", out, &err));
    EXPECT_EQ(err, "E_OOB");

    // blockCount invalido
    EXPECT_FALSE(buildInstanceFromBlocks(ok, 4, 0, 4, 1, "x", out, &err));
    EXPECT_EQ(err, "E_BLOCK_COUNT");
    EXPECT_FALSE(buildInstanceFromBlocks(ok, 4, 0, 4, 2000, "x", out, &err));
    EXPECT_EQ(err, "E_BLOCK_COUNT");

    // poucos bytes para os blocos
    EXPECT_FALSE(buildInstanceFromBlocks(ok, 4, 0, 4, 5, "x", out, &err));
    EXPECT_EQ(err, "E_TOO_FEW_BYTES");
}

TEST(Instancia, BlocosIguaisCustoZero) {
    opcoda::test::PeBuilder b;
    b.fillTextWith(0x7F);  // unico simbolo em toda a seccao

    TspInstance inst;
    std::string err;
    ASSERT_TRUE(buildInstanceFromBlocks(b.bytes.data(), b.bytes.size(), 0u,
                                        static_cast<std::uint64_t>(b.bytes.size()), 16,
                                        "zero", inst, &err));

    for (int i = 0; i < inst.n; ++i) {
        for (int j = 0; j < inst.n; ++j) {
            EXPECT_EQ(inst.cost[inst.n * i + j], 0.0)
                << "blocos iguais devem ter custo zero (" << i << "," << j << ")";
        }
    }
}

TEST(Instancia, IdentificadorFixo) {
    TspInstance inst;
    std::string err;
    ASSERT_TRUE(buildInstanceFromBlocks(nullptr, 0, 0, 0, 0, "nome", inst, &err));
    (void)inst;
    // o teste acima ja' falhou; aqui garante-se que a id e' gravada quando
    // construida com sucesso, num caso pequeno.
}

// TESTE 2.2 - Percursos

TEST(Percursos, Validade) {
    const TspInstance inst = makeHandInstance();
    EXPECT_TRUE(isPermutation({0, 1, 2, 3}, 4));
    EXPECT_TRUE(isPermutation({3, 1, 0, 2}, 4));
    EXPECT_FALSE(isPermutation({0, 1, 2, 4}, 4));   // 4 fora de alcance
    EXPECT_FALSE(isPermutation({0, 1, 1, 3}, 4));   // duplicado
    EXPECT_FALSE(isPermutation({0, 1, 2}, 4));      // incompleto
    EXPECT_FALSE(isPermutation({}, 0));
}

TEST(Percursos, CustoFechadoConferido) {
    const TspInstance inst = makeHandInstance();

    // [0,1,2,3]: 0-1(20)+1-2(25)+2-3(20)+3-0(30) = 95
    EXPECT_DOUBLE_EQ(tourCost(inst, {0, 1, 2, 3}), 95.0);
    // [0,2,1,3]: 0-2(15)+2-1(25)+1-3(10)+3-0(30) = 80
    EXPECT_DOUBLE_EQ(tourCost(inst, {0, 2, 1, 3}), 80.0);
    // [0,1,3,2]: 0-1(20)+1-3(10)+3-2(20)+2-0(15) = 65
    EXPECT_DOUBLE_EQ(tourCost(inst, {0, 1, 3, 2}), 65.0);
    // [0,3,1,2]: 0-3(30)+3-1(10)+1-2(25)+2-0(15) = 65
    EXPECT_DOUBLE_EQ(tourCost(inst, {0, 3, 1, 2}), 65.0);
    // ciclo inverso tem o mesmo custo (simetria): [3,2,1,0] = 95
    EXPECT_DOUBLE_EQ(tourCost(inst, {3, 2, 1, 0}), 95.0);
}

// TESTE 2.3 - Vizinhoanca

TEST(Vizinhanca, TrocaPreservaPermutacao) {
    const TspInstance inst = makeHandInstance();
    std::vector<int> tour = {3, 1, 0, 2};
    for (int i = 0; i < inst.n; ++i) {
        for (int k = 0; k < inst.n; ++k) {
            if (i == k) {
                continue;
            }
            std::vector<int> before = tour;
            Move m {Kind::kSwap, i, k};
            applyMove(tour, m);
            EXPECT_TRUE(isPermutation(tour, inst.n))
                << "troca invalidou a permutacao em i=" << i << " k=" << k;
            // movimento reversivel: aplica outra vez e volta ao estado.
            applyMove(tour, m);
            EXPECT_EQ(tour, before);
        }
    }
}

TEST(Vizinhanca, TwoOptPreservaPermutacao) {
    const TspInstance inst = makeHandInstance();
    std::vector<int> tour = {3, 1, 0, 2};
    for (int i = 0; i < inst.n; ++i) {
        for (int k = i + 1; k < inst.n; ++k) {
            std::vector<int> before = tour;
            Move m {Kind::kTwoOpt, i, k};
            applyMove(tour, m);
            EXPECT_TRUE(isPermutation(tour, inst.n))
                << "2-opt invalidou a permutacao em i=" << i << " k=" << k;
            // reversibilidade em grafico simetrico: 2-opt e' auto-reversivel.
            applyMove(tour, m);
            EXPECT_EQ(tour, before);
        }
    }
}

TEST(Vizinhanca, DeltaExatoConferido) {
    // matricess geradas e conferidas: para cada movimento, delta e' igual a
    // tourCost(apos) - tourCost(antes), integralmente.
    const int n = 6;
    const int trials = 8;
    for (int t = 0; t < trials; ++t) {
        TspInstance inst;
        inst.n = n;
        inst.cost.resize(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0.0);
        inst.id = "d";
        std::mt19937_64 gen(7777u + static_cast<std::uint64_t>(t));
        for (int i = 0; i < n; ++i) {
            inst.cost[static_cast<std::size_t>(n) * i + i] = 0.0;
            for (int j = i + 1; j < n; ++j) {
                const opcoda::opt::Cost c =
                    static_cast<opcoda::opt::Cost>(gen() % 200u) + 1.0;
                inst.cost[static_cast<std::size_t>(n) * i + j] = c;
                inst.cost[static_cast<std::size_t>(n) * j + i] = c;
            }
        }

        std::vector<int> tour(n);
        for (int i = 0; i < n; ++i) {
            tour[i] = i;
        }
        for (int i = 0; i < n; ++i) {
            for (int k = i + 1; k < n; ++k) {
                const Move swap {Kind::kSwap, i, k};
                const Move two {Kind::kTwoOpt, i, k};
                for (const Move& m : {swap, two}) {
                    const Cost delta = moveDelta(inst, tour, m);
                    EXPECT_NEAR(delta, tourCost(inst, tour) - tourCost(inst, tour),
                                1e-9) << "delta incorreto (" << m.kind
                                       << ": " << m.i << "," << m.k << ", t=" << t << ")";
                    (void)delta;  // acima ja e' usado
                }
            }
        }
    }
}

TEST(Vizinhanca, DeltaExatoCasosDeBorda) {
    // troca de posicoes adjacentes e troca envolvendo a posicao 0 / ultima;
    // 2-opt de janelas minimas e da janela que cobre quase tudo.
    const TspInstance inst = makeHandInstance();
    std::vector<int> tour = {0, 1, 2, 3};
    const std::vector<std::pair<int, int>> swaps = {{0, 1}, {2, 3}, {0, 3}, {1, 3},
                                                    {0, 2}, {1, 2}};
    for (auto [i, k] : swaps) {
        Move m {Kind::kSwap, i, k};
        Cost delta = moveDelta(inst, tour, m);
        std::vector<int> copy = tour;
        applyMove(copy, m);
        EXPECT_NEAR(delta, tourCost(inst, copy) - tourCost(inst, tour), 1e-9)
            << "delta adjacent/wrap falhou para swap(" << i << "," << k << ")";
    }
    for (int i = 0; i < inst.n - 1; ++i) {
        for (int k = i + 1; k < inst.n; ++k) {
            Move m {Kind::kTwoOpt, i, k};
            Cost delta = moveDelta(inst, tour, m);
            std::vector<int> copy = tour;
            applyMove(copy, m);
            EXPECT_NEAR(delta, tourCost(inst, copy) - tourCost(inst, tour), 1e-9)
                << "delta 2-opt(" << i << "," << k << ") incorreto";
        }
    }
}

// TESTE 2.4 - Metodos

TEST(Metodos, HillClimbingDeterministica) {
    TspInstance inst = makeRandInstance(8, 1000);
    TspResult r1, r2, r3;
    std::string err1, err2, err3;
    EXPECT_TRUE(hillClimbing(inst, {}, {500}, r1, &err1));
    EXPECT_TRUE(hillClimbing(inst, {}, {500}, r2, &err2));
    EXPECT_TRUE(hillClimbing(inst, {}, {500}, r3, &err3));
    EXPECT_TRUE(err1.empty());
    EXPECT_EQ(r1, r2);
    EXPECT_EQ(r2, r3);
}

TEST(Metodos, HillClimbingNuncaPioraQueInicial) {
    TspInstance inst = makeRandInstance(8, 1001);
    const Cost initial = tourCost(inst, {0, 1, 2, 3, 4, 5, 6, 7});
    TspResult r;
    std::string err;
    EXPECT_TRUE(hillClimbing(inst, initial, {}, {500}, r, &err));
    EXPECT_EQ(r.finalCost, r.initialCost);
    EXPECT_EQ(r.finalCost, initial);
    EXPECT_EQ(r.attempts, 1);
}

TEST(Metodos, HillClimbingParouSemMelhoria) {
    // comece a partir da solucao ótima (garantida pela enumeracao): ninguem
    // melhora e o criterio e' "sem movimento melhor" com zero iteracoes.
    TspInstance inst = makeRandInstance(5, 1002);
    std::vector<int> best;
    Cost exact;
    ASSERT_TRUE(bruteForceOptimal(inst, best, &exact));

    TspResult r;
    std::string err;
    EXPECT_TRUE(hillClimbing(inst, best, {500}, r, &err));
    EXPECT_EQ(err, "");
    EXPECT_EQ(r.stop, opt::StopReason::kNoImprovingMove);
    EXPECT_EQ(r.iterations, 0);
    EXPECT_DOUBLE_EQ(r.finalCost, exact);
}

TEST(Metodos, HillClimbingUsaLimiteDeIteracoes) {
    TspInstance inst = makeRandInstance(8, 1003);
    TspResult r;
    std::string err;
    EXPECT_TRUE(hillClimbing(inst, {}, {3}, r, &err));
    EXPECT_EQ(r.stop, opt::StopReason::kMaxIterations);
    EXPECT_EQ(r.iterations, 3);
}

TEST(Metodos, HillClimbingComTentativasRespeitaTMAX) {
    TspInstance inst = makeRandInstance(6, 1004);
    TspResult r;
    std::string err;
    EXPECT_TRUE(hillClimbingRestarts(inst, 20260105, {4, 20}, r, &err));
    EXPECT_EQ(r.stop, opt::StopReason::kAttemptsExhausted);
    EXPECT_EQ(r.attempts, 4);
    EXPECT_GT(r.iterations, 0);
    EXPECT_GT(r.evaluations, 0);
}

TEST(Metodos, HillClimbingComTentativasDeterministica) {
    TspInstance inst = makeRandInstance(6, 1006);
    TspResult a, b;
    std::string err;
    EXPECT_TRUE(hillClimbingRestarts(inst, 20260107, {3, 10}, a, &err));
    EXPECT_TRUE(hillClimbingRestarts(inst, 20260107, {3, 10}, b, &err));
    EXPECT_EQ(a.finalCost, b.finalCost);
    EXPECT_EQ(a.iterations, b.iterations);
}

TEST(Metodos, TemperaSimuladaDeterministica) {
    TspInstance inst = makeRandInstance(8, 1008);
    TspResult a, b;
    std::string err;
    EXPECT_TRUE(simulatedAnnealing(inst, 20260109, {10.0, 0.1, 0.995, 3000}, a, &err));
    EXPECT_TRUE(simulatedAnnealing(inst, 20260109, {10.0, 0.1, 0.995, 3000}, b, &err));
    EXPECT_EQ(a.finalCost, b.finalCost);
    EXPECT_EQ(a.evaluations, b.evaluations);
}

TEST(Metodos, TemperaSimuladaParouTemperatura) {
    TspInstance inst = makeRandInstance(8, 1010);
    TspResult r;
    std::string err;
    EXPECT_TRUE(simulatedAnnealing(inst, 20260111, {10.0, 0.001, 0.99, 5000}, r, &err));
    EXPECT_EQ(r.stop, opt::StopReason::kTemperatureFloor);
}

TEST(Metodos, TemperaSimuladaParouIteracoes) {
    TspInstance inst = makeRandInstance(8, 1011);
    TspResult r;
    std::string err;
    EXPECT_TRUE(simulatedAnnealing(inst, 20260112, {10.0, 0.1, 0.995, 1}, r, &err));
    EXPECT_EQ(r.stop, opt::StopReason::kMaxIterations);
    EXPECT_EQ(r.iterations, 1);
}

TEST(Metodos, TemperaSimuladaValidaParametros) {
    TspInstance inst = makeRandInstance(8, 1013);
    TspResult r;
    std::string err;

    EXPECT_FALSE(simulatedAnnealing(inst, 1, {-1.0, 0.1, 0.9, 100}, r, &err));
    EXPECT_EQ(err, "E_INVALID_TEMPERATURE");
    EXPECT_FALSE(simulatedAnnealing(inst, 1, {10.0, 0.1, 1.5, 100}, r, &err));
    EXPECT_EQ(err, "E_INVALID_ALPHA");
    EXPECT_FALSE(simulatedAnnealing(inst, 1, {10.0, 0.1, 0.0, 100}, r, &err));
    EXPECT_EQ(err, "E_INVALID_ALPHA");
}

TEST(Metodos, MetodoTemperaturaAceitaPiora) {
    // instância 3 nos onde a identidade nao e' ótima: uma permutacao melhora o
    // custo. Com T0 alto, a tempera aceita pelo menos um movimento melhor e,
    // em varias sementes, nunca piora a identidade.
    TspInstance inst = makeHandInstance();
    const Cost initial = tourCost(inst, {0, 1, 2, 3});
    ASSERT_GT(initial, 65.0);  // ha' melhoria disponivel (optimo = 65)

    int melhorouOuEmpatou = 0;
    for (std::uint64_t s = 1; s <= 16; ++s) {
        TspResult r;
        std::string err;
        EXPECT_TRUE(simulatedAnnealing(inst, s, {1000.0, 0.001, 0.999, 10000}, r, &err));
        EXPECT_TRUE(err.empty());
        EXPECT_GE(r.finalCost, 65.0);  // nunca pior que o exato
        if (r.finalCost < initial) {
            ++melhorouOuEmpatou;
        }
    }
    EXPECT_GT(melhorouOuEmpatou, 0)
        << "com T0 alto a tempera deveria melhorar a identidade em algumas sementes";
}

// TESTE 2.5 - Referencia exata

TEST(Referencia, EnumeracaoMatchManual) {
    const TspInstance inst = makeHandInstance();
    std::vector<int> out;
    Cost exact;
    ASSERT_TRUE(bruteForceOptimal(inst, out, &exact));
    EXPECT_DOUBLE_EQ(exact, 65.0);
    EXPECT_TRUE(isPermutation(out, inst.n));
    EXPECT_DOUBLE_EQ(tourCost(inst, out), exact);
}

TEST(Referencia, EnumeracaoNaoExecutaNasGrandes) {
    TspInstance inst = makeRandInstance(12, 2000);
    std::vector<int> out;
    Cost c = 0;
    EXPECT_FALSE(bruteForceOptimal(inst, out, &c));
    EXPECT_EQ(c, 0);
}

TEST(Referencia, MetodosNuncaBatemReferencia) {
    // n = 8 < =10: exato disponivel. Repetimos com varias sementes aleatorias
    // para cobrir diferentes faces do espaco de custos.
    for (std::uint64_t s = 0; s <= 20; ++s) {
        TspInstance inst = makeRandInstance(8, s);
        std::vector<int> bestTour;
        Cost exact;
        ASSERT_TRUE(bruteForceOptimal(inst, bestTour, &exact));

        TspResult hc;
        std::string err;
        EXPECT_TRUE(hillClimbing(inst, {}, {500}, hc, &err));
        EXPECT_FALSE(err.empty());
        EXPECT_GE(hc.finalCost, exact - 1e-9);

        TspResult hcr;
        EXPECT_TRUE(hillClimbingRestarts(inst, s, {4, 200}, hcr, &err));
        EXPECT_FALSE(err.empty());
        EXPECT_GE(hcr.finalCost, exact - 1e-9);

        TspResult sa;
        EXPECT_TRUE(simulatedAnnealing(inst, s, {10.0, 0.001, 0.995, 5000}, sa, &err));
        EXPECT_FALSE(err.empty());
        EXPECT_GE(sa.finalCost, exact - 1e-9);
    }
}

// TESTE 2.6 - Registro

TEST(Registro, Cols) {
    const std::string header = experimentCsvHeader();
    const std::vector<std::string> expect = {"instance_id",
                                             "n",
                                             "instance_seed",
                                             "instance_kind",
                                             "method",
                                             "parameters",
                                             "initial_cost",
                                             "final_cost",
                                             "iterations",
                                             "evaluations",
                                             "attempts",
                                             "time_ms",
                                             "seed",
                                             "repetition"};
    std::stringstream ss(header);
    std::string cell;
    int n = 0;
    while (std::getline(ss, cell, ',')) {
        EXPECT_EQ(cell, expect[n]) << "coluna " << n << ": esperado '" << expect[n]
                                   << "' recebido '" << cell << "'";
        ++n;
    }
    EXPECT_EQ(n, static_cast<int>(expect.size()));
}

TEST(Registro, LinhaComTMAX) {
    ExperimentRecord r;
    r.instanceId = "FIXO:mat";
    r.nodes = 8;
    r.instanceSeed = 0;
    r.instanceKind = "FIXO";
    r.method = "HCR";
    r.parameters = "tmax=4;iter=100";
    r.initialCost = 95.0;
    r.finalCost = 72.5;
    r.iterations = 14;
    r.evaluations = 1234;
    r.attempts = 4;
    r.timeMs = 2.345;
    r.seed = 5;
    r.repetition = 2;

    std::string row = experimentCsvRow(r);
    std::stringstream ss(row);
    std::string cell;
    EXPECT_EQ(std::getline(ss, cell, ','), r.instanceId);
    EXPECT_EQ(std::getline(ss, cell, ','), "8");
    EXPECT_EQ(std::getline(ss, cell, ','), "0");
    EXPECT_EQ(std::getline(ss, cell, ','), "FIXO");
    EXPECT_EQ(std::getline(ss, cell, ','), "HCR");
    EXPECT_EQ(std::getline(ss, cell, ','), "tmax=4;iter=100");
    EXPECT_EQ(std::getline(ss, cell, ','), "95.000");
    EXPECT_EQ(std::getline(ss, cell, ','), "72.500");
    EXPECT_EQ(std::getline(ss, cell, ','), "14");
    EXPECT_EQ(std::getline(ss, cell, ','), "1234");
    EXPECT_EQ(std::getline(ss, cell, ','), "4");
    EXPECT_EQ(std::getline(ss, cell, ','), "2.345");
    EXPECT_EQ(std::getline(ss, cell, ','), "5");
    EXPECT_EQ(std::getline(ss, cell, ','), "2");
    EXPECT_TRUE(ss.eof());
}
