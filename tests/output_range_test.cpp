#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"

#include <algorithm>
#include <cmath>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

// fonte quente: dois tons, pico perto da amplitude pedida.
std::vector<float> hotSource(float amplitude) {
    std::vector<float> samples(24000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = amplitude *
                     (0.9f * std::sin(static_cast<float>(i) * 0.05f) +
                      0.2f * std::sin(static_cast<float>(i) * 0.31f));
    }
    return samples;
}

GranularParams baseParams() {
    GranularParams params;
    params.grainSizeMs = 20.0f;
    params.densityGrainsPerSec = 40.0f;
    params.position = 0.5f;
    params.pan = 0.0f;
    return params;
}

float renderPeak(float driveDb, float amplitude = 0.3f, float density = 40.0f) {
    const auto material = hotSource(amplitude);
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(material.data(), material.size());
    engine.setSounding(true);

    auto params = baseParams();
    params.f1DriveDb = driveDb;
    params.densityGrainsPerSec = density;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float peak = 0.0f;
    for (int block = 0; block < 48; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        if (block < 32) {
            continue;
        }
        for (int i = 0; i < kBlock; ++i) {
            peak = std::max(peak, std::abs(left[static_cast<std::size_t>(i)]));
        }
    }
    return peak;
}

float renderCrest(float driveDb, float amplitude = 1.5f) {
    const auto material = hotSource(amplitude);
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(material.data(), material.size());
    engine.setSounding(true);

    auto params = baseParams();
    params.f1DriveDb = driveDb;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<float> collected;
    for (int block = 0; block < 20; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        if (block >= 2) {
            collected.insert(collected.end(), left.begin(), left.end());
        }
    }

    double sum = 0.0;
    float peak = 0.0f;
    for (const auto value : collected) {
        sum += static_cast<double>(value) * static_cast<double>(value);
        peak = std::max(peak, std::abs(value));
    }
    const auto rms = std::sqrt(sum / static_cast<double>(collected.size()));
    return rms > 0.0 ? static_cast<float>(static_cast<double>(peak) / rms) : 0.0f;
}

} // namespace

TEST(Drive, CeilingBindsAtTwentyFourDb) {
    // 1,0 - 0,60 * (24 - 6) / 18 = 0,40.
    EXPECT_NEAR(renderPeak(24.0f, 0.3f), 0.40f, 0.01f);
    EXPECT_NEAR(renderPeak(24.0f, 1.5f), 0.40f, 0.01f);
}

TEST(Drive, CeilingStaysOpenBelowSixDb) {
    EXPECT_GT(renderPeak(6.0f, 1.5f, 200.0f), 0.5f);
}

TEST(Drive, CeilingInterpolatesWithTheKnob) {
    // 1,0 - 0,60 * (12 - 6) / 18 = 0,80: o teto e' uma rampa, nao um degrau.
    EXPECT_NEAR(renderPeak(12.0f, 1.5f, 200.0f), 0.80f, 0.02f);
}

TEST(Drive, CeilingClipsInsteadOfScaling) {
    const auto drivenCrest = renderCrest(24.0f, 1.5f);
    ASSERT_GT(drivenCrest, 0.5f) << "saida muda a 24 dB: o ensaio mediria silencio";
    EXPECT_LT(drivenCrest, 1.8f);
    // o material limpo, medido da mesma forma, tem crista bem acima disso.
    EXPECT_GT(renderCrest(0.0f, 1.5f), 2.5f);
}

namespace {

// 1 kHz a 48 kHz.
std::vector<float> toneSource() {
    std::vector<float> samples(48000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.5f * std::sin(static_cast<float>(i) * 6.2831853f / 48.0f);
    }
    return samples;
}

int crossingsAtPitch(float semitones, float density = 80.0f) {
    const auto material = toneSource();
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(material.data(), material.size());
    engine.setSounding(true);

    auto params = baseParams();
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = density;
    params.pitchSemitones = semitones;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<float> collected;
    for (int block = 0; block < 32; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        collected.insert(collected.end(), left.begin(), left.end());
    }

    const auto window = std::min<std::size_t>(4096, collected.size());
    int count = 0;
    for (std::size_t i = collected.size() - window + 1; i < collected.size(); ++i) {
        const auto previous = collected[i - 1];
        const auto current = collected[i];
        if ((previous < 0.0f) != (current < 0.0f)) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST(Pitch, PlusFortyEightIsFourOctaves) {
    const auto plain = crossingsAtPitch(0.0f);
    const auto high = crossingsAtPitch(48.0f);

    ASSERT_GT(plain, 8) << "ha' de haver tom para medir";
    ASSERT_GT(high, 8) << "a oitava alta ficou muda";
    const auto ratio = static_cast<double>(high) / static_cast<double>(plain);
    EXPECT_NEAR(ratio, 16.0, 2.0) << "razao medida " << ratio;
}

TEST(Pitch, SilenceHasNoCrossings) {
    EXPECT_EQ(crossingsAtPitch(0.0f, 0.0f), 0);
}
