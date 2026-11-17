#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/rt/alloc_guard.h"
#include "opcoda_core/rt/spsc_ring.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::rt::ScopedAudioThread;
using opcoda::rt::SpscRing;

namespace {

class SampleExchange {
public:
    using Buffer = std::shared_ptr<const std::vector<float>>;
    using Pointer = const std::vector<float>*;

    bool publish(std::vector<float> samples) {
        owned_.push_back(std::make_shared<const std::vector<float>>(std::move(samples)));
        if (!incoming_.push(owned_.back().get())) {
            owned_.pop_back();
            return false;
        }
        return true;
    }

    void releaseReturned() {
        Pointer returned {nullptr};
        while (returned_.pop(returned)) {
            for (auto it = owned_.begin(); it != owned_.end(); ++it) {
                if (it->get() == returned) {
                    owned_.erase(it);
                    break;
                }
            }
        }
    }

    // roda na thread de audio.
    Pointer drainOnto(GranularEngine& engine) noexcept {
        Pointer incoming {nullptr};
        while (incoming_.pop(incoming)) {
            if (live_ != nullptr) {
                returned_.push(live_);
            }
            live_ = incoming;
            engine.setSource(live_->data(), live_->size());
        }
        return live_;
    }

private:
    SpscRing<Pointer, 4> incoming_;
    SpscRing<Pointer, 4> returned_;
    std::vector<Buffer> owned_;
    Pointer live_ {nullptr};
};

std::vector<float> makeSamples(std::size_t count, float seed) {
    std::vector<float> samples(count);
    for (std::size_t i = 0; i < count; ++i) {
        samples[i] = std::sin(static_cast<float>(i + seed) * 0.01f);
    }
    return samples;
}

} // namespace

TEST(SampleExchange, AudioThreadAllocatesNothingDuringSwap) {
    SampleExchange exchange;
    GranularEngine engine;
    engine.prepare(44100.0, 256);

    constexpr int kBlock = 256;
    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    GranularParams params;

    std::atomic<bool> running {true};
    std::atomic<std::size_t> audioViolations {0};
    std::atomic<int> swaps {0};

    // thread de audio simulada, no padrao do callback.
    std::thread audio([&] {
        while (running.load(std::memory_order_relaxed)) {
            ScopedAudioThread scope;
            scope.resetWindow();
            exchange.drainOnto(engine);
            engine.processBlock(left.data(), right.data(), kBlock, params);
            if (opcoda::rt::alloc_guard::violationCount() > 0) {
                audioViolations.fetch_add(opcoda::rt::alloc_guard::violationCount());
            }
            swaps.fetch_add(1);
        }
    });

    for (int i = 0; i < 200; ++i) {
        EXPECT_TRUE(exchange.publish(makeSamples(40000 + (i * 137), static_cast<float>(i))));
        exchange.releaseReturned();
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }

    running.store(false, std::memory_order_relaxed);
    audio.join();

    EXPECT_GT(swaps.load(), 100) << "a thread de audio processou pouco demais para provar nada";
    EXPECT_EQ(audioViolations.load(), 0u)
        << "a troca de material alocou " << audioViolations.load()
        << " vez(es) na thread de audio";
}

TEST(SampleExchange, OldBufferIsOnlyReleasedAfterEngineMovedOn) {
    SampleExchange exchange;
    GranularEngine engine;
    engine.prepare(44100.0, 256);

    ASSERT_TRUE(exchange.publish(makeSamples(65536, 1.0f)));
    const auto* first = exchange.drainOnto(engine);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->size(), 65536u);

    ASSERT_TRUE(exchange.publish(makeSamples(16384, 2.0f)));
    const auto* second = exchange.drainOnto(engine);
    ASSERT_NE(second, nullptr);

    EXPECT_EQ(second->size(), 16384u);
    EXPECT_NE(first, second);
    EXPECT_EQ(engine.sourceSize(), 16384u);

    EXPECT_NE(engine.sourceSize(), first->size());

    exchange.releaseReturned();
}

TEST(SampleExchange, RejectsPublishWhenQueueIsFull) {
    SampleExchange exchange;
    int accepted = 0;
    for (int i = 0; i < 16; ++i) {
        if (exchange.publish(makeSamples(1024, static_cast<float>(i)))) {
            ++accepted;
        }
    }
    EXPECT_LT(accepted, 16) << "a fila nunca recusou: a capacidade esta errada";
    EXPECT_GE(accepted, 3) << "a fila recusou cedo demais para o teste provar algo";
}

TEST(SampleExchange, EngineProducesAudioAfterSwap) {
    // o swap tem efeito: nao e' so um ponteiro a passar.
    SampleExchange exchange;
    GranularEngine engine;
    engine.prepare(44100.0, 256);
    engine.setSounding(true);

    ASSERT_TRUE(exchange.publish(makeSamples(65536, 0.0f)));
    exchange.drainOnto(engine);

    GranularParams params;
    params.densityGrainsPerSec = 200.0f;

    constexpr int kBlock = 256;
    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);

    float peak = 0.0f;
    for (int block = 0; block < 200; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        for (const auto sample : left) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    EXPECT_GT(peak, 0.01f) << "o material publicado nao chegou ao motor: pico " << peak;
}
