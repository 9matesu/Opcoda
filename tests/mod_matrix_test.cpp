#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/dsp/mod_matrix.h"

#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::dsp::ModDest;
using opcoda::dsp::ModMatrix;
using opcoda::dsp::ModMatrixArray;
using opcoda::dsp::ModRoute;
using opcoda::dsp::ModSource;
using opcoda::dsp::ModSourceValues;
using opcoda::dsp::fromArray;
using opcoda::dsp::matrixOffset;
using opcoda::dsp::toArray;

namespace {

ModRoute route(ModSource source, ModDest dest, float amount) {
    ModRoute r;
    r.source = source;
    r.destination = dest;
    r.amount = amount;
    return r;
}

ModMatrix matrixWith(std::initializer_list<ModRoute> routes) {
    ModMatrix matrix;
    int index = 0;
    for (const auto& r : routes) {
        matrix.routes[static_cast<std::size_t>(index++)] = r;
    }
    return matrix;
}

} // namespace

TEST(Matrix, TwoRoutesFromOneSourceSumAtDestination) {
    const auto matrix = matrixWith({route(ModSource::kVelocity, ModDest::kPitch, 0.5f),
                                    route(ModSource::kVelocity, ModDest::kPitch, 0.25f)});
    ModSourceValues values;
    values.velocity = 1.0f;

    EXPECT_NEAR(matrixOffset(matrix, ModDest::kPitch, values), 0.75f, 1.0e-5f);
    // a mesma matriz nao contribui para outro destino.
    EXPECT_FLOAT_EQ(matrixOffset(matrix, ModDest::kPan, values), 0.0f);
}

TEST(Matrix, TwoSourcesSum) {
    const auto matrix = matrixWith({route(ModSource::kLfo, ModDest::kCutoff1, 0.5f),
                                    route(ModSource::kModWheel, ModDest::kCutoff1, -0.25f)});
    ModSourceValues values;
    values.lfo = 1.0f;
    values.modWheel = 1.0f;

    EXPECT_NEAR(matrixOffset(matrix, ModDest::kCutoff1, values), 0.25f, 1.0e-5f);
}

TEST(Matrix, InvalidRouteIsIgnored) {
    const auto matrix = matrixWith({route(ModSource::kNone, ModDest::kPitch, 1.0f),
                                    route(ModSource::kLfo, ModDest::kNone, 1.0f),
                                    route(ModSource::kLfo, ModDest::kPan, std::nanf(""))});
    ModSourceValues values;
    values.lfo = 1.0f;

    // fonte ou destino vazio nao modula; amount NaN vale zero, e nao propaga NaN
    // para o parametro.
    EXPECT_FLOAT_EQ(matrixOffset(matrix, ModDest::kPitch, values), 0.0f);
    EXPECT_FLOAT_EQ(matrixOffset(matrix, ModDest::kPan, values), 0.0f);
}

TEST(Matrix, OffsetIsClamped) {
    const auto matrix = matrixWith({route(ModSource::kVelocity, ModDest::kPitch, 1.0f),
                                    route(ModSource::kVelocity, ModDest::kPitch, 1.0f)});
    ModSourceValues values;
    values.velocity = 1.0f;

    // duas rotas a 100 % pediam 2,0; o destino recebe 1,0 e satura no extremo.
    EXPECT_FLOAT_EQ(matrixOffset(matrix, ModDest::kPitch, values), 1.0f);
}

TEST(Matrix, RoutesSurviveStateRoundTrip) {
    const auto original = matrixWith({route(ModSource::kEnvelope, ModDest::kCutoff1, -0.4f),
                                      route(ModSource::kEntropy, ModDest::kPosition, 0.75f),
                                      route(ModSource::kCc74, ModDest::kGrainSize, 1.0f)});

    const ModMatrixArray words = toArray(original);
    const ModMatrix restored = fromArray(words);

    for (int i = 0; i < opcoda::dsp::kMaxModRoutes; ++i) {
        const auto index = static_cast<std::size_t>(i);
        EXPECT_EQ(restored.routes[index].source, original.routes[index].source);
        EXPECT_EQ(restored.routes[index].destination, original.routes[index].destination);
        EXPECT_NEAR(restored.routes[index].amount, original.routes[index].amount, 1.0e-3f);
    }
}

TEST(Matrix, CorruptStateOpensEmpty) {
    ModMatrixArray words {};
    words[0] = 99;  // fonte inexistente
    words[1] = 99;  // destino inexistente
    words[2] = 5000;  // amount fora de faixa

    const ModMatrix restored = fromArray(words);
    EXPECT_EQ(restored.routes[0].source, ModSource::kNone);
    EXPECT_EQ(restored.routes[0].destination, ModDest::kNone);
    EXPECT_FLOAT_EQ(restored.routes[0].amount, 0.0f);
}

TEST(Matrix, EngineAppliesRouteToFilterCutoff) {
    // prova de ligacao: a rota nao pode viver so no teste da funcao pura.
    const std::vector<float> source(4800, 0.1f);
    GranularEngine engine;
    engine.prepare(48000.0, 256);
    engine.setSource(source.data(), source.size());

    GranularParams params;
    params.f1cutoffHz = 100.0f;
    params.velocity = 1.0f;
    params.modMatrix = matrixWith({route(ModSource::kVelocity, ModDest::kCutoff1, 1.0f)});

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    engine.processBlock(left.data(), right.data(), 256, params);

    // amount 1 a 5 oitavas: 100 Hz * 32.
    EXPECT_NEAR(engine.f1CutoffHz(), 3200.0f, 2.0f);

    // sem rota o corte fica no declarado, e a matriz nao deixa rasto entre blocos
    // (o params e' copiado por bloco).
    params.modMatrix = ModMatrix {};
    engine.processBlock(left.data(), right.data(), 256, params);
    EXPECT_NEAR(engine.f1CutoffHz(), 100.0f, 0.5f);
}

TEST(Matrix, WidthDestinationSurvivesTheWordRoundTrip) {
    // o destino novo entrou no fim do enum: os indices antigos continuam
    // validos, e o 13 tem de voltar como kWidth.
    const auto matrix = matrixWith({route(ModSource::kLfo, ModDest::kWidth, 0.5f)});
    const ModMatrixArray words = toArray(matrix);
    EXPECT_EQ(words[1], static_cast<int>(ModDest::kWidth));

    const auto restored = fromArray(words);
    EXPECT_EQ(restored.routes[0].destination, ModDest::kWidth);
    EXPECT_NEAR(restored.routes[0].amount, 0.5f, 1.0e-3f);
}

TEST(Matrix, EngineAppliesRouteToStereoWidth) {
    // prova de ligacao do destino novo, a mesma que o corte tem: amount 1 a 1
    // oitava leva a largura a 2.0, e com pan ao centro o unico efeito que resta
    // e' o haas — canais iguais na origem, separados na saida. sem a rota, a
    // saida continua mono.
    std::vector<float> material(24000);
    for (std::size_t i = 0; i < material.size(); ++i) {
        material[i] = 0.3f * std::sin(static_cast<float>(i) * 0.05f);
    }

    const auto run = [&material](bool withRoute) {
        GranularEngine engine;
        engine.prepare(48000.0, 256);
        engine.setSource(material.data(), material.size());
        engine.setSounding(true);

        GranularParams params;
        params.grainSizeMs = 20.0f;
        params.densityGrainsPerSec = 40.0f;
        params.position = 0.5f;
        params.widthStereo = 1.0f;
        params.velocity = 1.0f;
        if (withRoute) {
            params.modMatrix =
                matrixWith({route(ModSource::kVelocity, ModDest::kWidth, 1.0f)});
        }

        std::vector<float> left(256, 0.0f);
        std::vector<float> right(256, 0.0f);
        for (int block = 0; block < 48; ++block) {
            engine.processBlock(left.data(), right.data(), 256, params);
        }

        float difference = 0.0f;
        for (int i = 0; i < 256; ++i) {
            difference = std::max(
                difference, std::abs(left[static_cast<std::size_t>(i)] -
                                     right[static_cast<std::size_t>(i)]));
        }
        return difference;
    };

    EXPECT_LT(run(false), 1.0e-3f);
    EXPECT_GT(run(true), 0.01f);
}
