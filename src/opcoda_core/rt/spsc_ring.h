#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>

namespace opcoda::rt {

// Um produtor, um consumidor, sem alocação e sem trava. O consumidor sempre
// recebe dado velho ou novo, nunca leitura parcial nem espera: é o que elimina
// a inversão de prioridade entre a thread de interface e a de áudio.
template <typename T, std::size_t Capacity>
class SpscRing {
    static_assert(Capacity >= 2, "Capacidade mínima de 2 para distinguir cheio de vazio");
    static_assert(std::is_trivially_copyable_v<T>, "SpscRing exige tipo trivial");

public:
    SpscRing() = default;

    bool push(const T& item) noexcept {
        const std::size_t write = writeIndex_.load(std::memory_order_relaxed);
        const std::size_t next = increment(write);
        if (next == readIndex_.load(std::memory_order_acquire)) {
            return false;
        }
        slots_[write] = item;
        writeIndex_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(T& out) noexcept {
        const std::size_t read = readIndex_.load(std::memory_order_relaxed);
        if (read == writeIndex_.load(std::memory_order_acquire)) {
            return false;
        }
        out = slots_[read];
        readIndex_.store(increment(read), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return readIndex_.load(std::memory_order_acquire) ==
               writeIndex_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t read = readIndex_.load(std::memory_order_acquire);
        const std::size_t write = writeIndex_.load(std::memory_order_acquire);
        return (write >= read) ? (write - read) : (Capacity - read + write);
    }

    static constexpr std::size_t capacity() noexcept { return Capacity - 1; }

    void clear() noexcept {
        readIndex_.store(0, std::memory_order_relaxed);
        writeIndex_.store(0, std::memory_order_relaxed);
    }

private:
    [[nodiscard]] static constexpr std::size_t increment(std::size_t index) noexcept {
        return (index + 1) % Capacity;
    }

    T slots_[Capacity] {};

    // Um bloco de uma linha de cache por índice: são escritos por threads
    // diferentes em cadência alta, e sem a separação toda operação causaria
    // false sharing. O bloco não declara alignas porque o MSVC emite C4324 ao
    // empacotar struct com membro alinhado, e o portão A trata warning como
    // erro; o alinhamento natural de 64 em x86-64 já produz o mesmo efeito.
    static constexpr std::size_t kCacheLineBytes = 64;

    std::atomic<std::size_t> writeIndex_ {0};
    char writeIndexPad_[kCacheLineBytes] {};

    std::atomic<std::size_t> readIndex_ {0};
    char readIndexPad_[kCacheLineBytes] {};
};

} // namespace opcoda::rt
