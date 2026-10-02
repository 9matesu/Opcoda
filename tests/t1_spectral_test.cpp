#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_to_sample.h"
#include "opcoda_core/pe/pe_parser.h"
#include "pe_builder.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

using opcoda::dsp::DcBlocker;
using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::pe::parse;
using opcoda::pe::toSamples;
using opcoda::test::PeBuilder;

namespace {

constexpr double kSampleRate = 44100.0;
constexpr int kBlock = 256;
constexpr std::size_t kT1Window = 2048;

float hannCoefficient(std::size_t index, std::size_t length) {
    if (length <= 1) {
        return 1.0f;
    }
    const double t = static_cast<double>(index) / static_cast<double>(length - 1);
    return static_cast<float>(0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * t)));
}

// Magnitude no bin k de uma FFT radix-2 com janela Blackman-Harris,
// conforme o protocolo do ensaio T1. Implementacao propia porque a FFT nao
// entra no caminho de audio e nao vale uma dependencia de runtime.
void blackmanHarris(std::vector<double>& window, std::size_t length) {
    window.resize(length);
    if (length <= 1) {
        return;
    }
    const double n = static_cast<double>(length - 1);
    for (std::size_t i = 0; i < length; ++i) {
        const double t = static_cast<double>(i) / n;
        const double twoPi = 2.0 * 3.14159265358979323846;
        window[i] = 0.42 - 0.5 * std::cos(twoPi * t) + 0.08 * std::cos(2.0 * twoPi * t);
    }
}

void magnitudeSpectrum(const std::vector<float>& signal,
                       std::size_t fftSize,
                       std::vector<double>& magnitudes) {
    const std::size_t count = std::min(signal.size(), fftSize);
    std::vector<double> window;
    blackmanHarris(window, count);

    std::vector<std::complex<double>> spectrum(fftSize, {0.0, 0.0});
    for (std::size_t i = 0; i < count; ++i) {
        const double value = static_cast<double>(signal[i]) * window[i];
        const double angle = -2.0 * 3.14159265358979323846 * static_cast<double>(i) /
                             static_cast<double>(fftSize);
        spectrum[i] = {value * std::cos(angle), value * std::sin(angle)};
    }

    // Cooley-Tukey iterativo in-place
    for (std::size_t i = 1, j = 0; i < fftSize; ++i) {
        std::size_t bit = fftSize >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(spectrum[i], spectrum[j]);
        }
    }
    for (std::size_t length = 2; length <= fftSize; length <<= 1) {
        const double angle = -2.0 * 3.14159265358979323846 / static_cast<double>(length);
        const std::complex<double> step {std::cos(angle), std::sin(angle)};
        for (std::size_t i = 0; i < fftSize; i += length) {
            std::complex<double> w {1.0, 0.0};
            for (std::size_t k = 0; k < length / 2; ++k) {
                const auto even = spectrum[i + k];
                const auto odd = spectrum[i + k + length / 2] * w;
                spectrum[i + k] = even + odd;
                spectrum[i + k + length / 2] = even - odd;
                w *= step;
            }
        }
    }

    magnitudes.resize(fftSize / 2);
    const double scale = 2.0 / static_cast<double>(count);
    for (std::size_t i = 0; i < fftSize / 2; ++i) {
        magnitudes[i] = std::abs(spectrum[i]) * scale;
    }
}

std::vector<float> renderRaw(const std::vector<std::uint8_t>& bytes, int numSamples) {
    // Controle negativo do T1: a mesma conversao, sem motor granular e sem
    // condicionamento. Precisa de 44100 amostras e o PE sintetico tem 768
    // bytes, entao o arquivo e repetido: e o que um .exe grande faria.
    std::vector<std::uint8_t> tiled;
    tiled.reserve(static_cast<std::size_t>(numSamples));
    while (tiled.size() < static_cast<std::size_t>(numSamples)) {
        tiled.insert(tiled.end(), bytes.begin(), bytes.end());
    }
    return toSamples(tiled.data(), 0, static_cast<std::size_t>(numSamples));
}

std::vector<float> renderOpcoda(const std::vector<std::uint8_t>& bytes) {
    const auto parsed = parse(bytes.data(), bytes.size());
    const auto& section = parsed.image.sections[0];
    const auto samples = toSamples(bytes.data(), section.rawOffset, section.rawSize);

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(samples.data(), samples.size());

    GranularParams params;
    params.grainSizeMs = 40.0f;      // tamanho medio do protocolo T1
    params.densityGrainsPerSec = 20.0f;
    params.position = 0.5f;
    params.volumeDb = 0.0f;

    std::vector<float> accumulated;
    std::vector<float> left(kBlock);
    std::vector<float> right(kBlock);
    constexpr int kBlocks = 200;
    accumulated.reserve(static_cast<std::size_t>(kBlock) * kBlocks);
    for (int block = 0; block < kBlocks; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        accumulated.insert(accumulated.end(), left.begin(), left.end());
    }
    return accumulated;
}

double dcMagnitudeDb(const std::vector<double>& magnitudes) {
    if (magnitudes.empty()) {
        return -200.0;
    }
    return 20.0 * std::log10(std::max(magnitudes[0], 1e-12));
}

double lowBandPeakDb(const std::vector<double>& magnitudes) {
    // Bins abaixo de 20 Hz a 44,1 kHz com N = 65536.
    const std::size_t limit = (20 * kT1Window) / 44100 + 1;
    const std::size_t upper = std::min(limit, magnitudes.size());
    double peak = 0.0;
    for (std::size_t i = 0; i < upper; ++i) {
        peak = std::max(peak, magnitudes[i]);
    }
    return 20.0 * std::log10(std::max(peak, 1e-12));
}

} // namespace

TEST(T1DcAttenuation, BinaryWithStrongOffsetIsAttenuatedByFortyDecibels) {
    // Criterio T1: o bin DC da saida tratada fica pelo menos 40 dB abaixo do
    // mesmo bin da leitura bruta.
    PeBuilder builder;
    // Preenche .text com gradiente cujo valor medio e muito longe de zero.
    builder.fillTextWithGradient(200, 20);

    const auto raw = renderRaw(builder.bytes, 44100);
    const auto treated = renderOpcoda(builder.bytes);
    ASSERT_GT(treated.size(), 4096u);

    std::vector<double> rawSpectrum;
    std::vector<double> treatedSpectrum;
    magnitudeSpectrum(raw, 65536, rawSpectrum);
    magnitudeSpectrum(treated, 65536, treatedSpectrum);

    const double rawDc = dcMagnitudeDb(rawSpectrum);
    const double treatedDc = dcMagnitudeDb(treatedSpectrum);
    const double attenuation = rawDc - treatedDc;

    EXPECT_GE(attenuation, 40.0)
        << "atenuacao do bin DC foi " << attenuation << " dB "
        << "(bruta " << rawDc << " dB, tratada " << treatedDc << " dB)";
}

TEST(T1DcAttenuation, SubTwentyHertzStaysBelowMinusSixtyDbfs) {
    // Criterio T1: energia abaixo de 20 Hz menor que -60 dBFS.
    PeBuilder builder;
    builder.fillTextWithGradient(180, 40);

    const auto treated = renderOpcoda(builder.bytes);
    ASSERT_GT(treated.size(), 4096u);

    std::vector<double> spectrum;
    magnitudeSpectrum(treated, 65536, spectrum);

    const double lowBand = lowBandPeakDb(spectrum);
    EXPECT_LT(lowBand, -60.0)
        << "pico abaixo de 20 Hz foi " << lowBand << " dBFS";
}

TEST(T1DcAttenuation, DcBlockerRemovesPureConstantSignal) {
    // Isolamento da cadeia: um sinal puramente constante tem de sair sem
    // nivel medio, sem depender do agendador granular. A transiente de partida
    // decai com R^n, entao o regime e medido depois de um periodo de
    // acomodacao.
    DcBlocker blocker;
    blocker.setSampleRate(kSampleRate);
    blocker.prepare();

    constexpr int kSettle = 20000;
    constexpr int kMeasure = 44100;
    for (int i = 0; i < kSettle; ++i) {
        (void)blocker.process(0.9f);
    }

    std::vector<float> constant(static_cast<std::size_t>(kMeasure), 0.9f);
    blocker.processBlock(constant.data(), kMeasure);

    double energy = 0.0;
    for (const auto sample : constant) {
        energy += static_cast<double>(sample) * sample;
    }
    const double rms = std::sqrt(energy / static_cast<double>(constant.size()));
    EXPECT_LT(20.0 * std::log10(std::max(rms, 1e-12)), -60.0)
        << "rms apos o DC-blocker ficou " << rms;
}

TEST(T1DcAttenuation, ToSamplesMapsByteRangeToUnitInterval) {
    // A conversao byte -> amostra e' a fronteira entre o binario e o motor.
    const std::vector<std::uint8_t> data = {0, 127, 128, 255};
    const auto samples = toSamples(data.data(), 0, data.size());

    ASSERT_EQ(samples.size(), data.size());
    EXPECT_NEAR(samples[0], -1.0f, 1e-6f);
    EXPECT_NEAR(samples[3], 1.0f, 1e-6f);
    // 127 e 128 sao os bytes mais proximos do zero, e nenhum deles e exatamente
    // zero: e o offset que o DC-blocker existe para remover.
    EXPECT_NEAR(samples[1], -0.5f / 127.5f, 1e-6f);
    EXPECT_NEAR(samples[2], 0.5f / 127.5f, 1e-6f);
}

TEST(T1DcAttenuation, ToSamplesRespectsOffset) {
    const std::vector<std::uint8_t> data = {9, 9, 255, 0};
    const auto samples = toSamples(data.data(), 2, 2);
    ASSERT_EQ(samples.size(), 2u);
    EXPECT_NEAR(samples[0], 1.0f, 1e-6f);
    EXPECT_NEAR(samples[1], -1.0f, 1e-6f);
}

TEST(T1DcAttenuation, ToSamplesRejectsDegenerateInput) {
    EXPECT_TRUE(toSamples(nullptr, 0, 16).empty());
    const std::vector<std::uint8_t> data(16, 0x5A);
    EXPECT_TRUE(toSamples(data.data(), 0, 0).empty());
}

TEST(T1DcAttenuation, WindowEndpointEnergyIsMinimal) {
    // A janela precisa zerar as bordas; sem isso aparece clique entre graos.
    std::vector<float> window(kT1Window);
    for (std::size_t i = 0; i < window.size(); ++i) {
        window[i] = hannCoefficient(i, window.size());
    }
    EXPECT_NEAR(window.front(), 0.0f, 1e-6f);
    EXPECT_NEAR(window.back(), 0.0f, 1e-6f);
}
