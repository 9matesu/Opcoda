#include "opcoda_core/dsp/granular_engine.h"

#include "opcoda_core/dsp/window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace opcoda::dsp {
namespace {

// -- sorteio --
// xorshift. a semente fica no motor para o ensaio poder troca-la; por omissao
// o som repete entre corridas, que e' o que T1 e T2 comparam.
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

    // todas as tabelas ficam prontas aqui. preencher no callback custava 4096
    // cos na primeira troca de tipo e voltava a pagar a cada alternancia.
    for (std::size_t i = 0; i < windowTables_.size(); ++i) {
        fillWindow(static_cast<WindowType>(i), windowTables_[i].data(), kWindowTableSamples);
    }
    activeWindow_ = windowTables_[0].data();
    activeWindowType_ = WindowType::kHann;

    // o haas guarda 12 ms; a capacidade e' o que a taxa permite dentro do array
    // fixo, e o prepare e' o unico lugar onde o nucleo preenche memoria.
    haasCapacity_ =
        std::clamp(static_cast<int>(sampleRate_ * 0.012), 1, kHaasMaxSamples - 2);

    reset();
}

// -- gate --
void GranularEngine::setSounding(bool on) noexcept {
    // so a aresta dispara. por nivel, o ataque recomecaria a cada bloco.
    if (on == soundingCmd_) {
        return;
    }
    soundingCmd_ = on;

    if (on) {
        // retrigger parte do nivel atual. zerar antes de subir cria o degrau
        // que o envelope deveria evitar.
        envelopePhase_ = EnvelopePhase::attack;
        stagePhase_ = 0.0f;
        attackStartLevel_ = envelopeLevel_;
    } else {
        // o release guarda o nivel de partida. com inclinacao fixa, soltar no
        // meio do ataque fecharia antes do tempo do knob.
        releaseStartLevel_ = envelopeLevel_;
        envelopePhase_ = EnvelopePhase::release;
        stagePhase_ = 0.0f;
    }
}

void GranularEngine::reset() noexcept {
    for (auto& voice : voices_) {
        voice = Grain {};
    }
    lfo_.reset();
    for (auto& filter : filterA_) {
        filter.reset();
    }
    for (auto& filter : filterA2_) {
        filter.reset();
    }
    for (auto& filter : filterB_) {
        filter.reset();
    }
    spawnAccumulator_ = 0.0;
    grainSerial_ = 0;
    lastActiveVoices_ = 0;
    envelopeLevel_ = 0.0f;
    soundingCmd_ = false;
    envelopePhase_ = EnvelopePhase::idle;
    releaseStartLevel_ = 0.0f;
    stagePhase_ = 0.0f;
    attackStartLevel_ = 0.0f;
    glideSemitones_ = 0.0;
    glideFrom_ = 0.0;
    glideTarget_ = 0.0;
    glideRemaining_ = 0.0;
    syncPhaseOffset_ = 0.0;
    syncEngaged_ = false;
    // a rampa da largura tambem recomeca: sem isto o primeiro bloco pos-reset
    // parte do valor anterior em vez do declarado.
    lastWidth_ = 1.0f;
    haasWritePos_ = 0;
    haasDelay_ = 0.0f;
    haasBuffer_.fill(0.0f);
    lastClipCeiling_ = 1.0f;
    dcBlockerLeft_.prepare();
    dcBlockerRight_.prepare();
    limiterLeft_.prepare();
    limiterRight_.prepare();
}

// -- material --
void GranularEngine::setSource(const float* samples, std::size_t count) noexcept {
    // sem curva, o motor nao calcula entropia. quem tem a curva usa a versao
    // de quatro argumentos.
    setSource(samples, count, nullptr, 0);
}

void GranularEngine::setSource(const float* samples,
                               std::size_t count,
                               const float* entropyCurve,
                               std::size_t entropyPoints) noexcept {
    source_ = samples;
    sourceCount_ = (samples != nullptr) ? count : 0;
    entropyPointCount_ = 0;

    // sem material, vozes mortas. sem isso elas continuam contando e o display
    // desenha graos de um ficheiro que ja saiu.
    // hoje isso so acontece no arranque; se a interface fechar o ficheiro,
    // este ramo passa a ser o caminho normal.
    if (sourceCount_ == 0) {
        for (auto& voice : voices_) {
            voice.active = false;
        }
        lastActiveVoices_ = 0;
    }

    // a curva chega pronta da interface. calcular aqui era O(regiao) dentro do
    // processBlock: um ficheiro de 12 MB virava 12 milhoes de leituras e um xrun
    // a cada arrasto.
    if (source_ != nullptr && sourceCount_ > 0 && entropyCurve != nullptr &&
        entropyPoints > 0) {
        const auto take = std::min(entropyPoints, kMaxEntropyPoints);
        std::copy_n(entropyCurve, take, entropyCurve_.begin());
        entropyPointCount_ = take;
    }
}

void GranularEngine::rebuildWindow(const GranularParams& params) noexcept {
    const auto window = sanitizeWindowType(params.window);
    if (window == activeWindowType_) {
        return;
    }

    // as quatro tabelas ja' existem: trocar de janela e' so' apontar para outra.
    // nada de fillWindow (4096 cos) na thread de audio.
    activeWindow_ = windowTables_[static_cast<std::size_t>(window)].data();
    activeWindowType_ = window;
}

// -- grao --
void GranularEngine::startGrain(int voice,
                                const GranularParams& params,
                                double entropy,
                                int blockOffset,
                                const LFOSettings& lfo) noexcept {
    Grain& grain = voices_[static_cast<std::size_t>(voice)];
    (void)entropy;

    // estado novo por grao, derivado da semente, da voz e de um contador. com o
    // estado a nascer sempre do mesmo valor, cada voz repetia exatamente o mesmo
    // spray, o mesmo detune e o mesmo spread, e a nuvem ficava parada.
    std::uint64_t state =
        seed_ + 0x9E3779B97F4A7C15ull *
                    (static_cast<std::uint64_t>(voice) +
                     (grainSerial_++) * static_cast<std::uint64_t>(kMaxVoices) + 1ull);

    // o LFO e' lido no offset do bloco em que o grao nasce. a 20 Hz, inicio e fim
    // de um bloco de 256 ja' estao a 0,13 ciclo de distancia.
    const double lfoPosition =
        (lfo.target == LFOTarget::kPosition)
            ? static_cast<double>(lfo_.valueAt(lfo.wave, lfo.rateHz, blockOffset, sampleRate_,
                                               lfo.steps)) *
                  static_cast<double>(lfo.depth) * 0.25
            : 0.0;

    const double base = std::clamp(static_cast<double>(params.position), 0.0, 1.0);
    // spray = dispersao da posicao de leitura. nao olha para entropia; se o
    // utilizador quiser, roteia entropia -> spray na matriz.
    const double spread =
        std::isfinite(static_cast<double>(params.spray))
            ? std::clamp(static_cast<double>(params.spray), 0.0, 1.0)
            : 0.0;
    grain.position =
        std::clamp(base + ((nextRandom(state) * 2.0 - 1.0) * spread) + lfoPosition, 0.0, 1.0);

    // detune por grao. usa a mesma sequencia do spray para o som repetir entre
    // corridas. NaN vira zero: NaN em readStep e' cast indefinido na leitura.
    const double randomRange =
        std::isfinite(static_cast<double>(params.pitchRandomSemitones))
            ? std::clamp(static_cast<double>(params.pitchRandomSemitones), 0.0, 12.0)
            : 0.0;
    const double detune = (nextRandom(state) * 2.0 - 1.0) * randomRange;

    // pitch spread da nuvem. so' sorteia quando o knob sai do zero, senao a
    // sequencia muda e os tres knobs a zero deixam de repetir a saida antiga.
    const double spreadRange =
        std::isfinite(static_cast<double>(params.pitchSpreadSemitones))
            ? std::clamp(static_cast<double>(params.pitchSpreadSemitones), 0.0, 24.0)
            : 0.0;
    const double spreadOffset =
        (spreadRange > 0.0) ? (nextRandom(state) * 2.0 - 1.0) * spreadRange : 0.0;

    // atraso uniforme da nuvem. o grao ocupa o slot e espera; o proximo sorteia
    // de novo, sem herdar.
    const double cloudWindow =
        std::isfinite(static_cast<double>(params.cloudSize))
            ? std::clamp(static_cast<double>(params.cloudSize), 0.0, 1.0) * 0.5
            : 0.0;
    const int delaySamples = (cloudWindow > 0.0)
                                 ? static_cast<int>(nextRandom(state) * cloudWindow * sampleRate_)
                                 : 0;

    // jitter de duracao: fator de 0,5x a 1,5x, recortado ao intervalo do knob. a
    // janela e' lida por escala; a tabela e' uma so' e nao depende do comprimento.
    const double jitter =
        std::isfinite(static_cast<double>(params.grainJitter))
            ? std::clamp(static_cast<double>(params.grainJitter), 0.0, 1.0)
            : 0.0;
    // a janela tem 4096 pontos e o grao tem grainLength: a escala leva o primeiro
    // indice a zero e o ultimo a 4095, senao um grao curto terminava com a janela
    // a meio (2 amostras caiam no pico da Hann) e o corte ouvia-se.
    int grainLength = clampGrainSamples(sampleRate_, params.grainSizeMs);
    double windowScale = windowScaleForLength(grainLength);
    if (jitter > 0.0) {
        const double factor = 1.0 + (nextRandom(state) * 2.0 - 1.0) * 0.5 * jitter;
        const double jitteredMs = std::clamp(
            static_cast<double>(params.grainSizeMs) * factor,
            static_cast<double>(kMinGrainMs), static_cast<double>(kMaxGrainMs));
        grainLength = clampGrainSamples(sampleRate_, static_cast<float>(jitteredMs));
        windowScale = windowScaleForLength(grainLength);
    }

    // LFO no pitch: +-12 st com depth cheio. entra na soma antes da razao.
    const double lfoPitch = (lfo.target == LFOTarget::kPitch)
                                ? static_cast<double>(lfo_.valueAt(
                                      lfo.wave, lfo.rateHz, blockOffset, sampleRate_, lfo.steps)) *
                                      static_cast<double>(lfo.depth) * 12.0
                                : 0.0;

    // a posicao e' normalizada em [0,1], entao o passo por amostra e' a razao
    // dividida pelo tamanho da fonte. sem a divisao o grao varre tudo em poucas
    // amostras. pitch NaN vira zero para nao envenenar o passo.
    const double tuned = std::isfinite(static_cast<double>(params.pitchSemitones))
                             ? static_cast<double>(params.pitchSemitones)
                             : 0.0;
    // a nota vem em semitons relativos a Do4, ja' quantizada pela escala. o
    // glide interpola esse termo; o knob PITCH fica de fora e responde na hora.
    const double ratio =
        std::pow(2.0, (tuned + detune + lfoPitch + spreadOffset + glideSemitones_) / 12.0);
    // quatro oitavas para cada lado. o limite antigo de 0,5..2,0 cortava a nota
    // no primeiro semitom fora da oitava central.
    const double rate = std::clamp(ratio, 0.0625, 16.0);
    grain.readStep = (sourceCount_ > 0) ? rate / static_cast<double>(sourceCount_) : 0.0;

    // **um grao que nasce no fim morre na primeira amostra.** a posicao 1,0 aponta
    // para o ultimo byte e o grao precisa de `grainLength` amostras a' frente; sem
    // margem o instrumento cala-se e parece avariado (era o clique no canto direito
    // do mapa de entropia). puxar o inicio para tras pelo comprimento do grao
    // garante material para a janela inteira sem mudar o ponto que o utilizador
    // escolheu mais do que o necessario.
    //
    // e o grao nunca ocupa mais de metade do material: sem esse tecto, uma regiao
    // curta com pitch alto pedia mais material do que existe, o tecto da posicao
    // ficava em zero e todos os graos nasciam no inicio -- o knob de posicao e o
    // spray ficavam mortos. encurtar a janela, com a escala do envelope, mantem o
    // mapa a escolher o sitio.
    if (sourceCount_ > 0 && grain.readStep > 0.0) {
        const auto maxSpanSamples = static_cast<int>(std::min(
            static_cast<double>(kMaxGrainSamples),
            std::floor(0.5 / grain.readStep)));
        if (maxSpanSamples > 0 && grainLength > maxSpanSamples) {
            grainLength = maxSpanSamples;
            windowScale = windowScaleForLength(grainLength);
        }

        const double span = static_cast<double>(grainLength) * grain.readStep;
        const double ceiling = 1.0 - std::min(span, 1.0);
        grain.position = std::clamp(grain.position, 0.0, std::max(0.0, ceiling));
    }

    grain.remaining = grainLength;
    grain.windowIndex = 0;
    grain.windowScale = windowScale;
    grain.window = activeWindow_;
    grain.delay = delaySamples;
    lastGrainDelay_ = delaySamples;
    lastGrainSpread_ = static_cast<float>(spreadOffset);
    lastGrainLength_ = grainLength;
    grain.active = true;

    // grao novo, filtro limpo. sem isso o slot herda a cauda congelada do grao
    // anterior, e com Q alto ela entra como clique no ataque.
    filterA_[static_cast<std::size_t>(voice)].reset();
    filterA2_[static_cast<std::size_t>(voice)].reset();
    filterB_[static_cast<std::size_t>(voice)].reset();

    // NaN no nivel vira silencio. ganho NaN contamina o barramento sem aviso.
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

// -- glide --
void GranularEngine::updateGlide(const GranularParams& params, int numSamples) noexcept {
    const int note = params.noteNumber;
    const double target = (note >= 0)
                              ? semitonesForNote(note, params.scale, params.scaleRoot)
                              : 0.0;

    const float glide = std::isfinite(params.glideSeconds)
                            ? std::clamp(params.glideSeconds, 0.0f, 2.0f)
                            : 0.0f;

    // glide zero: a nota entra direto.
    if (glide <= 0.0f) {
        glideSemitones_ = target;
        glideFrom_ = target;
        glideTarget_ = target;
        glideRemaining_ = 0.0;
        return;
    }

    // nota nova: parte do ponto atual e conta o tempo a partir daqui.
    if (target != glideTarget_) {
        glideFrom_ = glideSemitones_;
        glideTarget_ = target;
        glideRemaining_ = static_cast<double>(glide);
    }

    if (glideRemaining_ <= 0.0) {
        return;
    }

    const double blockSeconds = static_cast<double>(numSamples) / sampleRate_;
    const double step = std::min(blockSeconds, glideRemaining_);
    glideRemaining_ -= step;

    const double total = static_cast<double>(glide);
    const double fraction = (total > 0.0) ? (1.0 - (glideRemaining_ / total)) : 1.0;
    glideSemitones_ = glideFrom_ + (glideTarget_ - glideFrom_) * fraction;
    if (glideRemaining_ <= 0.0) {
        glideSemitones_ = glideTarget_;
    }
}

// -- bloco --
void GranularEngine::processBlock(float* left,
                                  float* right,
                                  int numSamples,
                                  const GranularParams& paramsIn) noexcept {
    if (left == nullptr || right == nullptr || numSamples <= 0) {
        return;
    }

    // copia por bloco. a matriz escreve nos parametros, e a soma tem de sair do
    // valor declarado deste bloco, sem acumular no seguinte.
    GranularParams params = paramsIn;

    // lfo saneado uma vez por bloco. taxa zero congela a fase e vira offset
    // estatico, o que e' uso legitimo. em sync a taxa vem da divisao e do bpm.
    float lfoRate = std::isfinite(params.lforateHz)
                        ? std::clamp(params.lforateHz, 0.0f, 20.0f)
                        : 1.0f;
    if (params.lfoSync) {
        const int div = sanitizeLFODiv(params.lfoDiv);
        lfoRate = static_cast<float>(syncedRateHz(div, params.hostBpm));
        const double ppq = params.hostPpq;
        if (std::isfinite(ppq) && ppq >= 0.0) {
            const double cpb = cyclesPerBeat(div);
            // fase presa ao ppq: segue o andamento e salta no seek. o offset
            // evita degrau na troca de free para sync.
            if (!syncEngaged_) {
                syncPhaseOffset_ = lfo_.phase() - ppq * cpb;
                syncEngaged_ = true;
            }
            lfo_.setPhase(ppq * cpb + syncPhaseOffset_);
        }
    } else {
        syncEngaged_ = false;
    }
    const float lfoDepth = std::isfinite(params.lfodepth)
                               ? std::clamp(params.lfodepth, 0.0f, 1.0f)
                               : 0.0f;
    const LFOSettings lfoSettings{lfoRate, lfoDepth, sanitizeLFOTarget(params.lfotarget),
                                  sanitizeLFOWave(params.lfowave),
                                  sanitizeLFOSteps(params.lfoSteps)};
    // glide antes do agendamento: o grao que nasce neste bloco ja' usa a altura
    // interpolada.
    updateGlide(params, numSamples);
    // valor do lfo no inicio do bloco, para densidade e cutoff.
    const float lfoBlock = lfo_.blockValue(lfoSettings.wave, lfoSettings.steps);
    lastLfoValue_ = lfoBlock;
    // matriz depois do lfo e antes dos coeficientes.
    applyModMatrix(params, lfoBlock);

    for (int i = 0; i < numSamples; ++i) {
        left[i] = 0.0f;
        right[i] = 0.0f;
    }

    if (source_ == nullptr || sourceCount_ == 0) {
        dcBlockerLeft_.processBlock(left, numSamples);
        dcBlockerRight_.processBlock(right, numSamples);
        // gate e lfo andam sem material. parar a fase aqui quebrava a repeticao
        // entre corridas.
        applyGate(left, right, numSamples, params);
        lfo_.advance(lfoRate, numSamples, sampleRate_);
        // publica sem graos para o display limpar o ultimo desenho.
        telemetry_.publish(voices_);
        return;
    }

    rebuildWindow(params);

    // coeficientes uma vez por bloco. recalcular por amostra seria trigonometria
    // demais; o estado fica por voz e os coeficientes sao partilhados.
    // o cutoff ja' entra com o lfo, e o cache compara o valor efetivo.
    const FilterType f1type = sanitizeFilterType(params.f1type);
    const FilterType f2type = sanitizeFilterType(params.f2type);
    float f1cutoff =
        (lfoSettings.target == LFOTarget::kCutoff)
            ? params.f1cutoffHz *
                  static_cast<float>(std::pow(2.0, static_cast<double>(lfoBlock) *
                                                       static_cast<double>(lfoDepth) * 2.0))
            : params.f1cutoffHz;
    // keytrack e envelope movem o corte em oitavas. sem nota, keytrack nao age.
    // o envelope e' o nivel do bloco anterior para nao recalcular por amostra.
    const float keytrack = std::isfinite(params.f1Keytrack)
                               ? std::clamp(params.f1Keytrack, 0.0f, 1.0f)
                               : 0.0f;
    const float f1env = std::isfinite(params.f1Env) ? std::clamp(params.f1Env, -1.0f, 1.0f) : 0.0f;
    if (keytrack > 0.0f && params.noteNumber >= 0) {
        f1cutoff *= static_cast<float>(std::pow(
            2.0, static_cast<double>(params.noteNumber - 60) * static_cast<double>(keytrack) / 12.0));
    }
    if (f1env != 0.0f) {
        f1cutoff *= static_cast<float>(std::pow(
            2.0, static_cast<double>(envelopeLevel_) * static_cast<double>(f1env) * 5.0));
    }
    f1cutoff = std::clamp(f1cutoff, 20.0f, 20000.0f);

    // drive do filtro 1: ganho e tanh. o pico fica no 1.0 do tanh e o clip do
    // barramento desce-o depois; como o tanh agora nao e' normalizado por 1/g, o
    // knob tambem levanta o material que ainda nao satura.
    const float driveDb = std::isfinite(params.f1DriveDb)
                              ? std::clamp(params.f1DriveDb, 0.0f, 24.0f)
                              : 0.0f;
    const bool driveOn = driveDb > 0.01f;
    const float driveGain = driveOn ? std::pow(10.0f, driveDb / 20.0f) : 1.0f;
    const bool slope24 = params.f1Slope != 0;
    if (f1type != lastF1Type_ || f1cutoff != lastF1Cutoff_ ||
        params.f1q != lastF1Q_ || f2type != lastF2Type_ ||
        params.f2cutoffHz != lastF2Cutoff_ || params.f2q != lastF2Q_ ||
        sampleRate_ != lastFilterSampleRate_) {
        coeffsA_ = makeBiquadCoeffs(f1type, f1cutoff, params.f1q, sampleRate_);
        coeffsB_ = makeBiquadCoeffs(f2type, params.f2cutoffHz, params.f2q, sampleRate_);
        lastF1Type_ = f1type;
        lastF1Cutoff_ = f1cutoff;
        lastF1Q_ = params.f1q;
        lastF2Type_ = f2type;
        lastF2Cutoff_ = params.f2cutoffHz;
        lastF2Q_ = params.f2q;
        lastFilterSampleRate_ = sampleRate_;
    }

    // densidade com lfo: 0,5x a 1,5x. nunca zero nem negativa.
    const float densityMod = (lfoSettings.target == LFOTarget::kDensity)
                                 ? 1.0f + lfoBlock * lfoDepth * 0.5f
                                 : 1.0f;
    // densidade saneada e nao negativa. NaN/Inf viram zero; negativo tambem.
    // zero e' silencio, e densidade inventada e' pior que motor quieto.
    const double rawDensity = std::isfinite(static_cast<double>(params.densityGrainsPerSec))
                                  ? std::max(static_cast<double>(params.densityGrainsPerSec), 0.0)
                                  : 0.0;
    const double spawnPerSample = rawDensity * static_cast<double>(densityMod) / sampleRate_;
    const double lastIndex = static_cast<double>(sourceCount_ - 1);
    const double positionPoint = std::clamp(static_cast<double>(params.position), 0.0, 1.0);

    // os graos que nascem neste bloco. o segundo passe so' renderiza estes: os
    // antigos ja' foram renderizados no primeiro, e renderizar outra vez consumia
    // dois blocos de vida por bloco -- um grao de 1,5 s morria em 0,75 s.
    std::array<bool, kMaxVoices> bornThisBlock {};

    auto spawnInto = [&](int voice, int atSample) {
        const std::size_t point = (entropyPointCount_ > 0)
            ? static_cast<std::size_t>(positionPoint * static_cast<double>(entropyPointCount_ - 1))
            : 0;
        const double entropy = (entropyPointCount_ > 0)
            ? static_cast<double>(entropyCurve_[point])
            : 4.0;
        startGrain(voice, params, entropy, atSample, lfoSettings);
        voices_[static_cast<std::size_t>(voice)].writeIndex = atSample;
        bornThisBlock[static_cast<std::size_t>(voice)] = true;
    };

    // writeIndex e' a posicao dentro do bloco, nao um contador global.
    auto renderVoice = [&](int voice) {
        Grain& grain = voices_[static_cast<std::size_t>(voice)];
        if (grain.writeIndex >= numSamples) {
            grain.writeIndex = 0;
        }
        // atraso da nuvem: o slot fica ocupado sem escrever, e o que sobra passa
        // para o bloco seguinte.
        if (grain.delay > 0) {
            const int skip = std::min(grain.delay, numSamples - grain.writeIndex);
            grain.delay -= skip;
            grain.writeIndex += skip;
            if (grain.delay > 0) {
                grain.writeIndex = 0;
                return;
            }
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
            // a tabela e' a do nascimento do grao, nao a que o knob aponta agora:
            // trocar de janela a meio de um grao longo saltava o envelope.
            const auto windowIndex = std::min(
                static_cast<std::size_t>(static_cast<double>(grain.windowIndex) *
                                         grain.windowScale),
                static_cast<std::size_t>(kWindowTableSamples - 1));
            const float envelope =
                (grain.window != nullptr) ? grain.window[windowIndex] : 0.0f;

            float driven = sample * envelope * grain.gain;
            // drive antes do filtro: ganho e tanh. o clip que fecha o topo do
            // curso vive no barramento, depois da soma (applyDriveClip).
            if (driveOn) {
                driven = std::tanh(driven * driveGain);
            }

            // filtros por voz, antes do pan: cada grao tem a sua historia.
            const auto voiceIndex = static_cast<std::size_t>(voice);
            float filtered = filterA_[voiceIndex].process(driven, coeffsA_);
            if (slope24) {
                filtered = filterA2_[voiceIndex].process(filtered, coeffsA_);
            }
            const float value = filterB_[voiceIndex].process(filtered, coeffsB_);

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

    // fracao que sobra vai para o proximo bloco. e' o que mantem a densidade
    // estavel entre blocos de tamanhos diferentes.
    const double exact = spawnPerSample * numSamples;
    const int whole = static_cast<int>(exact);
    spawnAccumulator_ += exact - static_cast<double>(whole);
    const int toSpawn = whole + static_cast<int>(spawnAccumulator_);
    spawnAccumulator_ -= static_cast<double>(toSpawn - whole);

    // espalha os graos pelo bloco. com densidade baixa, i/spawnPerSample jogaria
    // todos no fim; o espacamento evita isso e o int(Inf) da densidade zero.
    const int spacing = (spawnPerSample > 0.0) ? static_cast<int>(1.0 / spawnPerSample) : numSamples;
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
        if (voices_[static_cast<std::size_t>(i)].active &&
            bornThisBlock[static_cast<std::size_t>(i)]) {
            renderVoice(i);
        }
    }

    dcBlockerLeft_.processBlock(left, numSamples);
    dcBlockerRight_.processBlock(right, numSamples);

    // gate antes do limiter: a saturacao nao depende da nota.
    applyGate(left, right, numSamples, params);
    // clip do drive no barramento, depois do gate e antes da largura: corta o que
    // a soma dos graos ja' empurrou acima do tecto. a largura vem depois e pode
    // levantar o pico outra vez; o limiter e' o ultimo guarda.
    applyDriveClip(left, right, numSamples, params.f1DriveDb);
    // largura depois do gate e antes do limiter: o limite ve a imagem final.
    applyStereoWidth(left, right, numSamples, params.widthStereo);

    limiterLeft_.processBlock(left, numSamples);
    limiterRight_.processBlock(right, numSamples);

    lastActiveVoices_ = activeVoiceCount();

    // avanca a fase depois dos spawns. uma vez por bloco, sem wrap.
    lfo_.advance(lfoRate, numSamples, sampleRate_);

    // publica depois de escrever todas as vozes, para a interface ler o bloco
    // fechado. 60 Hz de display nao precisa de mais.
    telemetry_.publish(voices_);
}

// -- clip do drive --
void GranularEngine::applyDriveClip(float* left, float* right, int numSamples,
                                    float driveDb) noexcept {
    const float db = std::isfinite(driveDb) ? std::clamp(driveDb, 0.0f, 24.0f) : 0.0f;
    // abaixo de 6 dB o teto e' 1.0 e o clip nao se ouve; a partir daí desce ate'
    // 0.40 aos 24 dB. o teto desliza dentro do bloco para a automacao nao
    // estalar no limite do buffer.
    const float target =
        db > 6.0f
            ? 1.0f - 0.60f * std::clamp((db - 6.0f) / 18.0f, 0.0f, 1.0f)
            : 1.0f;

    const float start = std::clamp(lastClipCeiling_, 0.0f, 1.0f);
    if (target >= 0.9999f && start >= 0.9999f) {
        lastClipCeiling_ = 1.0f;
        return;
    }

    const float inc = (target - start) / static_cast<float>(numSamples);
    float ceiling = start;
    for (int i = 0; i < numSamples; ++i) {
        left[i] = std::clamp(left[i], -ceiling, ceiling);
        right[i] = std::clamp(right[i], -ceiling, ceiling);
        ceiling += inc;
    }
    lastClipCeiling_ = target;
}

// -- largura stereo --
void GranularEngine::applyStereoWidth(float* left, float* right, int numSamples,
                                      float targetWidth) noexcept {
    const float width = std::isfinite(targetWidth)
                            ? std::clamp(targetWidth, 0.0f, 2.0f)
                            : 1.0f;
    if (numSamples <= 0 || haasCapacity_ < 1) {
        lastWidth_ = width;
        return;
    }

    // alvo do atraso do haas: so' acima de 1.0, 0..12 ms. e' o spread artificial
    // que material mono (bytes, um canal so') nao tem. abaixo de 1.0 o atraso
    // volta a zero com a mesma rampa da largura, sem degrau.
    const float targetDelay =
        width > 1.0f
            ? std::min(static_cast<float>(width - 1.0f) * 0.012f *
                           static_cast<float>(sampleRate_),
                       static_cast<float>(haasCapacity_ - 1))
            : 0.0f;

    const float startWidth = std::clamp(lastWidth_, 0.0f, 2.0f);
    const float widthInc = (width - startWidth) / static_cast<float>(numSamples);
    const float startDelay =
        std::clamp(haasDelay_, 0.0f, static_cast<float>(haasCapacity_ - 1));

    // o atraso nao pode rampar mais depressa do que um quarto de amostra por
    // amostra: com a rampa do bloco inteiro (12 ms em 128 amostras) o ponteiro
    // de leitura andava para tras ~3 amostras por amostra e o canal direito
    // tocava ao contrario. o slew deixa o gesto da roda atravessar 1.0 num
    // deslize de ~50 ms, que e' o que se ouve como abertura, nao como chirp.
    constexpr float kDelaySlewPerSample {0.25f};

    float w = startWidth;
    float d = startDelay;
    const float capacity = static_cast<float>(haasCapacity_);
    for (int i = 0; i < numSamples; ++i) {
        // mid/side com a largura do instante. o mid fica intocado, por isso o
        // centro nao anda.
        if (std::abs(w - 1.0f) > 1.0e-4f) {
            const float mid = 0.5f * (left[i] + right[i]);
            const float side = 0.5f * (left[i] - right[i]);
            left[i] = mid + side * w;
            right[i] = mid - side * w;
        }

        // haas no canal direito: escreve o valor seco, le o atrasado
        // interpolado. a d = 0 a leitura devolve o proprio valor, e o gesto da
        // roda atravessa 1.0 sem clique.
        haasBuffer_[static_cast<std::size_t>(haasWritePos_)] = right[i];
        if (d > 0.01f) {
            auto readAbs = static_cast<float>(haasWritePos_) - d;
            while (readAbs < 0.0f) {
                readAbs += capacity;
            }
            auto index = static_cast<int>(readAbs);
            if (index >= haasCapacity_) {
                index -= haasCapacity_;
            }
            const auto next = index + 1 == haasCapacity_ ? 0 : index + 1;
            const float frac = readAbs - std::floor(readAbs);
            right[i] = haasBuffer_[static_cast<std::size_t>(index)] *
                           (1.0f - frac) +
                       haasBuffer_[static_cast<std::size_t>(next)] * frac;
        }
        haasWritePos_ = haasWritePos_ + 1 == haasCapacity_ ? 0 : haasWritePos_ + 1;

        w += widthInc;
        d = std::clamp(d + std::clamp((targetDelay - d), -kDelaySlewPerSample,
                                      kDelaySlewPerSample),
                       0.0f, static_cast<float>(haasCapacity_ - 1));
    }

    lastWidth_ = width;
    haasDelay_ = d;
}

// -- envelope no mix --
void GranularEngine::applyGate(float* left, float* right, int numSamples,
                                const GranularParams& params) noexcept {
    // tempos saneados uma vez por bloco. NaN volta ao default; zero vira uma
    // amostra; o teto de 5 s barra release infinito.
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
    // passos de fase separados dos de nivel: o decay anda (1-sustain) de nivel
    // e 1 de fase.
    const float decayPhaseStep = 1.0f / samplesFor(decaySeconds);
    const float releasePhaseStep = 1.0f / samplesFor(releaseSeconds);

    const auto finiteOrZero = [](float value) { return std::isfinite(value) ? value : 0.0f; };
    const float attackCurve = finiteOrZero(params.attackCurve);
    const float decayCurve = finiteOrZero(params.decayCurve);
    const float releaseCurve = finiteOrZero(params.releaseCurve);
    // qualquer curva nao nula muda o laco. com as tres a zero, o laco antigo
    // continua bit a bit.
    const bool curved = (attackCurve != 0.0f) || (decayCurve != 0.0f) || (releaseCurve != 0.0f);

    for (int i = 0; i < numSamples; ++i) {
        if (curved) {
            switch (envelopePhase_) {
                case EnvelopePhase::idle:
                    envelopeLevel_ = 0.0f;
                    stagePhase_ = 0.0f;
                    break;
                case EnvelopePhase::attack:
                    stagePhase_ = std::min(1.0f, stagePhase_ + attackStep);
                    envelopeLevel_ =
                        attackStartLevel_ +
                        (1.0f - attackStartLevel_) * curveShape(stagePhase_, attackCurve);
                    if (stagePhase_ >= 1.0f) {
                        envelopeLevel_ = 1.0f;
                        envelopePhase_ = EnvelopePhase::decay;
                        stagePhase_ = 0.0f;
                    }
                    break;
                case EnvelopePhase::decay:
                    stagePhase_ = std::min(1.0f, stagePhase_ + decayPhaseStep);
                    envelopeLevel_ =
                        1.0f - (1.0f - sustain) * curveShape(stagePhase_, decayCurve);
                    if (stagePhase_ >= 1.0f) {
                        envelopeLevel_ = sustain;
                        envelopePhase_ = EnvelopePhase::sustain;
                    }
                    break;
                case EnvelopePhase::sustain:
                    envelopeLevel_ = sustain;
                    break;
                case EnvelopePhase::release:
                    stagePhase_ = std::min(1.0f, stagePhase_ + releasePhaseStep);
                    envelopeLevel_ = releaseStartLevel_ * (1.0f - curveShape(stagePhase_, releaseCurve));
                    if (stagePhase_ >= 1.0f) {
                        envelopeLevel_ = 0.0f;
                        envelopePhase_ = EnvelopePhase::idle;
                    }
                    break;
            }
        } else {
            switch (envelopePhase_) {
                case EnvelopePhase::idle:
                    envelopeLevel_ = 0.0f;
                    stagePhase_ = 0.0f;
                    break;
                case EnvelopePhase::attack:
                    envelopeLevel_ = std::min(1.0f, envelopeLevel_ + attackStep);
                    stagePhase_ = std::min(1.0f, stagePhase_ + attackStep);
                    if (envelopeLevel_ >= 1.0f) {
                        envelopePhase_ = EnvelopePhase::decay;
                        stagePhase_ = 0.0f;
                    }
                    break;
                case EnvelopePhase::decay:
                    envelopeLevel_ = std::max(sustain, envelopeLevel_ - decayStep);
                    stagePhase_ = std::min(1.0f, stagePhase_ + decayPhaseStep);
                    if (envelopeLevel_ <= sustain) {
                        envelopeLevel_ = sustain;
                        envelopePhase_ = EnvelopePhase::sustain;
                    }
                    break;
                case EnvelopePhase::sustain:
                    envelopeLevel_ = sustain;
                    break;
                case EnvelopePhase::release:
                    envelopeLevel_ = std::max(0.0f, envelopeLevel_ - releaseStep);
                    stagePhase_ = std::min(1.0f, stagePhase_ + releasePhaseStep);
                    if (envelopeLevel_ <= 0.0f) {
                        envelopeLevel_ = 0.0f;
                        envelopePhase_ = EnvelopePhase::idle;
                    }
                    break;
            }
        }
        left[i] *= envelopeLevel_;
        right[i] *= envelopeLevel_;
    }
}

float GranularEngine::entropyAtPosition(float position) const noexcept {
    if (entropyPointCount_ == 0) {
        return 0.0f;
    }
    const double clamped = std::clamp(static_cast<double>(position), 0.0, 1.0);
    const auto point = static_cast<std::size_t>(
        clamped * static_cast<double>(entropyPointCount_ - 1));
    return std::clamp(entropyCurve_[point] / 8.0f, 0.0f, 1.0f);
}

// -- matriz --
void GranularEngine::applyModMatrix(GranularParams& params, float lfoValue) noexcept {
    ModSourceValues values;
    values.lfo = std::isfinite(lfoValue) ? std::clamp(lfoValue, -1.0f, 1.0f) : 0.0f;
    values.envelope = std::clamp(envelopeLevel_, 0.0f, 1.0f);
    values.velocity = std::isfinite(params.velocity) ? std::clamp(params.velocity, 0.0f, 1.0f) : 0.0f;
    values.keytrack = (params.noteNumber >= 0)
                          ? std::clamp(static_cast<float>(params.noteNumber - 60) / 48.0f, -1.0f, 1.0f)
                          : 0.0f;
    values.modWheel = std::isfinite(params.modWheel) ? std::clamp(params.modWheel, 0.0f, 1.0f) : 0.0f;
    values.cc74 = std::isfinite(params.cc74) ? std::clamp(params.cc74, 0.0f, 1.0f) : 0.0f;
    values.cc71 = std::isfinite(params.cc71) ? std::clamp(params.cc71, 0.0f, 1.0f) : 0.0f;
    values.entropy = entropyAtPosition(params.position);

    // cada destino tem a sua escala, na unidade do parametro: semitons em pitch,
    // fracao em posicao, oitavas em corte. o recorte usa a faixa do knob.
    const auto offset = [&params, &values](ModDest dest) {
        return matrixOffset(params.modMatrix, dest, values);
    };
    const auto octaves = [](float amount, float span) {
        return static_cast<float>(std::pow(2.0, static_cast<double>(amount) *
                                                   static_cast<double>(span)));
    };

    const float pitch = offset(ModDest::kPitch);
    if (pitch != 0.0f) {
        params.pitchSemitones =
            std::clamp(params.pitchSemitones + pitch * 12.0f, -48.0f, 48.0f);
    }

    const float density = offset(ModDest::kDensity);
    if (density != 0.0f) {
        params.densityGrainsPerSec =
            std::clamp(params.densityGrainsPerSec * octaves(density, 2.0f), 0.5f, 400.0f);
    }

    const float position = offset(ModDest::kPosition);
    if (position != 0.0f) {
        params.position = std::clamp(params.position + position * 0.5f, 0.0f, 1.0f);
    }

    const float spray = offset(ModDest::kSpray);
    if (spray != 0.0f) {
        params.spray = std::clamp(params.spray + spray, 0.0f, 1.0f);
    }

    const float grainSize = offset(ModDest::kGrainSize);
    if (grainSize != 0.0f) {
        params.grainSizeMs = std::clamp(params.grainSizeMs * octaves(grainSize, 2.0f),
                                       kMinGrainMs, kMaxGrainMs);
    }

    const float cutoff1 = offset(ModDest::kCutoff1);
    if (cutoff1 != 0.0f) {
        params.f1cutoffHz =
            std::clamp(params.f1cutoffHz * octaves(cutoff1, 5.0f), 20.0f, 20000.0f);
    }

    const float q1 = offset(ModDest::kQ1);
    if (q1 != 0.0f) {
        params.f1q = std::clamp(params.f1q * octaves(q1, 2.0f), 0.5f, 12.0f);
    }

    const float cutoff2 = offset(ModDest::kCutoff2);
    if (cutoff2 != 0.0f) {
        params.f2cutoffHz =
            std::clamp(params.f2cutoffHz * octaves(cutoff2, 5.0f), 20.0f, 20000.0f);
    }

    const float q2 = offset(ModDest::kQ2);
    if (q2 != 0.0f) {
        params.f2q = std::clamp(params.f2q * octaves(q2, 2.0f), 0.5f, 12.0f);
    }

    const float pan = offset(ModDest::kPan);
    if (pan != 0.0f) {
        params.pan = std::clamp(params.pan + pan, -1.0f, 1.0f);
    }

    const float grainLevel = offset(ModDest::kGrainLevel);
    if (grainLevel != 0.0f) {
        params.grainLevel = std::clamp(params.grainLevel + grainLevel, 0.0f, 1.0f);
    }

    const float drive = offset(ModDest::kDrive);
    if (drive != 0.0f) {
        params.f1DriveDb = std::clamp(params.f1DriveDb + drive * 24.0f, 0.0f, 24.0f);
    }

    const float width = offset(ModDest::kWidth);
    if (width != 0.0f) {
        // multiplicativo, como densidade e corte: +1 oitava dobra o side. o teto
        // de 2 e' o mesmo do knob; a base de 0 e' o mono.
        params.widthStereo = std::clamp(params.widthStereo * octaves(width, 1.0f), 0.0f, 2.0f);
    }
}

} // namespace opcoda::dsp
