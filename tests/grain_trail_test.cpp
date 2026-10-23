// o rastro dos graos, sem JUCE.
//
// tudo aqui e' aritmetica pura sobre arrays fixos. a logica morava dentro de um
// juce::Component, onde nao da para testar sem abrir janela; o precedente no
// nucleo e' pe::reduceToColumns. a suite GrainTelemetry (dsp_test.cpp) trata do
// que a thread de audio publica; aqui, do que a thread de interface faz com isso.

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

// empurra frames posicoes para uma voz, avancando step por quadro. e' o que a
// thread de interface faz a 60 Hz.
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
    // um quadro nao e' rastro, e' ponto, e o desenho trata curso zero assim. e' o
    // que acontece com spray e pitch a zero.
    GrainTrailSet set = makeSet();
    set.push(0, 0.5f);

    EXPECT_EQ(set.trail(0).count, 1);
    EXPECT_FLOAT_EQ(set.trail(0).newest(), 0.5f);
    EXPECT_FLOAT_EQ(set.trail(0).oldest(), 0.5f);
    EXPECT_FLOAT_EQ(set.trail(0).span(), 0.0f);
    EXPECT_TRUE(set.active(0));
}

TEST(GrainTrail, TrailGrowsUpToTwentyFourFramesAndThenStops) {
    // o comprimento e' em quadros, nao em tempo: o rastro e' sempre os ultimos 24,
    // seja qual for a taxa de repintura. passar de 24 trunca a cauda.
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, GrainTrailSet::kFrames + 50);

    EXPECT_EQ(set.trail(0).count, GrainTrailSet::kFrames);
    EXPECT_EQ(GrainTrailSet::kFrames, 24) << "o valor e' o que o desenho mede em px";

    // o mais recente e' a ultima posicao empurrada; o mais antigo, 24 posicoes atras.
    // tolerancia e nao igualdade: 0,001 acumulado em 73 passos perde precisao em float.
    EXPECT_NEAR(set.trail(0).newest(), 0.10f + 73.0f * 0.001f, 1.0e-5f);
    EXPECT_NEAR(set.trail(0).oldest(), 0.10f + 50.0f * 0.001f, 1.0e-5f);
}

TEST(GrainTrail, TrailFollowsTheVoiceNotThePublishedSlot) {
    // **o ensaio que justifica a identidade.** o publish compacta os activos, entao
    // o slot diz a ordem de aparicao, nao o grao. um rastro ligado ao slot saltava
    // de grao a cada bloco.
    // aqui a voz 0 anda 10 quadros e a 2 anda 5, cada uma a partir da sua posicao.
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
    // a voz 3 acaba e no bloco seguinte a 6 ocupa o slot 0 do publish. sem
    // identidade, a 6 herdava a cauda da 3 e o ecra mostrava um grao a atravessar
    // o material de ponta a ponta.
    // a 3 para em 0,20, a 6 entra em 0,80. o curso da 6 tem de ser o dela.
    GrainTrailSet set = makeSet();
    advance(set, 3, 0.20f, 0.001f, 15);
    advance(set, 6, 0.80f, 0.002f, 15);

    EXPECT_NEAR(set.trail(6).span(), 14.0 * 0.002f, 1.0e-5f)
        << "o rastro da voz nova herdou a cauda da velha";
    EXPECT_GT(set.trail(6).oldest(), 0.5f)
        << "a cauda da voz 6 voltou a 0,20, o que significa queMisturou as duas";
}

TEST(GrainTrail, ClearEmptiesEveryVoiceAtOnce) {
    // e' o que setRegion e setMode chamam. sem isto, a cauda de um grao que leu a
    // regiao antiga aparecia sobre o material novo e lia-se como salto.
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
    // fracao fora de [0, 1] nao e' grao a ler: e' publish antigo ou bug a montante.
    // recusar e' melhor do que pintar no sitio errado.
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
    // GrainView::voice vem da thread de audio. um -1 la seria bug, e escrever com
    // -1 indexaria o array por baixo.
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, 5);

    EXPECT_FALSE(set.push(-1, 0.5f));
    EXPECT_FALSE(set.push(GrainTrailSet::kMaxVoices, 0.5f));
    EXPECT_FALSE(set.push(1000, 0.5f));

    EXPECT_EQ(set.trail(0).count, 5) << "o rastro da voz 0 mexeu-se";
    EXPECT_FALSE(set.active(GrainTrailSet::kMaxVoices));
}

TEST(GrainTrail, TheAddressMatchesTheRegionFractionNotTheFileFraction) {
    // **a regiao e' 0,1% do ficheiro**, e e' por isso que o hex precisa do endereco
    // e nao da fracao: 0,5 da regiao e' 0,0005 do ficheiro. com a regiao errada o
    // grao marcava uma celula a um terco do ficheiro.
    GrainTrailSet set = makeSet();

    set.push(0, 0.5f);
    const auto address = set.address(0);

    EXPECT_EQ(address, 0x0005D140u);
    EXPECT_GE(address, kRegionStart);
    EXPECT_LT(address, kRegionEnd);

    // **a fracao do ficheiro e' ordens de grandeza menor.** a regiao comeca em
    // 0xA400, entao nao e' 0,5 x 0,0013 mas isso mais o offset; o medido e' 0,00074.
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
    // o caminho completo, do publish ao rastro, com o encadeamento do editor: ler a
    // telemetria e empurrar cada voz.
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
    // o grao acaba e a voz deixa de ser publicada. o rastro fica com o que tem, a
    // desvanecer, em vez de desaparecer e aparecer outro no sitio.
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

    // a voz 1 nao recebeu nada novo: o rastro dela esta' intacto e a voz 4 nao o
    // contaminou.
    EXPECT_EQ(set.trail(1).count, before);
    EXPECT_NEAR(set.trail(1).newest(), 0.30f + 11.0f * 0.002f, 1.0e-5f);
    EXPECT_EQ(set.trail(4).count, 1);
}

// **o que este ensaio diz e o que nao diz.** com regiao de 678 528 bytes e grao
// de 40 ms a 60 Hz, o curso de oito quadros da 0,025 px e o de 24 da 0,075 px.
// nenhum chega a um pixel, e nao ha comprimento de rastro que resolva: so mudando
// de escala.
// com spray e pitch a zero numa regiao grande, o rastro le-se como ponto, e isso
// e' honesto. a cauda aparece com movimento real.
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

    // o limite: 64 quadros dao 2,4 px. a partir de um pixel a cauda distingue-se
    // de um ponto, e isso pedia um rastro de 64 quadros, mais de meio segundo a
    // 60 Hz, tempo demais para descrever um grao de 40 ms. dai os 24.
    EXPECT_GT(fractionPerFrame * kDisplayWidth * 64.0, 1.0)
        << "a regiao encolheu o suficiente para 64 quadros serem viaveis";
    EXPECT_LT(GrainTrailSet::kFrames, 64)
        << "o rastro esta' a aproximar-se do limite em que a cauda descreve "
           "outro grao em vez do seu";

    // por isso o desenho nao inventa comprimento: curso zero e' ponto, curso real
    // e' desenhado no comprimento que tem.
    EXPECT_FLOAT_EQ(GrainTrailSet::Trail {}.span(), 0.0f);
}

TEST(GrainTrail, TheTrailSetNeverAllocates) {
    // a thread de interface repinta a 60 Hz, e um std::vector aqui alocava a cada
    // quadro. os arrays sao fixos e o numero de vozes e' constante.
    EXPECT_EQ(GrainTrailSet::kMaxVoices, 8);
    EXPECT_LE(GrainTrailSet::kFrames, 64) << "rastros tao compridos custam push por quadro";
}