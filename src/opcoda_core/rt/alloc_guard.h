#pragma once

#include <cstddef>

namespace opcoda::rt {

// o operator new global consulta esta flag. o motor marca entrada e saida de
// process(), e qualquer alocacao dentro da janela e' bug de tempo real.
// e' o portao C virado codigo, nao revisao manual.
namespace alloc_guard {

[[nodiscard]] bool inAudioThread() noexcept;
[[nodiscard]] std::size_t violationCount() noexcept;
void resetViolationCount() noexcept;
void noteAllocation() noexcept;
void enterAudioThread() noexcept;
void leaveAudioThread() noexcept;

} // namespace alloc_guard

class ScopedAudioThread {
public:
    ScopedAudioThread() noexcept {
        alloc_guard::resetViolationCount();
        alloc_guard::enterAudioThread();
    }
    ~ScopedAudioThread() noexcept { alloc_guard::leaveAudioThread(); }

    ScopedAudioThread(const ScopedAudioThread&) = delete;
    ScopedAudioThread& operator=(const ScopedAudioThread&) = delete;

    void resetWindow() noexcept { alloc_guard::resetViolationCount(); }
};

} // namespace opcoda::rt
