#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"

#include <cmath>
#include <limits>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr double kSampleRate = 44100.0;

constexpr float kAudibleFloor {0.01f};

std::vector<float> renderSine(float grainLevel, int blocks = 200) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    engine.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(2.0f * static_cast<float>(M_PI) * 440.0f *
                             static_cast<float>(i) / 44100.0f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 50.0f;
    params.position = 0.5f;
    params.grainLevel = grainLevel;

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    std::vector<float> out;
    for (int block = 0; block < blocks; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        out.insert(out.end(), left.begin(), left.end());
    }
    return out;
}

float peakOf(const std::vector<float>& buffer) {
    float peak = 0.0f;
    for (const auto sample : buffer) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

} // namespace

TEST(GrainLevel, ScalesOutput) {
    const auto full = renderSine(1.0f);
    const auto fullPeak = peakOf(full);
    ASSERT_GT(fullPeak, kAudibleFloor) << "sem som audivel, nada para medir";

    const auto half = renderSine(0.5f);
    const auto halfPeak = peakOf(half);
    ASSERT_GT(halfPeak, kAudibleFloor);
    EXPECT_NEAR(halfPeak / fullPeak, 0.5, 0.05)
        << "pico cheio=" << fullPeak << " pico meio=" << halfPeak;
}

TEST(GrainLevel, ZeroIsSilent) {
    const auto out = renderSine(0.0f);
    for (const auto sample : out) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(GrainLevel, AboveOneClampsToOne) {
    EXPECT_EQ(renderSine(1.5f), renderSine(1.0f));
}

TEST(GrainLevel, NegativeClampsToSilence) {
    // nivel negativo nao e' fase invertida: trunca em zero, como o clamp diz.
    const auto out = renderSine(-1.0f);
    for (const auto sample : out) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(PitchRandom, ChangesVoices) {
    const auto render = [](float pitchRandom) {
        GranularEngine engine;
        engine.prepare(kSampleRate, 256);
        engine.setSounding(true);

        std::vector<float> source(4096);
        for (std::size_t i = 0; i < source.size(); ++i) {
            source[i] = std::sin(static_cast<float>(i) * 0.01f);
        }
        engine.setSource(source.data(), source.size());

        GranularParams params;
        params.grainSizeMs = 10.0f;
        params.densityGrainsPerSec = 200.0f;
        params.position = 0.5f;
        params.pitchRandomSemitones = pitchRandom;

        std::vector<float> left(256, 0.0f);
        std::vector<float> right(256, 0.0f);
        std::vector<float> out;
        for (int block = 0; block < 40; ++block) {
            engine.processBlock(left.data(), right.data(), 256, params);
            out.insert(out.end(), left.begin(), left.end());
        }
        return out;
    };

    const auto plain = render(0.0f);
    const auto detuned = render(6.0f);

    std::size_t differing = 0;
    ASSERT_EQ(plain.size(), detuned.size());
    for (std::size_t i = 0; i < plain.size(); ++i) {
        if (plain[i] != detuned[i]) {
            ++differing;
        }
    }
    EXPECT_GT(differing, plain.size() / 2) << "diferiram " << differing << " de "
                                           << plain.size();

    for (const auto sample : plain) {
        ASSERT_TRUE(std::isfinite(sample));
    }
    for (const auto sample : detuned) {
        ASSERT_TRUE(std::isfinite(sample));
    }

    // com o mesmo valor o resultado repete: o detune nao custa o determinismo do T1.
    EXPECT_EQ(detuned, render(6.0f));
}

TEST(PitchRandom, AboveTwelveClampsToTwelve) {
    // 24 semitons dao exatamente os mesmos buffers que 12, e nao so parecidos.
    GranularEngine first;
    first.prepare(kSampleRate, 256);
    first.setSounding(true);

    GranularEngine second;
    second.prepare(kSampleRate, 256);
    second.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.01f);
    }
    first.setSource(source.data(), source.size());
    second.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    params.position = 0.5f;

    std::vector<float> wide;
    std::vector<float> clamped;
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    for (int block = 0; block < 40; ++block) {
        params.pitchRandomSemitones = 24.0f;
        first.processBlock(left.data(), right.data(), 256, params);
        wide.insert(wide.end(), left.begin(), left.end());

        params.pitchRandomSemitones = 12.0f;
        second.processBlock(left.data(), right.data(), 256, params);
        clamped.insert(clamped.end(), left.begin(), left.end());
    }
    EXPECT_EQ(wide, clamped);
}

namespace {

void renderNaNParam(bool nanLevel, bool nanDetune) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    engine.setSounding(true);

    std::vector<float> source(4096, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    if (nanLevel) {
        params.grainLevel = std::numeric_limits<float>::quiet_NaN();
    }
    if (nanDetune) {
        params.pitchRandomSemitones = std::numeric_limits<float>::quiet_NaN();
    }

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
}

} // namespace

TEST(GrainParams, NaNLevelDoesNotPoisonOutput) {
    renderNaNParam(true, false);
}

TEST(GrainParams, NaNDetuneDoesNotPoisonOutput) {
    renderNaNParam(false, true);
}

TEST(GrainParams, NaNPitchDoesNotPoisonOutput) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    engine.setSounding(true);

    std::vector<float> source(4096, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    params.pitchSemitones = std::numeric_limits<float>::quiet_NaN();

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    for (int block = 0; block < 40; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        for (const auto sample : left) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no bloco " << block;
        }
        for (const auto sample : right) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no bloco " << block;
        }
    }
}
