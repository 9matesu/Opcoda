#pragma once

#include "opcoda_core/dsp/biquad.h"
#include "opcoda_core/dsp/curve.h"
#include "opcoda_core/dsp/dc_blocker.h"
#include "opcoda_core/dsp/lfo.h"
#include "opcoda_core/dsp/limiter.h"
#include "opcoda_core/dsp/mod_matrix.h"
#include "opcoda_core/dsp/window.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace opcoda::dsp {

// -- parametros --
struct GranularParams {
    float grainSizeMs {40.0f};
    float densityGrainsPerSec {20.0f};
    float position {0.0f};
    float spray {0.0f};
    float pitchSemitones {0.0f};
    float volumeDb {0.0f};
    float pan {0.0f};
    WindowType window {WindowType::kHann};
    float attackSeconds {0.005f};
    float decaySeconds {0.1f};
    float sustainLevel {1.0f};
    float releaseSeconds {0.05f};
    // curva por estagio: -1 exponencial, 0 linear, +1 logaritmica.
    float attackCurve {0.0f};
    float decayCurve {0.0f};
    float releaseCurve {0.0f};
    FilterType f1type {FilterType::kLowPass};
    float f1cutoffHz {20000.0f};
    float f1q {0.7071f};
    int f1Slope {0};
    float f1DriveDb {0.0f};
    float f1Keytrack {0.0f};
    float f1Env {0.0f};
    FilterType f2type {FilterType::kLowPass};
    float f2cutoffHz {20000.0f};
    float f2q {0.7071f};
    float lforateHz {1.0f};
    float lfodepth {0.0f};
    LFOTarget lfotarget {LFOTarget::kPitch};
    LFOWave lfowave {LFOWave::kSine};
    float grainLevel {1.0f};
    float widthStereo {1.0f};
    // detune por grao, 0..12 st. zero desliga.
    float pitchRandomSemitones {0.0f};
    int noteNumber {-1};
    float glideSeconds {0.0f};
    bool lfoSync {false};
    int lfoDiv {2};
    int lfoSteps {16};
    double hostBpm {120.0};
    double hostPpq {-1.0};
    ModMatrix modMatrix {};
    // fontes medidas pelo plugin: velocidade, mod wheel e os dois CCs, 0..1.
    float velocity {1.0f};
    float modWheel {0.0f};
    float cc74 {0.0f};
    float cc71 {0.0f};
};

struct Grain {
    bool active {false};
    double position {};
    double readStep {};
    int remaining {};
    int windowIndex {};
    int delay {0};
    double windowScale {0.0};
    int writeIndex {0};
    float gain {};
    float panLeft {};
    float panRight {};
    const float* window {nullptr};
};

struct GrainView {
    int voice {-1};

    float position {0.0f}; ///< fracao do material, 0..1.
    float gain {0.0f};     ///< 0..1, sem a rampa de saida.
    float phase {0.0f};    ///< 0..1 dentro da janela do grao.
};

class GrainTelemetry {
public:
    // igual a GranularEngine::kMaxVoices, checado em baixo.
    static constexpr int kMaxVoices = 8;

    static_assert(std::atomic<float>::is_always_lock_free,
                  "store relaxado nao pode travar: o portao C e' probatorio");
    static_assert(std::atomic<int>::is_always_lock_free,
                  "idem para o count, que e' o que o leitor sincroniza com");

    void publish(const std::array<Grain, kMaxVoices>& voices) noexcept {
        int count = 0;

        for (std::size_t voice = 0; voice < voices.size(); ++voice) {
            const auto& grain = voices[voice];
            if (!grain.active) {
                continue;
            }

            voice_[static_cast<std::size_t>(count)].store(static_cast<int>(voice),
                                                           std::memory_order_relaxed);

            position_[static_cast<std::size_t>(count)].store(
                static_cast<float>(std::isfinite(grain.position)
                                       ? std::clamp(grain.position, 0.0, 1.0)
                                       : 0.0),
                std::memory_order_relaxed);

            // NaN no ganho apaga o grao do desenho. vira zero.
            gain_[static_cast<std::size_t>(count)].store(
                std::isfinite(grain.gain) ? grain.gain : 0.0f,
                std::memory_order_relaxed);

            const auto total = grain.windowIndex + grain.remaining;
            phase_[static_cast<std::size_t>(count)].store(
                total > 0 ? static_cast<float>(grain.windowIndex) / static_cast<float>(total)
                          : 0.0f,
                std::memory_order_relaxed);

            ++count;
        }

        count_.store(count, std::memory_order_release);
    }

    // thread de interface. devolve quantos slots preencheu. zero e' valido.
    [[nodiscard]] int read(std::array<GrainView, kMaxVoices>& out) const noexcept {
        const auto count = count_.load(std::memory_order_acquire);
        if (count <= 0) {
            return 0;
        }

        const auto toRead = std::min(count, kMaxVoices);
        for (int i = 0; i < toRead; ++i) {
            const auto index = static_cast<std::size_t>(i);
            out[static_cast<std::size_t>(i)] = GrainView {
                voice_[index].load(std::memory_order_relaxed),
                position_[index].load(std::memory_order_relaxed),
                gain_[index].load(std::memory_order_relaxed),
                phase_[index].load(std::memory_order_relaxed)};
        }
        return toRead;
    }

    private:
    std::array<std::atomic<int>, kMaxVoices> voice_ {};
    std::array<std::atomic<float>, kMaxVoices> position_ {};
    std::array<std::atomic<float>, kMaxVoices> gain_ {};
    std::array<std::atomic<float>, kMaxVoices> phase_ {};
    std::atomic<int> count_ {0};
};

// -- motor --
class GranularEngine {
public:
    static constexpr int kMaxVoices = 8;
    static constexpr int kMaxGrainSamples = 768000;
    static constexpr float kMaxGrainMs = 2000.0f;
    static constexpr float kMinGrainMs = 1.0f;

    static constexpr int kWindowTableSamples = 4096;

    [[nodiscard]] static constexpr double windowScaleForLength(int length) noexcept {
        return (length > 1) ? static_cast<double>(kWindowTableSamples - 1) /
                                  static_cast<double>(length - 1)
                            : 0.0;
    }

    static constexpr std::size_t kMaxEntropyPoints {1024};

    static_assert(GrainTelemetry::kMaxVoices == kMaxVoices,
                  "GrainTelemetry::kMaxVoices tem de ser igual a kMaxVoices");

    void prepare(double sampleRate, int maximumBlockSize) noexcept;
    void reset() noexcept;

    void setSampleRate(double sampleRate) noexcept {
        if (!std::isfinite(sampleRate) || sampleRate <= 0.0) {
            return;
        }
        sampleRate_ = sampleRate;
        haasCapacity_ = std::clamp(static_cast<int>(sampleRate_ * 0.012), 1,
                                   kHaasMaxSamples - 2);
        haasWritePos_ = 0;
        haasDelay_ = 0.0f;
    }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    void setSource(const float* samples, std::size_t count) noexcept;

    void setSource(const float* samples,
                   std::size_t count,
                   const float* entropyCurve,
                   std::size_t entropyPoints) noexcept;

    [[nodiscard]] bool hasSource() const noexcept { return source_ != nullptr; }
    [[nodiscard]] std::size_t sourceSize() const noexcept { return sourceCount_; }

    void setSounding(bool on) noexcept;
    [[nodiscard]] bool isSounding() const noexcept { return soundingCmd_; }

    [[nodiscard]] float gateLevel() const noexcept { return envelopeLevel_; }

    [[nodiscard]] int envelopePhaseIndex() const noexcept {
        return static_cast<int>(envelopePhase_);
    }
    [[nodiscard]] float envelopeStagePhase() const noexcept { return stagePhase_; }

    // semitons do glide, para o teste. em producao ninguem le.
    [[nodiscard]] double glideSemitones() const noexcept { return glideSemitones_; }

    [[nodiscard]] float f1CutoffHz() const noexcept { return lastF1Cutoff_; }

    [[nodiscard]] float f2CutoffHz() const noexcept { return lastF2Cutoff_; }

    [[nodiscard]] int lastGrainLengthSamples() const noexcept { return lastGrainLength_; }

    static constexpr std::uint64_t kDefaultSeed = 0x9E3779B97F4A7C15ull;
    void setSeed(std::uint64_t seed) noexcept {
        seed_ = seed;
        grainSerial_ = 0;
    }
    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }

    // fase do lfo, para os ensaios de sync. so a thread de audio escreve.
    [[nodiscard]] double lfoPhase() const noexcept { return lfo_.phase(); }
    [[nodiscard]] float lfoBlockValue() const noexcept { return lastLfoValue_; }

    void processBlock(float* left, float* right, int numSamples,
                      const GranularParams& params) noexcept;

    [[nodiscard]] int activeVoiceCount() const noexcept;
    [[nodiscard]] int lastActiveVoices() const noexcept { return lastActiveVoices_; }

    // estado dos graos para o display. escreve no fim do bloco; a interface le.
    [[nodiscard]] const GrainTelemetry& telemetry() const noexcept { return telemetry_; }

private:
    enum class EnvelopePhase : std::uint8_t { idle, attack, decay, sustain, release };

    struct LFOSettings {
        float rateHz {1.0f};
        float depth {0.0f};
        LFOTarget target {LFOTarget::kPitch};
        LFOWave wave {LFOWave::kSine};
        // 1..16 degraus por ciclo; 16 e' "sem quantizacao".
        int steps {16};
    };

    void rebuildWindow(const GranularParams& params) noexcept;
    void startGrain(int voice,
                    const GranularParams& params,
                    double entropy,
                    int blockOffset,
                    const LFOSettings& lfo) noexcept;
    void applyGate(float* left, float* right, int numSamples,
                   const GranularParams& params) noexcept;
    void applyStereoWidth(float* left, float* right, int numSamples,
                          float targetWidth) noexcept;

    void applyDriveClip(float* left, float* right, int numSamples,
                        float driveDb) noexcept;
    void updateGlide(const GranularParams& params, int numSamples) noexcept;
    // matriz aplicada a copia dos parametros. fontes: params e estado do motor.
    void applyModMatrix(GranularParams& params, float lfoValue) noexcept;
    [[nodiscard]] float entropyAtPosition(float position) const noexcept;

    double sampleRate_ {44100.0};

    const float* source_ {nullptr};
    std::size_t sourceCount_ {0};

    std::array<Grain, kMaxVoices> voices_ {};

    static constexpr std::size_t kWindowTypeCount =
        static_cast<std::size_t>(WindowType::kBlackman) + 1;
    std::array<std::array<float, kWindowTableSamples>, kWindowTypeCount> windowTables_ {};
    const float* activeWindow_ {nullptr};
    WindowType activeWindowType_ {WindowType::kHann};

    double spawnAccumulator_ {0.0};

    // glide: de onde veio, para onde vai, quanto falta.
    double glideSemitones_ {0.0};
    double glideFrom_ {0.0};
    double glideTarget_ {0.0};
    double glideRemaining_ {0.0};

    double syncPhaseOffset_ {0.0};
    bool syncEngaged_ {false};

    std::array<float, kMaxEntropyPoints> entropyCurve_ {};
    std::size_t entropyPointCount_ {0};

    DcBlocker dcBlockerLeft_ {};
    DcBlocker dcBlockerRight_ {};
    Limiter limiterLeft_ {};
    Limiter limiterRight_ {};

    std::array<Biquad, kMaxVoices> filterA_ {};
    std::array<Biquad, kMaxVoices> filterA2_ {};
    std::array<Biquad, kMaxVoices> filterB_ {};
    BiquadCoeffs coeffsA_ {};
    BiquadCoeffs coeffsB_ {};
    FilterType lastF1Type_ {FilterType::kLowPass};
    float lastF1Cutoff_ {-1.0f};
    float lastF1Q_ {-1.0f};
    FilterType lastF2Type_ {FilterType::kLowPass};
    float lastF2Cutoff_ {-1.0f};
    float lastF2Q_ {-1.0f};
    double lastFilterSampleRate_ {-1.0};

    float envelopeLevel_ {0.0f};
    bool soundingCmd_ {false};
    EnvelopePhase envelopePhase_ {EnvelopePhase::idle};
    float releaseStartLevel_ {0.0f};
    float stagePhase_ {0.0f};
    float attackStartLevel_ {0.0f};

    // semente e o que o ultimo grao recebeu da nuvem.
    std::uint64_t seed_ {kDefaultSeed};
    std::uint64_t grainSerial_ {0};
    int lastGrainLength_ {0};

    int lastActiveVoices_ {0};
    float lastWidth_ {1.0f};
    // teto do clip do drive, com rampa entre blocos para a automacao nao estalar.
    float lastClipCeiling_ {1.0f};
    static constexpr int kHaasMaxSamples {4096};
    std::array<float, kHaasMaxSamples> haasBuffer_ {};
    int haasWritePos_ {0};
    float haasDelay_ {0.0f};
    int haasCapacity_ {1};

    GrainTelemetry telemetry_ {};

    LFO lfo_ {};
    float lastLfoValue_ {0.0f};
};

} // namespace opcoda::dsp
