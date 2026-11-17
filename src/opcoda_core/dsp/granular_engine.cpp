#include "opcoda_core/dsp/granular_engine.h"

#include "opcoda_core/dsp/window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace opcoda::dsp {
namespace {

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

    for (std::size_t i = 0; i < windowTables_.size(); ++i) {
        fillWindow(static_cast<WindowType>(i), windowTables_[i].data(), kWindowTableSamples);
    }
    activeWindow_ = windowTables_[0].data();
    activeWindowType_ = WindowType::kHann;

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
        envelopePhase_ = EnvelopePhase::attack;
        stagePhase_ = 0.0f;
        attackStartLevel_ = envelopeLevel_;
    } else {
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
    setSource(samples, count, nullptr, 0);
}

void GranularEngine::setSource(const float* samples,
                               std::size_t count,
                               const float* entropyCurve,
                               std::size_t entropyPoints) noexcept {
    source_ = samples;
    sourceCount_ = (samples != nullptr) ? count : 0;
    entropyPointCount_ = 0;

    if (sourceCount_ == 0) {
        for (auto& voice : voices_) {
            voice.active = false;
        }
        lastActiveVoices_ = 0;
    }

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

    std::uint64_t state =
        seed_ + 0x9E3779B97F4A7C15ull *
                    (static_cast<std::uint64_t>(voice) +
                     (grainSerial_++) * static_cast<std::uint64_t>(kMaxVoices) + 1ull);

    const double lfoPosition =
        (lfo.target == LFOTarget::kPosition)
            ? static_cast<double>(lfo_.valueAt(lfo.wave, lfo.rateHz, blockOffset, sampleRate_,
                                               lfo.steps)) *
                  static_cast<double>(lfo.depth) * 0.25
            : 0.0;

    const double base = std::clamp(static_cast<double>(params.position), 0.0, 1.0);
    const double spread =
        std::isfinite(static_cast<double>(params.spray))
            ? std::clamp(static_cast<double>(params.spray), 0.0, 1.0)
            : 0.0;
    grain.position =
        std::clamp(base + ((nextRandom(state) * 2.0 - 1.0) * spread) + lfoPosition, 0.0, 1.0);

    const double randomRange =
        std::isfinite(static_cast<double>(params.pitchRandomSemitones))
            ? std::clamp(static_cast<double>(params.pitchRandomSemitones), 0.0, 12.0)
            : 0.0;
    const double detune = (nextRandom(state) * 2.0 - 1.0) * randomRange;

    int grainLength = clampGrainSamples(sampleRate_, params.grainSizeMs);
    double windowScale = windowScaleForLength(grainLength);

    // LFO no pitch: +-12 st com depth cheio. entra na soma antes da razao.
    const double lfoPitch = (lfo.target == LFOTarget::kPitch)
                                ? static_cast<double>(lfo_.valueAt(
                                      lfo.wave, lfo.rateHz, blockOffset, sampleRate_, lfo.steps)) *
                                      static_cast<double>(lfo.depth) * 12.0
                                : 0.0;

    const double tuned = std::isfinite(static_cast<double>(params.pitchSemitones))
                             ? static_cast<double>(params.pitchSemitones)
                             : 0.0;
    const double ratio =
        std::pow(2.0, (tuned + detune + lfoPitch + glideSemitones_) / 12.0);
    const double rate = std::clamp(ratio, 0.0625, 16.0);
    grain.readStep = (sourceCount_ > 0) ? rate / static_cast<double>(sourceCount_) : 0.0;

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
    grain.delay = 0;
    lastGrainLength_ = grainLength;
    grain.active = true;

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
                              ? static_cast<double>(note - 60)
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

    GranularParams params = paramsIn;

    float lfoRate = std::isfinite(params.lforateHz)
                        ? std::clamp(params.lforateHz, 0.0f, 20.0f)
                        : 1.0f;
    if (params.lfoSync) {
        const int div = sanitizeLFODiv(params.lfoDiv);
        lfoRate = static_cast<float>(syncedRateHz(div, params.hostBpm));
        const double ppq = params.hostPpq;
        if (std::isfinite(ppq) && ppq >= 0.0) {
            const double cpb = cyclesPerBeat(div);
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
        applyGate(left, right, numSamples, params);
        lfo_.advance(lfoRate, numSamples, sampleRate_);
        // publica sem graos para o display limpar o ultimo desenho.
        telemetry_.publish(voices_);
        return;
    }

    rebuildWindow(params);

    const FilterType f1type = sanitizeFilterType(params.f1type);
    const FilterType f2type = sanitizeFilterType(params.f2type);
    float f1cutoff =
        (lfoSettings.target == LFOTarget::kCutoff)
            ? params.f1cutoffHz *
                  static_cast<float>(std::pow(2.0, static_cast<double>(lfoBlock) *
                                                       static_cast<double>(lfoDepth) * 2.0))
            : params.f1cutoffHz;
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
    const double rawDensity = std::isfinite(static_cast<double>(params.densityGrainsPerSec))
                                  ? std::max(static_cast<double>(params.densityGrainsPerSec), 0.0)
                                  : 0.0;
    const double spawnPerSample = rawDensity * static_cast<double>(densityMod) / sampleRate_;
    const double lastIndex = static_cast<double>(sourceCount_ - 1);
    const double positionPoint = std::clamp(static_cast<double>(params.position), 0.0, 1.0);

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
            const auto windowIndex = std::min(
                static_cast<std::size_t>(static_cast<double>(grain.windowIndex) *
                                         grain.windowScale),
                static_cast<std::size_t>(kWindowTableSamples - 1));
            const float envelope =
                (grain.window != nullptr) ? grain.window[windowIndex] : 0.0f;

            float driven = sample * envelope * grain.gain;
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

    const double exact = spawnPerSample * numSamples;
    const int whole = static_cast<int>(exact);
    spawnAccumulator_ += exact - static_cast<double>(whole);
    const int toSpawn = whole + static_cast<int>(spawnAccumulator_);
    spawnAccumulator_ -= static_cast<double>(toSpawn - whole);

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
    applyDriveClip(left, right, numSamples, params.f1DriveDb);
    // largura depois do gate e antes do limiter: o limite ve a imagem final.
    applyStereoWidth(left, right, numSamples, params.widthStereo);

    limiterLeft_.processBlock(left, numSamples);
    limiterRight_.processBlock(right, numSamples);

    lastActiveVoices_ = activeVoiceCount();

    // avanca a fase depois dos spawns. uma vez por bloco, sem wrap.
    lfo_.advance(lfoRate, numSamples, sampleRate_);

    telemetry_.publish(voices_);
}

// -- clip do drive --
void GranularEngine::applyDriveClip(float* left, float* right, int numSamples,
                                    float driveDb) noexcept {
    const float db = std::isfinite(driveDb) ? std::clamp(driveDb, 0.0f, 24.0f) : 0.0f;
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

    constexpr float kDelaySlewPerSample {0.25f};

    float w = startWidth;
    float d = startDelay;
    const float capacity = static_cast<float>(haasCapacity_);
    for (int i = 0; i < numSamples; ++i) {
        if (std::abs(w - 1.0f) > 1.0e-4f) {
            const float mid = 0.5f * (left[i] + right[i]);
            const float side = 0.5f * (left[i] - right[i]);
            left[i] = mid + side * w;
            right[i] = mid - side * w;
        }

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
    const float decayPhaseStep = 1.0f / samplesFor(decaySeconds);
    const float releasePhaseStep = 1.0f / samplesFor(releaseSeconds);

    const auto finiteOrZero = [](float value) { return std::isfinite(value) ? value : 0.0f; };
    const float attackCurve = finiteOrZero(params.attackCurve);
    const float decayCurve = finiteOrZero(params.decayCurve);
    const float releaseCurve = finiteOrZero(params.releaseCurve);
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
        params.widthStereo = std::clamp(params.widthStereo * octaves(width, 1.0f), 0.0f, 2.0f);
    }
}

} // namespace opcoda::dsp
