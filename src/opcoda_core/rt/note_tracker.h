#pragma once

#include <array>
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

    // 0 = densidade (CC74), 1 = posicao (CC71), 2 = roda de modulacao (CC1).
    // A ordem e' contrato do plugin: os indices sao constantes la.
    static constexpr int kTrackedControllers = 3;
    // Pilha de teclas premidas. Com o instrumento mono, a nota ativa e' o topo;
    // 16 notas chegam para dez dedos e dois pedais sem depender do host.
    static constexpr int kMaxHeldNotes = 16;

    void handle(const Event& event) noexcept;

    // Vozes a soar: teclas premidas mais notas presas pelo pedal. Com o pedal
    // premido, largar o teclado nao desliga o som, como num teclado real.
    [[nodiscard]] int held() const noexcept { return sounding_ ? 1 : 0; }
    [[nodiscard]] bool sounding() const noexcept { return sounding_; }

    // Nota ativa, 60 quando nao ha nenhuma. Prioridade a ultima nota: soltar a
    // ultima tecla devolve a penultima, e nao o silencio, porque a pilha guarda
    // a ordem em vez de so a contagem.
    [[nodiscard]] int activeNote() const noexcept { return activeNote_; }

    // Velocidade da ultima nota premida, 0..1. E' fonte da matriz de modulacao
    // (F020) e nao altera o ganho por si: quem decide a profundidade e' a rota.
    [[nodiscard]] float velocity() const noexcept { return activeVelocity_; }

    void reset() noexcept;

    // Ultimo valor de um indice rastreado, normalizado em [0, 1]. Devolve 0 se
    // o indice nao for rastreado, para nao haver estado invisivel dependente da
    // ordem de chamada.
    [[nodiscard]] float controller(int index) const noexcept;
    void setController(int index, float normalised) noexcept;

    [[nodiscard]] bool controllersChanged() const noexcept { return controllersChanged_; }
    void clearControllersChanged() noexcept { controllersChanged_ = false; }

private:
    void pushHeld(int note) noexcept;
    void removeHeld(int note) noexcept;
    [[nodiscard]] bool isHeld(int note) const noexcept;
    void pushSustained(int note) noexcept;

    // Teclas fisicamente premidas e notas que ja foram largadas mas o pedal
    // segura. Sao separados porque so o segundo grupo depende do pedal: uma
    // nota sustentada continua a soar e mantem a altura ate o pedal subir.
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
    bool controllersChanged_ {false};
};

} // namespace opcoda::rt