#include <gtest/gtest.h>

#include "opcoda_core/rt/atomic_param.h"
#include "opcoda_core/rt/spsc_ring.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

using opcoda::rt::AtomicParam;
using opcoda::rt::SpscRing;

TEST(AtomicParam, StoresAndLoads) {
    AtomicParam<float> param {0.5f};
    EXPECT_FLOAT_EQ(param.load(), 0.5f);
    param.store(0.75f);
    EXPECT_FLOAT_EQ(param.loadForAudio(), 0.75f);
}

TEST(AtomicParam, SmoothesTowardsTarget) {
    AtomicParam<float> param {0.0f};
    param.store(1.0f);
    const float target = param.loadForAudio();

    // Rampa com coeficiente 0.5 avanca metade da distancia por passo. O
    // historico fica com o consumidor, entao quem chama mantem `current`.
    float current = 0.0f;
    current = AtomicParam<float>::nextSmoothed(current, target, 0.5f);
    EXPECT_NEAR(current, 0.5f, 1e-6f);
    current = AtomicParam<float>::nextSmoothed(current, target, 0.5f);
    EXPECT_NEAR(current, 0.75f, 1e-6f);
    current = AtomicParam<float>::nextSmoothed(current, target, 0.5f);
    EXPECT_NEAR(current, 0.875f, 1e-6f);
}

TEST(AtomicParam, TargetSurvivesSmoothing) {
    // O atomic guarda o alvo, nao o valor suavizado: ler durante a rampa tem
    // de devolver o que a interface escreveu.
    AtomicParam<float> param {0.0f};
    param.store(1.0f);
    float current = 0.0f;
    current = AtomicParam<float>::nextSmoothed(current, param.loadForAudio(), 0.5f);
    EXPECT_FLOAT_EQ(param.loadForAudio(), 1.0f);
    EXPECT_NEAR(current, 0.5f, 1e-6f);
}

TEST(AtomicParam, CrossesThreadsWithoutCorruption) {
    // O contrato acquire/release e' o que a constitution exige: sem trava, sem
    // espera, e o valor lido e sempre um valor valido.
    AtomicParam<int> param {0};
    constexpr int kIterations = 200000;

    std::thread writer([&] {
        for (int i = 0; i < kIterations; ++i) {
            param.store(i);
        }
    });

    std::thread reader([&] {
        int previous = 0;
        for (int i = 0; i < kIterations; ++i) {
            const int value = param.loadForAudio();
            // Nunca retrocede: o escritor so cresce.
            EXPECT_GE(value, previous);
            previous = value;
        }
    });

    writer.join();
    reader.join();
}

TEST(SpscRing, PushThenPopReturnsSameValue) {
    SpscRing<int, 8> ring;
    ASSERT_TRUE(ring.push(42));
    int value = 0;
    ASSERT_TRUE(ring.pop(value));
    EXPECT_EQ(value, 42);
}

TEST(SpscRing, PopOnEmptyReturnsFalse) {
    SpscRing<int, 8> ring;
    int value = -1;
    EXPECT_FALSE(ring.pop(value));
    EXPECT_EQ(value, -1);
}

TEST(SpscRing, ReportsFullAndRejectsPush) {
    // Capacidade util = 3, porque um indice reservado distingue cheio de vazio.
    using Ring4 = SpscRing<int, 4>;
    Ring4 ring;
    EXPECT_EQ(Ring4::capacity(), std::size_t{3});
    EXPECT_TRUE(ring.push(1));
    EXPECT_TRUE(ring.push(2));
    EXPECT_TRUE(ring.push(3));
    EXPECT_FALSE(ring.push(4));
    EXPECT_EQ(ring.size(), 3u);
    EXPECT_FALSE(ring.empty());
}

TEST(SpscRing, WrapsAroundReusingSlots) {
    SpscRing<int, 4> ring;
    for (int cycle = 0; cycle < 100; ++cycle) {
        ASSERT_TRUE(ring.push(cycle)) << "cycle " << cycle;
        int value = -1;
        ASSERT_TRUE(ring.pop(value));
        EXPECT_EQ(value, cycle);
    }
}

TEST(SpscRing, MaintainsFifoOrder) {
    SpscRing<int, 16> ring;
    for (int i = 0; i < 15; ++i) {
        ASSERT_TRUE(ring.push(i));
    }
    for (int i = 0; i < 15; ++i) {
        int value = -1;
        ASSERT_TRUE(ring.pop(value));
        EXPECT_EQ(value, i);
    }
}

TEST(SpscRing, TransfersManyItemsAcrossThreads) {
    // Productor e consumidor reais: e o caminho de migracao de amostra do
    // parser para o DSP described em 06-metodologia-arquitetura.md.
    SpscRing<std::uint32_t, 256> ring;
    constexpr std::uint32_t kTotal = 100000;
    std::uint32_t received = 0;
    bool inOrder = true;

    std::thread consumer([&] {
        std::uint32_t expected = 0;
        while (expected < kTotal) {
            std::uint32_t value = 0;
            if (ring.pop(value)) {
                if (value != expected) {
                    inOrder = false;
                }
                ++expected;
            }
        }
        received = expected;
    });

    for (std::uint32_t i = 0; i < kTotal; ++i) {
        while (!ring.push(i)) {
            std::this_thread::yield();
        }
    }

    consumer.join();
    EXPECT_EQ(received, kTotal);
    EXPECT_TRUE(inOrder) << "a fila SPSC deve preservar a ordem de chegada";
}

TEST(SpscRing, ClearEmptiesQueue) {
    SpscRing<int, 8> ring;
    ring.push(1);
    ring.clear();
    EXPECT_TRUE(ring.empty());
    int value = 0;
    EXPECT_FALSE(ring.pop(value));
}
