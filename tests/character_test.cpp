#include <gtest/gtest.h>

#include "opcoda_core/dsp/curve.h"
#include "opcoda_core/dsp/granular_engine.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using opcoda::dsp::curveShape;
using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;

std::vector<float> sineSource(float toneHz, float amplitude, float seconds) {
    const auto count = static_cast<std::size_t>(seconds * static_cast<float>(kSampleRate));
    std::vector<float> samples(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double phase = 2.0 * std::numbers::pi * static_cast<double>(toneHz) *
                             static_cast<double>(i) / kSampleRate;
        samples[i] = amplitude * static_cast<float>(std::sin(phase));
    }
    return samples;
}

// grao longo, densidade baixa: o filtro ve' um tom quase continuo e o ensaio
// mede a resposta, nao o agendamento.
GranularParams toneParams(float cutoffHz) {
    GranularParams params;
    // 100 ms e' um grao de cada vez nesta densidade: o ensaio mede o tanh do
    // filtro, e nao a soma de janelas sobrepostas. o valor fica abaixo do maximo
    // de proposito, para a medicao nao mudar se o knob do grao crescer outra vez.
    params.grainSizeMs = 100.0f;
    // densidade alta: 8 janelas por segundo deixam o tom quase sempre presente, e
    // a media de 20 blocos apanha poucas pausas entre graos.
    params.densityGrainsPerSec = 8.0f;
    params.position = 0.5f;
    params.spray = 0.0f;
    params.volumeDb = 0.0f;
    params.attackSeconds = 0.001f;
    params.decaySeconds = 0.001f;
    params.sustainLevel = 1.0f;
    params.releaseSeconds = 0.1f;
    params.f1cutoffHz = cutoffHz;
    params.f2cutoffHz = 20000.0f;
    return params;
}

struct Rendered {
    std::vector<float> samples;
};

Rendered render(GranularParams params, const std::vector<float>& source, int blocks, int skipBlocks) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(source.data(), source.size());
    engine.setSounding(true);

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    Rendered out;
    for (int block = 0; block < blocks; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        if (block >= skipBlocks) {
            out.samples.insert(out.samples.end(), left.begin(), left.end());
        }
    }
    return out;
}

float rms(const std::vector<float>& samples) {
    double sum = 0.0;
    for (const float value : samples) {
        sum += static_cast<double>(value) * static_cast<double>(value);
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(samples.size())));
}

float peak(const std::vector<float>& samples) {
    float worst = 0.0f;
    for (const float value : samples) {
        worst = std::max(worst, std::abs(value));
    }
    return worst;
}

// magnitude de uma componente por correlacao directa. nao ha FFT no nucleo e o
// ensaio so precisa de dois bins.
double magnitudeAt(const std::vector<float>& samples, double frequency) {
    double real = 0.0;
    double imaginary = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * frequency *
                             static_cast<double>(i) / kSampleRate;
        real += static_cast<double>(samples[i]) * std::cos(phase);
        imaginary += static_cast<double>(samples[i]) * std::sin(phase);
    }
    return std::sqrt(real * real + imaginary * imaginary) /
           static_cast<double>(samples.size());
}

} // namespace

TEST(Filter, KeytrackMovesCutoffOneOctavePerOctave) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    // com material: sem fonte o processBlock sai antes do calculo dos
    // coeficientes, e o corte efectivo nunca seria escrito.
    const auto source = sineSource(220.0f, 0.1f, 0.1f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.f1cutoffHz = 1000.0f;
    params.f1Keytrack = 1.0f;
    params.noteNumber = 60;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    engine.processBlock(left.data(), right.data(), kBlock, params);
    EXPECT_NEAR(engine.f1CutoffHz(), 1000.0f, 1.0f);

    params.noteNumber = 72;
    engine.processBlock(left.data(), right.data(), kBlock, params);
    EXPECT_NEAR(engine.f1CutoffHz(), 2000.0f, 2.0f);

    // meia profundidade sobe meia oitava por oitava: Do5 a 0,5 da' 1414 Hz.
    params.noteNumber = 72;
    params.f1Keytrack = 0.5f;
    engine.processBlock(left.data(), right.data(), kBlock, params);
    EXPECT_NEAR(engine.f1CutoffHz(), 1414.2f, 3.0f);
}

TEST(Filter, SlopeTwentyFourAttenuatesTwiceInDbPerOctave) {
    const auto source = sineSource(4000.0f, 0.5f, 1.0f);
    auto twelve = toneParams(1000.0f);
    twelve.f1Slope = 0;
    auto twentyFour = twelve;
    twentyFour.f1Slope = 1;

    const auto renderedTwelve = render(twelve, source, 90, 50);
    const auto renderedTwentyFour = render(twentyFour, source, 90, 50);
    const float rmsTwelve = rms(renderedTwelve.samples);
    const float rmsTwentyFour = rms(renderedTwentyFour.samples);

    ASSERT_GT(rmsTwelve, 1.0e-6f);
    // duas oitavas acima do corte: 12 dB/oitava da' -24 dB e 24 dB/oitava da'
    // -48 dB, um quarto em amplitude. a folga e' para o Q e a soma de graos, nao
    // para metade da atenuacao.
    EXPECT_LT(rmsTwentyFour, rmsTwelve * 0.15f);
}

TEST(Filter, DriveAddsHarmonicsAndStopsAtTheCeiling) {
    const auto source = sineSource(220.0f, 0.5f, 1.0f);
    auto clean = toneParams(20000.0f);
    clean.f1DriveDb = 0.0f;
    auto driven = clean;
    driven.f1DriveDb = 24.0f;

    const auto cleanOut = render(clean, source, 90, 70);
    const auto drivenOut = render(driven, source, 90, 70);

    // a 24 dB o teto do clip e' 0,40: o pico para *nele*, e o drive nao deixa o
    // material passar do limite (o limiter fica depois, como rede). o EXPECT_NEAR
    // tambem apanha o silencio, que passaria num EXPECT_LE.
    EXPECT_NEAR(peak(drivenOut.samples), 0.40f, 0.02f);

    const double cleanFundamental = magnitudeAt(cleanOut.samples, 220.0);
    const double cleanThird = magnitudeAt(cleanOut.samples, 660.0);
    const double drivenFundamental = magnitudeAt(drivenOut.samples, 220.0);
    const double drivenThird = magnitudeAt(drivenOut.samples, 660.0);

    ASSERT_GT(cleanFundamental, 1.0e-6);
    ASSERT_GT(drivenFundamental, 1.0e-6);
    // a terceira harmonica existe no limpo (o filtro nao e' ideal) e cresce muito
    // com o drive: e' a definicao mensuravel de "ganhou harmonicas".
    EXPECT_GT(drivenThird / drivenFundamental, 5.0 * (cleanThird / cleanFundamental));
}

TEST(Envelope, CurveMinusOneIsExponential) {
    // 0,25^(1/8) = 0,8409 e 0,75^(1/8) = 0,9647: sobe depressa e aproxima
    // devagar. contra 0,25 da rampa nao e' variacao de alguns por cento.
    EXPECT_NEAR(curveShape(0.25f, -1.0f), 0.8408964f, 1.0e-5f);
    EXPECT_NEAR(curveShape(0.75f, -1.0f), 0.9646786f, 1.0e-5f);
    EXPECT_GT(curveShape(0.25f, -1.0f), 0.25f + 0.1f);
}

TEST(Envelope, CurvePlusOneIsLogarithmic) {
    // 0,25^8 = 1,5e-5 e 0,75^8 = 0,1001: fica baixo e fecha depressa.
    EXPECT_NEAR(curveShape(0.25f, 1.0f), 1.52588e-5f, 1.0e-7f);
    EXPECT_NEAR(curveShape(0.75f, 1.0f), 0.10011292f, 1.0e-5f);
    EXPECT_LT(curveShape(0.25f, 1.0f), 0.1f * 0.25f);
}

TEST(Envelope, CurvesReachTheEngine) {
    // a curva nao pode viver so na funcao pura: com ataque de 10 ms e um bloco de
    // 48 amostras a 48 kHz, tres blocos sao 30 % do estagio.
    const auto levelAfterThreeBlocks = [](float curve) {
        GranularEngine engine;
        engine.prepare(kSampleRate, 48);
        engine.setSounding(true);

        GranularParams params;
        params.attackSeconds = 0.01f;
        params.attackCurve = curve;

        std::vector<float> left(48, 0.0f);
        std::vector<float> right(48, 0.0f);
        for (int block = 0; block < 3; ++block) {
            engine.processBlock(left.data(), right.data(), 48, params);
        }
        return engine.gateLevel();
    };

    const float linear = levelAfterThreeBlocks(0.0f);
    const float exponential = levelAfterThreeBlocks(-1.0f);
    const float logarithmic = levelAfterThreeBlocks(1.0f);

    EXPECT_NEAR(linear, 0.3f, 0.02f);
    EXPECT_GT(exponential, 0.8f);
    EXPECT_LT(logarithmic, 0.01f);
}

TEST(GranularEngine, GrainAtTheEndOfTheSourceStillSounds) {
    // O canto direito do mapa de entropia levava POSITION a 1,0, e o grao morria
    // na primeira amostra: o instrumento emudecia e parecia avariado. O inicio
    // recua pelo comprimento da janela, e o ensaio fixa isso. O piso e' a ordem
    // de grandeza do material, nao um valor que o silencio tambem satisfaz.
    const auto source = sineSource(440.0f, 0.5f, 2.0f);

    auto params = toneParams(20000.0f);
    params.position = 1.0f;
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = 40.0f;

    const auto rendered = render(params, source, 24, 8);
    ASSERT_FALSE(rendered.samples.empty());

    const auto level = rms(rendered.samples);
    EXPECT_GT(level, 0.02f) << "rms " << level;

    for (const float value : rendered.samples) {
        EXPECT_TRUE(std::isfinite(value));
        EXPECT_LE(std::abs(value), 1.0f);
    }
}

TEST(GranularEngine, GrainAtTheEndKeepsTheLevelOfTheMiddle) {
    // Nao basta sair do silencio: o recuo nao pode ser um corte de nivel. O fim
    // do material toca com o mesmo nivel do meio, nao apenas com metade.
    const auto source = sineSource(440.0f, 0.5f, 2.0f);

    auto params = toneParams(20000.0f);
    params.grainSizeMs = 60.0f;
    params.densityGrainsPerSec = 40.0f;

    params.position = 0.5f;
    const auto middle = rms(render(params, source, 24, 8).samples);

    params.position = 1.0f;
    const auto end = rms(render(params, source, 24, 8).samples);

    ASSERT_GT(middle, 0.02f);
    EXPECT_NEAR(end, middle, middle * 0.1f) << "middle " << middle << " end " << end;
}

TEST(GranularEngine, PositionStillChoosesTheMaterial) {
    // O recuo do inicio podia ser um clamp que colava tudo na posicao 0: a saida
    // ficava audivel e o knob morto. Material com dois tons, um em cada metade,
    // mostra que a posicao ainda escolhe o que se le.
    auto source = sineSource(220.0f, 0.5f, 1.0f);
    const auto high = sineSource(880.0f, 0.5f, 1.0f);
    source.insert(source.end(), high.begin(), high.end());

    auto params = toneParams(20000.0f);
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = 40.0f;

    params.position = 0.2f;
    const auto low = render(params, source, 24, 8).samples;
    params.position = 0.8f;
    const auto upper = render(params, source, 24, 8).samples;

    const auto lowAtLow = magnitudeAt(low, 220.0);
    const auto lowAtHigh = magnitudeAt(low, 880.0);
    const auto highAtLow = magnitudeAt(upper, 220.0);
    const auto highAtHigh = magnitudeAt(upper, 880.0);

    EXPECT_GT(lowAtLow, lowAtHigh * 3.0);
    EXPECT_GT(highAtHigh, highAtLow * 3.0);
}

TEST(GranularEngine, GrainLongerThanTheSourceStillSoundsAndMoves) {
    // Regiao curta com pitch alto: o grao pediria mais material do que existe.
    // Sem o tecto da metade do material, o unico inicio valido era zero e o knob
    // de posicao e o spray ficavam mortos.
    const auto source = sineSource(333.0f, 0.5f, 0.5f);

    auto params = toneParams(20000.0f);
    params.grainSizeMs = 100.0f;
    params.pitchSemitones = 48.0f;
    params.densityGrainsPerSec = 40.0f;

    params.position = 0.0f;
    const auto atStart = render(params, source, 24, 8).samples;
    params.position = 1.0f;
    const auto atEnd = render(params, source, 24, 8).samples;

    const auto startLevel = rms(atStart);
    const auto endLevel = rms(atEnd);
    EXPECT_GT(startLevel, 0.01f) << "start " << startLevel;
    EXPECT_GT(endLevel, 0.01f) << "end " << endLevel;

    // e a posicao ainda muda o material lido, em vez de tudo nascer no mesmo byte.
    double difference = 0.0;
    const auto count = std::min(atStart.size(), atEnd.size());
    for (std::size_t i = 0; i < count; ++i) {
        difference += std::abs(atStart[i] - atEnd[i]);
    }
    difference /= static_cast<double>(count);
    EXPECT_GT(difference, 0.01) << "difference " << difference;
}

TEST(GranularEngine, NanSprayDoesNotPoisonTheReadIndex) {
    // spray NaN atravessava o clamp da posicao (std::clamp nao sanea NaN) e ia
    // parar a um static_cast para indice de leitura, que e' indefinido. o
    // paramatro tem de ser saneado antes de tocar no buffer.
    const auto source = sineSource(440.0f, 0.5f, 1.0f);

    auto params = toneParams(20000.0f);
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = 40.0f;
    params.spray = std::nanf("");

    const auto rendered = render(params, source, 16, 4);
    ASSERT_FALSE(rendered.samples.empty());
    // o spray invalido e' lido como zero: o instrumento toca, em vez de morrer
    // num indice indefinido.
    EXPECT_GT(rms(rendered.samples), 0.02f);
    for (const float value : rendered.samples) {
        EXPECT_TRUE(std::isfinite(value));
    }
}
