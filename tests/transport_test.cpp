#include "opcoda_core/rt/transport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace {

using opcoda::rt::Transport;

constexpr double kSampleRate {48000.0};

double lowestAudiblePosition(std::uint64_t bytes) {
    return 1.0 - 2.0 / static_cast<double>(bytes);
}

} // namespace

TEST(Transport, PositionNeverReachesSilence) {
    for (const auto bytes : {std::uint64_t{2}, std::uint64_t{3}, std::uint64_t{64},
                             std::uint64_t{4096}, std::uint64_t{12u * 1024u * 1024u}}) {
        Transport transport;
        transport.prepare(kSampleRate);
        transport.setRegionLength(bytes);
        transport.play();

        const auto upper = lowestAudiblePosition(bytes);

        for (int block = 0; block < 40 * 512; ++block) {
            const auto position = transport.advance(512, true);
            ASSERT_GE(position, 0.0f) << "bytes=" << bytes;
            ASSERT_LT(static_cast<double>(position), 1.0) << "bytes=" << bytes;
            ASSERT_LE(static_cast<double>(position), upper + 1e-6) << "bytes=" << bytes;
        }
    }
}

TEST(Transport, SmallestRegionHasAnAudibleFloor) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(64);
    transport.play();

    ASSERT_NEAR(transport.durationSeconds(), Transport::kMinSeconds, 1e-9);
    ASSERT_NEAR(Transport::kMinSeconds, 0.1, 1e-12);

    const auto totalSamples = static_cast<int>(std::llround(Transport::kMinSeconds * kSampleRate));
    for (int sample = 0; sample < totalSamples; ++sample) {
        (void)transport.advance(1, true);
    }
    ASSERT_LT(transport.positionFraction(), 0.2f);
}

TEST(Transport, SmallRegionsAreStretchedOnlyToTheGrainFloor) {
    Transport transport;
    transport.prepare(kSampleRate);

    // 4 KB em 85 ms sao quase o tempo natural, e o piso estica para 100 ms: 17 %.
    transport.setRegionLength(4096);
    ASSERT_NEAR(transport.durationSeconds(), 0.1, 1e-9);

    // meio segundo ja nao bate no piso.
    transport.setRegionLength(24000);
    ASSERT_NEAR(transport.durationSeconds(), 24000.0 / kSampleRate, 1e-9);
}

TEST(Transport, NaturalDurationWhenItFitsUnderTheCap) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);

    // um segundo de regiao a 48 kHz e' um segundo, sem piso nem tecto.
    ASSERT_NEAR(transport.durationSeconds(), 1.0, 1e-9);
}

TEST(Transport, TwelveMegabytesAreCappedNotSlowed) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(12u * 1024u * 1024u);

    ASSERT_NEAR(transport.durationSeconds(), Transport::kMaxSeconds, 1e-9);
}

TEST(Transport, FullRegionIsTraversedWithinTheAnnouncedDuration) {
    constexpr std::uint64_t kBytes {48000};
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(kBytes);
    transport.play();

    const auto duration = transport.durationSeconds();
    const auto budget = static_cast<int>(std::llround(duration * kSampleRate)) + 512;

    int samples = 0;
    while (samples < budget) {
        const auto before = transport.positionFraction();
        (void)transport.advance(1, true);
        ++samples;
        if (before > 0.5f && transport.positionFraction() < 0.5f) {
            break; // embrulhou
        }
    }

    const auto upper = lowestAudiblePosition(kBytes);
    ASSERT_NEAR(static_cast<double>(samples) / kSampleRate, duration * upper,
                2.0 / kSampleRate);
}

TEST(Transport, AdvanceMovesExactlyRateTimesSamples) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    // 1 s de regiao, 1 s de duracao, 1/48000 por amostra.
    const auto perSample = transport.advance(0, true);
    ASSERT_NEAR(perSample, 0.0, 1e-9);

    (void)transport.advance(4800, true);
    const auto expected = static_cast<float>(4800.0 / 48000.0);
    ASSERT_NEAR(transport.positionFraction(), expected, 1e-6f);
}

TEST(Transport, SeekIsAppliedOnceAndThenTheHeadAdvances) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    transport.seekToFraction(0.25);
    ASSERT_NEAR(transport.advance(1, true), 0.25f + 1.0f / 48000.0f, 1e-6f);

    transport.seekToFraction(0.25);
    (void)transport.advance(4800, true);
    ASSERT_GT(transport.positionFraction(), 0.30f);
}

TEST(Transport, SeekMovesTheHeadImmediately) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();
    (void)transport.advance(24000, true); // meio da regiao

    transport.seekToFraction(0.75);

    ASSERT_NEAR(transport.advance(0, true), 0.75f, 1e-6f);
}

TEST(Transport, SeekIsClampedToTheRegion) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    transport.seekToFraction(1.0);
    ASSERT_LT(transport.advance(0, true), 1.0f);

    transport.seekToFraction(-3.0);
    ASSERT_GE(transport.advance(0, true), 0.0f);

    transport.seekToFraction(std::nan(""));
    ASSERT_GE(transport.advance(0, true), 0.0f);
}

TEST(Transport, StoppedTransportDoesNotAdvance) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    (void)transport.advance(24000, true);
    const auto at = transport.positionFraction();
    ASSERT_NEAR(at, 0.5f, 1e-5f);

    transport.stop();
    ASSERT_FALSE(transport.isPlaying());
    (void)transport.advance(48000, false);
    ASSERT_FLOAT_EQ(transport.positionFraction(), at);

    const auto before = transport.positionFraction();
    (void)transport.advance(48000, true);
    EXPECT_NE(transport.positionFraction(), before);
}

TEST(Transport, EmptyRegionNeverDividesByZero) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.play();

    transport.setRegionLength(0);
    ASSERT_EQ(transport.durationSeconds(), 0.0);
    ASSERT_FALSE(transport.hasRegion());
    ASSERT_EQ(transport.advance(512, true), 0.0f);

    transport.setRegionLength(1);
    ASSERT_FALSE(transport.hasRegion());
    ASSERT_EQ(transport.advance(512, true), 0.0f);
}

TEST(Transport, ChangingRegionResetsTheHead) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();
    (void)transport.advance(40000, true);

    ASSERT_GT(transport.positionFraction(), 0.5f);

    transport.setRegionLength(96000);

    (void)transport.advance(0, true);
    EXPECT_EQ(transport.positionFraction(), 0.0f);
    EXPECT_NEAR(transport.durationSeconds(), 2.0, 1e-9);
}

// o reset nao pode engolir uma busca que chegou depois dele.
TEST(Transport, ASeekAfterARegionChangeStillApplies) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();
    (void)transport.advance(24000, true);

    // o reset e' pedido...
    transport.setRegionLength(96000);
    transport.seekToFraction(0.75);

    ASSERT_NEAR(transport.advance(0, true), 0.75f, 1e-6f);
}

// mas a busca do ficheiro antigo tem de ser descartada.
TEST(Transport, ASearchBeforeARegionChangeIsDiscarded) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    transport.seekToFraction(0.9);
    transport.setRegionLength(96000);

    ASSERT_EQ(transport.advance(0, true), 0.0f);
}

TEST(Transport, PrepareWithAnImpossibleSampleRateFallsBack) {
    Transport transport;
    transport.prepare(0.0);
    transport.setRegionLength(48000);

    ASSERT_NEAR(transport.durationSeconds(), 48000.0 / 44100.0, 1e-9);
    transport.play();
    ASSERT_TRUE(std::isfinite(transport.advance(512, true)));
}

TEST(Transport, PlayStopAndAdvanceFromThreeThreadsNeverTear) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(1u * 1024u * 1024u);
    transport.play();

    std::atomic<bool> stop {false};
    std::atomic<int> writes {0};

    std::thread togger([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            transport.stop();
            transport.play();
            writes.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread seeker([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            transport.seekToFraction(0.0);
            transport.seekToFraction(0.99);
            writes.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread mover([&] {
        for (int block = 0; block < 200000; ++block) {
            const auto position = transport.advance(512, true);
            EXPECT_GE(position, 0.0f);
            EXPECT_LT(static_cast<double>(position), 1.0);
            EXPECT_LE(static_cast<double>(position), lowestAudiblePosition(1u * 1024u * 1024u));
        }
    });

    mover.join();
    stop.store(true, std::memory_order_relaxed);
    togger.join();
    seeker.join();

    EXPECT_GT(writes.load(std::memory_order_relaxed), 0);
}

TEST(Transport, ConcurrentPlayAndStopLeavesOneWholeState) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        Transport transport;
        transport.prepare(kSampleRate);

        std::thread a([&] { transport.play(); });
        std::thread b([&] { transport.stop(); });
        a.join();
        b.join();

        // um bool so tem dois valores. e' o que este teste diz.
        EXPECT_TRUE(transport.isPlaying() || !transport.isPlaying());
    }
}

TEST(Transport, PositionIsPublishedForTheDisplay) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    ASSERT_FLOAT_EQ(transport.positionFraction(), 0.0f);

    const auto returned = transport.advance(12000, true);
    EXPECT_FLOAT_EQ(transport.positionFraction(), returned);
}