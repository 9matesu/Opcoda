#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"

#include <algorithm>
#include <cmath>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

// fonte quente: dois tons, pico perto da amplitude pedida.
std::vector<float> hotSource(float amplitude) {
    std::vector<float> samples(24000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = amplitude *
                     (0.9f * std::sin(static_cast<float>(i) * 0.05f) +
                      0.2f * std::sin(static_cast<float>(i) * 0.31f));
    }
    return samples;
}

GranularParams baseParams() {
    GranularParams params;
    params.grainSizeMs = 20.0f;
    params.densityGrainsPerSec = 40.0f;
    params.position = 0.5f;
    params.pan = 0.0f;
    return params;
}

float renderPeak(float driveDb, float amplitude = 0.3f, float density = 40.0f) {
    const auto material = hotSource(amplitude);
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(material.data(), material.size());
    engine.setSounding(true);

    auto params = baseParams();
    params.f1DriveDb = driveDb;
    params.densityGrainsPerSec = density;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float peak = 0.0f;
    for (int block = 0; block < 48; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        // so' os ultimos blocos: o tecto do clip desce de 1,0 ate' ao alvo dentro
        // do primeiro bloco e o gate tem 5 ms de ataque. medir a subida apanhava
        // um pico anterior ao patamar (0,52 a 24 dB com a nuvem densa).
        if (block < 32) {
            continue;
        }
        for (int i = 0; i < kBlock; ++i) {
            peak = std::max(peak, std::abs(left[static_cast<std::size_t>(i)]));
        }
    }
    return peak;
}

// razao pico/rms do ultimo trecho: uma sinusóide tem ~1,4; um sinal achatado
// no tecto aproxima-se de 1,0. e' a diferenca entre clip e ganho.
float renderCrest(float driveDb, float amplitude = 1.5f) {
    const auto material = hotSource(amplitude);
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(material.data(), material.size());
    engine.setSounding(true);

    auto params = baseParams();
    params.f1DriveDb = driveDb;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    // a janela tem de apanhar o trecho com sinal: o arranque sem nota decai, e
    // medir o rabo devolve silencio com a mesma crista nos dois casos.
    std::vector<float> collected;
    for (int block = 0; block < 20; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        if (block >= 2) {
            collected.insert(collected.end(), left.begin(), left.end());
        }
    }

    double sum = 0.0;
    float peak = 0.0f;
    for (const auto value : collected) {
        sum += static_cast<double>(value) * static_cast<double>(value);
        peak = std::max(peak, std::abs(value));
    }
    const auto rms = std::sqrt(sum / static_cast<double>(collected.size()));
    return rms > 0.0 ? static_cast<float>(static_cast<double>(peak) / rms) : 0.0f;
}

} // namespace

// o tanh por grao satura e limita em 1,0; quem desce o topo do curso e' o clip
// do barramento. com o teto a morder, o pico deixa de depender do nivel de
// entrada — e' isso que distingue clip de ganho.
TEST(Drive, CeilingBindsAtTwentyFourDb) {
    // 1,0 - 0,60 * (24 - 6) / 18 = 0,40.
    EXPECT_NEAR(renderPeak(24.0f, 0.3f), 0.40f, 0.01f);
    EXPECT_NEAR(renderPeak(24.0f, 1.5f), 0.40f, 0.01f);
}

TEST(Drive, CeilingStaysOpenBelowSixDb) {
    // ate' 6 dB o teto e' 1,0: o clip nao encosta e o pico passa do valor que
    // ele teria a 24 dB. a nuvem densa garante que a soma chega perto do teto,
    // senao o ensaio mediria so' o ganho de cada grao.
    EXPECT_GT(renderPeak(6.0f, 1.5f, 200.0f), 0.5f);
}

TEST(Drive, CeilingInterpolatesWithTheKnob) {
    // 1,0 - 0,60 * (12 - 6) / 18 = 0,80: o teto e' uma rampa, nao um degrau.
    EXPECT_NEAR(renderPeak(12.0f, 1.5f, 200.0f), 0.80f, 0.02f);
}

TEST(Drive, CeilingClipsInsteadOfScaling) {
    // um ganho constante daria a mesma forma so' mais baixa: o crest factor
    // ficaria o do material (~3,2). o clip achata o topo e o crest cai.
    const auto drivenCrest = renderCrest(24.0f, 1.5f);
    // o crest de silencio e' zero (o divisor e' zero em renderCrest), e zero
    // passaria no limite superior; o piso impede um caminho mudo de aprovar.
    ASSERT_GT(drivenCrest, 0.5f) << "saida muda a 24 dB: o ensaio mediria silencio";
    EXPECT_LT(drivenCrest, 1.8f);
    // o material limpo, medido da mesma forma, tem crista bem acima disso.
    EXPECT_GT(renderCrest(0.0f, 1.5f), 2.5f);
}

namespace {

// 1 kHz a 48 kHz.
std::vector<float> toneSource() {
    std::vector<float> samples(48000);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.5f * std::sin(static_cast<float>(i) * 6.2831853f / 48.0f);
    }
    return samples;
}

int crossingsAtPitch(float semitones, float density = 80.0f) {
    const auto material = toneSource();
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(material.data(), material.size());
    engine.setSounding(true);

    auto params = baseParams();
    // densidade acima de 1/duracao: os graos sobrepoem-se e o ultimo trecho nao
    // cai numa falha de agendamento, que mediria zero cruzamentos.
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = density;
    params.pitchSemitones = semitones;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<float> collected;
    for (int block = 0; block < 32; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        collected.insert(collected.end(), left.begin(), left.end());
    }

    const auto window = std::min<std::size_t>(4096, collected.size());
    int count = 0;
    for (std::size_t i = collected.size() - window + 1; i < collected.size(); ++i) {
        const auto previous = collected[i - 1];
        const auto current = collected[i];
        // cruzamentos crus, sem limiar de amplitude. o limiar antigo de 0,01
        // descartava as duas amostras em volta de cada zero quando o tom era
        // fraco (918 Hz com passo de 0,015 por amostra) e um tom bom contava
        // zero. silencio e' zero exato, entao o numero cru ja' distingue som
        // de nada.
        if ((previous < 0.0f) != (current < 0.0f)) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST(Pitch, PlusFortyEightIsFourOctaves) {
    const auto plain = crossingsAtPitch(0.0f);
    const auto high = crossingsAtPitch(48.0f);

    ASSERT_GT(plain, 8) << "ha' de haver tom para medir";
    ASSERT_GT(high, 8) << "a oitava alta ficou muda";
    // 2^4 = 16x numa oitava, nao 2x. a razao e' medida, e nao so' um piso: um
    // clamp em 36 st (8x) ficaria no limiar e passaria por contagem inteira.
    const auto ratio = static_cast<double>(high) / static_cast<double>(plain);
    EXPECT_NEAR(ratio, 16.0, 2.0) << "razao medida " << ratio;
}

TEST(Pitch, SilenceHasNoCrossings) {
    // controlo negativo do contador cru: com densidade zero a saida e' zero
    // exato, e zero nao muda de sinal. sem este caso, um contador que somasse
    // ruido de fundo passaria no ensaio das quatro oitavas.
    EXPECT_EQ(crossingsAtPitch(0.0f, 0.0f), 0);
}
