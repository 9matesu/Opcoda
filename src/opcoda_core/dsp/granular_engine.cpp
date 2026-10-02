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

void GranularEngine::reset() noexcept {
    for (auto& voice : voices_) {
        voice = Grain {};
    }
    spawnAccumulator_ = 0.0;
    lastActiveVoices_ = 0;
    dcBlockerLeft_.prepare();
    dcBlockerRight_.prepare();
    limiterLeft_.prepare();
    limiterRight_.prepare();
}

void GranularEngine::setSource(const float* samples, std::size_t count) noexcept {
    source_ = samples;
    sourceCount_ = (samples != nullptr) ? count : 0;
    entropyPointCount_ = 0;

    if (source_ == nullptr || sourceCount_ == 0) {
        return;
    }

    const std::size_t step = std::max<std::size_t>(1, sourceCount_ / kMaxEntropyPoints);
    std::size_t index = 0;
    while (index < sourceCount_ && entropyPointCount_ < kMaxEntropyPoints) {
        const std::size_t length = std::min(step, sourceCount_ - index);
        std::array<std::uint32_t, 256> histogram {};
        for (std::size_t i = 0; i < length; ++i) {
            const auto byte = static_cast<std::uint8_t>(
                std::clamp(source_[index + i], -1.0f, 1.0f) * 127.0f + 127.0f);
            ++histogram[byte];
        }
        const double n = static_cast<double>(length);
        double sum = 0.0;
        for (const auto occurrences : histogram) {
            if (occurrences == 0) {
                continue;
            }
            const double p = static_cast<double>(occurrences) / n;
            sum += p * std::log2(p);
        }
        entropyCurve_[entropyPointCount_++] = static_cast<float>(-sum);
        index += length;
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

    // A taxa de reproducao e' a razao de semitons, e o avanco por amostra tem
    // que ser ela dividida pelo comprimento da fonte: 'position' e' normalizada
    // em [0, 1], entao somar a taxa crua faz o grao varrer o arquivo inteiro em
    // poucas amostras.
    const double ratio = std::pow(2.0, static_cast<double>(params.pitchSemitones) / 12.0);
    const double rate = std::clamp(ratio, 0.5, 2.0);
    grain.readStep = (sourceCount_ > 0) ? rate / static_cast<double>(sourceCount_) : 0.0;

    grain.remaining = activeWindowLength_;
    grain.windowIndex = 0;
    grain.active = true;
    grain.gain = dbToGain(params.volumeDb) * 0.25f;

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
    limiterLeft_.processBlock(left, numSamples);
    limiterRight_.processBlock(right, numSamples);

    lastActiveVoices_ = activeVoiceCount();
}

} // namespace opcoda::dsp
