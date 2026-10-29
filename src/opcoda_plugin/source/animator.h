#pragma once

#include <cmath>

namespace opcoda {

// interpolacao por meia-vida exponencial:
//
//     valor += (alvo - valor) * (1 - 2^(-dt / meiaVida))
//
// so precisa do valor e do alvo. uma mola precisaria de velocidade, e um quadro
// atrasado fazia-a passar do alvo.
//
// **independente da taxa de quadros e' o ponto.** a mesma meia-vida da o mesmo
// aspecto a 20 Hz e a 144 Hz. interpolacao linear por quadro parecia igual a 60
// e a 120 Hz, e ao dobro a 240.
//
// nao aloca nem guarda estado escondido.
class Eased {
public:
    Eased() = default;

    explicit Eased(float initial) noexcept : value_(initial), target_(initial) {}

    void set(float target) noexcept {
        target_ = target;
    }

    // repoe valor e alvo sem animacao. usado quando o alvo muda de escala: animar
    // a transicao daria um salto falso.
    void jumpTo(float value) noexcept {
        value_ = value;
        target_ = value;
    }

    void tick(float deltaSeconds, float halfLifeSeconds) noexcept {
        if (halfLifeSeconds <= 0.0f || deltaSeconds <= 0.0f) {
            value_ = target_;
            return;
        }

        // log(2) como constante: e' o que faz dt/meiaVida contar meias vidas.
        constexpr float kLn2 {0.69314718055994530942f};
        const auto weight = 1.0f - std::exp(-kLn2 * deltaSeconds / halfLifeSeconds);

        value_ += (target_ - value_) * weight;
    }

    [[nodiscard]] float value() const noexcept { return value_; }

    // alvo corrente: quem ja' anima precisa de saber para onde o valor vai sem
    // guardar uma copia paralela do parametro.
    [[nodiscard]] float target() const noexcept { return target_; }

    // verdadeiro quando a diferenca ja nao se ve. serve para parar de repintar:
    // um repaint por quadro sem mudanca visivel cobra o custo da animacao.
    [[nodiscard]] bool settled(float epsilon = 0.001f) const noexcept {
        return std::fabs(target_ - value_) < epsilon;
    }

private:
    float value_ {0.0f};
    float target_ {0.0f};
};

} // namespace opcoda