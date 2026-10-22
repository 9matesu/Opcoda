#include <gtest/gtest.h>

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/dsp/scale.h"

#include <cmath>
#include <vector>

using opcoda::dsp::GranularEngine;
using opcoda::dsp::GranularParams;
using opcoda::dsp::ScaleType;
using opcoda::dsp::quantizeToScale;
using opcoda::dsp::semitonesForNote;

namespace {
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 128;
} // namespace

TEST(Scale, ChromaticPassesEveryNoteThrough) {
    for (int note = 0; note <= 127; ++note) {
        EXPECT_EQ(quantizeToScale(note, ScaleType::kChromatic, 0), note);
    }
}

TEST(Scale, MajorQuantizesToNearestDegree) {
    // C maior: C D E F G A B. C# desce a C, D# desce a D, F# desce a F.
    EXPECT_EQ(quantizeToScale(60, ScaleType::kMajor, 0), 60);
    EXPECT_EQ(quantizeToScale(61, ScaleType::kMajor, 0), 60);
    EXPECT_EQ(quantizeToScale(63, ScaleType::kMajor, 0), 62);
    EXPECT_EQ(quantizeToScale(66, ScaleType::kMajor, 0), 65);
}

TEST(Scale, TieGoesDown) {
    // F# esta a um semitom de F e de G: o empate desce, por decisao escrita.
    EXPECT_EQ(quantizeToScale(66, ScaleType::kMajor, 0), 65);
    // G# idem entre G e A.
    EXPECT_EQ(quantizeToScale(68, ScaleType::kMajor, 0), 67);
}

TEST(Scale, RootTransposesTheWholeScale) {
    // Re maior tem F# e C#: as notas que sao acidentes em Do passam intactas.
    EXPECT_EQ(quantizeToScale(66, ScaleType::kMajor, 2), 66);
    EXPECT_EQ(quantizeToScale(61, ScaleType::kMajor, 2), 61);
    // F (65) nao pertence a Re maior: desce a E (64), que e' o grau seguinte.
    EXPECT_EQ(quantizeToScale(65, ScaleType::kMajor, 2), 64);
}

TEST(Scale, PentatonicMinorDropsTheHalfSteps) {
    // La menor pentatonica: A C D E G. Em semitons sobre A (69): 0 3 5 7 10.
    EXPECT_EQ(quantizeToScale(70, ScaleType::kPentatonicMinor, 9), 69);
    EXPECT_EQ(quantizeToScale(71, ScaleType::kPentatonicMinor, 9), 72);
}

// Razao de reproducao = 2^(semitons/12). Do4 e' a unidade, Do5 e' o dobro, Do3
// e' metade: e' o que o motor aplica ao readStep do grao.
TEST(Pitch, NoteSixtyIsUnity) {
    const double ratio = std::pow(2.0, semitonesForNote(60, ScaleType::kChromatic, 0) / 12.0);
    EXPECT_NEAR(ratio, 1.0, 1e-9);
}

TEST(Pitch, NoteSeventyTwoIsOctave) {
    const double ratio = std::pow(2.0, semitonesForNote(72, ScaleType::kChromatic, 0) / 12.0);
    EXPECT_NEAR(ratio, 2.0, 1e-9);
}

TEST(Pitch, NoteFortyEightIsHalf) {
    const double ratio = std::pow(2.0, semitonesForNote(48, ScaleType::kChromatic, 0) / 12.0);
    EXPECT_NEAR(ratio, 0.5, 1e-9);
}

TEST(Glide, ReachesTargetInDeclaredTime) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    GranularParams params;
    params.noteNumber = 72; // Do5, +12 semitons
    params.glideSeconds = 0.05f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);

    // 50 ms a 48 kHz sao 2400 amostras: 18 blocos de 128 = 2304 (meio bloco
    // antes do alvo) e 19 blocos = 2432 (depois).
    for (int block = 0; block < 18; ++block) {
        engine.processBlock(left.data(), right.data(), kBlock, params);
    }
    EXPECT_LT(engine.glideSemitones(), 12.0);
    EXPECT_GT(engine.glideSemitones(), 10.0);

    engine.processBlock(left.data(), right.data(), kBlock, params);
    EXPECT_DOUBLE_EQ(engine.glideSemitones(), 12.0);
}

TEST(Glide, ZeroJumpsImmediately) {
    GranularEngine engine;
    engine.prepare(kSampleRate, kBlock);

    GranularParams params;
    params.noteNumber = 72;
    params.glideSeconds = 0.0f;

    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    engine.processBlock(left.data(), right.data(), kBlock, params);
    EXPECT_DOUBLE_EQ(engine.glideSemitones(), 12.0);
}
