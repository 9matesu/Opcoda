#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"

#include <cmath>
#include <limits>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;

namespace {
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 128;

std::vector<float> traceLevels(GranularParams params, int blocks, bool sounding = true) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(sounding);

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<float> levels;
    for (int block = 0; block < blocks; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        levels.push_back(engine.gateLevel());
    }
    return levels;
}

} // namespace

TEST(EnvelopeAdsr, AttackReachesOneInAttackTime) {
    GranularParams params;
    params.attackSeconds = 0.01f;

    const auto levels = traceLevels(params, 5);
    ASSERT_EQ(levels.size(), 5u);
    EXPECT_GT(levels[2], 0.0f);
    EXPECT_LT(levels[2], 1.0f);
    EXPECT_FLOAT_EQ(levels[3], 1.0f);
    EXPECT_FLOAT_EQ(levels[4], 1.0f);
}

TEST(EnvelopeAdsr, DecayFallsToSustain) {
    GranularParams params;
    params.attackSeconds = 0.001f;
    params.decaySeconds = 0.05f;
    params.sustainLevel = 0.4f;

    const auto levels = traceLevels(params, 25);
    EXPECT_GT(levels[9], 0.4f);
    EXPECT_LT(levels[9], 1.0f);
    for (std::size_t i = 20; i < levels.size(); ++i) {
        EXPECT_FLOAT_EQ(levels[i], 0.4f);
    }
}

TEST(EnvelopeAdsr, SustainHolds) {
    GranularParams params;
    params.attackSeconds = 0.001f;
    params.decaySeconds = 0.005f;
    params.sustainLevel = 0.3f;

    const auto levels = traceLevels(params, 100);
    for (std::size_t i = 50; i < levels.size(); ++i) {
        EXPECT_FLOAT_EQ(levels[i], 0.3f);
    }
}

TEST(EnvelopeAdsr, RepeatedSoundingDoesNotRestart) {
    GranularEngine once;
    once.prepare(kSampleRate, kBlock);
    once.setSounding(true);

    GranularEngine twice;
    twice.prepare(kSampleRate, kBlock);
    twice.setSounding(true);

    GranularParams params;
    params.attackSeconds = 0.05f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    once.processBlock(left.data(), right.data(), kBlock, params);
    once.processBlock(left.data(), right.data(), kBlock, params);
    const auto expected = once.gateLevel();

    twice.processBlock(left.data(), right.data(), kBlock, params);
    twice.setSounding(true);
    twice.processBlock(left.data(), right.data(), kBlock, params);
    EXPECT_FLOAT_EQ(twice.gateLevel(), expected);
}

TEST(EnvelopeAdsr, SoundingDuringDecayDoesNotRestartAttack) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    GranularParams params;
    params.attackSeconds = 0.001f;
    params.decaySeconds = 0.5f;
    params.sustainLevel = 0.0f;

    engine.setSounding(true);

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    for (int block = 0; block < 10; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
    }
    const auto falling = engine.gateLevel();
    ASSERT_GT(falling, 0.0f);
    ASSERT_LT(falling, 1.0f);

    // o mesmo que o processador faz: reafirmar a nota a cada bloco.
    for (int block = 0; block < 10; ++block) {
        engine.setSounding(true);
        engine.processBlock(left.data(), right.data(), kBlock, params);
    }
    // continuou a cair, nao voltou ao ataque.
    EXPECT_LT(engine.gateLevel(), falling);
}

TEST(EnvelopeAdsr, ReleaseTakesReleaseTime) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    GranularParams params;
    params.attackSeconds = 0.001f;
    params.sustainLevel = 1.0f;
    params.releaseSeconds = 0.05f;

    engine.setSounding(true);

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    for (int block = 0; block < 4; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
    }
    ASSERT_FLOAT_EQ(engine.gateLevel(), 1.0f);

    engine.setSounding(false);

    std::vector<float> closing;
    for (int block = 0; block < 20; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        closing.push_back(engine.gateLevel());
    }

    // monotonico a fechar: um release que sobe no meio e' segunda nota fantasma.
    for (std::size_t i = 1; i < closing.size(); ++i) {
        EXPECT_LE(closing[i], closing[i - 1] + 1e-7f) << "bloco " << i;
    }
    EXPECT_GT(closing[4], 0.0f);
    EXPECT_FLOAT_EQ(closing.back(), 0.0f);
}

TEST(EnvelopeAdsr, RetriggerDuringReleaseHasNoStep) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    GranularParams params;
    params.attackSeconds = 0.05f;
    params.releaseSeconds = 0.05f;

    engine.setSounding(true);

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    for (int block = 0; block < 5; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
    }
    const auto interrupted = engine.gateLevel();
    ASSERT_GT(interrupted, 0.0f);
    ASSERT_LT(interrupted, 1.0f);

    engine.setSounding(false);
    engine.processBlock(left.data(), right.data(), kBlock, params);
    engine.processBlock(left.data(), right.data(), kBlock, params);
    // o release desceu de verdade, senao o "during release" do titulo e' vacuo.
    EXPECT_LT(engine.gateLevel(), interrupted);

    engine.setSounding(true);
    float previous = engine.gateLevel();
    for (int block = 0; block < 30; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        const auto level = engine.gateLevel();
        EXPECT_GE(level, previous - 1e-7f) << "bloco " << block;
        previous = level;
    }
    EXPECT_FLOAT_EQ(engine.gateLevel(), 1.0f);
}

TEST(EnvelopeAdsr, ReleaseFromSilenceStaysSilent) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    GranularParams params;
    engine.setSounding(false);

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    for (int block = 0; block < 8; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        EXPECT_FLOAT_EQ(engine.gateLevel(), 0.0f);
    }
}

TEST(EnvelopeAdsr, NaNEnvelopeParamsAreSafe) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    params.attackSeconds = nan;
    params.decaySeconds = nan;
    params.sustainLevel = nan;
    params.releaseSeconds = nan;

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    for (int block = 0; block < 40; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        for (const auto sample : left) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no esquerdo, bloco " << block;
        }
        for (const auto sample : right) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no direito, bloco " << block;
        }
    }
    // com os defaults aplicados a nota abre: o envelope nao ficou preso.
    EXPECT_FLOAT_EQ(engine.gateLevel(), 1.0f);

    engine.setSounding(false);
    for (int block = 0; block < 40; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        for (const auto sample : left) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no release, esquerdo, bloco " << block;
        }
        for (const auto sample : right) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no release, direito, bloco " << block;
        }
    }
    EXPECT_FLOAT_EQ(engine.gateLevel(), 0.0f);
}

TEST(EnvelopeAdsr, ZeroAttackIsOneSampleNotDivZero) {
    GranularParams params;
    params.attackSeconds = 0.0f;

    const auto levels = traceLevels(params, 2);
    EXPECT_FLOAT_EQ(levels[0], 1.0f);
}
