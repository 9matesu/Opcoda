#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"

#include <cmath>
#include <limits>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr double kSampleRate = 44100.0;

// Piso de audibilidade: o pico de uma senoide granulada audivel fica ordens
// acima disso, e um mix emudecido por regressao fica abaixo. Sem o piso, um
// pico de 5.6e-7 passava no EXPECT_GT e o ensaio media residuo de
// arredondamento em vez de graos.
constexpr float kAudibleFloor {0.01f};

// Renderiza `blocks` blocos de 256 amostras de uma senoide de 440 Hz com o
// gate aberto. O motor e' fresco por chamada: o estado interno (vozes,
// acumulador de disparo) nao pode vazar de um ensaio para o outro.
std::vector<float> renderSine(float grainLevel, int blocks = 200) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    engine.setSounding(true);

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
    params.grainLevel = grainLevel;

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    std::vector<float> out;
    for (int block = 0; block < blocks; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        out.insert(out.end(), left.begin(), left.end());
    }
    return out;
}

float peakOf(const std::vector<float>& buffer) {
    float peak = 0.0f;
    for (const auto sample : buffer) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

} // namespace

TEST(GrainLevel, ScalesOutput) {
    // Nivel 1.0 e' o comportamento antigo: o ganho do grao sai so do volume em
    // dB. O piso ancora que ha som audivel antes de medir proporcao: sem ele o
    // ensaio media residuo e passava em cima de um mix emudecido.
    const auto full = renderSine(1.0f);
    const auto fullPeak = peakOf(full);
    ASSERT_GT(fullPeak, kAudibleFloor) << "sem som audivel, nada para medir";

    // Metade do nivel, metade do pico. A tolerancia e' 0,05 porque o pre-limiter
    // fica em ~0,1-0,2 — uma ordem abaixo do teto de -1 dBFS — e portanto na
    // regiao linear: proporcao aqui e' quase identidade, nao aproximacao.
    const auto half = renderSine(0.5f);
    const auto halfPeak = peakOf(half);
    ASSERT_GT(halfPeak, kAudibleFloor);
    EXPECT_NEAR(halfPeak / fullPeak, 0.5, 0.05)
        << "pico cheio=" << fullPeak << " pico meio=" << halfPeak;
}

TEST(GrainLevel, ZeroIsSilent) {
    // Nivel zero zera o ganho de todo grao que nasce: o motor processa, o gate
    // abre, e nada sai. E' o que separa "nivel" de "mute do host" — o primeiro
    // e' por voz, o segundo e' global.
    const auto out = renderSine(0.0f);
    for (const auto sample : out) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(GrainLevel, AboveOneClampsToOne) {
    // Acima de 1.0 o nivel trunca: o processamento e' deterministico, entao os
    // buffers tem de ser bit-identicos, e nao so "parecidos".
    EXPECT_EQ(renderSine(1.5f), renderSine(1.0f));
}

TEST(GrainLevel, NegativeClampsToSilence) {
    // Nivel negativo nao e' fase invertida: trunca em zero, como o clamp diz.
    const auto out = renderSine(-1.0f);
    for (const auto sample : out) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(PitchRandom, ChangesVoices) {
    // O detune e' por grao e usa a sequencia do spray: com 6 semitons de faixa,
    // a leitura de cada grao cai noutro sitio da senoide e o bloco sai
    // diferente. Vetores deterministicos, entao basta um sample diferente — mas
    // o ensaio exige maioria diferente, porque um unico ULP passaria e nao
    // provaria detune nenhum.
    const auto render = [](float pitchRandom) {
        GranularEngine engine;
        engine.prepare(kSampleRate, 256);
        engine.setSounding(true);

        std::vector<float> source(4096);
        for (std::size_t i = 0; i < source.size(); ++i) {
            source[i] = std::sin(static_cast<float>(i) * 0.01f);
        }
        engine.setSource(source.data(), source.size());

        GranularParams params;
        params.grainSizeMs = 10.0f;
        params.densityGrainsPerSec = 200.0f;
        params.position = 0.5f;
        params.pitchRandomSemitones = pitchRandom;

        std::vector<float> left(256, 0.0f);
        std::vector<float> right(256, 0.0f);
        std::vector<float> out;
        for (int block = 0; block < 40; ++block) {
            engine.processBlock(left.data(), right.data(), 256, params);
            out.insert(out.end(), left.begin(), left.end());
        }
        return out;
    };

    const auto plain = render(0.0f);
    const auto detuned = render(6.0f);

    std::size_t differing = 0;
    ASSERT_EQ(plain.size(), detuned.size());
    for (std::size_t i = 0; i < plain.size(); ++i) {
        if (plain[i] != detuned[i]) {
            ++differing;
        }
    }
    // Seis semitons movem quase todo sample: a maioria tem de diferir, e nao
    // um ULP perdido. O limiar e' folgado de proposito — o que ele barra e'
    // detune de mentira, nao detune pequeno.
    EXPECT_GT(differing, plain.size() / 2) << "diferiram " << differing << " de "
                                           << plain.size();

    for (const auto sample : plain) {
        ASSERT_TRUE(std::isfinite(sample));
    }
    for (const auto sample : detuned) {
        ASSERT_TRUE(std::isfinite(sample));
    }

    // E com o mesmo valor o resultado repete: o detune nao pode custar o
    // determinismo que o T1 e o T2 assumem.
    EXPECT_EQ(detuned, render(6.0f));
}

TEST(PitchRandom, AboveTwelveClampsToTwelve) {
    // O clamp e' deterministico: 24 semitons dao exatamente os mesmos buffers
    // que 12, e nao so "parecidos".
    GranularEngine first;
    first.prepare(kSampleRate, 256);
    first.setSounding(true);

    GranularEngine second;
    second.prepare(kSampleRate, 256);
    second.setSounding(true);

    std::vector<float> source(4096);
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i] = std::sin(static_cast<float>(i) * 0.01f);
    }
    first.setSource(source.data(), source.size());
    second.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    params.position = 0.5f;

    std::vector<float> wide;
    std::vector<float> clamped;
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    for (int block = 0; block < 40; ++block) {
        params.pitchRandomSemitones = 24.0f;
        first.processBlock(left.data(), right.data(), 256, params);
        wide.insert(wide.end(), left.begin(), left.end());

        params.pitchRandomSemitones = 12.0f;
        second.processBlock(left.data(), right.data(), 256, params);
        clamped.insert(clamped.end(), left.begin(), left.end());
    }
    EXPECT_EQ(wide, clamped);
}

namespace {

// Renderiza com um so parametro NaN por vez, nos dois canais. Isolar importa:
// com os dois juntos, uma falha nao diz qual saneador regrediu.
void renderNaNParam(bool nanLevel, bool nanDetune) {
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    engine.setSounding(true);

    std::vector<float> source(4096, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    if (nanLevel) {
        params.grainLevel = std::numeric_limits<float>::quiet_NaN();
    }
    if (nanDetune) {
        params.pitchRandomSemitones = std::numeric_limits<float>::quiet_NaN();
    }

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    for (int block = 0; block < 40; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        // Os dois canais acumulam `value * grain.gain` com estados
        // independentes de DC-blocker, gate e limiter: checar so o esquerdo
        // deixava o direito envenenar em silencio numa refatoracao futura.
        for (const auto sample : left) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no esquerdo, bloco " << block;
        }
        for (const auto sample : right) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no direito, bloco " << block;
        }
    }
}

} // namespace

TEST(GrainParams, NaNLevelDoesNotPoisonOutput) {
    // Nivel NaN vira silencio do grao, e nao volume cheio: um ganho NaN
    // envenenaria a mistura e sairia como NaN no barramento.
    renderNaNParam(true, false);
}

TEST(GrainParams, NaNDetuneDoesNotPoisonOutput) {
    // Detune NaN vira zero: um readStep NaN envenenava a posicao e o cast para
    // size_t na leitura era indefinido.
    renderNaNParam(false, true);
}

TEST(GrainParams, NaNPitchDoesNotPoisonOutput) {
    // O pitch vinha sem guarda: pow(2, NaN) = NaN, e o clamp — que nao trata
    // NaN — devolvia NaN adiante. Zero e' fail-open: mantem a afinacao antiga.
    GranularEngine engine;
    engine.prepare(kSampleRate, 256);
    engine.setSounding(true);

    std::vector<float> source(4096, 0.5f);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.grainSizeMs = 10.0f;
    params.densityGrainsPerSec = 200.0f;
    params.pitchSemitones = std::numeric_limits<float>::quiet_NaN();

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    for (int block = 0; block < 40; ++block) {
        engine.processBlock(left.data(), right.data(), 256, params);
        for (const auto sample : left) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no bloco " << block;
        }
        for (const auto sample : right) {
            ASSERT_TRUE(std::isfinite(sample)) << "NaN no bloco " << block;
        }
    }
}
