#pragma once

#include <array>
#include <cstdint>

namespace opcoda::rt {


class NoteTracker {
public:
    struct Event {
        enum class Kind : std::uint8_t {
            noteOn,
            noteOff,
            sustain,
            controller,
            allNotesOff,
            other,
        };

        Kind kind {Kind::other};
        int number {0};
        int value {0};
    };

    static constexpr int kTrackedControllers = 3;
    // pilha de teclas premidas. 16 chega para dez dedos e dois pedais.
    static constexpr int kMaxHeldNotes = 16;

    void handle(const Event& event) noexcept;

    [[nodiscard]] int held() const noexcept { return sounding_ ? 1 : 0; }
    [[nodiscard]] bool sounding() const noexcept { return sounding_; }

    [[nodiscard]] int activeNote() const noexcept { return activeNote_; }

    [[nodiscard]] float velocity() const noexcept { return activeVelocity_; }

    [[nodiscard]] bool isHeld(int note) const noexcept;

    void reset() noexcept;

    [[nodiscard]] float controller(int index) const noexcept;
    void setController(int index, float normalised) noexcept;

    [[nodiscard]] bool controllerDirty(int index) const noexcept {
        return index >= 0 && index < kTrackedControllers
               && (controllersDirty_ & (1u << index)) != 0;
    }
    [[nodiscard]] bool controllersChanged() const noexcept {
        return controllersDirty_ != 0;
    }
    void clearControllersChanged() noexcept { controllersDirty_ = 0; }

private:
    void pushHeld(int note) noexcept;
    void removeHeld(int note) noexcept;
    void pushSustained(int note) noexcept;

    int pressed_ {0};
    int sustained_ {0};
    bool sustain_ {false};
    bool sounding_ {false};
    std::array<int, kMaxHeldNotes> heldNotes_ {};
    int heldCount_ {0};
    std::array<int, kMaxHeldNotes> sustainedNotes_ {};
    int sustainedCount_ {0};
    int activeNote_ {60};
    float activeVelocity_ {1.0f};
    float controllers_[kTrackedControllers] {0.0f};
    std::uint32_t controllersDirty_ {0};
};

} // namespace opcoda::rt