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

std::vector<float> source() {
    std::vector<float> samples(24000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.3f * std::sin(static_cast<float>(i) * 0.05f);
    }
    return samples;
}

struct StereoBlock {
    std::vector<float> left;
    std::vector<float> right;
};

// corre `blocks` blocos com a largura pedida e devolve o ultimo. os primeiros
// blocos sao ralo: os graos ainda a nascer. no fim ha' energia dos dois lados.
StereoBlock run(float width, int blocks = 48) {
    const auto material = source();
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(material.data(), material.size());
    engine.setSounding(true);

    auto params = GranularParams {};
    params.grainSizeMs = 20.0f;
    params.densityGrainsPerSec = 40.0f;
    params.position = 0.5f;
    // pan fora do centro: com pan 0 os dois canais sao iguais e o side e' zero,
    // o que tornaria o teste cego a largura.
    params.pan = 0.5f;
    params.widthStereo = width;

    StereoBlock out {std::vector<float>(kBlock, 0.0f), std::vector<float>(kBlock, 0.0f)};
    for (int block = 0; block < blocks; ++block) {
        engine.processBlock(out.left.data(), out.right.data(), kBlock, params);
    }
    return out;
}

TEST(StereoWidth, IdentityKeepsTheMidUntouched) {
    // comparar duas corridas com a mesma largura era tautologico: nunca podia
    // falhar. o que o desenho promete e' que a largura mexe no side e nunca no
    // mid, por isso o ensaio compara a soma dos canais entre duas larguras.
    const auto normal = run(1.0f);
    const auto narrow = run(0.5f);

    float normalMid = 0.0f;
    float narrowMid = 0.0f;
    float normalSide = 0.0f;
    for (std::size_t i = 0; i < normal.left.size(); ++i) {
        normalMid += std::abs(normal.left[i] + normal.right[i]);
        narrowMid += std::abs(narrow.left[i] + narrow.right[i]);
        normalSide += std::abs(normal.left[i] - normal.right[i]);
    }
    // ha' de existir side: com mid puro as duas larguras davam o mesmo por
    // vacuidade.
    EXPECT_GT(normalSide, 1.0f);
    EXPECT_NEAR(narrowMid, normalMid, normalMid * 0.02f);
}

TEST(StereoWidth, MonoCollapsesBothChannels) {
    const auto mono = run(0.0f);
    float peakDifference = 0.0f;
    for (std::size_t i = 0; i < mono.left.size(); ++i) {
        peakDifference = std::max(peakDifference, std::abs(mono.left[i] - mono.right[i]));
    }
    // mid/side com width 0 da' mid nos dois canais; a diferenca residual e' o
    // ruido de denormais, nao estereo.
    EXPECT_LT(peakDifference, 1.0e-4f);
}

TEST(StereoWidth, NarrowingScalesTheSideDown) {
    // mid/side puro, sem haas (o atraso so' existe acima de 1.0): meia largura
    // da' meia diferenca entre canais.
    const auto normal = run(1.0f);
    const auto narrow = run(0.5f);

    float normalSide = 0.0f;
    float narrowSide = 0.0f;
    for (std::size_t i = 0; i < normal.left.size(); ++i) {
        normalSide += std::abs(normal.left[i] - normal.right[i]);
        narrowSide += std::abs(narrow.left[i] - narrow.right[i]);
    }
    // ha' de haver side para medir: um bloco mudo nao prova nada.
    EXPECT_GT(normalSide, 1.0f);
    EXPECT_NEAR(narrowSide, normalSide * 0.5f, normalSide * 0.1f);
}

TEST(StereoWidth, WideAddsHaasSpread) {
    // com pan ao centro o mid/side nao faz nada (side = 0), e a unica diferenca
    // que resta e' o atraso do haas no canal direito: a largura tem de separar
    // os canais num material que nasceu igual dos dois lados.
    const auto wide = run(2.0f);

    float difference = 0.0f;
    for (std::size_t i = 0; i < wide.left.size(); ++i) {
        difference = std::max(difference, std::abs(wide.left[i] - wide.right[i]));
    }
    EXPECT_GT(difference, 0.01f);
}

TEST(StereoWidth, HaasDelayFollowsTheWidth) {
    // o atraso tem de ser dirigido pela largura, nao um valor fixo. material de
    // banda larga: a correlacao cruzada entre canais tem um pico unico, ao
    // contrario de uma senoide, que repete a cada periodo. com pan ao centro o
    // mid/side nao toca no sinal, por isso o pico mede so' o haas.
    std::vector<float> material(48000);
    std::uint32_t state = 0x12345678u;
    for (auto& sample : material) {
        state = state * 1664525u + 1013904223u;
        sample = 0.3f * (static_cast<float>(state >> 9) / 4194304.0f - 1.0f);
    }

    const auto measure = [&material](float width) {
        GranularEngine engine;
        engine.prepare(kSampleRate, kBlock);
        engine.setSource(material.data(), material.size());
        engine.setSounding(true);

        auto params = GranularParams {};
        params.grainSizeMs = 20.0f;
        params.densityGrainsPerSec = 40.0f;
        params.position = 0.5f;
        params.pan = 0.0f;
        params.widthStereo = width;

        std::vector<float> left(kBlock, 0.0f);
        std::vector<float> right(kBlock, 0.0f);
        std::vector<float> allLeft;
        std::vector<float> allRight;
        for (int block = 0; block < 80; ++block) {
            engine.processBlock(left.data(), right.data(), kBlock, params);
            allLeft.insert(allLeft.end(), left.begin(), left.end());
            allRight.insert(allRight.end(), right.begin(), right.end());
        }

        int bestLag = 100;
        double best = -1.0e30;
        for (int lag = 100; lag <= 800; ++lag) {
            double sum = 0.0;
            for (std::size_t i = static_cast<std::size_t>(lag); i < allLeft.size(); i += 3) {
                sum += static_cast<double>(allRight[i]) *
                       static_cast<double>(allLeft[i - static_cast<std::size_t>(lag)]);
            }
            if (sum > best) {
                best = sum;
                bestLag = lag;
            }
        }
        return bestLag;
    };

    // 6 ms e 12 ms a 48 kHz. a folga cobre a interpolacao e a rampa.
    EXPECT_NEAR(measure(1.5f), 288, 8);
    EXPECT_NEAR(measure(2.0f), 576, 8);
}

TEST(StereoWidth, OutOfRangeClampsInsteadOfExploding) {
    const auto clamped = run(std::nanf(""));
    for (std::size_t i = 0; i < clamped.left.size(); ++i) {
        EXPECT_TRUE(std::isfinite(clamped.left[i]));
        EXPECT_TRUE(std::isfinite(clamped.right[i]));
    }
}

} // namespace

