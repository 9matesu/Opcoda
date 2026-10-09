#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/dsp/lfo.h"

#include <cmath>
#include <limits>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::dsp::LFO;
using opcoda::dsp::LFOTarget;
using opcoda::dsp::LFOWave;
using opcoda::dsp::lfoTargetName;
using opcoda::dsp::lfoWaveName;
using opcoda::dsp::sanitizeLFOTarget;
using opcoda::dsp::sanitizeLFOWave;

namespace {
constexpr double kSampleRate = 44100.0;
constexpr int kBlock = 256;

// Maior diferenca absoluta entre dois buffers. Piso de audibilidade, nao de
// sinal: sem ele, 1 ULP satisfazia um teste chamado "Audible" — a licao do
// 5.6e-7. So o canal esquerdo: o pan e' deterministico e estatico, e a
// diferenca aparece igual nos dois; anotado aqui para ninguem "corrigir".
float maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b) {
    float worst = 0.0f;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        worst = std::max(worst, std::abs(a[i] - b[i]));
    }
    return worst;
}

} // namespace

TEST(LFO, AllWavesStayBipolar) {
    // Nenhuma forma sai de [-1, 1]: o LFO multiplica afinacao, posicao e
    // densidade, e um valor fora da faixa viraria range excedido no alvo.
    for (const auto wave : {LFOWave::kSine, LFOWave::kTri, LFOWave::kSaw, LFOWave::kSquare,
                            LFOWave::kSampleHold}) {
        LFO lfo;
        for (int step = 0; step <= 100; ++step) {
            lfo.advance(1.0f, 480, 48000.0);
            const auto value = lfo.blockValue(wave);
            EXPECT_GE(value, -1.0f) << "onda=" << static_cast<int>(wave);
            EXPECT_LE(value, 1.0f) << "onda=" << static_cast<int>(wave);
        }
    }
}

TEST(LFO, SpotValues) {
    // Ancoras fechadas das quatro formas periodicas. Se a fase ou a forma
    // mudarem de convencao, e' aqui que quebra — e nao num ouvido a 60 Hz.
    LFO lfo;
    EXPECT_FLOAT_EQ(lfo.valueAt(LFOWave::kSine, 1.0f, 0, 48000.0), 0.0f);
    EXPECT_NEAR(lfo.valueAt(LFOWave::kSine, 1.0f, 12000, 48000.0), 1.0f, 1e-6);
    EXPECT_NEAR(lfo.valueAt(LFOWave::kTri, 1.0f, 0, 48000.0), 1.0f, 1e-6);
    EXPECT_NEAR(lfo.valueAt(LFOWave::kTri, 1.0f, 24000, 48000.0), -1.0f, 1e-6);
    EXPECT_NEAR(lfo.valueAt(LFOWave::kSaw, 1.0f, 0, 48000.0), -1.0f, 1e-6);
    EXPECT_NEAR(lfo.valueAt(LFOWave::kSaw, 1.0f, 24000, 48000.0), 0.0f, 1e-6);
    EXPECT_FLOAT_EQ(lfo.valueAt(LFOWave::kSquare, 1.0f, 0, 48000.0), 1.0f);
    EXPECT_FLOAT_EQ(lfo.valueAt(LFOWave::kSquare, 1.0f, 24000, 48000.0), -1.0f);
}

TEST(LFO, SampleHoldIsDeterministicPerCycle) {
    // Mesmo ciclo, mesmo valor — em instancias diferentes, porque nao ha
    // estado de RNG: o ciclo 7 de hoje e' o ciclo 7 de amanha. E nem todos os
    // ciclos sao iguais, senao o S&H seria DC com outro nome.
    LFO first;
    LFO second;
    first.advance(1.0f, 48000 * 3 + 100, 48000.0);
    second.advance(1.0f, 48000 * 3 + 100, 48000.0);
    EXPECT_FLOAT_EQ(first.blockValue(LFOWave::kSampleHold),
                    second.blockValue(LFOWave::kSampleHold));

    bool varied = false;
    float previous = first.blockValue(LFOWave::kSampleHold);
    for (int cycle = 0; cycle < 100; ++cycle) {
        // 48000 + 173 amostras: um ciclo e um quebrado, para cada iteracao cair
        // num ciclo diferente. Avancar de exatamente 1 ciclo repetiria o mesmo
        // ciclo para sempre — e foi o que a primeira versao deste ensaio fazia.
        first.advance(1.0f, 48000 + 173, 48000.0);
        const auto value = first.blockValue(LFOWave::kSampleHold);
        EXPECT_GE(value, -1.0f);
        EXPECT_LE(value, 1.0f);
        varied = varied || (value != previous);
        previous = value;
    }
    EXPECT_TRUE(varied) << "100 ciclos identicos: S&H virou DC";
}

TEST(LFO, AdvanceCountsWholeCycles) {
    // 1 Hz a 48 kHz em 48000 amostras e' exatamente uma volta: a fase marca
    // 1,0 (sem wrap — o indice do ciclo do S&H precisa dela inteira) e o seno,
    // que dobra a fracao, repete o zero. Ancora a aritmetica, nao so a ideia.
    LFO lfo;
    lfo.advance(1.0f, 48000, 48000.0);
    EXPECT_DOUBLE_EQ(lfo.phase(), 1.0);
    EXPECT_NEAR(lfo.blockValue(LFOWave::kSine), 0.0, 1e-6);
}

TEST(LFO, NamesCoverAllTargetsAndWaves) {
    EXPECT_STREQ(lfoTargetName(LFOTarget::kPitch), "Pitch");
    EXPECT_STREQ(lfoTargetName(LFOTarget::kDensity), "Density");
    EXPECT_STREQ(lfoTargetName(LFOTarget::kCutoff), "Cutoff");
    EXPECT_STREQ(lfoTargetName(LFOTarget::kPosition), "Position");
    EXPECT_STREQ(lfoWaveName(LFOWave::kSine), "Sine");
    EXPECT_STREQ(lfoWaveName(LFOWave::kTri), "Tri");
    EXPECT_STREQ(lfoWaveName(LFOWave::kSaw), "Saw");
    EXPECT_STREQ(lfoWaveName(LFOWave::kSquare), "Square");
    EXPECT_STREQ(lfoWaveName(LFOWave::kSampleHold), "S&H");
}

TEST(LFO, InvalidTargetAndWaveFallBack) {
    // Indice corrompido de choice vira o primeiro da lista, e nao erro: tem de
    // soar. E' a mesma regra do sanitizeFilterType, pelo mesmo motivo.
    // Fronteira exata (4/5, logo apos o ultimo valido) mais um valor absurdo.
    EXPECT_EQ(sanitizeLFOTarget(static_cast<LFOTarget>(99)), LFOTarget::kPitch);
    EXPECT_EQ(sanitizeLFOWave(static_cast<LFOWave>(99)), LFOWave::kSine);
    EXPECT_EQ(sanitizeLFOTarget(static_cast<LFOTarget>(4)), LFOTarget::kPitch);
    EXPECT_EQ(sanitizeLFOWave(static_cast<LFOWave>(5)), LFOWave::kSine);
    EXPECT_EQ(sanitizeLFOTarget(LFOTarget::kPosition), LFOTarget::kPosition);
    EXPECT_EQ(sanitizeLFOWave(LFOWave::kSampleHold), LFOWave::kSampleHold);
}

TEST(LFO, RateZeroFreezesPhase) {
    // Taxa zero congela a fase: vira offset estatico, que e' uso legitimo (um
    // detune fixo sem tocar no knob), nao erro. Sem este ensaio, uma regressao
    // que tratasse 0 como "pula o LFO" vs "offset congelado" passava em tudo.
    LFO lfo;
    lfo.advance(1.0f, 24000, 48000.0);
    const auto frozen = lfo.phase();
    lfo.advance(0.0f, 48000, 48000.0);
    EXPECT_DOUBLE_EQ(lfo.phase(), frozen);
    // E congelado significa constante em qualquer offset do bloco.
    EXPECT_FLOAT_EQ(lfo.valueAt(LFOWave::kSine, 0.0f, 0, 48000.0),
                    lfo.valueAt(LFOWave::kSine, 0.0f, 255, 48000.0));
}

namespace {

// Renderiza 200 blocos de senoide com o LFO dado. Motor fresco por chamada.
std::vector<float> renderLFO(float depth,
                             LFOTarget target,
                             LFOWave wave = LFOWave::kSine,
                             float rateHz = 2.0f) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.01f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 50.0f;
    params.position = 0.5f;
    params.lfodepth = depth;
    params.lfotarget = target;
    params.lfowave = wave;
    params.lforateHz = rateHz;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<float> out;
    for (int block = 0; block < 200; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        out.insert(out.end(), left.begin(), left.end());
    }
    return out;
}

} // namespace

TEST(LFOEngine, DepthZeroIsIdentity) {
    // Com depth 0, taxa/forma/alvo malucos nao podem mudar nada: todas as
    // contribuicoes multiplicam por zero. E' o que garante que o LFO desligado
    // preserva o T1 e o comportamento antigo bit a bit.
    const auto plain = renderLFO(0.0f, LFOTarget::kPitch);
    EXPECT_EQ(plain, renderLFO(0.0f, LFOTarget::kCutoff, LFOWave::kSquare, 20.0f));
    EXPECT_EQ(plain, renderLFO(0.0f, LFOTarget::kDensity, LFOWave::kSampleHold, 0.0f));
}

TEST(LFOEngine, PitchTargetIsAudible) {
    // ±12 st a 2 Hz sobre 200 blocos movem cada grao para um sitio diferente
    // da senoide. Vetores deterministicos, e o piso de 1e-3 barra placebo de
    // 1 ULP: o nome diz audivel, e 1e-3 com picos em 0,1 e' audivel com folga.
    const auto dry = renderLFO(0.0f, LFOTarget::kPitch);
    const auto wet = renderLFO(1.0f, LFOTarget::kPitch);
    ASSERT_FALSE(dry.empty());
    ASSERT_EQ(dry.size(), wet.size());
    EXPECT_GT(maxAbsDiff(dry, wet), 1e-3f);
    for (const auto sample : wet) {
        ASSERT_TRUE(std::isfinite(sample));
    }
}

TEST(LFOEngine, DensityTargetChangesSpawning) {
    // Densidade modulada muda o agendamento: com a mesma fonte e os mesmos
    // graos, so o *quando* muda — e o quando muda o som. Sem diferenca aqui, o
    // alvo density seria placebo.
    const auto dry = renderLFO(0.0f, LFOTarget::kDensity);
    const auto wet = renderLFO(1.0f, LFOTarget::kDensity);
    ASSERT_EQ(dry.size(), wet.size());
    EXPECT_GT(maxAbsDiff(dry, wet), 1e-3f);
}

TEST(LFOEngine, CutoffTargetIsAudible) {
    const auto dry = renderLFO(0.0f, LFOTarget::kCutoff);
    const auto wet = renderLFO(1.0f, LFOTarget::kCutoff);
    ASSERT_EQ(dry.size(), wet.size());
    EXPECT_GT(maxAbsDiff(dry, wet), 1e-3f);
}

TEST(LFOEngine, PositionTargetIsAudible) {
    const auto dry = renderLFO(0.0f, LFOTarget::kPosition);
    const auto wet = renderLFO(1.0f, LFOTarget::kPosition);
    ASSERT_EQ(dry.size(), wet.size());
    EXPECT_GT(maxAbsDiff(dry, wet), 1e-3f);
}

TEST(LFOEngine, SampleHoldModulatesThroughEngine) {
    // O S&H aparecia em finitude e em profundidade zero, mas nunca modulando:
    // um waveAt(kSampleHold) que devolvesse DC constante passava em tudo. Aqui
    // ele tem de mover o som de um jeito diferente do seno — mesma taxa, mesma
    // profundidade, so a forma muda.
    const auto sine = renderLFO(1.0f, LFOTarget::kPitch, LFOWave::kSine, 2.0f);
    const auto held = renderLFO(1.0f, LFOTarget::kPitch, LFOWave::kSampleHold, 2.0f);
    ASSERT_EQ(sine.size(), held.size());
    EXPECT_GT(maxAbsDiff(sine, held), 1e-3f);
}

TEST(LFOEngine, GarbageEnumsThroughEngineStayFinite) {
    // O sanitize e' testado em unidade, mas ninguem provava que o motor o
    // chama: sem a chamada no processBlock, um indice 99 caia no `return 0.0f`
    // do waveAt ou, pior, num switch sem cobertura. Aqui o 99 tem de soar
    // finito — e igual ao default saneado, porque e' para isso que o sanitize
    // existe.
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.01f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 50.0f;
    params.position = 0.5f;
    params.lfodepth = 1.0f;
    params.lforateHz = 2.0f;
    params.lfotarget = static_cast<LFOTarget>(99);
    params.lfowave = static_cast<LFOWave>(99);

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float peak = 0.0f;
    for (int block = 0; block < 60; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        for (const auto sample : left) {
            ASSERT_TRUE(std::isfinite(sample)) << "bloco " << block;
            peak = std::max(peak, std::abs(sample));
        }
    }
    // E soa: saneado para pitch+sine, que modulam de verdade.
    EXPECT_GT(peak, 0.01f);
}

TEST(LFOEngine, AllWavesStayFiniteThroughEngine) {
    // Todas as formas, inclusive quadrada e S&H com seus degraus, passam pelo
    // motor sem NaN — em todos os alvos, porque o degrau entra por caminho
    // diferente em cada um (pitch soma, densidade multiplica, cutoff expoe,
    // posicao desloca). 20 blocos por combinacao chegam: sao 20 combinacoes
    // rapidas.
    for (const auto target :
         {LFOTarget::kPitch, LFOTarget::kDensity, LFOTarget::kCutoff, LFOTarget::kPosition}) {
        for (const auto wave :
             {LFOWave::kSine, LFOWave::kTri, LFOWave::kSaw, LFOWave::kSquare,
              LFOWave::kSampleHold}) {
            const auto out = renderLFO(1.0f, target, wave, 5.0f);
            for (const auto sample : out) {
                ASSERT_TRUE(std::isfinite(sample))
                    << "alvo=" << static_cast<int>(target) << " onda=" << static_cast<int>(wave);
            }
        }
    }
}

TEST(LFOEngine, NaNRateAndDepthAreSafe) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    params.lforateHz = std::numeric_limits<float>::quiet_NaN();
    params.lfodepth = std::numeric_limits<float>::quiet_NaN();

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    for (int block = 0; block < 40; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        for (const auto sample : left) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no esquerdo, bloco " << block;
        }
        for (const auto sample : right) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no direito, bloco " << block;
        }
    }
}
