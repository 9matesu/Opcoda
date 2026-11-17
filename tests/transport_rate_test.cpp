#include "opcoda_core/rt/transport.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

using opcoda::rt::Transport;

constexpr double kSampleRate {48000.0};

} // namespace

TEST(TransportRate, ScalesAdvance) {
    Transport normal;
    normal.prepare(kSampleRate);
    normal.setRegionLength(48000);
    normal.play();

    Transport fast;
    fast.prepare(kSampleRate);
    fast.setRegionLength(48000);
    fast.setRate(2.0);
    fast.play();

    const auto slow = normal.advance(4800, true);
    const auto quick = fast.advance(4800, true);

    ASSERT_GT(slow, 0.05f);
    EXPECT_NEAR(static_cast<double>(quick), 2.0 * static_cast<double>(slow), 1e-6);
}

TEST(TransportRate, ChangeMidPlayAppliesImmediately) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    const auto before = transport.advance(4800, true);
    transport.setRate(2.0);
    const auto after = transport.advance(4800, true);

    ASSERT_GT(before, 0.05f);
    EXPECT_NEAR(static_cast<double>(after - before),
                2.0 * static_cast<double>(before), 1e-6);
}

TEST(TransportRate, IsClampedAndNaNSafe) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);

    ASSERT_NEAR(transport.durationSeconds(), 1.0, 1e-9);

    transport.setRate(100.0);
    EXPECT_NEAR(transport.durationSeconds(), 0.25, 1e-9);

    transport.setRate(0.0);
    EXPECT_NEAR(transport.durationSeconds(), 4.0, 1e-9);

    transport.setRate(-3.0);
    EXPECT_NEAR(transport.durationSeconds(), 4.0, 1e-9);

    transport.setRate(std::nan(""));
    EXPECT_NEAR(transport.durationSeconds(), 1.0, 1e-9);

    transport.setRate(std::numeric_limits<double>::infinity());
    EXPECT_NEAR(transport.durationSeconds(), 1.0, 1e-9);

    transport.setRate(0.25);
    EXPECT_NEAR(transport.durationSeconds(), 4.0, 1e-9);
    transport.setRate(4.0);
    EXPECT_NEAR(transport.durationSeconds(), 0.25, 1e-9);
}

TEST(TransportRate, FloorHoldsOnShortRegionsAtHighRate) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(64);
    transport.setRate(4.0);

    EXPECT_NEAR(transport.durationSeconds(), Transport::kMinSeconds, 1e-9);
}
