#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

std::vector<float> source() {
    std::vector<float> samples(24000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.3f * std::sin(static_cast<float>(i) * 0.05f);
    }
    return samples;
}

GranularParams baseParams() {
    GranularParams params;
    params.grainSizeMs = 20.0f;
    params.densityGrainsPerSec = 40.0f;
    params.position = 0.5f;
    return params;
}

struct Sample {
    int delay {0};
    float spread {0.0f};
    int length {0};
};

// Corre `blocks` blocos e recolhe o que cada grao recebeu da nuvem. O gancho
// devolve o ultimo grao do bloco; com densidade 40/s e blocos de 5,3 ms ha
// varios blocos por grao, e a amostragem cobre o suficiente.
std::vector<Sample> run(GranularParams params, int blocks, std::uint64_t seed = 0,
                        std::vector<float>* rendered = nullptr) {
    const auto material = source();
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(material.data(), material.size());
    engine.setSounding(true);
    if (seed != 0) {
        engine.setSeed(seed);
    }

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<Sample> samples;
    for (int block = 0; block < blocks; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        samples.push_back({engine.lastGrainDelaySamples(),
                           engine.lastGrainSpreadSemitones(),
                           engine.lastGrainLengthSamples()});
        if (rendered != nullptr) {
            rendered->insert(rendered->end(), left.begin(), left.end());
        }
    }
    return samples;
}

} // namespace

TEST(Cloud, ZeroIsByteIdenticalToPrevious) {
    // Com os tres a zero o motor nao pode ter mudado. Compara-se contra o
    // mesmo material com os parametros explicitamente a zero: se o codigo novo
    // consumisse um sorteio a mais, a sequencia do gerador mudava e a saida
    // divergia aqui.
    std::vector<float> zeros;
    std::vector<float> defaults;
    auto explicitZeros = baseParams();
    explicitZeros.cloudSize = 0.0f;
    explicitZeros.pitchSpreadSemitones = 0.0f;
    explicitZeros.grainJitter = 0.0f;
    run(explicitZeros, 60, 0, &zeros);
    run(baseParams(), 60, 0, &defaults);

    ASSERT_EQ(zeros.size(), defaults.size());
    for (std::size_t i = 0; i < zeros.size(); ++i) {
        ASSERT_EQ(zeros[i], defaults[i]) << "amostra " << i;
    }

    // E os ganchos confirmam que nada foi sorteado.
    for (const auto& s : run(explicitZeros, 60)) {
        EXPECT_EQ(s.delay, 0);
        EXPECT_FLOAT_EQ(s.spread, 0.0f);
    }
}

TEST(Cloud, SizeSpreadsOnsetsWithinWindow) {
    auto params = baseParams();
    params.cloudSize = 1.0f;

    const auto samples = run(params, 400);
    int maxDelay = 0;
    int nonZero = 0;
    for (const auto& s : samples) {
        ASSERT_GE(s.delay, 0);
        ASSERT_LE(s.delay, static_cast<int>(0.5 * kSampleRate) + 1);
        maxDelay = std::max(maxDelay, s.delay);
        nonZero += (s.delay > 0) ? 1 : 0;
    }

    // A janela cheia e' 500 ms, e o sorteio tem de a usar: um atraso maximo
    // abaixo de 100 ms significaria que a janela nao chegou ao motor.
    EXPECT_GT(maxDelay, static_cast<int>(0.1 * kSampleRate));
    EXPECT_GT(nonZero, 0);

    auto off = baseParams();
    off.cloudSize = 0.0f;
    for (const auto& s : run(off, 200)) {
        EXPECT_EQ(s.delay, 0);
    }
}

TEST(Cloud, PitchSpreadStaysWithinRange) {
    auto params = baseParams();
    params.pitchSpreadSemitones = 12.0f;
    params.densityGrainsPerSec = 200.0f;

    const auto samples = run(params, 400);
    float minSpread = 1.0e9f;
    float maxSpread = -1.0e9f;
    double sum = 0.0;
    int counted = 0;
    for (const auto& s : samples) {
        ASSERT_LE(std::abs(s.spread), 12.0f + 1.0e-4f);
        minSpread = std::min(minSpread, s.spread);
        maxSpread = std::max(maxSpread, s.spread);
        sum += static_cast<double>(s.spread);
        ++counted;
    }

    ASSERT_GT(counted, 0);
    // Simetrica e a usar a faixa: os dois extremos aparecem e a media fica
    // perto de zero. Com poucos graos a media oscila; 200 blocos a 40/s dao
    // centenas de amostras.
    EXPECT_LT(minSpread, -4.0f);
    EXPECT_GT(maxSpread, 4.0f);
    EXPECT_NEAR(sum / static_cast<double>(counted), 0.0, 1.5);

    // Fora do intervalo: com spread 4 nenhum grao passa de 4.
    auto narrow = params;
    narrow.pitchSpreadSemitones = 4.0f;
    for (const auto& s : run(narrow, 200)) {
        ASSERT_LE(std::abs(s.spread), 4.0f + 1.0e-4f);
    }
}

TEST(Cloud, GrainJitterVariesDuration) {
    auto params = baseParams();
    params.grainSizeMs = 40.0f;
    params.grainJitter = 1.0f;

    const int baseLength = static_cast<int>(40.0f * 0.001f * static_cast<float>(kSampleRate));
    std::vector<int> lengths;
    for (const auto& s : run(params, 400)) {
        // Metade a uma vez e meia de 40 ms, com piso de 1 ms e teto de 100 ms.
        // Blocos sem grao novo reportam zero — nao sao amostras.
        if (s.length == 0) {
            continue;
        }
        ASSERT_GE(s.length, static_cast<int>(0.5f * static_cast<float>(baseLength)) - 1);
        ASSERT_LE(s.length, static_cast<int>(1.5f * static_cast<float>(baseLength)) + 1);
        lengths.push_back(s.length);
    }
    ASSERT_GT(lengths.size(), 20u);

    const double mean =
        std::accumulate(lengths.begin(), lengths.end(), 0.0) / static_cast<double>(lengths.size());
    double variance = 0.0;
    for (const int length : lengths) {
        const double delta = static_cast<double>(length) - mean;
        variance += delta * delta;
    }
    variance /= static_cast<double>(lengths.size());
    EXPECT_GT(std::sqrt(variance), 1.0);

    auto steady = baseParams();
    steady.grainSizeMs = 40.0f;
    steady.grainJitter = 0.0f;
    for (const auto& s : run(steady, 120)) {
        if (s.length == 0) {
            continue; // bloco sem grao novo
        }
        EXPECT_EQ(s.length, baseLength);
    }
}

TEST(Cloud, SameSeedSameCloud) {
    auto params = baseParams();
    params.cloudSize = 0.8f;
    params.pitchSpreadSemitones = 6.0f;
    params.grainJitter = 0.7f;

    std::vector<float> first;
    std::vector<float> second;
    std::vector<float> other;
    run(params, 120, 12345, &first);
    run(params, 120, 12345, &second);
    run(params, 120, 999, &other);

    ASSERT_EQ(first.size(), second.size());
    EXPECT_EQ(first, second);
    // Sementes diferentes poem outra nuvem: os atrasos nao podem ser os mesmos
    // em todas as amostras.
    EXPECT_NE(first, other);
}
