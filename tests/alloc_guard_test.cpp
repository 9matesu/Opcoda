#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/rt/alloc_guard.h"
#include "opcoda_core/rt/transport.h"

#include <cmath>
#include <thread>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::rt::ScopedAudioThread;
using opcoda::rt::Transport;

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

TEST(AllocGuard, TransportDrivenPlaybackDoesNotAllocate) {
    // O caminho novo: transporte em reproducao, com o gate aberto por ele e nao
    // por nota nenhuma.
    //
    // Este teste e' a razao de o transporte estar no nucleo e nao no editor. Se
    // estivesse na thread de interface, esta asercao nao teria nada que verificar
    // e o portao C passaria a falar de um caminho de audio que nunca foi medido.
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
            // A ordem e' a de PluginProcessor::processBlock: advance primeiro,
            // depois o gate — e o gate usa o MESMO booleano que decidiu o advance.
            //
            // Rele o atómico duas vezes, como a primeira versao fazia, seria uma
            // segunda leitura entre o advance e o setSounding. Se o utilizador
            // carregasse STOP nesse intervalo, a posicao seria de um estado e o
            // gate de outro, e o sintoma seria ate' um bloco de audio parado.
            const auto playing = transport.isPlaying();
            if (playing) {
                params.position = transport.advance(kBlock, playing);
            }
            engine.setSounding(playing);
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }

        // A fase de release tambem corre dentro da janela: parar a meio e
        // processar mais 20 blocos exercita o braco release do switch, que os
        // 200 blocos acima nunca tocam porque o gate so abre.
        transport.stop();
        for (int block = 0; block < 20; ++block) {
            engine.setSounding(transport.isPlaying());
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }
    });

    EXPECT_EQ(violations, 0u)
        << "transporte alocou " << violations << " vez(es) dentro da janela de audio";

    // E a prova de que o teste acima mediu alguma coisa: a posicao andou.
    EXPECT_GT(transport.positionFraction(), 0.0f);
}

// **O setSource de quatro argumentos e' o caminho de audio, e nao aloca nem mede.**
//
// O guard de alocacao e' cego ao custo de CPU: o setSource antigo media a entropia
// de toda a regiao dentro de processBlock, e nao alocava nada — o histograma era de
// pilha — pelo que este teste passava com 12 milhoes de operacoes dentro da janela
// de audio. E' o mesmo caminho que o TSan nao veria.
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
