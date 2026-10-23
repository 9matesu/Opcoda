#include <gtest/gtest.h>

#include "opcoda_core/dsp/biquad.h"
#include "opcoda_core/dsp/granular_engine.h"

#include <cmath>
#include <limits>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using opcoda::dsp::Biquad;
using opcoda::dsp::BiquadCoeffs;
using opcoda::dsp::FilterType;
using opcoda::dsp::filterName;
using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::dsp::makeBiquadCoeffs;
using opcoda::dsp::sanitizeFilterType;

namespace {
constexpr double kSampleRate = 44100.0;
constexpr int kBlock = 256;

// amplitude em regime de uma senoide filtrada: descarta os primeiros 4000 samples
// e mede o pico dos ultimos 1000. com a transiente dentro media-se a partida,
// nao o ganho em regime.
// nota: vale para Q <= 2. com Q = 12 em 100 Hz, tau ~1680 e 4000 amostras
// deixam ~9 % de transiente; quem precisar de Q alto aumenta o descarte junto.
float steadyPeak(FilterType type, float cutoffHz, float q, float toneHz,
                 double sampleRate = kSampleRate) {
    BiquadCoeffs coeffs = makeBiquadCoeffs(type, cutoffHz, q, sampleRate);
    Biquad filter;

    float peak = 0.0f;
    for (int i = 0; i < 5000; ++i) {
        const float x = std::sin(2.0f * static_cast<float>(M_PI) * toneHz *
                                 static_cast<float>(i) / static_cast<float>(sampleRate));
        const float y = filter.process(x, coeffs);
        if (i >= 4000) {
            peak = std::max(peak, std::abs(y));
        }
    }
    return peak;
}

} // namespace

TEST(Biquad, LowPassPassesLowRejectsHigh) {
    // 100 Hz passa perto de 1; 10 kHz com corte em 1 kHz cai mais de 40 dB.
    // duas oitavas e meia por decada a 12 dB/oitava dao folga para a margem.
    EXPECT_NEAR(steadyPeak(FilterType::kLowPass, 1000.0f, 0.7071f, 100.0f), 1.0, 0.05);
    EXPECT_LT(steadyPeak(FilterType::kLowPass, 1000.0f, 0.7071f, 10000.0f), 0.01f);
}

TEST(Biquad, HighPassPassesHighRejectsLow) {
    EXPECT_NEAR(steadyPeak(FilterType::kHighPass, 1000.0f, 0.7071f, 10000.0f), 1.0, 0.05);
    EXPECT_LT(steadyPeak(FilterType::kHighPass, 1000.0f, 0.7071f, 100.0f), 0.01f);
}

TEST(Biquad, CoefficientsFollowSampleRate) {
    // corte hardcoded em 44,1 kHz ou cache sem a taxa dariam a mesma resposta nas
    // duas taxas; a 48 kHz a matematica e' outra.
    EXPECT_NEAR(steadyPeak(FilterType::kLowPass, 1000.0f, 0.7071f, 100.0f, 48000.0), 1.0,
                0.05);
    EXPECT_LT(steadyPeak(FilterType::kLowPass, 1000.0f, 0.7071f, 10000.0f, 48000.0),
              0.01f);
}

TEST(Biquad, BandPassPeaksAtCenter) {
    // pico a 0 dB no centro por construcao (b0 = alpha), bordas caem. relativo,
    // nao absoluto, para nao depender da aritmetica exata do livro.
    const auto center = steadyPeak(FilterType::kBandPass, 1000.0f, 1.0f, 1000.0f);
    EXPECT_NEAR(center, 1.0, 0.15);
    EXPECT_LT(steadyPeak(FilterType::kBandPass, 1000.0f, 1.0f, 100.0f), center / 3.0f);
    EXPECT_LT(steadyPeak(FilterType::kBandPass, 1000.0f, 1.0f, 10000.0f), center / 3.0f);
}

TEST(Biquad, NotchRejectsCenter) {
    EXPECT_LT(steadyPeak(FilterType::kNotch, 1000.0f, 1.0f, 1000.0f), 0.05f);
    EXPECT_NEAR(steadyPeak(FilterType::kNotch, 1000.0f, 1.0f, 100.0f), 1.0, 0.05);
}

TEST(Biquad, ResetClearsRinging) {
    // depois de sinal, reset + zeros tem de dar zeros: residuo seria cauda fantasma.
    BiquadCoeffs coeffs = makeBiquadCoeffs(FilterType::kLowPass, 1000.0f, 2.0f, kSampleRate);
    Biquad filter;
    for (int i = 0; i < 1000; ++i) {
        static_cast<void>(filter.process(1.0f, coeffs));
    }
    filter.reset();
    for (int i = 0; i < 64; ++i) {
        EXPECT_FLOAT_EQ(filter.process(0.0f, coeffs), 0.0f);
    }
}

TEST(Biquad, ExtremeSettingsStayFiniteAndBounded) {
    // extremos de corte e Q com os piores sinais: DC cheio e alternancia a
    // nyquist, cada um no seu laco. |y| < 100 nao prova estabilidade - um Q
    // legitimo da 12x - so que nunca da NaN nem explode: e' rede contra fuga.
    for (const auto type :
         {FilterType::kLowPass, FilterType::kHighPass, FilterType::kBandPass, FilterType::kNotch}) {
        for (const auto cutoff : {20.0f, 20000.0f}) {
            for (const auto q : {0.5f, 12.0f}) {
                BiquadCoeffs coeffs = makeBiquadCoeffs(type, cutoff, q, kSampleRate);
                Biquad dc;
                for (int i = 0; i < 20000; ++i) {
                    const float y = dc.process(1.0f, coeffs);
                    ASSERT_TRUE(std::isfinite(y)) << "DC, tipo=" << static_cast<int>(type);
                    ASSERT_LT(std::abs(y), 100.0f) << "DC, tipo=" << static_cast<int>(type);
                }
                Biquad nyquist;
                for (int i = 0; i < 20000; ++i) {
                    const float x = (i % 2 == 0) ? 1.0f : -1.0f;
                    const float y = nyquist.process(x, coeffs);
                    ASSERT_TRUE(std::isfinite(y)) << "Nyquist, tipo=" << static_cast<int>(type);
                    ASSERT_LT(std::abs(y), 100.0f)
                        << "Nyquist, tipo=" << static_cast<int>(type);
                }
            }
        }
    }
}

TEST(Biquad, LongSilenceSnapsToExactZero) {
    // sem snap a cauda estaciona em subnormal e cada operacao custa 10-100x.
    // Q=12 a 1 kHz e depois 200 mil amostras: muito alem das ~7 mil necessarias.
    BiquadCoeffs coeffs = makeBiquadCoeffs(FilterType::kLowPass, 1000.0f, 12.0f, kSampleRate);
    Biquad filter;
    for (int i = 0; i < 5000; ++i) {
        static_cast<void>(filter.process(
            std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * static_cast<float>(i) /
                     44100.0f),
            coeffs));
    }
    for (int i = 0; i < 200000; ++i) {
        const float y = filter.process(0.0f, coeffs);
        if (i >= 199000) {
            EXPECT_FLOAT_EQ(y, 0.0f) << "residuo subnormal na amostra " << i;
        }
    }
}

TEST(Biquad, NaNInputsBecomeWire) {
    // NaN em corte ou Q nao sai no barramento: vira passagem (corte 20 kHz) com Q
    // de Butterworth, e a senoide passa finita.
    BiquadCoeffs coeffs = makeBiquadCoeffs(FilterType::kLowPass,
                                           std::numeric_limits<float>::quiet_NaN(),
                                           std::numeric_limits<float>::quiet_NaN(), kSampleRate);
    Biquad filter;
    float peak = 0.0f;
    for (int i = 0; i < 5000; ++i) {
        const float y = filter.process(0.5f, coeffs);
        ASSERT_TRUE(std::isfinite(y));
        if (i >= 4000) {
            peak = std::max(peak, std::abs(y));
        }
    }
    // passagem em 20 kHz sobre DC de 0,5: o nivel sobrevive quase inteiro.
    EXPECT_GT(peak, 0.4f);

    // e "vira passagem" e' literal: os cinco coeficientes sao bit-identicos aos
    // do passa-baixa explicito em 20 kHz. outro corte passava no pico e ninguem via.
    const BiquadCoeffs explicitWire =
        makeBiquadCoeffs(FilterType::kLowPass, 20000.0f, 0.7071f, kSampleRate);
    EXPECT_FLOAT_EQ(coeffs.b0, explicitWire.b0);
    EXPECT_FLOAT_EQ(coeffs.b1, explicitWire.b1);
    EXPECT_FLOAT_EQ(coeffs.b2, explicitWire.b2);
    EXPECT_FLOAT_EQ(coeffs.a1, explicitWire.a1);
    EXPECT_FLOAT_EQ(coeffs.a2, explicitWire.a2);
}

TEST(Biquad, NaNCutoffRespectsNyquistAtLowSampleRates) {
    // o NaN cai em 20000 cru, e 20000 acima de Nyquist (sr = 32 kHz) dobrava a
    // frequencia em vez de passar: finito e errado, que a rede de nao-finito nao
    // pegava. com o min() contra Nyquist, 100 Hz passa e o coeff bate com 15680 Hz.
    constexpr double kLowSr = 32000.0;
    const BiquadCoeffs nanCutoff = makeBiquadCoeffs(
        FilterType::kLowPass, std::numeric_limits<float>::quiet_NaN(), 0.7071f, kLowSr);
    const BiquadCoeffs explicitCutoff =
        makeBiquadCoeffs(FilterType::kLowPass, 15680.0f, 0.7071f, kLowSr);
    EXPECT_FLOAT_EQ(nanCutoff.b0, explicitCutoff.b0);
    EXPECT_FLOAT_EQ(nanCutoff.b1, explicitCutoff.b1);
    EXPECT_FLOAT_EQ(nanCutoff.b2, explicitCutoff.b2);
    EXPECT_FLOAT_EQ(nanCutoff.a1, explicitCutoff.a1);
    EXPECT_FLOAT_EQ(nanCutoff.a2, explicitCutoff.a2);

    Biquad filter;
    float peak = 0.0f;
    for (int i = 0; i < 5000; ++i) {
        const float y = filter.process(
            std::sin(2.0f * static_cast<float>(M_PI) * 100.0f * static_cast<float>(i) /
                     32000.0f),
            nanCutoff);
        ASSERT_TRUE(std::isfinite(y));
        if (i >= 4000) {
            peak = std::max(peak, std::abs(y));
        }
    }
    EXPECT_NEAR(peak, 1.0, 0.05);
}

TEST(Biquad, InvalidTypeFallsBackToLowPass) {
    // indice corrompido de choice vira passa-baixa, nao erro nem silencio: tem de
    // soar. o saneador e' a mesma funcao que o motor usa na chave do cache.
    const auto bad = static_cast<FilterType>(99);
    EXPECT_EQ(sanitizeFilterType(bad), FilterType::kLowPass);
    EXPECT_EQ(sanitizeFilterType(FilterType::kNotch), FilterType::kNotch);

    BiquadCoeffs coeffs = makeBiquadCoeffs(bad, 1000.0f, 0.7071f, kSampleRate);
    Biquad filter;
    float peak = 0.0f;
    for (int i = 0; i < 5000; ++i) {
        const float y = filter.process(
            std::sin(2.0f * static_cast<float>(M_PI) * 100.0f * static_cast<float>(i) / 44100.0f),
            coeffs);
        if (i >= 4000) {
            peak = std::max(peak, std::abs(y));
        }
    }
    EXPECT_NEAR(peak, 1.0, 0.05);
}

TEST(Biquad, InvalidTypeIsLowPassNotWire) {
    // o sanitize garante o mapeamento, mas nao prova que o som e' de passa-baixa:
    // um fio tambem passa 100 Hz. a perna de 10 kHz distingue.
    BiquadCoeffs coeffs = makeBiquadCoeffs(static_cast<FilterType>(99), 1000.0f, 0.7071f,
                                           kSampleRate);
    Biquad filter;
    float peak = 0.0f;
    for (int i = 0; i < 5000; ++i) {
        const float y = filter.process(
            std::sin(2.0f * static_cast<float>(M_PI) * 10000.0f * static_cast<float>(i) /
                     44100.0f),
            coeffs);
        if (i >= 4000) {
            peak = std::max(peak, std::abs(y));
        }
    }
    EXPECT_LT(peak, 0.01f);
}

TEST(Biquad, NamesCoverAllTypes) {
    EXPECT_STREQ(filterName(FilterType::kLowPass), "Low-pass");
    EXPECT_STREQ(filterName(FilterType::kHighPass), "High-pass");
    EXPECT_STREQ(filterName(FilterType::kBandPass), "Band-pass");
    EXPECT_STREQ(filterName(FilterType::kNotch), "Notch");
}

namespace {

// renderiza 440 Hz com os dois filtros dados, gate aberto. motor fresco por chamada.
float renderFilteredPeak(FilterType f1type,
                         float f1cutoff,
                         FilterType f2type,
                         float f2cutoff,
                         float sourceAmplitude = 1.0f) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = sourceAmplitude * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f *
                                               static_cast<float>(i) / 44100.0f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 50.0f;
    params.position = 0.5f;
    params.f1type = f1type;
    params.f1cutoffHz = f1cutoff;
    params.f2type = f2type;
    params.f2cutoffHz = f2cutoff;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float peak = 0.0f;
    for (int block = 0; block < 200; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        for (const auto sample : left) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    return peak;
}

} // namespace

TEST(FilterChain, LowPassAt100HzKills440HzSine) {
    // referencia transparente (20 kHz) contra corte em 100 Hz sobre 440 Hz: mais
    // de 20 dB de diferenca. fonte em meia escala para o limiter ficar linear.
    const auto open = renderFilteredPeak(FilterType::kLowPass, 20000.0f, FilterType::kLowPass,
                                         20000.0f, 0.5f);
    ASSERT_GT(open, 0.01f) << "referencia muda, nada para comparar";

    const auto closed = renderFilteredPeak(FilterType::kLowPass, 100.0f, FilterType::kLowPass,
                                           20000.0f, 0.5f);
    EXPECT_LT(closed, open * 0.1f) << "aberto=" << open << " fechado=" << closed;
}

TEST(FilterChain, HighPassAt10kHzSilences440HzSine) {
    // teoria: (440/10000)^2 = 0,0019 (-54 dB). limiar 0,01 (-40 dB), 14 dB de
    // folga. de proposito nao 0,05: 1a ordem daria 0,044 e o ensaio separa 6 de 12.
    const auto open = renderFilteredPeak(FilterType::kLowPass, 20000.0f, FilterType::kLowPass,
                                         20000.0f, 0.5f);
    ASSERT_GT(open, 0.01f);

    const auto closed = renderFilteredPeak(FilterType::kHighPass, 10000.0f, FilterType::kLowPass,
                                           20000.0f, 0.5f);
    EXPECT_LT(closed, open * 0.01f) << "aberto=" << open << " fechado=" << closed;
}

TEST(FilterChain, SecondFilterInSeriesFilters) {
    // todos os outros ensaios de cadeia fecham o primeiro e abrem o segundo. aqui
    // e' o contrario: primeiro aberto, segundo fechado, e o silencio prova os dois.
    const auto open = renderFilteredPeak(FilterType::kLowPass, 20000.0f, FilterType::kLowPass,
                                         20000.0f, 0.5f);
    ASSERT_GT(open, 0.01f);

    const auto closed = renderFilteredPeak(FilterType::kLowPass, 20000.0f, FilterType::kHighPass,
                                           10000.0f, 0.5f);
    EXPECT_LT(closed, open * 0.01f) << "aberto=" << open << " fechado=" << closed;
}

TEST(FilterChain, ResonanceRisesWithQ) {
    // tom no corte: Q alto ressoa mais que Q baixo. fonte baixa para o pico nao
    // bater no limiter.
    // dois motores frescos, mesma fonte, so o Q difere: copiar nao da, a
    // telemetria tem atomics.
    GranularEngine low;
    low.prepare(kSampleRate, kBlock);
    low.setSounding(true);
    GranularEngine high;
    high.prepare(kSampleRate, kBlock);
    high.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f *
                                     static_cast<float>(i) / 44100.0f);
    }
    low.setSource(source.data(), source.size());
    high.setSource(source.data(), source.size());

    // params separados por motor: mutar o objeto partilhado entre dois processBlock
    // alternados funciona, mas esconde qual motor viu qual valor.
    GranularParams lowParams;
    lowParams.grainSizeMs = 10.0f;
    lowParams.densityGrainsPerSec = 50.0f;
    lowParams.position = 0.5f;
    lowParams.f1type = FilterType::kLowPass;
    lowParams.f1cutoffHz = 1000.0f;
    lowParams.f1q = 0.5f;
    GranularParams highParams = lowParams;
    highParams.f1q = 8.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float peakLowQ = 0.0f;
    float peakHighQ = 0.0f;
    for (int block = 0; block < 200; ++block) {
        low.processBlock(left.data(), right.data(), kBlock, lowParams);
        for (const auto sample : left) {
            peakLowQ = std::max(peakLowQ, std::abs(sample));
        }
        high.processBlock(left.data(), right.data(), kBlock, highParams);
        for (const auto sample : left) {
            peakHighQ = std::max(peakHighQ, std::abs(sample));
        }
    }
    ASSERT_GT(peakLowQ, 0.005f);
    EXPECT_GT(peakHighQ, peakLowQ * 1.5f)
        << "Q baixo=" << peakLowQ << " Q alto=" << peakHighQ;
}

TEST(FilterChain, BandPassLetsCenterThroughEngine) {
    // BP e Notch so existiam no unitario: um erro no switch ou no sanitize mapeava
    // tudo para passa-baixa e ninguem via. 1 kHz no centro tem de sobreviver.
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f *
                                     static_cast<float>(i) / 44100.0f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 50.0f;
    params.position = 0.5f;
    params.f1type = FilterType::kBandPass;
    params.f1cutoffHz = 1000.0f;
    params.f1q = 1.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float peak = 0.0f;
    for (int block = 0; block < 200; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        for (const auto sample : left) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    EXPECT_GT(peak, 0.01f) << "passa-faixa no centro calou o tom";
}

TEST(FilterChain, NotchRemovesCenterThroughEngine) {
    // o dual do anterior: notch em 1 kHz sobre tom de 1 kHz afunda em relacao a
    // referencia aberta. um Notch mapeado para LP passava em tudo.
    const auto open = renderFilteredPeak(FilterType::kLowPass, 20000.0f, FilterType::kLowPass,
                                         20000.0f, 0.5f);

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f *
                                     static_cast<float>(i) / 44100.0f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 50.0f;
    params.position = 0.5f;
    params.f1type = FilterType::kNotch;
    params.f1cutoffHz = 440.0f;
    params.f1q = 2.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float peak = 0.0f;
    for (int block = 0; block < 200; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        for (const auto sample : left) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    // rejeicao estreita sobre material de banda larga: afunda, mas nao zera,
    // porque o resto do espectro passa. a razao ancora que o filtro mexe.
    EXPECT_LT(peak, open) << "aberto=" << open << " rejeitado=" << peak;
}

TEST(FilterChain, CutoffChangeMidStreamApplies) {
    // o caminho sujo do cache: um cache travado ou surdo passava verde, porque os
    // outros ensaios usam params constantes. aqui o corte muda a meio e a segunda
    // metade tem de cair.
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * 440.0f *
                                     static_cast<float>(i) / 44100.0f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 50.0f;
    params.position = 0.5f;
    params.f1type = FilterType::kLowPass;
    params.f1cutoffHz = 20000.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float firstHalf = 0.0f;
    for (int block = 0; block < 100; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        for (const auto sample : left) {
            firstHalf = std::max(firstHalf, std::abs(sample));
        }
    }

    params.f1cutoffHz = 100.0f;
    float secondHalf = 0.0f;
    for (int block = 0; block < 100; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        // os 10 primeiros blocos sao transicao: trocar coeficiente com estado
        // carregado da solavanco. a partir do 10 so resta o regime.
        if (block < 10) {
            continue;
        }
        for (const auto sample : left) {
            secondHalf = std::max(secondHalf, std::abs(sample));
        }
    }

    ASSERT_GT(firstHalf, 0.01f);
    EXPECT_LT(secondHalf, firstHalf * 0.3f)
        << "primeira metade=" << firstHalf << " segunda=" << secondHalf;
}

TEST(FilterChain, ReusedVoiceStartsClean) {
    // sem o zero no startGrain o grao novo herdava a cauda congelada do slot: voz
    // inativa nao renderiza, o filtro nao decai, e com Q=12 a cauda entrava audivel.
    // comparar bursts nao serve (o grao de 40 ms dura ~10 tau) nem o pico no ataque
    // (0,106 contra 0,057, margem fina). o que distingue e' grao lendo silencio:
    // com zero, entrada zero e estado zero dao zero.
    // protocolo: fase densa, morte por setSource(nullptr, 0), 30 blocos de quiesce,
    // densidade de volta, mede os 8 primeiros blocos. 0,755 sem zero contra ~0 com
    // zero; o limiar de 0,05 tem margem dos dois lados.
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(16384);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f *
                                     static_cast<float>(i) / 44100.0f);
    }
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = 200.0f;
    params.position = 0.5f;
    params.f1type = FilterType::kLowPass;
    params.f1cutoffHz = 1000.0f;
    params.f1q = 12.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    for (int block = 0; block < 100; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
    }

    engine.setSource(nullptr, 0);
    engine.processBlock(left.data(), right.data(), kBlock, params);
    ASSERT_EQ(engine.lastActiveVoices(), 0) << "vozes ainda vivas depois do corte";

    const std::vector<float> silence(16384, 0.0f);
    engine.setSource(silence.data(), silence.size());

    // quiesce com densidade zero: DCB, gate e limiter assentam e os filtros
    // congelam (voz inativa nao renderiza). so as caudas ficam quentes.
    params.densityGrainsPerSec = 0.0f;
    for (int block = 0; block < 30; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
    }

    // densidade de volta: os primeiros graos nascem sobre as caudas congeladas, e
    // com zero a soma e' exatamente zero.
    params.densityGrainsPerSec = 200.0f;
    float peak = 0.0f;
    int maxActive = 0;
    for (int block = 0; block < 8; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        maxActive = std::max(maxActive, engine.lastActiveVoices());
        for (const auto sample : left) {
            peak = std::max(peak, std::abs(sample));
        }
    }
    // prova de que houve graos para herdar: sem renderizacao o silencio e' vazio
    // e o ensaio nao prova nada.
    EXPECT_GT(maxActive, 0) << "nenhum grao nasceu na fase de silencio";
    EXPECT_LT(peak, 0.05f) << "pico=" << peak << ": cauda fantasma tocando sozinha";
}

TEST(FilterChain, NaNCutoffAndQStayFinite) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);

    std::vector<float> source(4096, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    params.f1cutoffHz = std::numeric_limits<float>::quiet_NaN();
    params.f1q = std::numeric_limits<float>::quiet_NaN();
    params.f2cutoffHz = std::numeric_limits<float>::quiet_NaN();
    params.f2q = std::numeric_limits<float>::quiet_NaN();

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
