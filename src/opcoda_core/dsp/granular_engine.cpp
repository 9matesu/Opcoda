#include "opcoda_core/dsp/granular_engine.h"

#include "opcoda_core/dsp/window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace opcoda::dsp {
namespace {

// A semente vive no motor (GranularEngine::kDefaultSeed): o ensaio da nuvem
// precisa de a poder trocar, e a mesma fonte com os mesmos parametros continua
// a dar o mesmo som por omissao — e' o que torna T1 e T2 reproduziveis.
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
        stagePhase_ = 0.0f;
        attackStartLevel_ = envelopeLevel_;
    } else {
        // O release mede a partir daqui para fechar em tempo exato, qualquer
        // que seja o nivel: com inclinacao fixa, soltar a meio do ataque
        // fecharia mais rapido do que o knob diz.
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
    const auto window = sanitizeWindowType(params.window);
    if (length == activeWindowLength_ && window == activeWindowType_) {
        return;
    }

    // Alterna entre dois buffers para nao invalidar o ponteiro que o agendador
    // pode estar lendo no meio de um bloco.
    float* target = (activeWindow_ == windowA_.data()) ? windowB_.data() : windowA_.data();
    fillWindow(window, target, length);
    activeWindow_ = target;
    activeWindowLength_ = length;
    activeWindowType_ = window;
}

void GranularEngine::startGrain(int voice,
                                const GranularParams& params,
                                double entropy,
                                int blockOffset,
                                const LFOSettings& lfo) noexcept {
    Grain& grain = voices_[static_cast<std::size_t>(voice)];

    // Entropia local modula a dispersao: regiao compressada (H alto) espalha
    // mais, regiao repetitiva (H baixo) fica mais ancorada.
    std::uint64_t state = seed_ + static_cast<std::uint64_t>(voice) * 0x9E3779B1ull;

    // LFO amostrado no instante exato do nascimento (offset dentro do bloco),
    // e nao no inicio do bloco: a 20 Hz com bloco de 256, o inicio e o fim
    // estao a 0,13 ciclos de distancia. So entra se o alvo for posicao; com
    // depth 0 o termo e' zero e nada muda.
    const double lfoPosition =
        (lfo.target == LFOTarget::kPosition)
            ? static_cast<double>(lfo_.valueAt(lfo.wave, lfo.rateHz, blockOffset, sampleRate_,
                                               lfo.steps)) *
                  static_cast<double>(lfo.depth) * 0.25
            : 0.0;

    const double base = std::clamp(static_cast<double>(params.position), 0.0, 1.0);
    const double spread = static_cast<double>(params.spray) * (0.5 + 0.5 * entropy / 8.0);
    grain.position =
        std::clamp(base + ((nextRandom(state) * 2.0 - 1.0) * spread) + lfoPosition, 0.0, 1.0);

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

    // F021: dispersao de afinacao da nuvem, simetrica em [-spread, +spread].
    // So' sorteia quando o knob sai do zero: um sorteio a mais mudaria a
    // sequencia do gerador e os tres knobs a zero deixariam de reproduzir a
    // saida anterior.
    const double spreadRange =
        std::isfinite(static_cast<double>(params.pitchSpreadSemitones))
            ? std::clamp(static_cast<double>(params.pitchSpreadSemitones), 0.0, 24.0)
            : 0.0;
    const double spreadOffset =
        (spreadRange > 0.0) ? (nextRandom(state) * 2.0 - 1.0) * spreadRange : 0.0;

    // F021: janela temporal da nuvem. O atraso e' uniforme em [0, janela] e o
    // grao ocupa o slot a espera; o grao seguinte sorteia de novo, nao herda.
    const double cloudWindow =
        std::isfinite(static_cast<double>(params.cloudSize))
            ? std::clamp(static_cast<double>(params.cloudSize), 0.0, 1.0) * 0.5
            : 0.0;
    const int delaySamples = (cloudWindow > 0.0)
                                 ? static_cast<int>(nextRandom(state) * cloudWindow * sampleRate_)
                                 : 0;

    // F021: jitter de duracao. O fator vai de metade a uma vez e meia, e o
    // resultado e' recortado a 1..100 ms. A janela e' lida por escala e nao
    // reconstruida: a tabela e' uma so' por bloco e os oito slots a partilham.
    const double jitter =
        std::isfinite(static_cast<double>(params.grainJitter))
            ? std::clamp(static_cast<double>(params.grainJitter), 0.0, 1.0)
            : 0.0;
    int grainLength = activeWindowLength_;
    double windowScale = 1.0;
    if (jitter > 0.0) {
        const double factor = 1.0 + (nextRandom(state) * 2.0 - 1.0) * 0.5 * jitter;
        const double jitteredMs = std::clamp(
            static_cast<double>(params.grainSizeMs) * factor, 1.0, 100.0);
        grainLength = clampGrainSamples(sampleRate_, static_cast<float>(jitteredMs));
        windowScale = static_cast<double>(activeWindowLength_) /
                      static_cast<double>(grainLength);
    }

    // LFO no pitch, em semitons: ±12 com depth cheio. Soma ao detune
    // aleatorio antes da razao, pelo mesmo caminho — as duas modulacoes sao
    // independentes e comutam na soma.
    const double lfoPitch = (lfo.target == LFOTarget::kPitch)
                                ? static_cast<double>(lfo_.valueAt(
                                      lfo.wave, lfo.rateHz, blockOffset, sampleRate_, lfo.steps)) *
                                      static_cast<double>(lfo.depth) * 12.0
                                : 0.0;

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
    // A nota da pilha mono entra como semitons relativos a Do4, ja quantizada
    // pela escala, e o glide interpola esse termo. A afinacao manual continua a
    // somar por cima e nao passa pelo glide: mover o knob PITCH tem de ser
    // imediato, como sempre foi.
    const double ratio =
        std::pow(2.0, (tuned + detune + lfoPitch + spreadOffset + glideSemitones_) / 12.0);
    // Quatro oitavas para cada lado: nota (+-24 st), afinacao manual (+-24 st) e
    // detune (12 st) somados. O limite antigo de 0,5..2,0 cortava a nota no
    // primeiro semitom fora da oitava central.
    const double rate = std::clamp(ratio, 0.0625, 16.0);
    grain.readStep = (sourceCount_ > 0) ? rate / static_cast<double>(sourceCount_) : 0.0;

    grain.remaining = grainLength;
    grain.windowIndex = 0;
    grain.windowScale = windowScale;
    grain.delay = delaySamples;
    lastGrainDelay_ = delaySamples;
    lastGrainSpread_ = static_cast<float>(spreadOffset);
    lastGrainLength_ = grainLength;
    grain.active = true;

    // Estado do filtro zerado no nascimento: grao e' evento isolado, e nao
    // continuacao da voz. Sem isto, o grao novo herdava a cauda congelada do
    // grao anterior no mesmo slot — o filtro nao avanca enquanto inativo, entao
    // nao havia decaimento no intervalo, e com Q alto a cauda entrava audivel
    // no ataque como clique. A alternativa ("voz continua") exigiria provar que
    // cauda congelada e' desejada; ninguem provou, e o reset parcial e' o que
    // um musico espera de um novo disparo.
    filterA_[static_cast<std::size_t>(voice)].reset();
    filterA2_[static_cast<std::size_t>(voice)].reset();
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

void GranularEngine::updateGlide(const GranularParams& params, int numSamples) noexcept {
    const int note = params.noteNumber;
    const double target = (note >= 0)
                              ? semitonesForNote(note, params.scale, params.scaleRoot)
                              : 0.0;

    const float glide = std::isfinite(params.glideSeconds)
                            ? std::clamp(params.glideSeconds, 0.0f, 2.0f)
                            : 0.0f;

    // Glide desligado: a nota entra direto, como antes da F018.
    if (glide <= 0.0f) {
        glideSemitones_ = target;
        glideFrom_ = target;
        glideTarget_ = target;
        glideRemaining_ = 0.0;
        return;
    }

    // Nota nova: parte de onde esta e conta o tempo declarado a partir daqui.
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

void GranularEngine::processBlock(float* left,
                                  float* right,
                                  int numSamples,
                                  const GranularParams& paramsIn) noexcept {
    if (left == nullptr || right == nullptr || numSamples <= 0) {
        return;
    }

    // Copia por bloco porque a matriz escreve nos parametros: a soma das rotas
    // tem de ser aplicada ao valor declarado deste bloco, e nao acumulada no
    // bloco seguinte. A copia sao uns duzentos bytes e evita que a modulacao
    // vire deriva.
    GranularParams params = paramsIn;

    // LFO saneado uma vez por bloco, aqui em cima para os dois caminhos de
    // saida o verem: taxa em [0, 20] (NaN volta a 1,0), profundidade em
    // [0, 1] (NaN desliga), alvo e forma saneados. Taxa zero congela a fase —
    // vira offset estatico, que e' uso legitimo, nao erro.
    //
    // Em Sync a taxa nao vem de lforateHz: sai de lfodiv e do BPM do host, e o
    // limite de 20 Hz deixa de valer porque 1/64 a 140 BPM ja' sao 37 Hz.
    float lfoRate = std::isfinite(params.lforateHz)
                        ? std::clamp(params.lforateHz, 0.0f, 20.0f)
                        : 1.0f;
    if (params.lfoSync) {
        const int div = sanitizeLFODiv(params.lfoDiv);
        lfoRate = static_cast<float>(syncedRateHz(div, params.hostBpm));
        const double ppq = params.hostPpq;
        if (std::isfinite(ppq) && ppq >= 0.0) {
            const double cpb = cyclesPerBeat(div);
            // A fase passa a ser funcao do ppq, o que segue o andamento e salta
            // quando o host reposiciona. Na troca de Free para Sync o offset
            // preserva a fase corrente: sem ele, mudar de modo dava um degrau
            // audivel que nao corresponde a nenhuma decisao musical.
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
    // O glide corre antes do agendamento: os graos que nascem neste bloco ja
    // usam a altura interpolada, e nao a do bloco anterior.
    updateGlide(params, numSamples);
    // Valor do bloco para densidade e cutoff, que sao por bloco e nao por grao.
    // Lido na fase do inicio do bloco: o advance so corre no fim.
    const float lfoBlock = lfo_.blockValue(lfoSettings.wave, lfoSettings.steps);
    // A matriz corre aqui, depois do LFO e antes dos coeficientes: os destinos
    // deste bloco ja levam a soma das rotas, e o LFO que alimenta a matriz e' o
    // mesmo que alimenta os alvos classicos.
    applyModMatrix(params, lfoBlock);

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
        // O LFO avanca mesmo sem material: e' tempo absoluto, e parar a fase
        // sem fonte partia o determinismo entre corridas com e sem material.
        lfo_.advance(lfoRate, numSamples, sampleRate_);
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
    //
    // O cutoff do f1 entra ja modulado pelo LFO (alvo cutoff, ±2 oitavas em
    // escala log, que e' como corte se ouve). A comparacao do cache usa o valor
    // efetivo, e nao o cru: comparar o cru recalcularia a cada bloco com o LFO
    // ligado, mesmo parado.
    const FilterType f1type = sanitizeFilterType(params.f1type);
    const FilterType f2type = sanitizeFilterType(params.f2type);
    float f1cutoff =
        (lfoSettings.target == LFOTarget::kCutoff)
            ? params.f1cutoffHz *
                  static_cast<float>(std::pow(2.0, static_cast<double>(lfoBlock) *
                                                       static_cast<double>(lfoDepth) * 2.0))
            : params.f1cutoffHz;
    // F020: keytrack e envelope movem o corte de 1 em oitavas, e o resultado e'
    // recortado a faixa audivel. Sem nota (noteNumber < 0) o keytrack nao actua:
    // num instrumento sem teclado nao ha nota a seguir. O envelope e' o nivel do
    // bloco anterior — o gate fecha no fim do bloco, e usar o valor corrente
    // exigiria recalcular coeficientes por amostra.
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

    // Drive do filtro 1: ganho, tanh, e ganho inverso. A compensacao linear 1/g
    // garante que o pico nunca sobe (tanh(y) <= y), ao contrario de dividir por
    // tanh(g), que normaliza para escala cheia e levanta 12 dB o material baixo.
    const float driveDb = std::isfinite(params.f1DriveDb)
                              ? std::clamp(params.f1DriveDb, 0.0f, 24.0f)
                              : 0.0f;
    const bool driveOn = driveDb > 0.01f;
    const float driveGain = driveOn ? std::pow(10.0f, driveDb / 20.0f) : 1.0f;
    const float driveInv = driveOn ? 1.0f / driveGain : 1.0f;
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

    // Densidade modulada pelo LFO (alvo density, x[0,5, 1,5]): com lfo em -1 e
    // depth cheio, metade da densidade — nunca zero nem negativa, porque zero
    // calava o motor por modulacao e negativo nao tem sentido.
    const float densityMod = (lfoSettings.target == LFOTarget::kDensity)
                                 ? 1.0f + lfoBlock * lfoDepth * 0.5f
                                 : 1.0f;
    // Saneada e nunca negativa: NaN/Inf em `static_cast<int>` e' UB, densidade
    // zero em `1,0 / spawnPerSample` e' `int(Inf)` (UB), e negativa punha `at`
    // negativo e escrevia fora do buffer. Zero e' silencio legitimo (sem
    // spawns); negativo trunca em zero; NaN/Inf viram zero, e nao default com
    // som, porque densidade inventada e' pior que motor quieto.
    const double rawDensity = std::isfinite(static_cast<double>(params.densityGrainsPerSec))
                                  ? std::max(static_cast<double>(params.densityGrainsPerSec), 0.0)
                                  : 0.0;
    const double spawnPerSample = rawDensity * static_cast<double>(densityMod) / sampleRate_;
    const double lastIndex = static_cast<double>(sourceCount_ - 1);
    const double positionPoint = std::clamp(static_cast<double>(params.position), 0.0, 1.0);

    auto spawnInto = [&](int voice, int atSample) {
        const std::size_t point = (entropyPointCount_ > 0)
            ? static_cast<std::size_t>(positionPoint * static_cast<double>(entropyPointCount_ - 1))
            : 0;
        const double entropy = (entropyPointCount_ > 0)
            ? static_cast<double>(entropyCurve_[point])
            : 4.0;
        startGrain(voice, params, entropy, atSample, lfoSettings);
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
        // F021: grao a espera da nuvem. O slot fica ocupado sem escrever, e o
        // que resta do atraso passa para o bloco seguinte — o inicio cai no
        // meio do bloco quando o atraso acaba ali, e nao no inicio do bloco.
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
            // Com jitter 0 o fator e' 1,0 e o indice e' o de antes; com jitter,
            // a mesma tabela e' lida mais depressa ou mais devagar para caber
            // na duracao sorteada. O ultimo indice e' o teto: sem ele, um grao
            // mais curto lia fora da janela ja' desenhada.
            const auto windowIndex = std::min(
                static_cast<std::size_t>(static_cast<double>(grain.windowIndex) *
                                         grain.windowScale),
                static_cast<std::size_t>(activeWindowLength_ - 1));
            const float envelope = activeWindow_[windowIndex];

            float driven = sample * envelope * grain.gain;
            // Drive antes do filtro: satura o grao, e o filtro esculpe o
            // resultado. O ganho inverso 1/g mantem o pico igual ou abaixo do
            // que entrou, por isso o knob nao vira um segundo volume.
            if (driveOn) {
                driven = std::tanh(driven * driveGain) * driveInv;
            }

            // Os dois filtros em serie, por voz e antes do pan: cada grao tem a
            // sua historia de estado, e filtrar depois do pan seria filtrar o
            // mix com atraso de fase diferente por canal.
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
    // Com densidade zero nao ha spawn e o spacing nao existe: o ramo evita o
    // `int(Inf)` que o calculo direto daria.
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

    // Avanca a fase para o proximo bloco, depois de todos os spawns a terem
    // lido a fase deste. Uma vez por bloco, e nunca por amostra: e' O(1) com
    // uma soma (sem wrap por desenho — o indice S&H precisa da fase inteira).
    lfo_.advance(lfoRate, numSamples, sampleRate_);

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
    // Passos de fase: 1/samples do estagio. Separados dos passos de nivel
    // porque o decay tem de percorrer (1 - sustain) de nivel e 1 de fase.
    const float decayPhaseStep = 1.0f / samplesFor(decaySeconds);
    const float releasePhaseStep = 1.0f / samplesFor(releaseSeconds);

    const auto finiteOrZero = [](float value) { return std::isfinite(value) ? value : 0.0f; };
    const float attackCurve = finiteOrZero(params.attackCurve);
    const float decayCurve = finiteOrZero(params.decayCurve);
    const float releaseCurve = finiteOrZero(params.releaseCurve);
    // Qualquer curva nao nula muda o laco. Com as tres a zero o laco antigo
    // continua a mandar, bit a bit: EXPECT_FLOAT_EQ vive de 4 ULP.
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

    // Cada destino tem a sua escala, e a escala vive aqui, num sitio so. A
    // unidade e' a do parametro que o destino controla: semitons em pitch,
    // fracao em posicao, oitavas em corte. O recorte usa a faixa declarada do
    // parametro, e nao um limite inventado: a modulacao chega ao extremo do
    // knob, e nao a um valor que o host nunca mostraria.
    const auto offset = [&params, &values](ModDest dest) {
        return matrixOffset(params.modMatrix, dest, values);
    };
    const auto octaves = [](float amount, float span) {
        return static_cast<float>(std::pow(2.0, static_cast<double>(amount) *
                                                   static_cast<double>(span)));
    };

    const float pitch = offset(ModDest::kPitch);
    if (pitch != 0.0f) {
        params.pitchSemitones = std::clamp(params.pitchSemitones + pitch * 12.0f, -24.0f, 24.0f);
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
        params.grainSizeMs = std::clamp(params.grainSizeMs * octaves(grainSize, 2.0f), 1.0f, 200.0f);
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
}

} // namespace opcoda::dsp
