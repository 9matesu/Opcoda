#pragma once

#include "opcoda_core/dsp/biquad.h"
#include "opcoda_core/dsp/curve.h"
#include "opcoda_core/dsp/dc_blocker.h"
#include "opcoda_core/dsp/lfo.h"
#include "opcoda_core/dsp/limiter.h"
#include "opcoda_core/dsp/mod_matrix.h"
#include "opcoda_core/dsp/scale.h"
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
    // envelope no mix, nao por grao. ataque e release partem do nivel atual
    // para nao criar degrau.
    float attackSeconds {0.005f};
    float decaySeconds {0.1f};
    float sustainLevel {1.0f};
    float releaseSeconds {0.05f};
    // curva por estagio: -1 exponencial, 0 linear, +1 logaritmica.
    float attackCurve {0.0f};
    float decayCurve {0.0f};
    float releaseCurve {0.0f};
    // dois filtros em serie por voz. tipo invalido cai em passa-baixa para soar,
    // nao calar. corte 20..20000 Hz, Q 0,5..12. os defaults sao transparentes.
    FilterType f1type {FilterType::kLowPass};
    float f1cutoffHz {20000.0f};
    float f1q {0.7071f};
    // filtro 1: 0 = 12 dB/oitava, 1 = 24 dB com um segundo biquad em serie.
    // drive satura antes; keytrack e f1env movem o corte em oitavas.
    int f1Slope {0};
    float f1DriveDb {0.0f};
    float f1Keytrack {0.0f};
    float f1Env {0.0f};
    FilterType f2type {FilterType::kLowPass};
    float f2cutoffHz {20000.0f};
    float f2q {0.7071f};
    // lfo: depth 0 nao muda nada. escalas: pitch +-12 st, posicao +-0,25,
    // densidade 0,5x..1,5x, cutoff +-2 oitavas.
    float lforateHz {1.0f};
    float lfodepth {0.0f};
    LFOTarget lfotarget {LFOTarget::kPitch};
    LFOWave lfowave {LFOWave::kSine};
    // nivel do grao, depois do volume. volume = saida; level = quanto cada grao
    // contribui.
    float grainLevel {1.0f};
    // detune por grao, 0..12 st. zero desliga.
    float pitchRandomSemitones {0.0f};
    // nuvem: cloud 0..1 vira 0..500 ms de atraso; pitch spread 0..24 st;
    // jitter 0..1 muda a duracao entre 0,5x e 1,5x, presa a 1..100 ms.
    float cloudSize {0.0f};
    float pitchSpreadSemitones {0.0f};
    float grainJitter {0.0f};
    // nota da pilha mono. -1 desliga; 60 (Do4) e' razao 1,0. a escala quantiza
    // antes da taxa, e root e' a tonica (0 = Do).
    int noteNumber {-1};
    ScaleType scale {ScaleType::kChromatic};
    int scaleRoot {0};
    // portamento em segundos. o tempo e' o da travessia inteira, nao depende do
    // intervalo.
    float glideSeconds {0.0f};
    // lfo no tempo do host: em sync a taxa vem de lfoDiv e do bpm, e a fase de
    // hostPpq. ppq < 0 ou NaN cai para 120 bpm.
    bool lfoSync {false};
    int lfoDiv {2};
    int lfoSteps {16};
    double hostBpm {120.0};
    double hostPpq {-1.0};
    // matriz de 8 rotas, no estado do projeto. nao e' automatizavel; o motor
    // aplica a uma copia dos parametros por bloco.
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
    // espera antes do grao, em amostras, e fator que mapeia a janela na tabela
    // quando o jitter muda a duracao. sem jitter, o fator e' 1,0.
    int delay {0};
    double windowScale {1.0};
    int writeIndex {0};
    float gain {};
    float panLeft {};
    float panRight {};
};

// -- telemetria --
// dados para desenhar um grao onde ele esta' lendo.
struct GrainView {
    // indice da voz. os slots sao compactados a cada publish, entao o slot 0 de
    // um bloco pode ser a voz 3 no bloco seguinte. o rastro segue a voz.
    int voice {-1};

    float position {0.0f}; ///< fracao do material, 0..1.
    float gain {0.0f};     ///< 0..1, sem a rampa de saida.
    float phase {0.0f};    ///< 0..1 dentro da janela do grao.
};

// estado dos graos para a interface. nao pode usar fila: quem publica e' a
// thread de audio, e fila aloca. vai por atomico, um por campo.
//
// um seqlock seria a resposta obvia e daria corrida de dados no leitor. com um
// atomico por campo nao ha corrida, mas os tres numeros de um grao podem vir de
// blocos diferentes. para desenho, tudo bem.
//
// o count tambem pode atrasar. o leitor nao deve ler os slots como "os graos de
// agora"; le como foto do ultimo publish.
class GrainTelemetry {
public:
    // igual a GranularEngine::kMaxVoices, checado em baixo.
    static constexpr int kMaxVoices = 8;

    // store que trava nao serve. no msvc x64 um store relaxado e' movss, mas isso
    // e' da plataforma, nao do standard.
    static_assert(std::atomic<float>::is_always_lock_free,
                  "store relaxado nao pode travar: o portao C e' probatorio");
    static_assert(std::atomic<int>::is_always_lock_free,
                  "idem para o count, que e' o que o leitor sincroniza com");

    // thread de audio, uma vez por bloco. compacta os graos ativos para a frente
    // para o count servir de indice.
    void publish(const std::array<Grain, kMaxVoices>& voices) noexcept {
        int count = 0;

        for (std::size_t voice = 0; voice < voices.size(); ++voice) {
            const auto& grain = voices[voice];
            if (!grain.active) {
                continue;
            }

            // a voz vai antes do resto. quem le um count novo sabe que a voz veio
            // com ele.
            voice_[static_cast<std::size_t>(count)].store(static_cast<int>(voice),
                                                           std::memory_order_relaxed);

            // position vem 0..1, mas com pitch alto passa de 1. recorta para o
            // grao nao sair do ecra. isfinite antes do clamp: clamp(NaN) e' NaN.
            position_[static_cast<std::size_t>(count)].store(
                static_cast<float>(std::isfinite(grain.position)
                                       ? std::clamp(grain.position, 0.0, 1.0)
                                       : 0.0),
                std::memory_order_relaxed);

            // NaN no ganho apaga o grao do desenho. vira zero.
            gain_[static_cast<std::size_t>(count)].store(
                std::isfinite(grain.gain) ? grain.gain : 0.0f,
                std::memory_order_relaxed);

            // fase = windowIndex / (windowIndex + remaining). grao terminado tem
            // os dois a zero; nesse caso, zero.
            const auto total = grain.windowIndex + grain.remaining;
            phase_[static_cast<std::size_t>(count)].store(
                total > 0 ? static_cast<float>(grain.windowIndex) / static_cast<float>(total)
                          : 0.0f,
                std::memory_order_relaxed);

            ++count;
        }

        // release no count. o leitor usa acquire: quem ve o count ve os campos
        // escritos antes.
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
    static constexpr int kMaxGrainSamples = 48000;
    static constexpr float kMaxGrainMs = 100.0f;
    static constexpr float kMinGrainMs = 1.0f;

    // pontos da curva de entropia. publico porque a interface precisa saber
    // quantos pontos cabem.
    static constexpr std::size_t kMaxEntropyPoints {1024};

    // a telemetria tem o seu proprio limite. se os dois divergirem, o read()
    // devolve um numero que o publish nunca escreveu.
    static_assert(GrainTelemetry::kMaxVoices == kMaxVoices,
                  "GrainTelemetry::kMaxVoices tem de ser igual a kMaxVoices");

    void prepare(double sampleRate, int maximumBlockSize) noexcept;
    void reset() noexcept;

    void setSampleRate(double sampleRate) noexcept { sampleRate_ = sampleRate; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    // thread de audio apenas, chamado de drainIncomingQueue. a interface manda
    // material pela fila SPSC; nunca chama isto. os campos escritos aqui nao sao
    // atomicos.
    void setSource(const float* samples, std::size_t count) noexcept;

    // igual, mas recebe a curva de entropia ja' reduzida. quem calcula e'
    // pe::reduceToColumns, na interface. aqui nao se calcula nada: seria O(regiao)
    // dentro do processBlock, e arrastar a regiao repetia a conta a cada evento.
    // sem curva, o motor usa 4,0 bits e segue.
    void setSource(const float* samples,
                   std::size_t count,
                   const float* entropyCurve,
                   std::size_t entropyPoints) noexcept;

    [[nodiscard]] bool hasSource() const noexcept { return source_ != nullptr; }
    [[nodiscard]] std::size_t sourceSize() const noexcept { return sourceCount_; }

    // envelope no mix, nao por voz. as vozes nascem e morrem sozinhas; a nota
    // manda na saida. abrir e fechar de uma vez e' degrau, e degrau e' estalo.
    //
    // setSounding e' aresta: true so na subida, false so na descida. o
    // processador chama a cada bloco, entao nivel reancoraria o ataque.
    // isSounding devolve o comando, nao a fase.
    void setSounding(bool on) noexcept;
    [[nodiscard]] bool isSounding() const noexcept { return soundingCmd_; }

    // nivel do envelope. o teste le isto para medir o envelope sem depender do
    // agendamento de graos.
    [[nodiscard]] float gateLevel() const noexcept { return envelopeLevel_; }

    // semitons do glide, para o teste. em producao ninguem le.
    [[nodiscard]] double glideSemitones() const noexcept { return glideSemitones_; }

    // corte do filtro 1 no ultimo calculo, ja' com lfo, keytrack e envelope.
    // para o ensaio de keytrack.
    [[nodiscard]] float f1CutoffHz() const noexcept { return lastF1Cutoff_; }

    // atraso, spread e duracao do ultimo grao. o ensaio da nuvem mede o que o
    // motor aplicou, nao o que o gerador devia ter dado.
    [[nodiscard]] int lastGrainDelaySamples() const noexcept { return lastGrainDelay_; }
    [[nodiscard]] float lastGrainSpreadSemitones() const noexcept { return lastGrainSpread_; }
    [[nodiscard]] int lastGrainLengthSamples() const noexcept { return lastGrainLength_; }

    // semente fixa por omissao: mesma fonte e mesmos parametros, mesmo som. quem
    // quiser outra nuvem chama setSeed com o relogio.
    static constexpr std::uint64_t kDefaultSeed = 0x9E3779B97F4A7C15ull;
    void setSeed(std::uint64_t seed) noexcept { seed_ = seed; }
    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }

    // fase do lfo, para os ensaios de sync. so a thread de audio escreve.
    [[nodiscard]] double lfoPhase() const noexcept { return lfo_.phase(); }

    void processBlock(float* left, float* right, int numSamples,
                      const GranularParams& params) noexcept;

    [[nodiscard]] int activeVoiceCount() const noexcept;
    [[nodiscard]] int lastActiveVoices() const noexcept { return lastActiveVoices_; }

    // estado dos graos para o display. escreve no fim do bloco; a interface le.
    [[nodiscard]] const GrainTelemetry& telemetry() const noexcept { return telemetry_; }

private:
    // idle e sustain sao paradas. attack, decay e release andam a taxa fixa por
    // amostra; nao ha mola nem velocidade guardada.
    enum class EnvelopePhase : std::uint8_t { idle, attack, decay, sustain, release };

    // lfo ja' saneado, uma vez por bloco. passado por valor para o caminho quente
    // nao revalidar por grao.
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
    // glide em semitons, uma vez por bloco. linear no tempo: a distancia nao
    // muda a duracao.
    void updateGlide(const GranularParams& params, int numSamples) noexcept;
    // matriz aplicada a copia dos parametros. fontes: params e estado do motor.
    void applyModMatrix(GranularParams& params, float lfoValue) noexcept;
    [[nodiscard]] float entropyAtPosition(float position) const noexcept;

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

    // glide: de onde veio, para onde vai, quanto falta.
    double glideSemitones_ {0.0};
    double glideFrom_ {0.0};
    double glideTarget_ {0.0};
    double glideRemaining_ {0.0};

    // offset entre fase livre e fase do ppq, fixado quando o sync liga. evita
    // degrau na troca; o seek do host continua a saltar.
    double syncPhaseOffset_ {0.0};
    bool syncEngaged_ {false};

    std::array<float, kMaxEntropyPoints> entropyCurve_ {};
    std::size_t entropyPointCount_ {0};

    DcBlocker dcBlockerLeft_ {};
    DcBlocker dcBlockerRight_ {};
    Limiter limiterLeft_ {};
    Limiter limiterRight_ {};

    // estado por voz: cada voz esta noutro ponto da sua historia. coeficientes
    // sao iguais para as oito e ficam em cache por bloco.
    std::array<Biquad, kMaxVoices> filterA_ {};
    // segundo estagio do filtro 1 no slope de 24 dB. estado proprio, senao
    // processar duas vezes no mesmo biquad nao vira cascata.
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
    // fase do estagio (0..1) e nivel de partida do ataque. com curva 0, o laco
    // antigo manda e estes dois so acompanham.
    float stagePhase_ {0.0f};
    float attackStartLevel_ {0.0f};

    // semente e o que o ultimo grao recebeu da nuvem.
    std::uint64_t seed_ {kDefaultSeed};
    int lastGrainDelay_ {0};
    float lastGrainSpread_ {0.0f};
    int lastGrainLength_ {0};

    int lastActiveVoices_ {0};

    GrainTelemetry telemetry_ {};

    // fase em voltas, sempre no inicio do bloco. avanca no fim, mesmo em
    // silencio: o lfo e' tempo absoluto.
    LFO lfo_ {};
};

} // namespace opcoda::dsp
