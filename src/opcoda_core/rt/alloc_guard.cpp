#include "opcoda_core/rt/alloc_guard.h"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace opcoda::rt::alloc_guard {
namespace {

// Por thread: a janela e' da thread de audio, e alocacoes da thread principal
// durante o teste nao contam como violacao.
thread_local bool tlsInAudioThread = false;
thread_local std::size_t tlsWindowCount = 0;

} // namespace

bool inAudioThread() noexcept {
    return tlsInAudioThread;
}

std::size_t violationCount() noexcept {
    return tlsWindowCount;
}

void resetViolationCount() noexcept {
    tlsWindowCount = 0;
}

void noteAllocation() noexcept {
    if (tlsInAudioThread) {
        ++tlsWindowCount;
    }
}

void enterAudioThread() noexcept {
    tlsInAudioThread = true;
}

void leaveAudioThread() noexcept {
    tlsInAudioThread = false;
}

} // namespace opcoda::rt::alloc_guard

void* operator new(std::size_t size) {
    opcoda::rt::alloc_guard::noteAllocation();
    void* pointer = std::malloc(size != 0 ? size : 1);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}

void* operator new[](std::size_t size) {
    opcoda::rt::alloc_guard::noteAllocation();
    void* pointer = std::malloc(size != 0 ? size : 1);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}
