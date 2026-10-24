#include "opcoda_core/optimization/knapsack.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

namespace {

using opcoda::optimization::Block;
using opcoda::optimization::Method;
using opcoda::optimization::Params;

std::vector<std::uint8_t> highEntropy(std::size_t size) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<std::uint8_t>(i & 0xFFu);
    }
    return bytes;
}

double selectedWeight(const opcoda::optimization::Problem& problem,
                      const opcoda::optimization::Result& result) {
    double weight = 0.0;
    for (std::size_t i = 0; i < problem.blocks.size(); ++i) {
        if (result.selected[i] != 0) {
            weight += problem.blocks[i].weightKb;
        }
    }
    return weight;
}

double selectedValue(const opcoda::optimization::Problem& problem,
                     const opcoda::optimization::Result& result) {
    double value = 0.0;
    for (std::size_t i = 0; i < problem.blocks.size(); ++i) {
        if (result.selected[i] != 0) {
            value += problem.blocks[i].value;
        }
    }
    return value;
}

void expectConsistent(const opcoda::optimization::Problem& problem,
                      const opcoda::optimization::Result& result) {
    ASSERT_TRUE(result.valid);
    ASSERT_EQ(result.selected.size(), problem.blocks.size());
    EXPECT_LE(selectedWeight(problem, result), problem.capacityKb + 1e-9);
    EXPECT_NEAR(result.weightKb, selectedWeight(problem, result), 1e-9);
    EXPECT_NEAR(result.value, selectedValue(problem, result), 1e-9);
    const auto count = std::accumulate(result.selected.begin(), result.selected.end(), 0);
    EXPECT_EQ(result.count, count);
}

} // namespace

TEST(Knapsack, BuildsOneItemPerBlock) {
    auto bytes = highEntropy(8 * 1024);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);

    ASSERT_TRUE(problem.valid);
    ASSERT_EQ(problem.blocks.size(), 8u);
    EXPECT_EQ(problem.blockBytes, 1024u);
    EXPECT_NEAR(problem.totalWeightKb, 8.0, 1e-9);
    EXPECT_NEAR(problem.capacityKb, 4.0, 1e-9);

    for (std::size_t i = 0; i < problem.blocks.size(); ++i) {
        EXPECT_EQ(problem.blocks[i].offset, i * 1024u);
        EXPECT_EQ(problem.blocks[i].length, 1024u);
        EXPECT_NEAR(problem.blocks[i].weightKb, 1.0, 1e-9);
        EXPECT_NEAR(problem.blocks[i].value, 8.0 * (1.0 - 4.0 / 1024.0), 1e-6);
    }
}

TEST(Knapsack, GrowsBlockToHonourMaxBlocks) {
    auto bytes = highEntropy(8 * 1024);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5, 3);

    ASSERT_TRUE(problem.valid);
    ASSERT_EQ(problem.blocks.size(), 3u);
    EXPECT_EQ(problem.blockBytes, 2731u);
    EXPECT_EQ(problem.blocks[0].length, 2731u);
    EXPECT_EQ(problem.blocks[1].length, 2731u);
    EXPECT_EQ(problem.blocks[2].length, 2730u);
}

TEST(Knapsack, BuildsProblemForSubRegion) {
    auto bytes = highEntropy(8 * 1024);
    std::fill(bytes.begin(), bytes.begin() + 1024, 0x00);

    const auto whole = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 1024, 5 * 1024, 1024, 0.5);

    ASSERT_TRUE(whole.valid);
    ASSERT_EQ(whole.blocks.size(), 8u);
    ASSERT_TRUE(problem.valid);
    ASSERT_EQ(problem.blocks.size(), 4u);
    for (std::size_t i = 0; i < problem.blocks.size(); ++i) {
        EXPECT_EQ(problem.blocks[i].offset, 1024u + i * 1024u);
        EXPECT_EQ(problem.blocks[i].length, 1024u);
    }

    // O bloco 0 do ficheiro e' todo zero; o bloco 1 e' o ramal de alta
    // entropia. Se a sub-regiao fosse medida a partir de data em vez de
    // data + start, o valor seria 0 e este teste falhava.
    EXPECT_NEAR(whole.blocks[0].value, 0.0, 1e-9);
    EXPECT_GT(whole.blocks[1].value, 0.0);
    EXPECT_NEAR(problem.blocks[0].value, whole.blocks[1].value, 1e-9);
    EXPECT_NEAR(problem.blocks[0].weightKb, whole.blocks[1].weightKb, 1e-9);
}

TEST(Knapsack, KeepsLastPartialBlock) {
    auto bytes = highEntropy(5000);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);

    ASSERT_TRUE(problem.valid);
    ASSERT_EQ(problem.blocks.size(), 5u);
    EXPECT_EQ(problem.blocks.back().length, 5000u - 4u * 1024u);
}

TEST(Knapsack, RejectsInvalidRegion) {
    auto bytes = highEntropy(1024);
    EXPECT_FALSE(opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 1024, 1024, 1024, 0.5).valid);
    EXPECT_FALSE(opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size() + 1, 1024, 0.5).valid);
    EXPECT_FALSE(opcoda::optimization::buildProblem(
        nullptr, bytes.size(), 0, bytes.size(), 1024, 0.5).valid);
    EXPECT_FALSE(opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 0, 0.5).valid);
    EXPECT_FALSE(opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5, 0).valid);
    EXPECT_FALSE(opcoda::optimization::buildProblem(
        bytes.data(), 0, 0, 0, 1024, 0.5).valid);
    EXPECT_FALSE(opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024,
        std::numeric_limits<double>::quiet_NaN()).valid);
}

TEST(Knapsack, ClampsCapacityFraction) {
    auto bytes = highEntropy(4 * 1024);
    const auto all = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 2.0);
    const auto none = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, -1.0);
    ASSERT_TRUE(all.valid);
    ASSERT_TRUE(none.valid);
    EXPECT_NEAR(all.capacityKb, all.totalWeightKb, 1e-9);
    EXPECT_NEAR(none.capacityKb, 0.0, 1e-9);

    const auto result = opcoda::optimization::optimize(
        none, Method::hillClimb, {});
    expectConsistent(none, result);
    EXPECT_EQ(result.count, 0);
    EXPECT_EQ(result.weightKb, 0.0);
}

TEST(Knapsack, EveryMethodRespectsCapacity) {
    auto bytes = highEntropy(16 * 1024);
    std::fill(bytes.begin() + 8 * 1024, bytes.end(), 0);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);

    for (const auto method : {Method::hillClimb, Method::multiRestart,
                              Method::simulatedAnnealing}) {
        Params params;
        params.maxIterations = 5000;
        params.coolingFactor = 0.99;
        const auto result = opcoda::optimization::optimize(problem, method, params);
        expectConsistent(problem, result);
        EXPECT_EQ(result.method, method);
        EXPECT_GT(result.count, 0);
    }
}

TEST(Knapsack, SameSeedProducesSameSelection) {
    auto bytes = highEntropy(8 * 1024);
    std::fill(bytes.begin() + 4 * 1024, bytes.end(), 0);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);

    Params params;
    params.seed = 1234;
    params.maxIterations = 2000;
    params.coolingFactor = 0.99;
    const auto first = opcoda::optimization::optimize(
        problem, Method::simulatedAnnealing, params);
    const auto second = opcoda::optimization::optimize(
        problem, Method::simulatedAnnealing, params);

    expectConsistent(problem, first);
    expectConsistent(problem, second);
    EXPECT_EQ(first.selected, second.selected);
    EXPECT_NEAR(first.value, second.value, 1e-9);
    EXPECT_EQ(first.iterations, second.iterations);
}

TEST(Knapsack, PrefersHighEntropyBlocks) {
    auto bytes = highEntropy(4 * 1024);
    std::fill(bytes.begin() + 2 * 1024, bytes.end(), 0);

    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);
    ASSERT_EQ(problem.blocks.size(), 4u);

    for (const auto method : {Method::hillClimb, Method::multiRestart,
                              Method::simulatedAnnealing}) {
        Params params;
        params.maxIterations = 20000;
        params.coolingFactor = 0.999;
        const auto result = opcoda::optimization::optimize(problem, method, params);
        expectConsistent(problem, result);
        EXPECT_EQ(result.selected[0], 1);
        EXPECT_EQ(result.selected[1], 1);
        EXPECT_EQ(result.selected[2], 0);
        EXPECT_EQ(result.selected[3], 0);
    }
}

TEST(Knapsack, MultiRestartIsNotWorseThanSingleClimb) {
    auto bytes = highEntropy(12 * 1024);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);

    Params params;
    params.restarts = 5;
    const auto single = opcoda::optimization::optimize(
        problem, Method::hillClimb, params);
    const auto multi = opcoda::optimization::optimize(
        problem, Method::multiRestart, params);

    expectConsistent(problem, single);
    expectConsistent(problem, multi);
    EXPECT_GE(multi.value, single.value - 1e-9);
    // O primeiro restart usa a mesma semente e o mesmo hillClimb, por isso o
    // valor sozinho nao prova que os restarts correram. As iteracoes provam.
    EXPECT_GT(multi.iterations, single.iterations);
}

TEST(Knapsack, ConcatenatesSelectedBlocksInOrder) {
    auto bytes = highEntropy(4 * 1024);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);
    const auto result = opcoda::optimization::optimize(
        problem, Method::hillClimb, {});

    const auto joined = opcoda::optimization::concatenate(
        bytes.data(), bytes.size(), problem, result);
    ASSERT_FALSE(joined.empty());
    EXPECT_EQ(joined.size(), static_cast<std::size_t>(result.weightKb * 1024.0));

    std::size_t cursor = 0;
    for (std::size_t i = 0; i < problem.blocks.size(); ++i) {
        if (result.selected[i] == 0) {
            continue;
        }
        const auto& block = problem.blocks[i];
        for (std::uint64_t b = 0; b < block.length; ++b) {
            ASSERT_LT(cursor, joined.size());
            EXPECT_EQ(joined[cursor], bytes[static_cast<std::size_t>(block.offset + b)]);
            ++cursor;
        }
    }
    EXPECT_EQ(cursor, joined.size());
}

TEST(Knapsack, ConcatenateRejectsMismatchedSolution) {
    auto bytes = highEntropy(4 * 1024);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);
    auto result = opcoda::optimization::optimize(problem, Method::hillClimb, {});
    ASSERT_FALSE(result.selected.empty());
    result.selected.pop_back();
    EXPECT_TRUE(opcoda::optimization::concatenate(
        bytes.data(), bytes.size(), problem, result).empty());

    auto empty = opcoda::optimization::Result {};
    empty.valid = true;
    empty.selected.assign(problem.blocks.size(), 0);
    EXPECT_TRUE(opcoda::optimization::concatenate(
        bytes.data(), bytes.size(), problem, empty).empty());
}

TEST(Knapsack, ConcatenateRejectsInvalidInput) {
    auto bytes = highEntropy(4 * 1024);
    const auto problem = opcoda::optimization::buildProblem(
        bytes.data(), bytes.size(), 0, bytes.size(), 1024, 0.5);
    const auto result = opcoda::optimization::optimize(
        problem, Method::hillClimb, {});

    EXPECT_TRUE(opcoda::optimization::concatenate(
        nullptr, bytes.size(), problem, result).empty());

    opcoda::optimization::Problem outOfRange = problem;
    outOfRange.blocks.clear();
    Block block;
    block.offset = bytes.size();
    block.length = 1024;
    block.weightKb = 1.0;
    block.value = 1.0;
    outOfRange.blocks.push_back(block);

    opcoda::optimization::Result selected;
    selected.valid = true;
    selected.selected = {1};
    EXPECT_TRUE(opcoda::optimization::concatenate(
        bytes.data(), bytes.size(), outOfRange, selected).empty());
}

TEST(Knapsack, OptimizeAcceptsEmptyProblem) {
    opcoda::optimization::Problem problem;
    problem.valid = true;
    problem.capacityKb = 0.0;
    problem.totalWeightKb = 0.0;

    const auto result = opcoda::optimization::optimize(
        problem, Method::hillClimb, {});
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.selected.empty());
    EXPECT_EQ(result.count, 0);
    EXPECT_EQ(result.value, 0.0);
}

TEST(Knapsack, InvalidProblemProducesInvalidResult) {
    const auto problem = opcoda::optimization::Problem {};
    const auto result = opcoda::optimization::optimize(problem, Method::hillClimb, {});
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.selected.empty());
}
