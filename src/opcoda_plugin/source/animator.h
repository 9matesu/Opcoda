#pragma once

#include <cmath>

namespace opcoda {

// Interpolacao por meia-vida exponencial.
//
// **Meia-vida e nao mola.** Uma mola precisa de velocidade, e velocidade precisa de
// estado: se um quadro atrasar, o valor salta e a mola passa do alvo. A meia-vida so
// precisa do valor e do alvo, e o que ela faz e'
//
//     valor += (alvo - valor) * (1 - 2^(-dt / meiaVida))
//
// que e' a mesma curva com qualquer taxa de quadros. A 20 Hz e a 144 Hz o mesmo
// meia-vida de 60 ms da o mesmo aspecto, e e' isso que interessa num display que o
// host decide a que ritmo repinta.
//
// **independente da taxa de quadros e' o ponto, nao um detalhe.** Uma interpolacao
// linear por quadro parece igual a 60 Hz e a 120 Hz, e o dobro de rapida a 240 Hz —
// e 240 Hz e' um monitor que existe. Com a forma exponencial o erro desaparece.
//
// Nao aloca, nao tem estado escondido e nao precisa de ser centrada.
class Eased {
public:
    Eased() = default;

    explicit Eased(float initial) noexcept : value_(initial), target_(initial) {}

    void set(float target) noexcept {
        target_ = target;
    }

    // Repoe o valor sem animacao. Usado quando o alvo muda de escala — o alvo deixa
    // de ser comparavel com o valor — e animar a transicao daria um salto falso.
    void jumpTo(float value) noexcept {
        value_ = value;
        target_ = value;
    }

    void tick(float deltaSeconds, float halfLifeSeconds) noexcept {
        if (halfLifeSeconds <= 0.0f || deltaSeconds <= 0.0f) {
            value_ = target_;
            return;
        }

        // log(2) escrito como constante: e' o que faz `dt / meiaVida` contar meios
        // periodos, e portanto o valor cair para metade ao fim de meiaVida.
        constexpr float kLn2 {0.69314718055994530942f};
        const auto weight = 1.0f - std::exp(-kLn2 * deltaSeconds / halfLifeSeconds);

        value_ += (target_ - value_) * weight;
    }

    [[nodiscard]] float value() const noexcept { return value_; }

    // Verdadeiro quando o valor ja' tao perto do alvo que a diferenca nao se ve.
    // Usado para parar de repintar: um repaint por quadro sem mudanca visivel e' o
    // custo que uma animacao mal feita cobra.
    [[nodiscard]] bool settled(float epsilon = 0.001f) const noexcept {
        return std::fabs(target_ - value_) < epsilon;
    }

private:
    float value_ {0.0f};
    float target_ {0.0f};
};

} // namespace opcoda