#pragma once

#include <atomic>
#include <cstdint>

namespace opcoda::rt {

class Transport {
public:
    static constexpr double kMaxSeconds {30.0};

    static constexpr double kMinSeconds {0.1};

    static_assert(std::atomic<bool>::is_always_lock_free,
                  "Transport exige atomic<bool> sem lock");
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Transport exige atomic<uint64_t> sem lock");
    static_assert(std::atomic<double>::is_always_lock_free,
                  "Transport exige atomic<double> sem lock");

    void prepare(double sampleRate) noexcept;

    void setRegionLength(std::uint64_t bytes) noexcept;

    void seekToFraction(double fraction) noexcept;

    void play() noexcept;
    void stop() noexcept;

    void setRate(double rate) noexcept;

    // repoe o estado inicial e para. chamado de releaseResources.
    void reset() noexcept;

    [[nodiscard]] bool isPlaying() const noexcept {
        return playing_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool hasRegion() const noexcept {
        return regionBytes_.load(std::memory_order_acquire) >= 3;
    }

    [[nodiscard]] float advance(int numSamples, bool playingSnapshot) noexcept;

    [[nodiscard]] float positionFraction() const noexcept {
        return static_cast<float>(publishedPosition_.load(std::memory_order_relaxed));
    }

    [[nodiscard]] double durationSeconds() const noexcept;

private:
    [[nodiscard]] double fractionPerSample() const noexcept;

    // lado de cima da posicao. abaixo de 1,0 porque 1,0 e' silencio.
    [[nodiscard]] double upperBound() const noexcept;

    void requestReset() noexcept;

    double sampleRate_ {44100.0};

    double position_ {0.0};
    std::uint64_t appliedSeekGeneration_ {0};

    std::atomic<std::uint64_t> regionBytes_ {0};
    std::atomic<bool> playing_ {false};
    std::atomic<double> anchor_ {0.0};
    std::atomic<double> rate_ {1.0};
    std::atomic<std::uint64_t> seekGeneration_ {0};
    std::atomic<bool> resetRequested_ {false};
    std::atomic<std::uint64_t> resetSeekGeneration_ {0};
    std::atomic<double> publishedPosition_ {0.0};
};

} // namespace opcoda::rt