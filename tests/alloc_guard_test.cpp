#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/rt/alloc_guard.h"

#include <cmath>
#include <thread>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::rt::ScopedAudioThread;

namespace {

// Executa `body` com a janela de audio marcada e devolve quantas alocacoes
// ocorreram dentro dela. E o portao C virando assercao automatica.
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
    // Prova que o guard funciona: alocar dentro da janela tem de ser
    // detectado, senao o portao C nao protege nada.
    const auto violations = allocationsInsideAudioWindow([] {
        auto* leaked = new int[64];
        leaked[0] = 1;
        delete[] leaked;
    });
    EXPECT_GT(violations, 0u);
}

TEST(AllocGuard, IgnoresAllocationOutsideWindow) {
    // A flag e' por thread: alocar na thread principal nao e violacao. O teste
    // roda numa thread nova para nao herdar a contagem de um teste anterior.
    std::vector<double> temporary;
    std::thread worker([&] {
        temporary.resize(1024);
        EXPECT_EQ(opcoda::rt::alloc_guard::violationCount(), 0u);
        EXPECT_FALSE(opcoda::rt::alloc_guard::inAudioThread());
    });
    worker.join();
}

TEST(AllocGuard, EngineProcessBlockDoesNotAllocate) {
    // O teste que importa: o caminho real de audio, com material carregado,
    // nao pode alocar nada dentro do callback.
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlock = 256;

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    // Fonte alocada antes da janela, como deve ser: a interface monta antes.
    std::vector<float> source(8192);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.01f);
    }
    engine.setSource(source.data(), source.size());

    // Buffers de saida tambem pre-alocados: o host os reaproveita a cada bloco.
    std::vector<float> left(kBlock);
    std::vector<float> right(kBlock);

    GranularParams params;
    params.densityGrainsPerSec = 120.0f;
    params.grainSizeMs = 20.0f;

    const auto violations = allocationsInsideAudioWindow([&] {
        for (int block = 0; block < 50; ++block) {
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }
    });

    EXPECT_EQ(violations, 0u)
        << "processBlock alocou " << violations << " vez(es) dentro da janela de audio";
}

TEST(AllocGuard, SustainedPlaybackStaysClean) {
    // 5 minutos de audio sintetico a 128 amostras por bloco, que e o menor
    // orcamento do ensaio T2. Sem alocacao, sem xrun.
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

TEST(AllocGuard, WindowIsReleasedAfterScope) {
    {
        ScopedAudioThread scope;
        EXPECT_TRUE(opcoda::rt::alloc_guard::inAudioThread());
    }
    EXPECT_FALSE(opcoda::rt::alloc_guard::inAudioThread());
}
