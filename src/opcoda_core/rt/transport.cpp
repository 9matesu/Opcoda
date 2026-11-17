#include "opcoda_core/rt/transport.h"

#include <algorithm>
#include <cmath>

namespace opcoda::rt {
namespace {

bool isUsableFraction(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value < 1.0;
}

} // namespace

void Transport::prepare(double sampleRate) noexcept {
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 44100.0;
    reset();
}

void Transport::reset() noexcept {
    stop();
    requestReset();
}

void Transport::requestReset() noexcept {
    resetRequested_.store(true, std::memory_order_relaxed);
    resetSeekGeneration_.store(seekGeneration_.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
}

void Transport::setRegionLength(std::uint64_t bytes) noexcept {
    regionBytes_.store(bytes, std::memory_order_release);

    requestReset();
}

void Transport::seekToFraction(double fraction) noexcept {
    anchor_.store(std::clamp(fraction, 0.0, 1.0), std::memory_order_relaxed);
    seekGeneration_.fetch_add(1, std::memory_order_release);
}

void Transport::setRate(double rate) noexcept {
    rate_.store(std::isfinite(rate) ? std::clamp(rate, 0.25, 4.0) : 1.0,
                std::memory_order_relaxed);
}

void Transport::play() noexcept {
    playing_.store(true, std::memory_order_release);
}

void Transport::stop() noexcept {
    playing_.store(false, std::memory_order_release);
}

double Transport::durationSeconds() const noexcept {
    const auto bytes = regionBytes_.load(std::memory_order_acquire);
    if (bytes < 2 || sampleRate_ <= 0.0) {
        return 0.0;
    }

    const auto natural = static_cast<double>(bytes) / sampleRate_;
    const auto rate = rate_.load(std::memory_order_relaxed);
    return std::clamp(natural / rate, kMinSeconds, kMaxSeconds);
}

double Transport::fractionPerSample() const noexcept {
    const auto duration = durationSeconds();
    if (duration <= 0.0) {
        return 0.0;
    }
    return 1.0 / (sampleRate_ * duration);
}

double Transport::upperBound() const noexcept {
    const auto bytes = regionBytes_.load(std::memory_order_acquire);
    if (bytes < 2) {
        return 0.0;
    }
    return std::max(0.0, 1.0 - 2.0 / static_cast<double>(bytes));
}

float Transport::advance(int numSamples, bool playingSnapshot) noexcept {
    const auto upper = upperBound();
    if (upper <= 0.0) {
        position_ = 0.0;
        publishedPosition_.store(0.0, std::memory_order_relaxed);
        return 0.0f;
    }

    if (resetRequested_.exchange(false, std::memory_order_acquire)) {
        position_ = 0.0;
        appliedSeekGeneration_ = resetSeekGeneration_.load(std::memory_order_relaxed);
    }

    const auto generation = seekGeneration_.load(std::memory_order_acquire);
    if (generation != appliedSeekGeneration_) {
        appliedSeekGeneration_ = generation;
        const auto anchor = anchor_.load(std::memory_order_relaxed);
        position_ = isUsableFraction(anchor) ? std::min(anchor, upper) : 0.0;
    }

    if (numSamples > 0 && playingSnapshot) {
        const auto step = fractionPerSample();
        if (step > 0.0) {
            position_ += step * static_cast<double>(numSamples);
        }

        if (position_ >= upper) {
            position_ = 0.0;
        }
    }

    publishedPosition_.store(position_, std::memory_order_relaxed);
    return static_cast<float>(position_);
}

} // namespace opcoda::rt