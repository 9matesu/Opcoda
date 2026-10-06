// Fecha a S1: o caminho completo, do binario em disco ao audio renderizado.
//
// Nao usa JUCE. A cadeia do Opcoda e' testada direto: parse -> toSamples ->
// motor granular -> DC-blocker -> limiter. O que o teste prova e que um .exe
// real vira som, nao que o plugin empacota isso corretamente.
#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_to_sample.h"
#include "opcoda_core/pe/pe_parser.h"
#include "opcoda_core/rt/transport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::pe::parseFile;
using opcoda::pe::toSamples;
using opcoda::rt::Transport;

namespace {

constexpr double kSampleRate = 44100.0;

std::vector<std::uint8_t> readFile(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const auto size = static_cast<std::size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(size);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    return bytes;
}

std::vector<float> render(const std::vector<std::uint8_t>& bytes, int blocks = 200) {
    constexpr int kBlock = 256;

    const auto parsed = opcoda::pe::parse(bytes.data(), bytes.size());
    if (!parsed.ok()) {
        return {};
    }
    const auto& section = parsed.image.sections[0];
    const auto samples = toSamples(bytes.data(), section.rawOffset, section.rawSize);

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSounding(true);
    engine.setSource(samples.data(), samples.size());

    GranularParams params;
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = 40.0f;
    params.position = 0.5f;
    params.volumeDb = 0.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(kBlock) * static_cast<std::size_t>(blocks));

    for (int i = 0; i < blocks; ++i) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
        out.insert(out.end(), left.begin(), left.end());
    }
    return out;
}

struct AudioStats {
    float peak {0.0f};
    double mean {0.0};
    bool allFinite {true};
};

AudioStats stats(const std::vector<float>& signal) {
    AudioStats result;
    double sum = 0.0;
    for (const auto sample : signal) {
        if (!std::isfinite(sample)) {
            result.allFinite = false;
        }
        result.peak = std::max(result.peak, std::abs(sample));
        sum += sample;
    }
    if (!signal.empty()) {
        result.mean = sum / static_cast<double>(signal.size());
    }
    return result;
}

// O caminho do transporte: o binario entra, a regiao e' percorrida sem uma unica
// nota MIDI, e sai audio.
//
// Reproduz o que PluginProcessor::processBlock faz, e nao o plugin: este
// executavel nao linka JUCE, e a cadeia do nucleo e' a mesma. O que o teste prova
// e que o transporte comanda o motor, e nao que o botao esta ligado.
std::vector<float> renderThroughTransport(const std::vector<std::uint8_t>& bytes,
                                          int blocks = 200,
                                          std::vector<float>* headPositions = nullptr) {
    constexpr int kBlock = 256;

    const auto parsed = opcoda::pe::parse(bytes.data(), bytes.size());
    if (!parsed.ok()) {
        return {};
    }
    const auto& section = parsed.image.sections[0];
    const auto samples = toSamples(bytes.data(), section.rawOffset, section.rawSize);

    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(samples.size());
    transport.play();

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    // A engine NUNCA recebe setSounding(true) nesta cadeia. O gate e' aberto
    // exclusivamente pelo transporte, e e' isso que o teste esta a provar.
    engine.setSource(samples.data(), samples.size());

    GranularParams params;
    params.grainSizeMs = 40.0f;
    params.densityGrainsPerSec = 40.0f;
    params.volumeDb = 0.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(kBlock) * static_cast<std::size_t>(blocks));

    for (int i = 0; i < blocks; ++i) {
        const auto playing = transport.isPlaying();
        if (playing) {
            params.position = transport.advance(kBlock);
            if (headPositions != nullptr) {
                headPositions->push_back(transport.positionFraction());
            }
        }
        engine.setSounding(playing);
        engine.processBlock(left.data(), right.data(), kBlock, params);
        out.insert(out.end(), left.begin(), left.end());
    }
    return out;
}

} // namespace

TEST(EndToEnd, RealExecutableBecomesAudio) {
    // notepad.exe e' o binario de referencia do ensaio T1.
    const auto bytes = readFile("C:\\Windows\\System32\\notepad.exe");
    ASSERT_FALSE(bytes.empty()) << "notepad.exe nao encontrado";

    const auto parsed = opcoda::pe::parse(bytes.data(), bytes.size());
    ASSERT_TRUE(parsed.ok()) << opcoda::pe::toString(parsed.error);
    EXPECT_GT(parsed.image.sampleCount, 100000u)
        << "um .exe real deve ter centenas de milhares de bytes de amostra";

    const auto audio = render(bytes);
    ASSERT_FALSE(audio.empty());

    const auto measured = stats(audio);
    EXPECT_TRUE(measured.allFinite) << "saida contem NaN ou infinito";
    EXPECT_GT(measured.peak, 0.01f) << "saida praticamente muda";
    EXPECT_LE(measured.peak, 1.0f) << "limitador deixou passar acima do teto";
}

TEST(EndToEnd, OutputHasNoDcOffset) {
    // A media da saida e' o que o DC-blocker existe para zerar. Um .exe real
    // tem byte medio muito longe de 127.5, entao sem o filtro a media ficaria
    // visivel.
    const auto bytes = readFile("C:\\Windows\\System32\\notepad.exe");
    ASSERT_FALSE(bytes.empty());

    const auto parsed = opcoda::pe::parse(bytes.data(), bytes.size());
    ASSERT_TRUE(parsed.ok());
    const auto& section = parsed.image.sections[0];

    double rawMean = 0.0;
    for (std::size_t i = 0; i < section.rawSize; ++i) {
        rawMean += static_cast<double>(bytes[section.rawOffset + i]) - 127.5;
    }
    rawMean /= static_cast<double>(section.rawSize);

    const auto measured = stats(render(bytes));
    EXPECT_GT(std::abs(rawMean), 0.1)
        << "o fixture nao tem offset forte, o teste nao prova nada";
    EXPECT_LT(std::abs(measured.mean), std::abs(rawMean) / 10.0)
        << "media da saida " << measured.mean << " contra bruta " << rawMean;
}

TEST(EndToEnd, CorruptedBinaryIsRejectedWithTypedError) {
    // Um .exe real corrompido precisa ser recusado com codigo, e nao lido
    // fora dos limites: e o ramo de erro do BPMN.
    auto bytes = readFile("C:\\Windows\\System32\\notepad.exe");
    ASSERT_FALSE(bytes.empty());

    bytes[0] = 'X';
    const auto bad = opcoda::pe::parse(bytes.data(), bytes.size());
    EXPECT_EQ(bad.error, opcoda::pe::PeError::kBadMz);
    EXPECT_STREQ(opcoda::pe::toString(bad.error), "E_BAD_MZ");
    EXPECT_TRUE(render(bytes).empty());
}

TEST(EndToEnd, TruncatedRealBinaryIsRejected) {
    auto bytes = readFile("C:\\Windows\\System32\\notepad.exe");
    ASSERT_FALSE(bytes.empty());

    bytes.resize(bytes.size() / 2);
    const auto parsed = opcoda::pe::parse(bytes.data(), bytes.size());

    // Metade do arquivo pode passar (a secao .text comeca no fim dos cabecalhos)
    // ou falhar, mas nunca pode ler alem do que sobrou.
    if (parsed.ok()) {
        for (std::size_t i = 0; i < parsed.image.numberOfSections; ++i) {
            const auto& section = parsed.image.sections[i];
            EXPECT_LE(static_cast<std::size_t>(section.rawOffset) + section.rawSize, bytes.size());
        }
    } else {
        EXPECT_NE(parsed.error, opcoda::pe::PeError::kOk);
    }
}

TEST(EndToEnd, TransportPlayProducesAudioWithoutMidi) {
    // A Historia 1 da specs/008-transporte-e-vistas: uma pessoa que abre o
    // Opcoda pela primeira vez nao tem teclado MIDI, e sem isto nao ha way de
    // ouvir o material. Este teste e' a prova de que o caminho existe.
    const auto bytes = readFile("C:\\Windows\\System32\\notepad.exe");
    ASSERT_FALSE(bytes.empty()) << "notepad.exe nao encontrado";

    std::vector<float> heads;
    const auto audio = renderThroughTransport(bytes, 200, &heads);
    ASSERT_FALSE(audio.empty());

    const auto measured = stats(audio);
    EXPECT_TRUE(measured.allFinite) << "saida contem NaN ou infinito";
    EXPECT_GT(measured.peak, 0.01f)
        << "o transporte produziu audio mudo; o gate nao foi aberto pelo transporte";
    EXPECT_LE(measured.peak, 1.0f) << "limitador deixou passar acima do teto";

    // A cabeca andou. Sem esta asercao o teste passa com um transporte parado a
    // devolver sempre a mesma posicao, que e' o modo de falha mais provavel.
    ASSERT_EQ(heads.size(), 200u);
    EXPECT_GT(heads.back(), heads.front());
    EXPECT_LT(heads.back(), 1.0f);
}

TEST(EndToEnd, StalledTransportIsSilent) {
    // O contrario do teste anterior, e pelo mesmo motivo: um transporte parado
    // tem de ser mudo. Se este teste falhasse, o teste de cima passaria sem
    // gate nenhum, porque a rampa do motor ja teria aberto a porta sozinha.
    const auto bytes = readFile("C:\\Windows\\System32\\notepad.exe");
    ASSERT_FALSE(bytes.empty());

    const auto parsed = opcoda::pe::parse(bytes.data(), bytes.size());
    ASSERT_TRUE(parsed.ok());
    const auto& section = parsed.image.sections[0];
    const auto samples = toSamples(bytes.data(), section.rawOffset, section.rawSize);

    constexpr int kBlock = 256;
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(samples.size());
    // play() de proposito omitido.

    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);
    engine.setSource(samples.data(), samples.size());

    GranularParams params;
    params.densityGrainsPerSec = 40.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    std::vector<float> out;

    for (int i = 0; i < 200; ++i) {
        const auto playing = transport.isPlaying();
        engine.setSounding(playing);
        engine.processBlock(left.data(), right.data(), kBlock, params);
        out.insert(out.end(), left.begin(), left.end());
    }

    EXPECT_LT(stats(out).peak, 1.0e-4f) << "transporte parado produziu audio";
}

TEST(EndToEnd, ParseFileReadsFromDisk) {
    const auto parsed = parseFile("C:\\Windows\\System32\\notepad.exe");
    ASSERT_TRUE(parsed.ok()) << opcoda::pe::toString(parsed.error);
    EXPECT_GT(parsed.image.numberOfSections, 3u);
    EXPECT_STREQ(parsed.image.sections[0].name, ".text");
}

TEST(EndToEnd, ParseFileRejectsMissingFile) {
    const auto parsed = parseFile("C:\\nao\\existe\\nem\\um\\byte.exe");
    EXPECT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.error, opcoda::pe::PeError::kEmpty);
}

TEST(EndToEnd, ParseFileRejectsNullPath) {
    EXPECT_EQ(parseFile(nullptr).error, opcoda::pe::PeError::kEmpty);
}
