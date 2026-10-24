#include "vu_scale.h"

#include "opcoda_core/dsp/limiter.h"

#include <gtest/gtest.h>

#include <limits>

// A escala do medidor e' puro mapeamento, sem JUCE: o teste corre no binario do
// nucleo, onde o componente nao entra. O defeito fechado aqui e' o dos oito
// segmentos acesos em silencio: o sentinela -1.0 dB caia dentro da formula
// linear e virava (59/60)*8, o topo da escala.
namespace {

static_assert(opcoda::kSilenceDb < opcoda::kVuFloorDb,
              "regressao: o sentinela de silencio voltou a cair na escala");

TEST(VuScale, SilenceSentinelLightsNothing) {
    EXPECT_EQ(opcoda::vuSegmentsForPeak(opcoda::kSilenceDb), 0);
}

TEST(VuScale, SentinelIsBelowTheFloorAndAwayFromTheLimiterCeiling) {
    // As tres condicoes que o defeito violava ao mesmo tempo. A comparacao com
    // o teto do limitador e' o que impede voltar a -1.0 dB: esse valor e' um
    // pico real, nao silencio.
    EXPECT_LT(opcoda::kSilenceDb, opcoda::kVuFloorDb);
    EXPECT_NE(opcoda::kSilenceDb, opcoda::dsp::Limiter::kCeilingDb);
    EXPECT_EQ(opcoda::vuSegmentsForPeak(opcoda::dsp::Limiter::kCeilingDb),
              opcoda::kVuSegments);
}

TEST(VuScale, BelowTheFloorLightsNothing) {
    EXPECT_EQ(opcoda::vuSegmentsForPeak(-60.1f), 0);
    EXPECT_EQ(opcoda::vuSegmentsForPeak(-120.0f), 0);
}

TEST(VuScale, MinusSixtyDecibelsLightsNothing) {
    EXPECT_EQ(opcoda::vuSegmentsForPeak(-60.0f), 0);
}

TEST(VuScale, FullScaleLightsEverySegment) {
    EXPECT_EQ(opcoda::vuSegmentsForPeak(0.0f), opcoda::kVuSegments);
    EXPECT_EQ(opcoda::vuSegmentsForPeak(6.0f), opcoda::kVuSegments);
}

TEST(VuScale, MidScaleIsHalf) {
    EXPECT_EQ(opcoda::vuSegmentsForPeak(-30.0f), 4);
}

TEST(VuScale, RoundingAtSegmentBoundaries) {
    // -56.25 cai exactamente em 0.5 segmento: o +0.5 da' 1, o truncamento daria
    // 0. -33.75 cai em 3.5: o +0.5 da' 4, o truncamento daria 3. Sao os dois
    // lados do arredondamento, e nao um meio arbitario.
    EXPECT_EQ(opcoda::vuSegmentsForPeak(-56.25f), 1);
    EXPECT_EQ(opcoda::vuSegmentsForPeak(-33.75f), 4);
}

TEST(VuScale, LimiterCeilingStillLightsEverySegment) {
    // O limitador corta em -1 dB; um pico real nesse valor tem de acender o
    // medidor, e nao ser confundido com silencio.
    EXPECT_EQ(opcoda::vuSegmentsForPeak(opcoda::dsp::Limiter::kCeilingDb),
              opcoda::kVuSegments);
}

TEST(VuScale, NotANumberLightsNothing) {
    EXPECT_EQ(opcoda::vuSegmentsForPeak(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(VuScale, InfinitiesSaturateAtBothEnds) {
    EXPECT_EQ(opcoda::vuSegmentsForPeak(std::numeric_limits<float>::infinity()),
              opcoda::kVuSegments);
    EXPECT_EQ(opcoda::vuSegmentsForPeak(-std::numeric_limits<float>::infinity()), 0);
}

} // namespace
