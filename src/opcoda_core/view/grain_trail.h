#pragma once

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace opcoda::view {

class GrainTrailSet {
public:
    static constexpr int kMaxVoices = dsp::GrainTelemetry::kMaxVoices;
    static constexpr int kFrames = 24;


    struct Trail {
        std::array<float, kFrames> positions {};
        int count {0};

        [[nodiscard]] float newest() const noexcept { return count > 0 ? positions[0] : 0.0f; }
        [[nodiscard]] float oldest() const noexcept {
            return count > 0 ? positions[static_cast<std::size_t>(count - 1)] : 0.0f;
        }

        [[nodiscard]] float span() const noexcept { return newest() - oldest(); }

        [[nodiscard]] static bool validPosition(float value) noexcept {
            return value >= 0.0f && value <= 1.0f;
        }
    };

    bool push(int voice, float position) noexcept {
        if (voice < 0 || voice >= kMaxVoices || !Trail::validPosition(position)) {
            return false;
        }
        if (region_.empty()) {
            return false;
        }

        auto& trail = trails_[static_cast<std::size_t>(voice)];

        for (int f = std::min(trail.count, kFrames - 1); f > 0; --f) {
            trail.positions[static_cast<std::size_t>(f)] =
                trail.positions[static_cast<std::size_t>(f - 1)];
        }
        if (trail.count < kFrames) {
            ++trail.count;
        }
        trail.positions[0] = position;

        addresses_[static_cast<std::size_t>(voice)] =
            pe::byteForPosition(static_cast<double>(position), region_);
        active_[static_cast<std::size_t>(voice)] = true;
        return true;
    }

    void clear() noexcept {
        for (auto& trail : trails_) {
            trail = Trail {};
        }
        active_.fill(false);
    }

    void setRegion(std::uint64_t start, std::uint64_t end) noexcept {
        region_ = pe::ByteRange {start, end};
    }

    [[nodiscard]] const Trail& trail(int voice) const noexcept {
        return trails_[static_cast<std::size_t>(clampVoice(voice))];
    }

    [[nodiscard]] bool active(int voice) const noexcept {
        if (voice < 0 || voice >= kMaxVoices) {
            return false;
        }
        return active_[static_cast<std::size_t>(voice)];
    }

    [[nodiscard]] std::uint64_t address(int voice) const noexcept {
        return addresses_[static_cast<std::size_t>(clampVoice(voice))];
    }

    [[nodiscard]] bool anyActive() const noexcept {
        for (std::size_t v = 0; v < active_.size(); ++v) {
            if (active_[v]) {
                return true;
            }
        }
        return false;
    }

private:
    [[nodiscard]] static int clampVoice(int voice) noexcept {
        return (voice < 0 || voice >= kMaxVoices) ? 0 : voice;
    }

    std::array<Trail, kMaxVoices> trails_ {};
    std::array<std::uint64_t, kMaxVoices> addresses_ {};
    std::array<bool, kMaxVoices> active_ {};
    pe::ByteRange region_ {};
};

} // namespace opcoda::view