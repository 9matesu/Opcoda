#include <gtest/gtest.h>

#include "opcoda_core/dsp/dc_blocker.h"
#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/dsp/limiter.h"
#include "opcoda_core/dsp/window.h"

#include <algorithm>
#include <cmath>
#include <vector>

using opcoda::dsp::DcBlocker;
using opcoda::dsp::windowName;
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
    // A fundamentacao exige corte entre 10 e 15 Hz a 44,1 kHz.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    const double cutoff = blocker.cutoffHz();
    EXPECT_GT(cutoff, 10.0);
    EXPECT_LT(cutoff, 15.0);
}

TEST(DcBlocker, AttenuationAtDcIsEffectivelyInfinite) {
    // H(1) = 0: o bin DC e' anulado pelo numerador. E o que sustenta o
    // criterio de 40 dB do ensaio T1.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    EXPECT_LT(blocker.dcGainDb(), -200.0);
}

TEST(DcBlocker, RemovesConstantOffset) {
    // Sinal com offset DC de 0,5 deve sair sem nivel medio. A transiente de
    // partida (y[0] = 0,5) decai com R^n, entao a media so converge depois de
    // algumas dezenas de milhares de amostras: medir a media inclui o decaimento
    // e nao representa o regime permanente.
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
    // A saida de um degrau constante tem de decair em direcao a zero, nunca
    // crescer: e o que garante ausencia de pop no comeco da reproducao.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    blocker.prepare();

    float previous = std::abs(blocker.process(0.5f));
    for (int i = 0; i < 20000; ++i) {
        const float magnitude = std::abs(blocker.process(0.5f));
        EXPECT_LE(magnitude, previous + 1e-7f) << "amostra " << i;
        previous = magnitude;
    }
    // R = 0,9983 decai 0,5 * R^n; com n = 20000 o resto fica abaixo de 1e-4.
    EXPECT_LT(previous, 1e-4f) << "transiente nao decaiu o bastante";
}

TEST(DcBlocker, AttenuatesVeryLowFrequencies) {
    // Filtro de 1a ordem, inclinacao de 6 dB/octava. Com corte em ~12 Hz, a
    // atenuacao em 5 Hz e de ~8 dB (menos de uma oitava abaixo do corte) e em
    // 1 Hz e de ~21 dB (duas oitavas). O que o ensaio T1 exige e -60 dBFS de
    // energia na banda, e nao -60 dB de ganho em uma frequencia isolada.
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
    // Uma oitava abaixo do corte a atenuacao e por volta de 6 dB. E o que
    // distingue este filtro de um de 2a ordem.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    const double cutoff = blocker.cutoffHz();
    const double oneOctaveBelow = blocker.gainDbAt(cutoff * 0.5);
    const double twoOctavesBelow = blocker.gainDbAt(cutoff * 0.25);
    EXPECT_NEAR(oneOctaveBelow, -6.0, 1.0);
    EXPECT_NEAR(twoOctavesBelow, -12.0, 1.5);
}

TEST(DcBlocker, GainIsMonotonicInFrequency) {
    // Resposta de passa-alta de primeira ordem: o ganho sobe com a frequencia.
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

    // Sinal muito acima do teto: a saida nao pode passar do limite.
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

    // Atras de um pico forte, o ganho deve recuperar devagar.
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
    // Hann e Blackman zeram nas bordas, e o que evita clique na sobreposicao
    // entre graos. Hamming de proposito nao zera: vale 0,08, o ramo de
    // sidelobe dele e o que a torna uma alternativa, nao o padrao do MVP.
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
    // Os nomes aparecem na interface, entao fazem parte do contrato visivel.
    EXPECT_STREQ(windowName(WindowType::kHann), "Hann");
    EXPECT_STREQ(windowName(WindowType::kGaussian), "Gaussiana");
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
    // O motor so toca com nota. Antes do gate esta chamada nao existia e o
    // motor granulava sozinho assim que havia material carregado.
    engine.setSounding(true);

    // Fonte sintetica: 4096 amostras de senoide. Evita depender de um .exe real
    // no teste unitario, mantendo o ensaio rapido e deterministico.
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

    // A densidade e' em graos por segundo: a 50/s com bloco de 256 a 44,1 kHz
    // o primeiro grao nasce depois de ~20 blocos. Renderiza um segundo inteiro
    // e mede o pico acumulado.
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

    // Alem do teto de 100 ms o motor precisa continuar produzindo, sem
    // estourar o buffer pre-alocado.
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
    // Mesmo material e mesmos parametros produzem o mesmo bloco. E o que torna
    // os ensaios T1 e T2 reproduziveis.
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
    // Regressao: o avanco por amostra tem que ser a taxa dividida pelo
    // comprimento da fonte. Somar a taxa crua a uma posicao normalizada fazia
    // o grao varrer o arquivo inteiro em duas amostras, o que zerava a saida.
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
    params.grainSizeMs = 40.0f;      // 1764 amostras a 44,1 kHz
    params.densityGrainsPerSec = 400.0f;
    params.position = 0.5f;
    params.pitchSemitones = 0.0f;
    params.spray = 0.0f;

    // Com a taxa correta, 1764 amostras a 1x percorrem 1764/65536 = 2,7% do
    // arquivo. A varredura inteira levaria o grao a terminar em bloco.
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    for (int block = 0; block < 8; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
    }
    EXPECT_GT(engine.lastActiveVoices(), 0)
        << "o grao de 40 ms precisa continuar vivo apos 8 blocos de 256";

    // E o sinal tem de ter energia: um grao que morreu no primeiro bloco
    // produzia pico residual de 1e-7.
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
// Fonte nivelada e constante: e' o pior caso para estalo, porque um degrau de
// saida aparece com nitidez no meio de um sinal sem envelope.
struct GateRig {
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlock = 128;

    GateRig() {
        engine.prepare(kSampleRate, kBlock);
        engine.setGateSeconds(0.005);
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

    // Sem nota, o motor consome blocos e nao devolve som. Antes do gate isto
    // era o comportamento normal do plugin, e era por isso que o motor
    // granulava sozinho assim que havia ficheiro carregado.
    rig.process(8);
    EXPECT_LT(rig.peak(), 1.0e-6f);
}

TEST(GranularEngineGate, OpensAndClosesWithTheNote) {
    GateRig rig;
    rig.engine.setSounding(true);
    EXPECT_TRUE(rig.engine.isSounding());

    // A 50 graos por segundo o primeiro grao so nasce depois de dezenas de
    // blocos, entao aaudio so aparece depois de renderizar tempo suficiente.
    rig.process(80);
    EXPECT_GT(rig.peak(), 1.0e-4f);

    rig.engine.setSounding(false);
    EXPECT_FALSE(rig.engine.isSounding());
    rig.process(8);
    EXPECT_LT(rig.peak(), 1.0e-6f);
}

TEST(GranularEngineGate, RampIsGradualAndNotBinary) {
    GateRig rig;

    EXPECT_FLOAT_EQ(rig.engine.gateLevel(), 0.0f);

    rig.engine.setSounding(true);
    rig.process(1);
    const float afterOneBlock = rig.engine.gateLevel();

    // 5 ms a 48 kHz sao 240 amostras, entao um bloco de 128 deve deixar a
    // rampa a meio caminho. Um gate binario chegaria a 1 aqui, e e' exatamente
    // esse degrau que produziria o estalo.
    EXPECT_GT(afterOneBlock, 0.4f) << "a rampa subiu devagar demais: " << afterOneBlock;
    EXPECT_LT(afterOneBlock, 1.0f) << "a rampa parece binaria";

    rig.process(2);
    EXPECT_FLOAT_EQ(rig.engine.gateLevel(), 1.0f);

    rig.engine.setSounding(false);
    rig.process(1);
    const float closing = rig.engine.gateLevel();
    EXPECT_GT(closing, 0.0f) << "fechou de uma vez, sem rampa";
    EXPECT_LT(closing, 1.0f);

    rig.process(2);
    EXPECT_FLOAT_EQ(rig.engine.gateLevel(), 0.0f);
}

TEST(GranularEngineGate, RampTimeMatchesTheRequestedSeconds) {
    GateRig rig;
    rig.engine.setGateSeconds(0.001);
    rig.engine.setSounding(true);

    // 1 ms a 48 kHz sao 48 amostras: menos da metade de um bloco, entao o
    // primeiro bloco ja fecha a rampa.
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