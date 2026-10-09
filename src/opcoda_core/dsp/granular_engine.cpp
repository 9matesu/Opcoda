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

    reset();
}

void GranularEngine::setSounding(bool on) noexcept {
    // So a aresta dispara: o processador chama a cada bloco, e disparar por
    // nivel reancoraria o ataque sessenta vezes por segundo e a nota nunca
    // saia do ataque.
    if (on == soundingCmd_) {
        return;
    }
    soundingCmd_ = on;

    if (on) {
        // Re-disparo parte do nivel corrente, e nao do zero: no meio de um
        // release, voltar a zero antes de subir seria o proprio degrau que o
        // envelope existe para impedir.
        envelopePhase_ = EnvelopePhase::attack;
    } else {
        // O release mede a partir daqui para fechar em tempo exato, qualquer
        // que seja o nivel: com inclinacao fixa, soltar a meio do ataque
        // fecharia mais rapido do que o knob diz.
        releaseStartLevel_ = envelopeLevel_;
        envelopePhase_ = EnvelopePhase::release;
    }
}

void GranularEngine::reset() noexcept {
    for (auto& voice : voices_) {
        voice = Grain {};
    }
    for (auto& filter : filterA_) {
        filter.reset();
    }
    for (auto& filter : filterB_) {
        filter.reset();
    }
    spawnAccumulator_ = 0.0;
    lastActiveVoices_ = 0;
    envelopeLevel_ = 0.0f;
    soundingCmd_ = false;
    envelopePhase_ = EnvelopePhase::idle;
    releaseStartLevel_ = 0.0f;
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

    // Estado do filtro zerado no nascimento: grao e' evento isolado, e nao
    // continuacao da voz. Sem isto, o grao novo herdava a cauda congelada do
    // grao anterior no mesmo slot — o filtro nao avanca enquanto inativo, entao
    // nao havia decaimento no intervalo, e com Q alto a cauda entrava audivel
    // no ataque como clique. A alternativa ("voz continua") exigiria provar que
    // cauda congelada e' desejada; ninguem provou, e o reset parcial e' o que
    // um musico espera de um novo disparo.
    // MUTACAO PROBE 2: sem zero (restaurar depois)
    filterA_[static_cast<std::size_t>(voice)].reset();
    filterB_[static_cast<std::size_t>(voice)].reset();

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
        applyGate(left, right, numSamples, params);
        // Publica mesmo assim. Sem material nao ha graos, e o display tem de
        // deixar de os desenhar: se este caminho nao publicasse, o ultimo
        // publish continuaria na tela depois do ficheiro ser fechado.
        telemetry_.publish(voices_);
        return;
    }

    rebuildWindow(params);

    // Coeficientes dos dois filtros, uma vez por bloco. Recalcular por amostra
    // seria 2 trigonometrias por amostra; recalcular so quando muda custa uma
    // comparacao de 7 valores por bloco. O estado continua por voz, porque cada
    // voz esta num ponto diferente da sua historia — so os coeficientes sao
    // partilhados.
    const FilterType f1type = sanitizeFilterType(params.f1type);
    const FilterType f2type = sanitizeFilterType(params.f2type);
    if (f1type != lastF1Type_ || params.f1cutoffHz != lastF1Cutoff_ ||
        params.f1q != lastF1Q_ || f2type != lastF2Type_ ||
        params.f2cutoffHz != lastF2Cutoff_ || params.f2q != lastF2Q_ ||
        sampleRate_ != lastFilterSampleRate_) {
        coeffsA_ = makeBiquadCoeffs(f1type, params.f1cutoffHz, params.f1q, sampleRate_);
        coeffsB_ = makeBiquadCoeffs(f2type, params.f2cutoffHz, params.f2q, sampleRate_);
        lastF1Type_ = f1type;
        lastF1Cutoff_ = params.f1cutoffHz;
        lastF1Q_ = params.f1q;
        lastF2Type_ = f2type;
        lastF2Cutoff_ = params.f2cutoffHz;
        lastF2Q_ = params.f2q;
        lastFilterSampleRate_ = sampleRate_;
    }

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
    auto renderVoice = [&](int voice) {
        Grain& grain = voices_[static_cast<std::size_t>(voice)];
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

            // Os dois filtros em serie, por voz e antes do pan: cada grao tem a
            // sua historia de estado, e filtrar depois do pan seria filtrar o
            // mix com atraso de fase diferente por canal.
            const auto voiceIndex = static_cast<std::size_t>(voice);
            const float value = filterB_[voiceIndex].process(
                filterA_[voiceIndex].process(sample * envelope * grain.gain, coeffsA_),
                coeffsB_);

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
            renderVoice(i);
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
            renderVoice(i);
        }
    }


    dcBlockerLeft_.processBlock(left, numSamples);
    dcBlockerRight_.processBlock(right, numSamples);

    // Gate antes do limiter: fechar a rampa antes de saturar mantem o
    // behaviour do limiter independente das notas.
    applyGate(left, right, numSamples, params);

    limiterLeft_.processBlock(left, numSamples);
    limiterRight_.processBlock(right, numSamples);

    lastActiveVoices_ = activeVoiceCount();

    // Publica no fim, e nao dentro do renderVoice: a partir daqui as janelas e as
    // posicoes de todos os graos deste bloco ja estao escritas, e e' o que a
    // thread de interface vai ler a seguir. Uma vez por bloco e' o suficiente
    // porque o display corre a 60 Hz e o bloco dura menos.
    telemetry_.publish(voices_);
}

void GranularEngine::applyGate(float* left, float* right, int numSamples,
                                const GranularParams& params) noexcept {
    // Tempos saneados uma vez por bloco, e nao por amostra: quatro divisoes
    // por bloco nao aparecem no perfil, e por amostra seriam 4x o bloco.
    //
    // NaN volta ao default de cada fase, e nao a zero nem ao minimo: zero no
    // attack calava a nota por um parametro corrompido, e o default preserva o
    // comportamento antigo. Negativo ou zero trunca em uma amostra — o mais
    // rapido sem divisao por zero — e o teto de 5 s barra release infinito por
    // automacao perdida.
    const auto secondsOr = [](float value, float fallback) {
        if (!std::isfinite(value) || value <= 0.0f) {
            return (std::isfinite(value) && value <= 0.0f) ? 1.0f / 44100.0f : fallback;
        }
        return std::min(value, 5.0f);
    };
    const float attackSeconds = secondsOr(params.attackSeconds, 0.005f);
    const float decaySeconds = secondsOr(params.decaySeconds, 0.1f);
    const float sustain =
        std::isfinite(params.sustainLevel) ? std::clamp(params.sustainLevel, 0.0f, 1.0f) : 1.0f;
    const float releaseSeconds = secondsOr(params.releaseSeconds, 0.05f);

    const auto samplesFor = [this](float seconds) {
        return std::max(1.0f, seconds * static_cast<float>(sampleRate_));
    };
    const float attackStep = 1.0f / samplesFor(attackSeconds);
    const float decayStep = (1.0f - sustain) / samplesFor(decaySeconds);
    const float releaseStep = releaseStartLevel_ / samplesFor(releaseSeconds);

    for (int i = 0; i < numSamples; ++i) {
        switch (envelopePhase_) {
            case EnvelopePhase::idle: envelopeLevel_ = 0.0f; break;
            case EnvelopePhase::attack:
                envelopeLevel_ = std::min(1.0f, envelopeLevel_ + attackStep);
                if (envelopeLevel_ >= 1.0f) {
                    envelopePhase_ = EnvelopePhase::decay;
                }
                break;
            case EnvelopePhase::decay:
                envelopeLevel_ = std::max(sustain, envelopeLevel_ - decayStep);
                if (envelopeLevel_ <= sustain) {
                    envelopeLevel_ = sustain;
                    envelopePhase_ = EnvelopePhase::sustain;
                }
                break;
            case EnvelopePhase::sustain: envelopeLevel_ = sustain; break;
            case EnvelopePhase::release:
                envelopeLevel_ = std::max(0.0f, envelopeLevel_ - releaseStep);
                if (envelopeLevel_ <= 0.0f) {
                    envelopeLevel_ = 0.0f;
                    envelopePhase_ = EnvelopePhase::idle;
                }
                break;
        }
        left[i] *= envelopeLevel_;
        right[i] *= envelopeLevel_;
    }
}

} // namespace opcoda::dsp
