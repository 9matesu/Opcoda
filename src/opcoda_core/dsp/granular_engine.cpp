#include "opcoda_core/dsp/granular_engine.h"

#include "opcoda_core/dsp/window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace opcoda::dsp {
namespace {

// Semente fixa: mesmo material e mesmos parametros produzem o mesmo som, o que
// torna os ensaios T1 e T2 reproduziveis.
constexpr std::uint64_t kRngSeed = 0x9E3779B97F4A7C15ull;

double nextRandom(std::uint64_t& state) noexcept {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return static_cast<double>((state * 0x2545F4914F6CDD1Dull) >> 11) /
           static_cast<double>(1ull << 53);
}

float dbToGain(float db) noexcept {
    if (db <= -100.0f) {
        return 0.0f;
    }
    return std::pow(10.0f, db / 20.0f);
}

int clampGrainSamples(double sampleRate, float sizeMs) noexcept {
    const double clampedMs = std::clamp(static_cast<double>(sizeMs),
                                        static_cast<double>(GranularEngine::kMinGrainMs),
                                        static_cast<double>(GranularEngine::kMaxGrainMs));
    const auto samples = static_cast<int>(clampedMs * 0.001 * sampleRate);
    return std::clamp(samples, 1, GranularEngine::kMaxGrainSamples);
}

} // namespace

void GranularEngine::prepare(double sampleRate, int maximumBlockSize) noexcept {
    static_cast<void>(maximumBlockSize);

    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 44100.0;

    dcBlockerLeft_.setSampleRate(sampleRate_);
    dcBlockerRight_.setSampleRate(sampleRate_);
    dcBlockerLeft_.prepare();
    dcBlockerRight_.prepare();

    limiterLeft_.setSampleRate(sampleRate_);
    limiterRight_.setSampleRate(sampleRate_);
    limiterLeft_.prepare();
    limiterRight_.prepare();

    activeWindow_ = windowA_.data();
    activeWindowLength_ = 512;
    fillWindow(WindowType::kHann, windowA_.data(), activeWindowLength_);
    activeWindowType_ = WindowType::kHann;

    // Rampa de 5 ms para abrir e fechar sem degrau no sinal.
    setGateSeconds(0.005);

    reset();
}

void GranularEngine::setGateSeconds(double seconds) noexcept {
    const auto samples = (seconds > 0.0) ? sampleRate_ * seconds : 1.0;
    gateStep_ = static_cast<float>(1.0 / samples);
}

void GranularEngine::reset() noexcept {
    for (auto& voice : voices_) {
        voice = Grain {};
    }
    spawnAccumulator_ = 0.0;
    lastActiveVoices_ = 0;
    gateLevel_ = 0.0f;
    gateTarget_ = 0.0f;
    dcBlockerLeft_.prepare();
    dcBlockerRight_.prepare();
    limiterLeft_.prepare();
    limiterRight_.prepare();
}

void GranularEngine::setSource(const float* samples, std::size_t count) noexcept {
    // Curva zero: o motor passa a modular a dispersao por 4,0 bits, que e' a media
    // de um executavel. Quem tem a curva verdadeira passa a versao de quatro
    // argumentos, e o nucleo nunca faz trabalho de interface.
    setSource(samples, count, nullptr, 0);
}

void GranularEngine::setSource(const float* samples,
                               std::size_t count,
                               const float* entropyCurve,
                               std::size_t entropyPoints) noexcept {
    source_ = samples;
    sourceCount_ = (samples != nullptr) ? count : 0;
    entropyPointCount_ = 0;

    // **Sem material nao ha graos.** Fechar o ficheiro deixava as vozes com
    // active = true, e elas continuavam a contar em lastActiveVoices_ e a ser
    // publicadas: o display mostraria graos a ler um ficheiro que ja nao esta' la.
    //
    // **Hoje este ramo so corre no arranque.** O `publishByteRange` do
    // processador recusa um buffer vazio e o sourceBytes_ nunca e' limpo em lado
    // nenhum, portanto em producao o count == 0 so acontece antes de carregar o
    // primeiro ficheiro, onde o reset() ja postou as vozes a zero. Isto e' uma
    // invariante do motor, nao um caminho de interface: o motor nao deve depender
    // de ninguem se lembrar de fechar as vozes. Um "fechar ficheiro" a vir pelo
    // lado da interface passa por aqui com count == 0 e passa a ser necessario.
    if (sourceCount_ == 0) {
        for (auto& voice : voices_) {
            voice.active = false;
        }
        lastActiveVoices_ = 0;
    }

    // **A curva e' copiada, nao calculada.** A versao anterior media a entropia de
    // todo o material dentro de processBlock, o que e' O(regiao) na thread de audio:
    // 12 milhoes de leituras de float para um ficheiro de 12 MB. Nao aloca nada, o
    // array do histograma e' de pilha, e por isso o guard de alocacao passava e era
    // cego para isto.
    //
    // Com o transporte, arrastar a regiao republica o material a cada evento de
    // rato, e cada publicacao era uma passagem completa na thread de audio. Varios
    // ms de pico por evento e' xrun. Quem calcula e' pe::reduceToColumns, na thread
    // de interface.
    if (source_ != nullptr && sourceCount_ > 0 && entropyCurve != nullptr &&
        entropyPoints > 0) {
        const auto take = std::min(entropyPoints, kMaxEntropyPoints);
        std::copy_n(entropyCurve, take, entropyCurve_.begin());
        entropyPointCount_ = take;
    }
}

void GranularEngine::rebuildWindow(const GranularParams& params) noexcept {
    const int length = clampGrainSamples(sampleRate_, params.grainSizeMs);
    if (length == activeWindowLength_ && params.window == activeWindowType_) {
        return;
    }

    // Alterna entre dois buffers para nao invalidar o ponteiro que o agendador
    // pode estar lendo no meio de um bloco.
    float* target = (activeWindow_ == windowA_.data()) ? windowB_.data() : windowA_.data();
    fillWindow(params.window, target, length);
    activeWindow_ = target;
    activeWindowLength_ = length;
    activeWindowType_ = params.window;
}

void GranularEngine::startGrain(int voice, const GranularParams& params, double entropy) noexcept {
    Grain& grain = voices_[static_cast<std::size_t>(voice)];

    // Entropia local modula a dispersao: regiao compressada (H alto) espalha
    // mais, regiao repetitiva (H baixo) fica mais ancorada.
    std::uint64_t state = kRngSeed + static_cast<std::uint64_t>(voice) * 0x9E3779B1ull;

    const double base = std::clamp(static_cast<double>(params.position), 0.0, 1.0);
    const double spread = static_cast<double>(params.spray) * (0.5 + 0.5 * entropy / 8.0);
    grain.position = std::clamp(base + ((nextRandom(state) * 2.0 - 1.0) * spread), 0.0, 1.0);

    // Detune aleatorio por grao, somado a afinacao antes da razao. Usa a mesma
    // sequencia do spray (state), e nao uma semente nova: uma semente nova por
    // parametro partia o determinismo entre corridas, e o ensaio
    // DeterministicAcrossRuns deixava de valer. Como o desvio entra depois do
    // sorteio da posicao, o spray existente nao muda.
    //
    // NaN entra como zero: um detune NaN envenenaria readStep e a posicao, e um
    // cast de NaN para size_t na leitura e' comportamento indefinido.
    const double randomRange =
        std::isfinite(static_cast<double>(params.pitchRandomSemitones))
            ? std::clamp(static_cast<double>(params.pitchRandomSemitones), 0.0, 12.0)
            : 0.0;
    const double detune = (nextRandom(state) * 2.0 - 1.0) * randomRange;

    // A taxa de reproducao e' a razao de semitons, e o avanco por amostra tem
    // que ser ela dividida pelo comprimento da fonte: 'position' e' normalizada
    // em [0, 1], entao somar a taxa crua faz o grao varrer o arquivo inteiro em
    // poucas amostras.
    //
    // O pitch entra saneado como o detune: um pitch NaN somava NaN a razao, o
    // pow devolvia NaN, e o clamp — que nao trata NaN — devolvia NaN adiante.
    // readStep NaN envenenava a posicao e o cast para size_t na leitura era
    // indefinido. Zero e' fail-open: mantem a afinacao antiga e o grao soa.
    const double tuned = std::isfinite(static_cast<double>(params.pitchSemitones))
                             ? static_cast<double>(params.pitchSemitones)
                             : 0.0;
    const double ratio = std::pow(2.0, (tuned + detune) / 12.0);
    const double rate = std::clamp(ratio, 0.5, 2.0);
    grain.readStep = (sourceCount_ > 0) ? rate / static_cast<double>(sourceCount_) : 0.0;

    grain.remaining = activeWindowLength_;
    grain.windowIndex = 0;
    grain.active = true;

    // NaN vira silencio do grao, e nao volume cheio: um ganho NaN envenenaria a
    // mistura e sairia como NaN no barramento, e o host nao avisa. O silencio
    // de um grao e' conservador; o volume cheio seria um estouro.
    const float level = std::isfinite(params.grainLevel)
                            ? std::clamp(params.grainLevel, 0.0f, 1.0f)
                            : 0.0f;
    grain.gain = dbToGain(params.volumeDb) * 0.25f * level;

    const double p = (static_cast<double>(params.pan) + 1.0) * 0.5;
    grain.panLeft = static_cast<float>(std::cos(p * std::numbers::pi * 0.5));
    grain.panRight = static_cast<float>(std::sin(p * std::numbers::pi * 0.5));
}

int GranularEngine::activeVoiceCount() const noexcept {
    int count = 0;
    for (const auto& voice : voices_) {
        if (voice.active) {
            ++count;
        }
    }
    return count;
}

void GranularEngine::processBlock(float* left,
                                  float* right,
                                  int numSamples,
                                  const GranularParams& params) noexcept {
    if (left == nullptr || right == nullptr || numSamples <= 0) {
        return;
    }

    for (int i = 0; i < numSamples; ++i) {
        left[i] = 0.0f;
        right[i] = 0.0f;
    }

    if (source_ == nullptr || sourceCount_ == 0) {
        dcBlockerLeft_.processBlock(left, numSamples);
        dcBlockerRight_.processBlock(right, numSamples);
        // O gate avanca mesmo sem material, senao fica congelado no meio da
        // rampa e a primeira nota depois de carregar entra com ganho parcial.
        applyGate(left, right, numSamples);
        // Publica mesmo assim. Sem material nao ha graos, e o display tem de
        // deixar de os desenhar: se este caminho nao publicasse, o ultimo
        // publish continuaria na tela depois do ficheiro ser fechado.
        telemetry_.publish(voices_);
        return;
    }

    rebuildWindow(params);

    const double spawnPerSample = static_cast<double>(params.densityGrainsPerSec) / sampleRate_;
    const double lastIndex = static_cast<double>(sourceCount_ - 1);
    const double positionPoint = std::clamp(static_cast<double>(params.position), 0.0, 1.0);

    auto spawnInto = [&](int voice, int atSample) {
        const std::size_t point = (entropyPointCount_ > 0)
            ? static_cast<std::size_t>(positionPoint * static_cast<double>(entropyPointCount_ - 1))
            : 0;
        const double entropy = (entropyPointCount_ > 0)
            ? static_cast<double>(entropyCurve_[point])
            : 4.0;
        startGrain(voice, params, entropy);
        voices_[static_cast<std::size_t>(voice)].writeIndex = atSample;
    };

    // Escreve uma voz ate ela acabar ou ate o fim do bloco. Uma voz que
    // atravessa o bloco recomeca em zero: writeIndex e' a posicao dentro do
    // bloco corrente, e nao um contador global de amostras.
    auto renderVoice = [&](Grain& grain) {
        if (grain.writeIndex >= numSamples) {
            grain.writeIndex = 0;
        }
        while (grain.remaining > 0 && grain.writeIndex < numSamples) {
            const double read = grain.position * lastIndex;
            const auto index = static_cast<std::size_t>(read);
            if (index + 1 >= sourceCount_ || grain.position > 1.0) {
                grain.active = false;
                return;
            }

            const float fraction = static_cast<float>(read - static_cast<double>(index));
            const float sample = source_[index] + (source_[index + 1] - source_[index]) * fraction;
            const float envelope = activeWindow_[static_cast<std::size_t>(grain.windowIndex)];
            const float value = sample * envelope * grain.gain;

            left[grain.writeIndex] += value * grain.panLeft;
            right[grain.writeIndex] += value * grain.panRight;

            ++grain.windowIndex;
            ++grain.writeIndex;
            --grain.remaining;
            grain.position += grain.readStep;
        }
        if (grain.remaining <= 0) {
            grain.active = false;
        }
    };

    for (int i = 0; i < kMaxVoices; ++i) {
        if (voices_[static_cast<std::size_t>(i)].active) {
            renderVoice(voices_[static_cast<std::size_t>(i)]);
        }
    }

    // Agenda ao longo do bloco inteiro a partir da fracao de fase, para que a
    // densidade em graos por segundo se mantenha estavel entre blocos de
    // tamanhos diferentes.
    // A fracao de fase e' o que mantem a densidade estavel entre blocos de
    // tamanhos diferentes: a parte que nao cabe em um inteiro vai para o
    // proximo bloco em vez de se perder.
    const double exact = spawnPerSample * numSamples;
    const int whole = static_cast<int>(exact);
    spawnAccumulator_ += exact - static_cast<double>(whole);
    const int toSpawn = whole + static_cast<int>(spawnAccumulator_);
    spawnAccumulator_ -= static_cast<double>(toSpawn - whole);

    // Espalha os graos ao longo do bloco. A posicao usa o intervalo entre dois
    // graos, e nao i/spawnPerSample: com densidade baixa o segundo grao nasce
    // depois do fim do bloco, e i/spawnPerSample jogaria todos no ultimo.
    const int spacing = static_cast<int>(1.0 / spawnPerSample);
    for (int i = 0; i < toSpawn; ++i) {
        const int at = std::min(i * spacing, numSamples - 1);
        for (int voice = 0; voice < kMaxVoices; ++voice) {
            if (!voices_[static_cast<std::size_t>(voice)].active) {
                spawnInto(voice, at);
                break;
            }
        }
    }

    for (int i = 0; i < kMaxVoices; ++i) {
        if (voices_[static_cast<std::size_t>(i)].active) {
            renderVoice(voices_[static_cast<std::size_t>(i)]);
        }
    }


    dcBlockerLeft_.processBlock(left, numSamples);
    dcBlockerRight_.processBlock(right, numSamples);

    // Gate antes do limiter: fechar a rampa antes de saturar mantem o
    // behaviour do limiter independente das notas.
    applyGate(left, right, numSamples);

    limiterLeft_.processBlock(left, numSamples);
    limiterRight_.processBlock(right, numSamples);

    lastActiveVoices_ = activeVoiceCount();

    // Publica no fim, e nao dentro do renderVoice: a partir daqui as janelas e as
    // posicoes de todos os graos deste bloco ja estao escritas, e e' o que a
    // thread de interface vai ler a seguir. Uma vez por bloco e' o suficiente
    // porque o display corre a 60 Hz e o bloco dura menos.
    telemetry_.publish(voices_);
}

void GranularEngine::applyGate(float* left, float* right, int numSamples) noexcept {
    for (int i = 0; i < numSamples; ++i) {
        if (gateLevel_ != gateTarget_) {
            if (gateLevel_ < gateTarget_) {
                gateLevel_ = std::min(gateTarget_, gateLevel_ + gateStep_);
            } else {
                gateLevel_ = std::max(gateTarget_, gateLevel_ - gateStep_);
            }
        }
        left[i] *= gateLevel_;
        right[i] *= gateLevel_;
    }
}

} // namespace opcoda::dsp
