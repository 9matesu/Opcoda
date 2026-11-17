
#include <gtest/gtest.h>

#include "opcoda_core/view/grain_trail.h"

#include <algorithm>

using opcoda::dsp::Grain;
using opcoda::dsp::GrainTelemetry;
using opcoda::dsp::GrainView;
using opcoda::view::GrainTrailSet;

namespace {

// a regiao das capturas: 678 528 bytes num ficheiro de 51,76 MB.
constexpr std::uint64_t kRegionStart {0x0000A400};
constexpr std::uint64_t kRegionEnd {0x000AFE80};

GrainTrailSet makeSet() {
    GrainTrailSet set;
    set.setRegion(kRegionStart, kRegionEnd);
    return set;
}

void advance(GrainTrailSet& set, int voice, float first, float step, int frames) {
    float position = first;
    for (int f = 0; f < frames; ++f) {
        set.push(voice, position);
        position += step;
    }
}

} // namespace

TEST(GrainTrail, NothingIsDrawnBeforeSomethingIsPublished) {
    GrainTrailSet set = makeSet();

    EXPECT_EQ(set.trail(0).count, 0);
    EXPECT_FALSE(set.active(0));
    EXPECT_FALSE(set.anyActive());

    // span e' o teste barato para "vale a pena pintar". sem material e' zero.
    EXPECT_FLOAT_EQ(set.trail(0).span(), 0.0f);
}

TEST(GrainTrail, OneFrameGivesAPointAndNoSpan) {
    GrainTrailSet set = makeSet();
    set.push(0, 0.5f);

    EXPECT_EQ(set.trail(0).count, 1);
    EXPECT_FLOAT_EQ(set.trail(0).newest(), 0.5f);
    EXPECT_FLOAT_EQ(set.trail(0).oldest(), 0.5f);
    EXPECT_FLOAT_EQ(set.trail(0).span(), 0.0f);
    EXPECT_TRUE(set.active(0));
}

TEST(GrainTrail, TrailGrowsUpToTwentyFourFramesAndThenStops) {
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, GrainTrailSet::kFrames + 50);

    EXPECT_EQ(set.trail(0).count, GrainTrailSet::kFrames);
    EXPECT_EQ(GrainTrailSet::kFrames, 24) << "o valor e' o que o desenho mede em px";

    EXPECT_NEAR(set.trail(0).newest(), 0.10f + 73.0f * 0.001f, 1.0e-5f);
    EXPECT_NEAR(set.trail(0).oldest(), 0.10f + 50.0f * 0.001f, 1.0e-5f);
}

TEST(GrainTrail, TrailFollowsTheVoiceNotThePublishedSlot) {
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, 10);
    advance(set, 2, 0.70f, 0.001f, 5);

    EXPECT_EQ(set.trail(0).count, 10);
    EXPECT_EQ(set.trail(2).count, 5);

    EXPECT_NEAR(set.trail(0).newest(), 0.109f, 1.0e-5f);
    EXPECT_NEAR(set.trail(2).newest(), 0.704f, 1.0e-5f);

    // e o curso de cada um e' o seu, nao o do outro.
    EXPECT_NEAR(set.trail(0).span(), 0.009f, 1.0e-5f);
    EXPECT_NEAR(set.trail(2).span(), 0.004f, 1.0e-5f);
}

TEST(GrainTrail, AReusedSlotFromAnotherVoiceDoesNotStretchTheTrailAcrossTheScreen) {
    GrainTrailSet set = makeSet();
    advance(set, 3, 0.20f, 0.001f, 15);
    advance(set, 6, 0.80f, 0.002f, 15);

    EXPECT_NEAR(set.trail(6).span(), 14.0 * 0.002f, 1.0e-5f)
        << "o rastro da voz nova herdou a cauda da velha";
    EXPECT_GT(set.trail(6).oldest(), 0.5f)
        << "a cauda da voz 6 voltou a 0,20, o que significa queMisturou as duas";
}

TEST(GrainTrail, ClearEmptiesEveryVoiceAtOnce) {
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, 10);
    advance(set, 5, 0.80f, 0.001f, 10);
    ASSERT_TRUE(set.anyActive());

    set.clear();

    EXPECT_EQ(set.trail(0).count, 0);
    EXPECT_EQ(set.trail(5).count, 0);
    EXPECT_FALSE(set.anyActive());
}

TEST(GrainTrail, APositionOutsideTheRegionIsRefused) {
    GrainTrailSet set = makeSet();

    EXPECT_FALSE(set.push(0, 1.5f));
    EXPECT_FALSE(set.push(0, -0.01f));
    EXPECT_EQ(set.trail(0).count, 0);
    EXPECT_FALSE(set.active(0));

    // os limites sao validos: 0,0 e' o primeiro byte, 1,0 o ultimo.
    EXPECT_TRUE(set.push(0, 0.0f));
    EXPECT_TRUE(set.push(0, 1.0f));
}

TEST(GrainTrail, AnInvalidVoiceIsRefusedAndChangesNothing) {
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, 5);

    EXPECT_FALSE(set.push(-1, 0.5f));
    EXPECT_FALSE(set.push(GrainTrailSet::kMaxVoices, 0.5f));
    EXPECT_FALSE(set.push(1000, 0.5f));

    EXPECT_EQ(set.trail(0).count, 5) << "o rastro da voz 0 mexeu-se";
    EXPECT_FALSE(set.active(GrainTrailSet::kMaxVoices));
}

TEST(GrainTrail, TheAddressMatchesTheRegionFractionNotTheFileFraction) {
    GrainTrailSet set = makeSet();

    set.push(0, 0.5f);
    const auto address = set.address(0);

    EXPECT_EQ(address, 0x0005D140u);
    EXPECT_GE(address, kRegionStart);
    EXPECT_LT(address, kRegionEnd);

    const auto fileFraction = static_cast<double>(address) / 517620736.0;
    EXPECT_NEAR(fileFraction, 0.00074, 0.00002);
    EXPECT_LT(fileFraction, 0.001)
        << "a regiao e' 0,13% do ficheiro e a fracao do grao tem de estar abaixo disso";
}

TEST(GrainTrail, AddressesTrackTheTrailAsTheGrainSweeps) {
    // o endereco do hex acompanha a varredura, nao fica no byte onde o grao nasceu.
    GrainTrailSet set = makeSet();

    set.push(0, 0.25f);
    const auto first = set.address(0);
    advance(set, 0, 0.30f, 0.002f, 5);
    const auto later = set.address(0);

    EXPECT_GT(later, first) << "o endereco do grao no hex nao acompanhou a varredura";
    EXPECT_GE(later, kRegionStart);
    EXPECT_LT(later, kRegionEnd);
}

TEST(GrainTrail, TelemetryPositionsFeedTheTrailWithoutConversion) {
    GrainTelemetry telemetry;
    std::array<Grain, GrainTelemetry::kMaxVoices> voices {};
    voices[2].active = true;
    voices[2].position = 0.42;
    voices[5].active = true;
    voices[5].position = 0.61;
    telemetry.publish(voices);

    GrainTrailSet set = makeSet();
    std::array<GrainView, GrainTelemetry::kMaxVoices> views {};
    const auto count = telemetry.read(views);
    for (int i = 0; i < count; ++i) {
        set.push(views[static_cast<std::size_t>(i)].voice,
                 views[static_cast<std::size_t>(i)].position);
    }

    // a voz publica 0 e' a voz 2 do motor, e o rastro fica na 2.
    EXPECT_EQ(set.trail(2).count, 1);
    EXPECT_EQ(set.trail(5).count, 1);
    EXPECT_FLOAT_EQ(set.trail(2).newest(), 0.42f);
    EXPECT_FLOAT_EQ(set.trail(5).newest(), 0.61f);
    EXPECT_FALSE(set.active(0)) << "o slot 0 foi lido como se fosse a voz 0 do motor";
}

TEST(GrainTrail, AFinishedGrainLeavesItsTrailToFadeRatherThanJump) {
    GrainTrailSet set = makeSet();
    advance(set, 1, 0.30f, 0.002f, 12);
    const auto before = set.trail(1).count;
    ASSERT_GT(before, 0);

    // a voz 1 deixa de ser publicada.
    std::array<Grain, GrainTelemetry::kMaxVoices> voices {};
    voices[4].active = true;
    voices[4].position = 0.55;
    GrainTelemetry telemetry;
    telemetry.publish(voices);
    std::array<GrainView, GrainTelemetry::kMaxVoices> views {};
    const auto count = telemetry.read(views);
    for (int i = 0; i < count; ++i) {
        set.push(views[static_cast<std::size_t>(i)].voice,
                 views[static_cast<std::size_t>(i)].position);
    }

    EXPECT_EQ(set.trail(1).count, before);
    EXPECT_NEAR(set.trail(1).newest(), 0.30f + 11.0f * 0.002f, 1.0e-5f);
    EXPECT_EQ(set.trail(4).count, 1);
}

TEST(GrainTrail, ASweepOfThisRegionIsSubPixelAndNoTrailLengthFixesIt) {
    constexpr double kSampleRate = 44100.0;
    constexpr double kFramesPerSecond = 60.0;
    constexpr float kGrainMs = 40.0f;
    constexpr double kDisplayWidth = 880.0;
    const auto regionSamples = static_cast<double>(kRegionEnd - kRegionStart);

    // amostras que o grao avanca num quadro, e a fracao da regiao que isso e'.
    const auto samplesPerFrame =
        (kGrainMs * 0.001 * kSampleRate) / kFramesPerSecond;
    const auto fractionPerFrame = samplesPerFrame / regionSamples;

    EXPECT_LT(fractionPerFrame * kDisplayWidth * 8.0, 1.0)
        << "oito quadros ja dariam um pixel inteiro; o ensaio deixou de valer";

    EXPECT_GT(fractionPerFrame * kDisplayWidth * 64.0, 1.0)
        << "a regiao encolheu o suficiente para 64 quadros serem viaveis";
    EXPECT_LT(GrainTrailSet::kFrames, 64)
        << "o rastro esta' a aproximar-se do limite em que a cauda descreve "
           "outro grao em vez do seu";

    EXPECT_FLOAT_EQ(GrainTrailSet::Trail {}.span(), 0.0f);
}

TEST(GrainTrail, TheTrailSetNeverAllocates) {
    EXPECT_EQ(GrainTrailSet::kMaxVoices, 8);
    EXPECT_LE(GrainTrailSet::kFrames, 64) << "rastros tao compridos custam push por quadro";
}