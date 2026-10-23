#include <gtest/gtest.h>

#include "opcoda_core/dsp/dc_blocker.h"
#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/dsp/limiter.h"
#include "opcoda_core/dsp/window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using opcoda::dsp::DcBlocker;
using opcoda::dsp::windowName;
using opcoda::dsp::Grain;
using opcoda::dsp::GrainTelemetry;
using opcoda::dsp::GrainView;
using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::dsp::Limiter;
using opcoda::dsp::WindowType;
using opcoda::dsp::fillWindow;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr double kSampleRate = 44100.0;
}

TEST(DcBlocker, CutoffFallsBetweenTenAndFifteenHz) {
    // a fundamentacao pede corte entre 10 e 15 Hz a 44,1 kHz.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    const double cutoff = blocker.cutoffHz();
    EXPECT_GT(cutoff, 10.0);
    EXPECT_LT(cutoff, 15.0);
}

TEST(DcBlocker, AttenuationAtDcIsEffectivelyInfinite) {
    // H(1) = 0: o numerador anula o bin DC. e' o que sustenta os 40 dB do T1.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    EXPECT_LT(blocker.dcGainDb(), -200.0);
}

TEST(DcBlocker, RemovesConstantOffset) {
    // offset de 0,5 tem de sair sem nivel medio. y[0] = 0,5 decai com R^n, entao
    // a media so vale em regime: medir com o decaimento dentro nao serve.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    blocker.prepare();

    constexpr int kSettle = 20000;
    constexpr int kMeasure = 44100;
    for (int i = 0; i < kSettle; ++i) {
        (void)blocker.process(0.5f);
    }

    double sum = 0.0;
    for (int i = 0; i < kMeasure; ++i) {
        sum += blocker.process(0.5f);
    }
    const double mean = sum / kMeasure;
    EXPECT_NEAR(mean, 0.0, 1e-6) << "nivel medio apos o regime";
}

TEST(DcBlocker, TransientDecaysMonotonically) {
    // degrau constante decai para zero, nunca cresce: evita pop no comeco.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    blocker.prepare();

    float previous = std::abs(blocker.process(0.5f));
    for (int i = 0; i < 20000; ++i) {
        const float magnitude = std::abs(blocker.process(0.5f));
        EXPECT_LE(magnitude, previous + 1e-7f) << "amostra " << i;
        previous = magnitude;
    }
    // em 20000 amostras o resto de 0,5 * R^n fica abaixo de 1e-4.
    EXPECT_LT(previous, 1e-4f) << "transiente nao decaiu o bastante";
}

TEST(DcBlocker, AttenuatesVeryLowFrequencies) {
    // 1a ordem, 6 dB/octava, corte em ~12 Hz: 5 Hz cai ~8 dB, 1 Hz ~21 dB. o T1
    // cobra -60 dBFS de energia na banda, nao -60 dB numa frequencia isolada.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    const double gain1Hz = blocker.gainDbAt(1.0);
    const double gain5Hz = blocker.gainDbAt(5.0);
    const double gain50Hz = blocker.gainDbAt(50.0);

    EXPECT_LT(gain1Hz, gain5Hz);
    EXPECT_LT(gain5Hz, gain50Hz);
    EXPECT_LT(gain1Hz, -18.0);
    EXPECT_LT(gain5Hz, -6.0);
}

TEST(DcBlocker, SlopeIsSixDecibelsPerOctave) {
    // uma oitava abaixo do corte cai ~6 dB; duas, ~12. e' o que separa de 2a ordem.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    const double cutoff = blocker.cutoffHz();
    const double oneOctaveBelow = blocker.gainDbAt(cutoff * 0.5);
    const double twoOctavesBelow = blocker.gainDbAt(cutoff * 0.25);
    EXPECT_NEAR(oneOctaveBelow, -6.0, 1.0);
    EXPECT_NEAR(twoOctavesBelow, -12.0, 1.5);
}

TEST(DcBlocker, GainIsMonotonicInFrequency) {
    // passa-alta de 1a ordem: o ganho so sobe com a frequencia.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    double previous = blocker.gainDbAt(1.0);
    for (double hz = 2.0; hz <= 2000.0; hz *= 1.5) {
        const double gain = blocker.gainDbAt(hz);
        EXPECT_GE(gain, previous - 1e-9) << "frequencia " << hz;
        previous = gain;
    }
}

TEST(DcBlocker, PassesMidbandSignal) {
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    const double gain1k = blocker.gainDbAt(1000.0);
    EXPECT_GT(gain1k, -1.0);
    EXPECT_LT(gain1k, 1.0);
}

TEST(DcBlocker, ResetClearsState) {
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    (void)blocker.process(1.0f);
    (void)blocker.process(1.0f);
    blocker.reset();
    EXPECT_FLOAT_EQ(blocker.process(0.0f), 0.0f);
}

TEST(DcBlocker, HandlesNullBlock) {
    DcBlocker blocker;
    blocker.processBlock(nullptr, 128); // nao deve escrever em lugar nenhum
    SUCCEED();
}

TEST(Limiter, CeilingIsMinusOneDb) {
    EXPECT_NEAR(Limiter::kCeilingDb, -1.0f, 1e-6f);
}

TEST(Limiter, HoldsSignalUnderCeiling) {
    Limiter limiter;
    limiter.setSampleRate(kSampleRate);
    limiter.prepare();
    limiter.setAttackMs(0.1);

    // sinal muito acima do teto: a saida nao passa do limite.
    float peak = 0.0f;
    for (int i = 0; i < 4410; ++i) {
        const float out = limiter.process(4.0f);
        peak = std::max(peak, std::abs(out));
    }
    EXPECT_LE(peak, limiter.ceiling() * 1.001f);
}

TEST(Limiter, AttackIsFastAndReleaseIsSlow) {
    Limiter limiter;
    limiter.setSampleRate(kSampleRate);
    limiter.prepare();
    limiter.setAttackMs(0.5);
    limiter.setReleaseMs(50.0);

    // depois de um pico forte o ganho recupera devagar.
    for (int i = 0; i < 100; ++i) {
        (void)limiter.process(2.0f);
    }
    const float afterPeak = limiter.currentGain();

    for (int i = 0; i < 100; ++i) {
        (void)limiter.process(0.0f);
    }
    const float afterSilence = limiter.currentGain();

    EXPECT_GT(afterSilence, afterPeak);
}

TEST(Limiter, ResetRestoresUnityGain) {
    Limiter limiter;
    limiter.setSampleRate(kSampleRate);
    limiter.prepare();
    (void)limiter.process(4.0f);
    limiter.reset();
    EXPECT_FLOAT_EQ(limiter.currentGain(), 1.0f);
}

TEST(Window, HannAndBlackmanAreZeroAtTheEdges) {
    // Hann e Blackman zeram nas bordas, o que evita clique na sobreposicao.
    // Hamming vale 0,08 de proposito: e' alternativa, nao o padrao do MVP.
    for (auto type : {WindowType::kHann, WindowType::kBlackman}) {
        std::vector<float> window(256);
        fillWindow(type, window.data(), 256);
        EXPECT_NEAR(window.front(), 0.0f, 1e-6f) << windowName(type);
        EXPECT_NEAR(window.back(), 0.0f, 1e-6f) << windowName(type);
    }
}

TEST(Window, HammingEdgesAreEightyMilli) {
    std::vector<float> window(256);
    fillWindow(WindowType::kHamming, window.data(), 256);
    EXPECT_NEAR(window.front(), 0.08f, 1e-6f);
    EXPECT_NEAR(window.back(), 0.08f, 1e-6f);
}

TEST(Window, GaussianIsSymmetricAndPositive) {
    std::vector<float> window(257);
    fillWindow(WindowType::kGaussian, window.data(), 257);
    for (const auto value : window) {
        EXPECT_GE(value, 0.0f);
    }
    for (std::size_t i = 0; i < window.size(); ++i) {
        EXPECT_NEAR(window[i], window[window.size() - 1 - i], 1e-6f);
    }
}

TEST(Window, SingleSampleIsUnity) {
    float value = 0.0f;
    fillWindow(WindowType::kHann, &value, 1);
    EXPECT_FLOAT_EQ(value, 1.0f);
}

TEST(Window, HandlesDegenerateInput) {
    fillWindow(WindowType::kHann, nullptr, 128);
    float value = 0.0f;
    fillWindow(WindowType::kHann, &value, 0);
    fillWindow(WindowType::kHann, &value, -5);
    SUCCEED();
}

TEST(Window, NamesAreStable) {
    // os nomes aparecem na interface, entao sao contrato visivel. contrato em
    // ingles, pela F015.
    EXPECT_STREQ(windowName(WindowType::kHann), "Hann");
    EXPECT_STREQ(windowName(WindowType::kGaussian), "Gaussian");
    EXPECT_STREQ(windowName(WindowType::kHamming), "Hamming");
    EXPECT_STREQ(windowName(WindowType::kBlackman), "Blackman");
}

TEST(Window, InvalidTypeFallsBackToHann) {
    // como nos filtros e no LFO: indice corrompido soa, nao cala.
    EXPECT_EQ(sanitizeWindowType(static_cast<WindowType>(99)), WindowType::kHann);
    EXPECT_EQ(sanitizeWindowType(WindowType::kBlackman), WindowType::kBlackman);
}

TEST(GranularEngine, PrepareWithoutSourceIsSilent) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 512);

    std::vector<float> left(512, 1.0f);
    std::vector<float> right(512, 1.0f);
    engine.processBlock(left.data(), right.data(), 512, GranularParams{});

    for (const auto sample : left) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(GranularEngine, ProducesAudioFromSyntheticSource) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    // o motor so toca com nota. antes do gate nao havia esta chamada e ele
    // granulava sozinho com material carregado.
    engine.setSounding(true);

    // senoide sintetica de 4096 amostras: nao depende de .exe real, fica rapida
    // e deterministica.
    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(2.0f * static_cast<float>(M_PI) * 440.0f *
                             static_cast<float>(i) / 44100.0f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 50.0f;
    params.position = 0.5f;

    // densidade em graos por segundo: 50/s a 44,1 kHz poe o primeiro grao por
    // volta do bloco 20. renderiza 1 s e mede o pico acumulado.
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float peak = 0.0f;
    for (int block = 0; block < 200; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        for (std::size_t i = 0; i < left.size(); ++i) {
            peak = std::max(peak, std::abs(left[i]));
        }
    }
    EXPECT_GT(peak, 0.0f);
}

TEST(GranularEngine, RespectsGrainSizeBounds) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    std::vector<float> source(8192, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    std::vector<float> left(256);
    std::vector<float> right(256);

    // acima do teto de 100 ms ainda produz, sem estourar o buffer pre-alocado.
    params.grainSizeMs = 1000.0f;
    params.densityGrainsPerSec = 200.0f;
    engine.processBlock(left.data(), right.data(), 256, params);

    for (const auto sample : left) {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(GranularEngine, NeverExceedsCeiling) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 512);
    std::vector<float> source(4096, 1.0f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.densityGrainsPerSec = 200.0f;
    params.volumeDb = 0.0f;

    for (int block = 0; block < 40; ++block) {
        std::vector<float> left(512, 0.0f);
        std::vector<float> right(512, 0.0f);
        engine.processBlock(left.data(), right.data(), 512, params);
        for (const auto sample : left) {
            EXPECT_LE(std::abs(sample), 1.0f);
        }
    }
}

TEST(GranularEngine, VoicePoolIsBounded) {
    EXPECT_EQ(GranularEngine::kMaxVoices, 8);
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    std::vector<float> source(4096, 0.3f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.densityGrainsPerSec = 200.0f;
    std::vector<float> left(256);
    std::vector<float> right(256);
    engine.processBlock(left.data(), right.data(), 256, params);

    EXPECT_LE(engine.lastActiveVoices(), GranularEngine::kMaxVoices);
}

TEST(GranularEngine, DeterministicAcrossRuns) {
    // mesmo material e mesmos params, mesmo bloco. e' o que torna T1 e T2
    // reproduziveis.
    const auto render = [] {
        GranularEngine engine;
        engine.prepare(kSampleRate, 256);
        std::vector<float> source(4096);
        for (std::size_t i = 0; i < source.size(); ++i) {
            source[i] = std::sin(static_cast<float>(i) * 0.01f);
        }
        engine.setSource(source.data(), source.size());

        GranularParams params;
        params.densityGrainsPerSec = 40.0f;
        std::vector<float> left(256, 0.0f);
        std::vector<float> right(256, 0.0f);
        engine.processBlock(left.data(), right.data(), 256, params);
        return left;
    };

    const auto first = render();
    const auto second = render();
    EXPECT_EQ(first, second);
}

TEST(GranularEngine, HandlesNullBuffers) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    engine.processBlock(nullptr, nullptr, 256, GranularParams{});
    engine.processBlock(nullptr, nullptr, 0, GranularParams{});
    SUCCEED();
}

TEST(GranularEngine, GrainSweepsBoundedFractionOfSource) {
    // regressao: o avanco por amostra e' a taxa dividida pelo comprimento da
    // fonte. somar a taxa crua a posicao normalizada varria o arquivo em duas
    // amostras e zerava a saida.
GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    engine.setSounding(true);

    constexpr std::size_t kSourceSize = 65536;
    std::vector<float> source(kSourceSize);
    for (std::size_t i = 0; i < kSourceSize; ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.05f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 40.0f;      // 40 ms = 1764 amostras a 44,1 kHz
    params.densityGrainsPerSec = 400.0f;
    params.position = 0.5f;
    params.pitchSemitones = 0.0f;
    params.spray = 0.0f;

    // a 1x, 1764 amostras andam 1764/65536 = 2,7% do arquivo. a varredura errada
    // matava o grao no primeiro bloco.
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    for (int block = 0; block < 8; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
    }
    EXPECT_GT(engine.lastActiveVoices(), 0)
        << "o grao de 40 ms precisa continuar vivo apos 8 blocos de 256";

    // e o sinal precisa de energia: um grao morto no primeiro bloco dava pico 1e-7.
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    float peak = 0.0f;
    for (int block = 0; block < 40; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        for (const auto sample : left) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    EXPECT_GT(peak, 0.01f) << "saida sem energia: pico " << peak;
}

TEST(GranularEngine, ResetSilencesVoices) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    std::vector<float> source(4096, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.densityGrainsPerSec = 100.0f;
    std::vector<float> left(256);
    std::vector<float> right(256);
    engine.processBlock(left.data(), right.data(), 256, params);
    engine.reset();
    EXPECT_EQ(engine.lastActiveVoices(), 0);
}

namespace {
// fonte constante e nivelada: pior caso para estalo, porque um degrau de saida
// aparece nitido no meio de um sinal sem envelope.
struct GateRig {
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlock = 128;

    GateRig() {
        engine.prepare(kSampleRate, kBlock);
        // sem setGateSeconds: os defaults (attack 5 ms, release 50 ms) ja sao o
        // comportamento antigo, e o release novo e' mais longo que a rampa antiga.
        source.assign(8192, 0.75f);
        engine.setSource(source.data(), source.size());
        params.densityGrainsPerSec = 60.0f;
        params.grainSizeMs = 20.0f;
        left.assign(kBlock, 0.0f);
        right.assign(kBlock, 0.0f);
    }

    void process(int blocks) {
        for (int i = 0; i < blocks; ++i) {
            engine.processBlock(left.data(), right.data(), kBlock, params);
        }
    }

    float peak() {
        float peakValue = 0.0f;
        for (const auto sample : left) {
            peakValue = std::max(peakValue, std::abs(sample));
        }
        return peakValue;
    }

    GranularEngine engine;
    std::vector<float> source;
    std::vector<float> left;
    std::vector<float> right;
    GranularParams params;
};
} // namespace

TEST(GranularEngineGate, SilentUntilSounding) {
    GateRig rig;

    // sem nota o motor consome blocos e nao devolve som. antes do gate o plugin
    // granulava sozinho assim que havia ficheiro carregado.
    rig.process(8);
    EXPECT_LT(rig.peak(), 1.0e-6f);
}

TEST(GranularEngineGate, OpensAndClosesWithTheNote) {
    GateRig rig;
    rig.engine.setSounding(true);
    EXPECT_TRUE(rig.engine.isSounding());

    // a 60 graos/s o primeiro grao nasce dezenas de blocos depois: precisa de
    // tempo renderizado.
    rig.process(80);
    EXPECT_GT(rig.peak(), 1.0e-4f);

    rig.engine.setSounding(false);
    EXPECT_FALSE(rig.engine.isSounding());
    // release de 50 ms a 48 kHz sao 2400 amostras: 24 blocos de 128 fecham com
    // folga. oito blocos, que bastavam para a rampa de 5 ms, deixariam cauda.
    rig.process(24);
    EXPECT_LT(rig.peak(), 1.0e-6f);
}

TEST(GranularEngineGate, RampIsGradualAndNotBinary) {
    GateRig rig;

    EXPECT_FLOAT_EQ(rig.engine.gateLevel(), 0.0f);

    rig.engine.setSounding(true);
    rig.process(1);
    const float afterOneBlock = rig.engine.gateLevel();

    // 5 ms a 48 kHz sao 240 amostras: um bloco de 128 deixa a rampa a meio. um
    // gate binario chegaria a 1 aqui, e esse degrau e' que estala.
    EXPECT_GT(afterOneBlock, 0.4f) << "a rampa subiu devagar demais: " << afterOneBlock;
    EXPECT_LT(afterOneBlock, 1.0f) << "a rampa parece binaria";

    rig.process(2);
    EXPECT_FLOAT_EQ(rig.engine.gateLevel(), 1.0f);

    rig.engine.setSounding(false);
    rig.process(1);
    const float closing = rig.engine.gateLevel();
    EXPECT_GT(closing, 0.0f) << "fechou de uma vez, sem rampa";
    EXPECT_LT(closing, 1.0f);

    // release de 50 ms: 20 blocos de 128 cobrem as 2400 amostras. o nivel fecha
    // em zero exato, porque o release conta a partir do nivel de soltura.
    rig.process(20);
    EXPECT_FLOAT_EQ(rig.engine.gateLevel(), 0.0f);
}

TEST(GranularEngineGate, RampTimeMatchesTheRequestedSeconds) {
    GateRig rig;
    rig.params.attackSeconds = 0.001f;
    rig.engine.setSounding(true);

    // 1 ms a 48 kHz sao 48 amostras: menos de meio bloco, a rampa fecha no primeiro.
    rig.process(1);
    EXPECT_FLOAT_EQ(rig.engine.gateLevel(), 1.0f);
}

TEST(GranularEngineGate, ResetClosesGate) {
    GateRig rig;
    rig.engine.setSounding(true);
    rig.process(4);
    rig.engine.reset();

    EXPECT_FALSE(rig.engine.isSounding());
    EXPECT_FLOAT_EQ(rig.engine.gateLevel(), 0.0f);
    rig.process(8);
    EXPECT_LT(rig.peak(), 1.0e-6f);
}
// ---------------------------------------------------------------------------
// telemetria dos graos
//
// o display le estes numeros para desenhar cada grao onde ele le o material. o
// que importa aqui nao e' o desenho: e' que o publish nunca minta sobre quantos
// graos existem nem sobre onde estao.
// ---------------------------------------------------------------------------

namespace {

// array de graos com as vozes indicadas vivas. as mortas ficam em 0,99 de
// proposito: e' o que sobraria se o publish escrevesse no indice da voz em vez
// de compactar, e e' isso que distingue as duas implementacoes.
std::array<Grain, GranularEngine::kMaxVoices> voicesWith(
    const std::vector<int>& activeVoiceIndices,
    double positionStep = 0.1) {
    std::array<Grain, GranularEngine::kMaxVoices> voices {};
    for (auto& grain : voices) {
        grain.active = false;
        grain.position = 0.99;
    }

    double position = positionStep;
    int grainCount = 0;
    for (const int voice : activeVoiceIndices) {
        auto& grain = voices[static_cast<std::size_t>(voice)];
        grain.active = true;
        grain.position = position;
        // o ganho sobe com a voz: com valor igual em todas, um erro de slot nao
        // aparece no ganho. 0,25 a 1,0; o motor nao passa de ~0,25 com 0 dB.
        grain.gain = 0.25f + 0.25f * static_cast<float>(grainCount);
        grain.windowIndex = 250;
        grain.remaining = 750;
        position += positionStep;
        ++grainCount;
    }
    return voices;
}

std::array<GrainView, GrainTelemetry::kMaxVoices> readAll(
    const GrainTelemetry& telemetry, int* countOut) {
    std::array<GrainView, GrainTelemetry::kMaxVoices> views {};
    *countOut = telemetry.read(views);
    return views;
}

} // namespace

TEST(GrainTelemetry, EachPublishedGrainCarriesItsOwnVoiceIndex) {
    // **a identidade e' o indice da voz, nunca a posicao no array.** o publish
    // compacta os activos para a frente, entao o slot diz a ordem, nao o grao.
    // um display ligado ao slot saltaria de grao sempre que um morresse.
    // nota: publica quatro graos nas vozes 0, 2, 3 e 5; a resposta tem de ser
    // 0, 2, 3, 5, e nao 0, 1, 2, 3.
    GrainTelemetry telemetry;
    telemetry.publish(voicesWith({0, 2, 3, 5}));

    int count = 0;
    const auto views = readAll(telemetry, &count);

    ASSERT_EQ(count, 4);
    EXPECT_EQ(views[0].voice, 0);
    EXPECT_EQ(views[1].voice, 2);
    EXPECT_EQ(views[2].voice, 3);
    EXPECT_EQ(views[3].voice, 5);
}

TEST(GrainTelemetry, VoiceIndexStaysWithTheGrainAcrossPublishes) {
    // a mesma voz, com posicao diferente em cada bloco, continua a reportar o
    // mesmo indice: e' o que deixa o display seguir o grao entre quadros.
    GrainTelemetry telemetry;

    auto first = voicesWith({1, 4});
    first[1].position = 0.20;
    first[4].position = 0.60;
    telemetry.publish(first);

    std::array<GrainView, GrainTelemetry::kMaxVoices> before {};
    ASSERT_EQ(telemetry.read(before), 2);
    ASSERT_EQ(before[0].voice, 1);
    ASSERT_EQ(before[1].voice, 4);

    auto second = voicesWith({1, 4});
    second[1].position = 0.25;
    second[4].position = 0.65;
    telemetry.publish(second);

    std::array<GrainView, GrainTelemetry::kMaxVoices> after {};
    ASSERT_EQ(telemetry.read(after), 2);
    EXPECT_EQ(after[0].voice, before[0].voice);
    EXPECT_EQ(after[1].voice, before[1].voice);

    // a posicao avancou mesmo, senao o teste passaria com um publish parado.
    EXPECT_GT(after[0].position, before[0].position);
    EXPECT_GT(after[1].position, before[1].position);
}

TEST(GrainTelemetry, AVoiceThatDiesFreesItsSlotForAnotherVoice) {
    // a voz 3 termina e a 6 nasce no mesmo bloco. as duas caem no slot 0 do
    // publish, porque compactam, e so a identidade diz que sao graos diferentes.
    // sem ela o display lia o mesmo grao a saltar e desenhava uma cauda falsa.
    auto before = voicesWith({3});
    GrainTelemetry telemetry;
    telemetry.publish(before);

    std::array<GrainView, GrainTelemetry::kMaxVoices> wasView {};
    ASSERT_EQ(telemetry.read(wasView), 1);
    ASSERT_EQ(wasView[0].voice, 3);

    telemetry.publish(voicesWith({6}));

    std::array<GrainView, GrainTelemetry::kMaxVoices> nowView {};
    ASSERT_EQ(telemetry.read(nowView), 1);
    EXPECT_EQ(nowView[0].voice, 6);
    EXPECT_NE(nowView[0].voice, wasView[0].voice)
        << "a voz nova ficou com a identidade da velha e o rastro vai atravessar o ecra";
}

TEST(GrainTelemetry, NaNGainIsPublishedAsZeroAndNotAsNaN) {
    // o clamp nao trata NaN - clamp(NaN) devolve NaN. um NaN publicado apaga o
    // brilho do grao sem erro nenhum, e o guard isfinite e' o que segura isto.
    auto voices = voicesWith({0});
    voices[0].gain = std::numeric_limits<float>::quiet_NaN();

    GrainTelemetry telemetry;
    telemetry.publish(voices);

    int count = 0;
    const auto views = readAll(telemetry, &count);
    ASSERT_EQ(count, 1);
    EXPECT_TRUE(std::isfinite(views[0].gain));
    EXPECT_FLOAT_EQ(views[0].gain, 0.0f);
}

TEST(GrainTelemetry, NaNPositionIsPublishedAsZeroAndNotAsNaN) {
    // mesma historia do ganho: NaN na posicao manda o grao para fora do ecra sem
    // erro. guard isfinite.
    auto voices = voicesWith({0});
    voices[0].position = std::numeric_limits<double>::quiet_NaN();

    GrainTelemetry telemetry;
    telemetry.publish(voices);

    int count = 0;
    const auto views = readAll(telemetry, &count);
    ASSERT_EQ(count, 1);
    EXPECT_TRUE(std::isfinite(views[0].position));
    EXPECT_FLOAT_EQ(views[0].position, 0.0f);
}

TEST(GrainTelemetry, ActiveGrainsAreCompactedToTheFront) {
    // vozes 1, 2 e 3 soam, a 0 esta parada. o count lido tem de servir de indice:
    // slots 0, 1 e 2 sao as vozes 1, 2 e 3. escrever no indice da voz traria a 0.
    GrainTelemetry telemetry;
    telemetry.publish(voicesWith({1, 2, 3}));

    int count = 0;
    const auto views = readAll(telemetry, &count);

    EXPECT_EQ(count, 3);
    EXPECT_FLOAT_EQ(views[0].position, 0.1f);
    EXPECT_FLOAT_EQ(views[1].position, 0.2f);
    EXPECT_FLOAT_EQ(views[2].position, 0.3f);

    // o ganho tambem tem de vir no slot certo: sem estas linhas o gain ficava sem
    // cobertura e podia ser achatado sem ninguem ver.
    EXPECT_FLOAT_EQ(views[0].gain, 0.25f);
    EXPECT_FLOAT_EQ(views[1].gain, 0.5f);
    EXPECT_FLOAT_EQ(views[2].gain, 0.75f);
}

TEST(GrainTelemetry, InactiveVoicesInTheMiddleDoNotTakeASlot) {
    // vozes 0, 2, 3 e 5 soam, com paradas no meio. escrever no indice da voz daria
    // um slot parado e tres activos.
    // nota: com as oito vozes activas a compactacao e o indice coincidem, e este
    // teste passava com a compactacao estragada.
    GrainTelemetry telemetry;
    telemetry.publish(voicesWith({0, 2, 3, 5}));

    int count = 0;
    const auto views = readAll(telemetry, &count);

    ASSERT_EQ(count, 4);
    EXPECT_FLOAT_EQ(views[0].position, 0.1f);
    EXPECT_FLOAT_EQ(views[1].position, 0.2f);
    EXPECT_FLOAT_EQ(views[2].position, 0.3f);
    EXPECT_FLOAT_EQ(views[3].position, 0.4f);

    // o ganho segue a voz, para o erro de slot aparecer mesmo quando as posicoes
    // batem certo.
    EXPECT_FLOAT_EQ(views[0].gain, 0.25f);
    EXPECT_FLOAT_EQ(views[1].gain, 0.5f);
    EXPECT_FLOAT_EQ(views[2].gain, 0.75f);
    EXPECT_FLOAT_EQ(views[3].gain, 1.0f);
}

TEST(GrainTelemetry, PhaseIsWindowIndexOverTheGrainLength) {
    // 250 de 1000 e' um quarto da janela: windowIndex e remaining somam o
    // comprimento original do grao.
    GrainTelemetry telemetry;
    telemetry.publish(voicesWith({0}));

    int count = 0;
    const auto views = readAll(telemetry, &count);
    ASSERT_EQ(count, 1);
    EXPECT_FLOAT_EQ(views[0].phase, 0.25f);
}

TEST(GrainTelemetry, GrainWithNoWindowProgressPublishesZeroPhaseAndNotNaN) {
    // **o GranularEngine nunca chega aqui:** o startGrain poe remaining =
    // comprimento e o renderVoice limpa active antes dos dois contadores zerarem
    // juntos. o guard existe porque o publish e' publico e recebe array do
    // chamador, logo tem de valer sozinho.
    // um NaN nao rebenta: propaga-se para a posicao do desenho e some com o unico
    // sinal de que ha graos.
    auto voices = voicesWith({0});
    voices[0].windowIndex = 0;
    voices[0].remaining = 0;

    GrainTelemetry telemetry;
    telemetry.publish(voices);

    int count = 0;
    const auto views = readAll(telemetry, &count);
    ASSERT_EQ(count, 1);
    EXPECT_TRUE(std::isfinite(views[0].phase));
    EXPECT_FLOAT_EQ(views[0].phase, 0.0f);
}

TEST(GrainTelemetry, PositionRunningPastTheEndIsClampedToOne) {
    // grao com pitch a subir passa de 1,0 e so volta por wrap. publicar o valor
    // cru punha-o fora do ecra a direita, e o drawRoutine saltava para o ecra inteiro.
    auto voices = voicesWith({0});
    voices[0].position = 1.4;

    GrainTelemetry telemetry;
    telemetry.publish(voices);

    int count = 0;
    const auto views = readAll(telemetry, &count);
    ASSERT_EQ(count, 1);
    EXPECT_FLOAT_EQ(views[0].position, 1.0f);
}

TEST(GrainTelemetry, ReadLeavesSlotsPastTheCountUntouched) {
    // o out do editor vive entre quadros: um read que escrevesse tudo deixaria
    // graos velhos no ecra. o contrato e' preencher so os slots devolvidos.
    GrainTelemetry telemetry;
    telemetry.publish(voicesWith({0, 1, 2}));

    std::array<GrainView, GrainTelemetry::kMaxVoices> views {};
    for (auto& view : views) {
        view = GrainView {-1, -1.0f, -1.0f, -1.0f};
    }

    const int count = telemetry.read(views);
    ASSERT_EQ(count, 3);
    for (int i = count; i < GrainTelemetry::kMaxVoices; ++i) {
        EXPECT_FLOAT_EQ(views[static_cast<std::size_t>(i)].position, -1.0f)
            << "o slot " << i << " foi escrito fora do count";
    }
}

TEST(GrainTelemetry, AGrainThatDiesInALaterBlockIsNoLongerPublished) {
    // o publish corre no fim de cada bloco: um grao que morre no bloco seguinte ja
    // nao pode estar publicado, senao o display mostrava-o parado, igual a um vivo.
    // nota: 20 ms a 44,1 kHz sao 882 amostras, um pouco mais de tres blocos de 256.
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);

    std::vector<float> material(48000, 0.4f);
    engine.setSource(material.data(), material.size());

    GranularParams params;
    params.grainSizeMs = 20.0f;
    params.densityGrainsPerSec = 10.0f; // ~1 grao a cada 17 blocos de 256
    params.spray = 0.0f;
    params.pitchSemitones = 0.0f;
    params.position = 0.5f;

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    std::array<GrainView, GrainTelemetry::kMaxVoices> views {};

    // a densidade vive no acumulador de fase: o grao nao nasce em bloco fixo, o
    // teste espera que ele apareca.
    int blocksUntilBorn = 0;
    while (engine.telemetry().read(views) == 0 && blocksUntilBorn < 60) {
        engine.processBlock(left.data(), right.data(), 256, params);
        ++blocksUntilBorn;
    }
    ASSERT_LT(blocksUntilBorn, 60) << "nenhum grao nasceu em 60 blocos";
    ASSERT_EQ(engine.telemetry().read(views), 1) << "esperava um grao vivo";

    // 882 amostras contra 256 por bloco: cinco blocos dao 1280, mais do que o grao
    // precisa mesmo nascendo no fim do bloco em que foi publicado.
    constexpr int kGrainBlocks = 5;
    int blocksAlive = 0;
    while (engine.telemetry().read(views) > 0 && blocksAlive < kGrainBlocks) {
        engine.processBlock(left.data(), right.data(), 256, params);
        ++blocksAlive;
    }

    EXPECT_EQ(engine.telemetry().read(views), 0)
        << "o grao durou mais do que " << kGrainBlocks
        << " blocos de 256 e continua publicado, aparecendo parado no ecra";
    EXPECT_LT(blocksAlive, kGrainBlocks)
        << "o grao sobreviveu a mais blocos do que 20 ms permiten";
}

TEST(GrainTelemetry, ProcessBlockPublishesTheGrainsItRendered) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 512);

    std::vector<float> material(48000);
    for (std::size_t i = 0; i < material.size(); ++i) {
        material[i] = static_cast<float>(std::sin(static_cast<double>(i) * 0.05) * 0.5);
    }
    engine.setSource(material.data(), material.size());

    GranularParams params;
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = 200.0f;
    params.position = 0.5f;

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);

    // densidade alta para aparecer cedo, mas nada garante o primeiro bloco: le ate 20.
    int count = 0;
    for (int block = 0; block < 20 && count == 0; ++block) {
        engine.processBlock(left.data(), right.data(), 512, params);
        std::array<GrainView, GrainTelemetry::kMaxVoices> views {};
        count = engine.telemetry().read(views);
    }

    ASSERT_GT(count, 0);
    EXPECT_LE(count, GranularEngine::kMaxVoices);

    std::array<GrainView, GrainTelemetry::kMaxVoices> views {};
    count = engine.telemetry().read(views);

    // **o count tem de bater com o motor, nao com o intervalo.** o clamp garante
    // [0, 1], entao um teste de intervalo passa mesmo com o publish a escrever
    // zero. activeVoiceCount diz se o numero desenhado e' o numero renderizado.
    EXPECT_EQ(count, engine.activeVoiceCount());

    for (int i = 0; i < count; ++i) {
        const auto& view = views[static_cast<std::size_t>(i)];
        EXPECT_GE(view.position, 0.0f);
        EXPECT_LE(view.position, 1.0f);
        EXPECT_GE(view.phase, 0.0f);
        EXPECT_LE(view.phase, 1.0f);
        EXPECT_TRUE(std::isfinite(view.gain));
    }

    // **a posicao obedece a identidade fechada, nao a um intervalo.** com spray e
    // pitch a zero: posicao - position == fase * comprimento / sourceCount.
    // um teste de intervalo teria tolerancia maior que a variacao, e ate uma
    // permutacao dos oito slots passaria.
    const double grainLength = 0.040 * kSampleRate; // comprimento do grao em amostras
    for (int i = 0; i < count; ++i) {
        const auto& view = views[static_cast<std::size_t>(i)];
        EXPECT_NEAR(static_cast<double>(view.position) - 0.5,
                    static_cast<double>(view.phase) * grainLength /
                        static_cast<double>(material.size()),
                    1.0e-4);
    }
}

TEST(GrainTelemetry, ClosingTheMaterialStopsPublishingGrains) {
    // sem material o caminho devolve antes de renderizar, e o publish foi posto
    // la tambem. sem ele os graos do ultimo bloco ficariam no ecra para sempre.
    GranularEngine engine;
    engine.prepare(kSampleRate, 512);

    std::vector<float> material(48000);
    for (std::size_t i = 0; i < material.size(); ++i) {
        material[i] = static_cast<float>(std::sin(static_cast<double>(i) * 0.05) * 0.5);
    }
    engine.setSource(material.data(), material.size());

    GranularParams params;
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = 400.0f;
    params.position = 0.5f;

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);

    int beforeClosing = 0;
    for (int block = 0; block < 20 && beforeClosing == 0; ++block) {
        engine.processBlock(left.data(), right.data(), 512, params);
        std::array<GrainView, GrainTelemetry::kMaxVoices> views {};
        beforeClosing = engine.telemetry().read(views);
    }
    ASSERT_GT(beforeClosing, 0);

    engine.setSource(nullptr, 0);
    engine.processBlock(left.data(), right.data(), 512, params);

    std::array<GrainView, GrainTelemetry::kMaxVoices> views {};
    EXPECT_EQ(engine.telemetry().read(views), 0);
}
