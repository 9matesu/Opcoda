#pragma once

#include <cmath>

namespace opcoda {

class Eased {
public:
    Eased() = default;

    explicit Eased(float initial) noexcept : value_(initial), target_(initial) {}

    void set(float target) noexcept {
        target_ = target;
    }

    void jumpTo(float value) noexcept {
        value_ = value;
        target_ = value;
    }

    void tick(float deltaSeconds, float halfLifeSeconds) noexcept {
        if (halfLifeSeconds <= 0.0f || deltaSeconds <= 0.0f) {
            value_ = target_;
            return;
        }

        // log(2) como constante: e' o que faz dt/meiaVida contar meias vidas.
        constexpr float kLn2 {0.69314718055994530942f};
        const auto weight = 1.0f - std::exp(-kLn2 * deltaSeconds / halfLifeSeconds);

        value_ += (target_ - value_) * weight;
    }

    [[nodiscard]] float value() const noexcept { return value_; }

    [[nodiscard]] float target() const noexcept { return target_; }

    [[nodiscard]] bool settled(float epsilon = 0.001f) const noexcept {
        return std::fabs(target_ - value_) < epsilon;
    }

private:
    float value_ {0.0f};
    float target_ {0.0f};
};

} // namespace opcoda