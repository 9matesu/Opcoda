#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/rt/alloc_guard.h"
#include "opcoda_core/rt/transport.h"

#include <cmath>
#include <thread>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::ModDest;
using opcoda::dsp::ModSource;
using opcoda::dsp::GranularParams;
using opcoda::rt::ScopedAudioThread;
using opcoda::rt::Transport;

namespace {

template <typename Body>
std::size_t allocationsInsideAudioWindow(Body&& body) {
    ScopedAudioThread scope;
    scope.resetWindow();
    body();
    return opcoda::rt::alloc_guard::violationCount();
}

} // namespace

TEST(AllocGuard, WindowStartsEmpty) {
    const auto violations = allocationsInsideAudioWindow([] {});
    EXPECT_EQ(violations, 0u);
}

TEST(AllocGuard, FlagsAllocationMadeInsideWindow) {
    const auto violations = allocationsInsideAudioWindow([] {
        auto* leaked = new int[64];
        leaked[0] = 1;
        delete[] leaked;
    });
    EXPECT_GT(violations, 0u);
}

TEST(AllocGuard, IgnoresAllocationOutsideWindow) {
    std::vector<double> temporary;
    std::thread worker([&] {
        temporary.resize(1024);
        EXPECT_EQ(opcoda::rt::alloc_guard::violationCount(), 0u);
        EXPECT_FALSE(opcoda::rt::alloc_guard::inAudioThread());
    });
    worker.join();
}

TEST(AllocGuard, EngineProcessBlockDoesNotAllocate) {
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlock = 256;

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    // fonte alocada antes da janela, como deve ser: a interface monta antes.
    std::vector<float> source(8192);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.01f);
    }
    engine.setSource(source.data(), source.size());

    // buffers de saida tambem pre-alocados: o host reaproveita-os a cada bloco.
    std::vector<float> left(kBlock);
    std::vector<float> right(kBlock);

    GranularParams params;
    params.densityGrainsPerSec = 120.0f;
    params.grainSizeMs = 20.0f;
    params.f1DriveDb = 18.0f;
    params.f1Slope = 1;
    params.f1Keytrack = 1.0f;
    params.f1Env = 0.5f;
    params.noteNumber = 72;
    params.attackCurve = -1.0f;
    params.decayCurve = 0.5f;
    params.releaseCurve = 1.0f;
    params.modMatrix.routes[0] = {ModSource::kLfo, ModDest::kCutoff1, 0.5f};
    params.modMatrix.routes[1] = {ModSource::kEnvelope, ModDest::kPitch, 0.25f};
    params.modMatrix.routes[2] = {ModSource::kVelocity, ModDest::kDensity, 0.5f};
    params.widthStereo = 2.0f;
    params.modMatrix.routes[3] = {ModSource::kCc71, ModDest::kWidth, 0.5f};

    const auto violations = allocationsInsideAudioWindow([&] {
        for (int block = 0; block < 50; ++block) {
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }
    });

    EXPECT_EQ(violations, 0u)
        << "processBlock alocou " << violations << " vez(es) dentro da janela de audio";
}

TEST(AllocGuard, SustainedPlaybackStaysClean) {
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlock = 128;
    constexpr int kBlocks = (5 * 44100) / kBlock;

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    std::vector<float> source(16384);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.001f);
    }
    engine.setSource(source.data(), source.size());

    std::vector<float> left(kBlock);
    std::vector<float> right(kBlock);

    GranularParams params;
    params.densityGrainsPerSec = 200.0f;
    params.grainSizeMs = 100.0f;
    params.spray = 1.0f;
    params.pitchSemitones = 24.0f; // pior caso: passo de leitura maximo
    params.pan = 0.0f;

    const auto violations = allocationsInsideAudioWindow([&] {
        for (int block = 0; block < kBlocks; ++block) {
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }
    });

    EXPECT_EQ(violations, 0u)
        << "tempo real violado em " << kBlocks << " blocos de " << kBlock;
}

TEST(AllocGuard, TransportDrivenPlaybackDoesNotAllocate) {
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlock = 256;

    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(16384);
    transport.play();

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    std::vector<float> source(16384);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.01f);
    }
    engine.setSource(source.data(), source.size());

    std::vector<float> left(kBlock);
    std::vector<float> right(kBlock);

    GranularParams params;
    params.densityGrainsPerSec = 120.0f;
    params.grainSizeMs = 40.0f;

    const auto violations = allocationsInsideAudioWindow([&] {
        for (int block = 0; block < 200; ++block) {
            const auto playing = transport.isPlaying();
            if (playing) {
                params.position = transport.advance(kBlock, playing);
            }
            engine.setSounding(playing);
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }

        transport.stop();
        for (int block = 0; block < 20; ++block) {
            engine.setSounding(transport.isPlaying());
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }
    });

    EXPECT_EQ(violations, 0u)
        << "transporte alocou " << violations << " vez(es) dentro da janela de audio";

    // e a prova de que o teste acima mediu alguma coisa: a posicao andou.
    EXPECT_GT(transport.positionFraction(), 0.0f);
}

TEST(AllocGuard, PublishingAPrecomputedCurveAllocatesNothing) {
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlock = 256;

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    std::vector<float> source(1u << 20);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.001f);
    }

    std::vector<float> curve(GranularEngine::kMaxEntropyPoints);
    for (std::size_t i = 0; i < curve.size(); ++i) {
        curve[i] = 5.5f + 0.01f * static_cast<float>(i);
    }

    std::vector<float> left(kBlock);
    std::vector<float> right(kBlock);
    GranularParams params;
    params.densityGrainsPerSec = 120.0f;

    const auto violations = allocationsInsideAudioWindow([&] {
        engine.setSource(source.data(), source.size(), curve.data(), curve.size());
        engine.setSounding(true);
        for (int block = 0; block < 50; ++block) {
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }
    });

    EXPECT_EQ(violations, 0u);
}

TEST(AllocGuard, WindowIsReleasedAfterScope) {
    {
        ScopedAudioThread scope;
        EXPECT_TRUE(opcoda::rt::alloc_guard::inAudioThread());
    }
    EXPECT_FALSE(opcoda::rt::alloc_guard::inAudioThread());
}
