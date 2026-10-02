#pragma once

#include <cstddef>

namespace opcoda::rt {

// O operator new global e substituido por uma versao que consulta a flag
// abaixo. O motor de audio marca a entrada e a saida de process(); qualquer
// alocacao dentro da janela e um bug de tempo real. E' o portao C virando
// codigo, e nao revisao manual.
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
