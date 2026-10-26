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

// corre `body` com a janela de audio marcada e devolve as alocacoes ocorridas
// dentro dela. e' o portao C como assercao automatica.
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
    // prova que o guard funciona: alocar dentro da janela tem de ser detectado,
    // senao o portao C nao protege nada.
    const auto violations = allocationsInsideAudioWindow([] {
        auto* leaked = new int[64];
        leaked[0] = 1;
        delete[] leaked;
    });
    EXPECT_GT(violations, 0u);
}

TEST(AllocGuard, IgnoresAllocationOutsideWindow) {
    // a flag e' por thread: alocar na thread principal nao e' violacao. corre numa
    // thread nova para nao herdar a contagem de um teste anterior.
    std::vector<double> temporary;
    std::thread worker([&] {
        temporary.resize(1024);
        EXPECT_EQ(opcoda::rt::alloc_guard::violationCount(), 0u);
        EXPECT_FALSE(opcoda::rt::alloc_guard::inAudioThread());
    });
    worker.join();
}

TEST(AllocGuard, EngineProcessBlockDoesNotAllocate) {
    // o teste que importa: o caminho real de audio, com material carregado, nao
    // pode alocar nada dentro do callback.
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
    // f020 ligado: drive, 24 dB, as tres curvas e tres rotas. sem isto o guard
    // media o motor de antes da matriz e nao provava o caminho novo.
    params.f1DriveDb = 18.0f;
    params.f1Slope = 1;
    params.f1Keytrack = 1.0f;
    params.f1Env = 0.5f;
    params.noteNumber = 72;
    params.attackCurve = -1.0f;
    params.decayCurve = 0.5f;
    params.releaseCurve = 1.0f;
    // f021: a nuvem no maximo — atraso, dispersao e jitter — pela mesma razao.
    params.cloudSize = 1.0f;
    params.pitchSpreadSemitones = 12.0f;
    params.grainJitter = 1.0f;
    params.modMatrix.routes[0] = {ModSource::kLfo, ModDest::kCutoff1, 0.5f};
    params.modMatrix.routes[1] = {ModSource::kEnvelope, ModDest::kPitch, 0.25f};
    params.modMatrix.routes[2] = {ModSource::kVelocity, ModDest::kDensity, 0.5f};
    // largura stereo fora do atalho: 1.0 sai cedo, e o guard tem de medir o
    // laco mid/side a valer. a rota a kWidth exerce o destino novo tambem.
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
    // 5 minutos de audio sintetico a 128 amostras por bloco, o menor orcamento do
    // ensaio T2. sem alocacao, sem xrun.
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
    // o caminho novo: transporte em reproducao, com o gate aberto por ele e nao
    // por nota nenhuma.
    // e' a razao de o transporte estar no nucleo. na thread de interface esta
    // assercao nao verificava nada, e o portao C falava de um caminho nunca medido.
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
            // a ordem e' a de PluginProcessor::processBlock: advance primeiro,
            // depois o gate, e o gate usa o MESMO booleano que decidiu o advance.
            // reler o atomico duas vezes abria uma janela entre o advance e o
            // setSounding: um STOP nesse intervalo dava posicao de um estado e gate
            // de outro, e o sintoma era ate' um bloco de audio parado.
            const auto playing = transport.isPlaying();
            if (playing) {
                params.position = transport.advance(kBlock, playing);
            }
            engine.setSounding(playing);
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }

        // a fase de release tambem corre dentro da janela: parar a meio e
        // processar mais 20 blocos exercita o braco release, que os 200 acima
        // nunca tocam porque o gate so abre.
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

// o setSource de quatro argumentos e' o caminho de audio, e nao aloca nem mede.
// o guard e' cego ao custo de CPU: o setSource antigo media a entropia da regiao
// inteira dentro de processBlock sem alocar (histograma de pilha), e este teste
// passava com 12 milhoes de operacoes dentro da janela. o TSan tambem nao via.
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
