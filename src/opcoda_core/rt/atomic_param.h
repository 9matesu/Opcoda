#pragma once

#include <atomic>
#include <type_traits>

namespace opcoda::rt {

// O atômico guarda o ALVO que a interface escreveu. O histórico da rampa fica
// com o consumidor, em nextSmoothed, porque o atômico não guarda estado.
template <typename T>
class AtomicParam {
    static_assert(std::is_trivially_copyable_v<T>, "AtomicParam exige tipo trivial");

public:
    AtomicParam() = default;
    explicit AtomicParam(T initial) noexcept : value_(initial) {}

    void store(T desired) noexcept {
        value_.store(desired, std::memory_order_release);
    }

    [[nodiscard]] T load() const noexcept {
        return value_.load(std::memory_order_acquire);
    }

    [[nodiscard]] T loadForAudio() const noexcept { return load(); }

    [[nodiscard]] static T nextSmoothed(T current, T target, float coefficient) noexcept {
        return static_cast<T>(
            static_cast<double>(current) +
            (static_cast<double>(target) - static_cast<double>(current)) * coefficient);
    }

private:
    std::atomic<T> value_ {};
};

} // namespace opcoda::rt
