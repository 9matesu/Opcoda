#pragma once

#include <array>
#include <cstdint>

namespace opcoda::rt {

// contagem de notas e ultimos valores de CC.
//
// fica no nucleo, sem JUCE: e' a regra que tem teste, e o plugin so traduz
// MidiMessage para Event. se a traducao alocar ou fizer mal, a regra continua
// verificada.

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

    // 0 = densidade (CC74), 1 = posicao (CC71), 2 = mod wheel (CC1). a ordem e'
    // contrato do plugin, que tem os indices como constantes.
    static constexpr int kTrackedControllers = 3;
    // pilha de teclas premidas. 16 chega para dez dedos e dois pedais.
    static constexpr int kMaxHeldNotes = 16;

    void handle(const Event& event) noexcept;

    // vozes a soar: teclas premidas mais notas presas pelo pedal. com o pedal
    // premido, largar o teclado nao desliga o som.
    [[nodiscard]] int held() const noexcept { return sounding_ ? 1 : 0; }
    [[nodiscard]] bool sounding() const noexcept { return sounding_; }

    // nota ativa, 60 quando nao ha nenhuma. prioridade a ultima nota: soltar a
    // ultima tecla devolve a penultima, porque a pilha guarda a ordem.
    [[nodiscard]] int activeNote() const noexcept { return activeNote_; }

    // velocidade da ultima nota premida, 0..1. e' fonte da matriz e nao mexe no
    // ganho por si: quem decide a profundidade e' a rota.
    [[nodiscard]] float velocity() const noexcept { return activeVelocity_; }

    void reset() noexcept;

    // ultimo valor de um indice rastreado, 0..1. 0 se o indice nao for rastreado,
    // para nao haver estado invisivel dependente da ordem de chamada.
    [[nodiscard]] float controller(int index) const noexcept;
    void setController(int index, float normalised) noexcept;

    // um bit por controlador. o flag unico antigo dizia "algum CC mudou" e o
    // plugin escrevia densidade e posicao a partir dos ultimos valores de ambos:
    // mexer so' no mod wheel repunha a densidade a zero. quem consome pergunta
    // pelo indice que lhe interessa.
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
    [[nodiscard]] bool isHeld(int note) const noexcept;
    void pushSustained(int note) noexcept;

    // teclas fisicamente premidas e notas que ja' foram largadas mas o pedal
    // segura. so o segundo grupo depende do pedal, e mantem a altura.
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