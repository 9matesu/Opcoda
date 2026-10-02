#pragma once

#include <cstdint>

namespace opcoda::rt {

// Contagem de notas e ultimos valores de CC.
//
// Fica no nucleo, sem JUCE, pela mesma razao que pe::toSamples: e' a regra que
// tem testes, e o plugin so traduz MidiMessage para Event. Se a traducao
// alocar ou fizer mal, a regra continua verificada.
//
// O sucessor de MIDIBuffer::Iterator no JUCE 8 itera por MidiMessageMetadata,
// que e' uma vista sobre os bytes do buffer. O plugin monta um Event por
// mensagem, e este codigo nao conhece JUCE.

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

    static constexpr int kTrackedControllers = 2;

    void handle(const Event& event) noexcept;

    // Vozes a soar: teclas premidas mais notas presas pelo pedal. Com o pedal
    // premido, largar o teclado nao desliga o som, como num teclado real.
    [[nodiscard]] int held() const noexcept { return sounding_ ? 1 : 0; }
    [[nodiscard]] bool sounding() const noexcept { return sounding_; }

    void reset() noexcept;

    // Ultimo valor de um indice rastreado, normalizado em [0, 1]. Devolve 0 se
    // o indice nao for rastreado, para nao haver estado invisivel dependente da
    // ordem de chamada.
    [[nodiscard]] float controller(int index) const noexcept;
    void setController(int index, float normalised) noexcept;

    [[nodiscard]] bool controllersChanged() const noexcept { return controllersChanged_; }
    void clearControllersChanged() noexcept { controllersChanged_ = false; }

private:
    // Teclas fisicamente premidas e notas que ja foram largadas mas o pedal
    // segura. Sao separados porque so o segundo grupo depende do pedal.
    int pressed_ {0};
    int sustained_ {0};
    bool sustain_ {false};
    bool sounding_ {false};
    float controllers_[kTrackedControllers] {0.0f};
    bool controllersChanged_ {false};
};

} // namespace opcoda::rt