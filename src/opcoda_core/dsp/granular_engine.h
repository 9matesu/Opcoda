#pragma once

#include "opcoda_core/dsp/dc_blocker.h"
#include "opcoda_core/dsp/limiter.h"
#include "opcoda_core/dsp/window.h"

#include <array>
#include <cstdint>

namespace opcoda::dsp {

struct GranularParams {
    float grainSizeMs {40.0f};
    float densityGrainsPerSec {20.0f};
    float position {0.0f};
    float spray {0.0f};
    float pitchSemitones {0.0f};
    float volumeDb {0.0f};
    float pan {0.0f};
    WindowType window {WindowType::kHann};
};

struct Grain {
    bool active {false};
    double position {};
    double readStep {};
    int remaining {};
    int windowIndex {};
    int writeIndex {0};
    float gain {};
    float panLeft {};
    float panRight {};
};

class GranularEngine {
public:
    static constexpr int kMaxVoices = 8;
    static constexpr int kMaxGrainSamples = 48000;
    static constexpr float kMaxGrainMs = 100.0f;
    static constexpr float kMinGrainMs = 1.0f;

// Numero de pontos da curva de entropia. Publico porque quem calcula a curva na
    // thread de interface tem de saber quantos pontos o motor aceita.
    static constexpr std::size_t kMaxEntropyPoints {1024};

    void prepare(double sampleRate, int maximumBlockSize) noexcept;
    void reset() noexcept;

    void setSampleRate(double sampleRate) noexcept { sampleRate_ = sampleRate; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    void setSource(const float* samples, std::size_t count) noexcept;

    // Publica o material E a curva de entropia ja reduzida, que e' o que a thread de
    // interface deve passar.
    //
    // **A curva nunca e' calculada na thread de audio.** A versao de um argumento
    // calcula-a, e isso e' O(region) dentro de processBlock: para 12 MB sao 12
    // milhoes de leituras de float, e nao aloca nada — o array do histograma e' de
    // pilha — pelo que o guard de alocacao passa e e' cego para isto. Com o
    // transporte, arrastar a regiao republica o material a cada evento de rato, e
    // cada publicacao era uma passagem completa na thread de audio. Varios ms de
    // pico por evento e' xrun.
    //
    // Quem calcula e' pe::reduceToColumns, na thread de interface, e a curva que
    // entra aqui tem no maximo kMaxEntropyPoints pontos. Se for nullptr, o motor usa
    // 4,0 bits, que e' o valor medio de um executavel, e nao calcula nada.
    void setSource(const float* samples,
                   std::size_t count,
                   const float* entropyCurve,
                   std::size_t entropyPoints) noexcept;

    [[nodiscard]] bool hasSource() const noexcept { return source_ != nullptr; }
    [[nodiscard]] std::size_t sourceSize() const noexcept { return sourceCount_; }

    // Gate com rampa linear, para as notas ligarem e desligarem sem estalo.
    //
    // Abrir e fechar a saida de uma vez produz um degrau no sinal, e um degrau
    // e' um transiente largo em frequencia: o ataque e' audivel mesmo com
    // release curto. A rampa de 5 ms custa duas multiplicacoes por amostra e
    // remove o problema.
    void setSounding(bool on) noexcept { gateTarget_ = on ? 1.0f : 0.0f; }
    [[nodiscard]] bool isSounding() const noexcept { return gateTarget_ > 0.5f; }
    void setGateSeconds(double seconds) noexcept;

    // Nivel corrente da rampa. Existe para o teste verificar a rampa em si,
    // sem depender do agendamento de graos, que precisa de dezenas de blocos
    // para produzir audio e tornaria a medicao lenta e fragil.
    [[nodiscard]] float gateLevel() const noexcept { return gateLevel_; }

    void processBlock(float* left, float* right, int numSamples,
                      const GranularParams& params) noexcept;

    [[nodiscard]] int activeVoiceCount() const noexcept;
    [[nodiscard]] int lastActiveVoices() const noexcept { return lastActiveVoices_; }

private:
    void rebuildWindow(const GranularParams& params) noexcept;
    void startGrain(int voice, const GranularParams& params, double entropy) noexcept;
    void applyGate(float* left, float* right, int numSamples) noexcept;


    double sampleRate_ {44100.0};

    const float* source_ {nullptr};
    std::size_t sourceCount_ {0};

    std::array<Grain, kMaxVoices> voices_ {};

    std::array<float, kMaxGrainSamples> windowA_ {};
    std::array<float, kMaxGrainSamples> windowB_ {};
    const float* activeWindow_ {nullptr};
    int activeWindowLength_ {0};
    WindowType activeWindowType_ {WindowType::kHann};

    double spawnAccumulator_ {0.0};

    std::array<float, kMaxEntropyPoints> entropyCurve_ {};
    std::size_t entropyPointCount_ {0};

    DcBlocker dcBlockerLeft_ {};
    DcBlocker dcBlockerRight_ {};
    Limiter limiterLeft_ {};
    Limiter limiterRight_ {};

    float gateLevel_ {0.0f};
    float gateTarget_ {0.0f};
    float gateStep_ {0.0f};

    int lastActiveVoices_ {0};
};

} // namespace opcoda::dsp
