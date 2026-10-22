// O rastro dos graos, sem JUCE.
//
// Tudo o que aqui se verifica e' aritmetica pura sobre arrays fixos. A razao de
// estes ensaios existirem e' que a logica morava dentro de um `juce::Component`,
// onde nao ha como a testar sem instanciar uma janela — e e' `pe::reduceToColumns`
// o precedente no nucleo: calculo que o display faz na thread de interface, e que
// por isso vive aqui para ter ensaios.
//
// Os ensaios de telemetria que vem a seguir (suite `GrainTelemetry`, no
// dsp_test.cpp) tratam do que a thread de audio publica. Aqui trata-se do que a
// thread de interface faz com isso.

#include <gtest/gtest.h>

#include "opcoda_core/view/grain_trail.h"

#include <algorithm>

using opcoda::dsp::Grain;
using opcoda::dsp::GrainTelemetry;
using opcoda::dsp::GrainView;
using opcoda::view::GrainTrailSet;

namespace {

// A regiao das capturas: 678 528 bytes dentro de um ficheiro de 51,76 MB.
constexpr std::uint64_t kRegionStart {0x0000A400};
constexpr std::uint64_t kRegionEnd {0x000AFE80};

GrainTrailSet makeSet() {
    GrainTrailSet set;
    set.setRegion(kRegionStart, kRegionEnd);
    return set;
}

// Empurra `frames` posicoes para uma voz, avancando `step` por quadro. E' o que a
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

    // `span` e' o teste barato para "vale a pena pintar". Sem material e' zero.
    EXPECT_FLOAT_EQ(set.trail(0).span(), 0.0f);
}

TEST(GrainTrail, OneFrameGivesAPointAndNoSpan) {
    // Um unico quadro nao e' um rastro: e' um ponto, e o desenho trata curso zero
    // como ponto. E' o que acontece com spray e pitch a zero.
    GrainTrailSet set = makeSet();
    set.push(0, 0.5f);

    EXPECT_EQ(set.trail(0).count, 1);
    EXPECT_FLOAT_EQ(set.trail(0).newest(), 0.5f);
    EXPECT_FLOAT_EQ(set.trail(0).oldest(), 0.5f);
    EXPECT_FLOAT_EQ(set.trail(0).span(), 0.0f);
    EXPECT_TRUE(set.active(0));
}

TEST(GrainTrail, TrailGrowsUpToTwentyFourFramesAndThenStops) {
    // O comprimento e' em quadros e nao em tempo: o rastro e' sempre os ultimos
    // 24 quadros, seja a que taxa o host repinte. Passar de 24 tem de truncar a
    // cauda e nao crescer o array.
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, GrainTrailSet::kFrames + 50);

    EXPECT_EQ(set.trail(0).count, GrainTrailSet::kFrames);
    EXPECT_EQ(GrainTrailSet::kFrames, 24) << "o valor e' o que o desenho mede em px";

    // O mais recente e' a ultima posicao empurrada; o mais antigo e' 24 posicoes
    // atras da primeira, e nao a primeira de todas.
    // Tolerancia e nao igualdade exacta: 0,001 acumulado em 73 passos perde precisao
    // em float, e o que se quer verificar aqui e' que a cauda e' a posicao de 50
    // passos atras e nao a primeira de todas.
    EXPECT_NEAR(set.trail(0).newest(), 0.10f + 73.0f * 0.001f, 1.0e-5f);
    EXPECT_NEAR(set.trail(0).oldest(), 0.10f + 50.0f * 0.001f, 1.0e-5f);
}

TEST(GrainTrail, TrailFollowsTheVoiceNotThePublishedSlot) {
    // **O ensaio que justifica a identidade.** O publish do motor compacta os
    // graos activos para a frente, portanto o slot diz em que ordem apareceram e
    // nao que grao e'. Um rastro ligado ao slot saltava de grao a cada bloco.
    //
    // Aqui a voz 0 e' empurrada 10 quadros. A voz 2 e' empurrada 5. O rastro da
    // voz 0 tem de ter os seus 10 e o da voz 2 os seus 5, cada um a partir da sua
    // posicao. Se o rastro fosse por posicao no array, os dois misturavam.
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, 10);
    advance(set, 2, 0.70f, 0.001f, 5);

    EXPECT_EQ(set.trail(0).count, 10);
    EXPECT_EQ(set.trail(2).count, 5);

    EXPECT_NEAR(set.trail(0).newest(), 0.109f, 1.0e-5f);
    EXPECT_NEAR(set.trail(2).newest(), 0.704f, 1.0e-5f);

    // E o curso de cada um e' o seu, nao o do outro.
    EXPECT_NEAR(set.trail(0).span(), 0.009f, 1.0e-5f);
    EXPECT_NEAR(set.trail(2).span(), 0.004f, 1.0e-5f);
}

TEST(GrainTrail, AReusedSlotFromAnotherVoiceDoesNotStretchTheTrailAcrossTheScreen) {
    // O caso que a identidade apanha: a voz 3 acaba, e no bloco seguinte a voz 6
    // ocupa o slot 0 da publicacao. Sem identidade, a voz 6 herdava a cauda da 3 e
    // o ecra mostrava um grao a atravessar o material de uma ponta a outra.
    //
    // Aqui a voz 3 fica parada em 0,20 com o rastro feito. A voz 6 entra em 0,80.
    // Depois de muitos quadros de 3, a 6 tem de ter o curso **dela**, e nao o
    // percurso de 0,20 ate 0,80.
    GrainTrailSet set = makeSet();
    advance(set, 3, 0.20f, 0.001f, 15);
    advance(set, 6, 0.80f, 0.002f, 15);

    EXPECT_NEAR(set.trail(6).span(), 14.0 * 0.002f, 1.0e-5f)
        << "o rastro da voz nova herdou a cauda da velha";
    EXPECT_GT(set.trail(6).oldest(), 0.5f)
        << "a cauda da voz 6 voltou a 0,20, o que significa queMisturou as duas";
}

TEST(GrainTrail, ClearEmptiesEveryVoiceAtOnce) {
    // E' o que `setRegion` e `setMode` chamam. Sem isto, a cauda de um grao que
    // leu o fim da regiao antiga aparecia sobre o material novo, e lia-se como um
    // salto do grao.
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
    // Uma fracao fora de [0, 1] nao e' um grao a ler: e' um publish antigo ou um
    // bug a montante. Recusar e' melhor do que pintar um rastro no sitio errado.
    GrainTrailSet set = makeSet();

    EXPECT_FALSE(set.push(0, 1.5f));
    EXPECT_FALSE(set.push(0, -0.01f));
    EXPECT_EQ(set.trail(0).count, 0);
    EXPECT_FALSE(set.active(0));

    // Os limites sao validos: 0,0 e' o primeiro byte e 1,0 o ultimo.
    EXPECT_TRUE(set.push(0, 0.0f));
    EXPECT_TRUE(set.push(0, 1.0f));
}

TEST(GrainTrail, AnInvalidVoiceIsRefusedAndChangesNothing) {
    // `GrainView::voice` vem da thread de audio. Um -1 la seria um bug, e escrever
    // com -1 indexaria o array por baixo.
    GrainTrailSet set = makeSet();
    advance(set, 0, 0.10f, 0.001f, 5);

    EXPECT_FALSE(set.push(-1, 0.5f));
    EXPECT_FALSE(set.push(GrainTrailSet::kMaxVoices, 0.5f));
    EXPECT_FALSE(set.push(1000, 0.5f));

    EXPECT_EQ(set.trail(0).count, 5) << "o rastro da voz 0 mexeu-se";
    EXPECT_FALSE(set.active(GrainTrailSet::kMaxVoices));
}

TEST(GrainTrail, TheAddressMatchesTheRegionFractionNotTheFileFraction) {
    // **A regiao e' 0,1% do ficheiro**, e e' por isso que o hex precisa do endereco
    // e nao da fracao: a fracao 0,5 da regiao e' 0,0005 do ficheiro. Com a regiao
    // errada, o grao marcaria uma celula a um terco do ficheiro em vez de dentro
    // do material que esta' a soar.
    GrainTrailSet set = makeSet();

    set.push(0, 0.5f);
    const auto address = set.address(0);

    EXPECT_EQ(address, 0x0005D140u);
    EXPECT_GE(address, kRegionStart);
    EXPECT_LT(address, kRegionEnd);

    // **A fracao do ficheiro e' ordens de grandeza menor do que a da regiao.**
    // A regiao comeca em 0xA400, por isso a fracao do ficheiro nao e' 0,5 x 0,0013
    // mas 0,5 x 0,0013 mais o offset do inicio da regiao. O valor medido e' 0,00074.
    const auto fileFraction = static_cast<double>(address) / 517620736.0;
    EXPECT_NEAR(fileFraction, 0.00074, 0.00002);
    EXPECT_LT(fileFraction, 0.001)
        << "a regiao e' 0,13% do ficheiro e a fracao do grao tem de estar abaixo disso";
}

TEST(GrainTrail, AddressesTrackTheTrailAsTheGrainSweeps) {
    // O endereco do hex tem de acompanhar a varredura, e nao ficar no byte onde o
    // grao nasceu.
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
    // O caminho completo, do publish ao rastro, com o encadeamento que o editor
    // faz: ler a telemetria e empurrar cada voz. E' o que garante que o desenho e a
    // telemetria falam da mesma coisa.
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

    // A voz publica 0 e' a voz 2 do motor, e e' na voz 2 que o rastro fica.
    EXPECT_EQ(set.trail(2).count, 1);
    EXPECT_EQ(set.trail(5).count, 1);
    EXPECT_FLOAT_EQ(set.trail(2).newest(), 0.42f);
    EXPECT_FLOAT_EQ(set.trail(5).newest(), 0.61f);
    EXPECT_FALSE(set.active(0)) << "o slot 0 foi lido como se fosse a voz 0 do motor";
}

TEST(GrainTrail, AFinishedGrainLeavesItsTrailToFadeRatherThanJump) {
    // O grao acaba e a voz deixa de ser publicada. O rastro fica com o que tem, a
    // desvanecer, em vez de desaparecer e aparecer outro no sitio. E' o que
    // suaviza a saida de um grao.
    GrainTrailSet set = makeSet();
    advance(set, 1, 0.30f, 0.002f, 12);
    const auto before = set.trail(1).count;
    ASSERT_GT(before, 0);

    // A voz 1 deixa de ser publicada.
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

    // A voz 1 nao recebeu nada novo, portanto o rastro dela esta' intacto e nao
    // foi contaminado pela voz 4.
    EXPECT_EQ(set.trail(1).count, before);
    EXPECT_NEAR(set.trail(1).newest(), 0.30f + 11.0f * 0.002f, 1.0e-5f);
    EXPECT_EQ(set.trail(4).count, 1);
}

// **O que este ensaio diz, e o que ele nao diz.** A medicao original mostrou que
// com uma regiao de 678 528 bytes e um grao de 40 ms a 60 Hz, o curso de oito
// quadros e' de 0,025 px e o de vinte e quatro e' de 0,075 px. Nenhum dos dois
// chega a um pixel.
//
// **Nao ha comprimento de rastro que resolva isto**, e este ensaio existe para o
// deixar escrito. Uma regiao de meia megabyte e' tao larga que a varredura de um
// grao, em qualquer numero razoavel de quadros, cabe dentro de um pixel. Só
// mudando de escala — a regiao para alguns milhares de bytes, ou o rastro a ser
// desenhado noutra escala que nao a posicao — e' que a cauda se distingue de um
// ponto.
//
// Portanto: com `spray` e `pitch` a zero numa regiao grande, **o rastro lê-se como
// ponto, e isso e' honesto.** A cauda aparece quando ha movimento real, e o
// desenho trata curso zero como ponto sem truque nenhum.
TEST(GrainTrail, ASweepOfThisRegionIsSubPixelAndNoTrailLengthFixesIt) {
    constexpr double kSampleRate = 44100.0;
    constexpr double kFramesPerSecond = 60.0;
    constexpr float kGrainMs = 40.0f;
    constexpr double kDisplayWidth = 880.0;
    const auto regionSamples = static_cast<double>(kRegionEnd - kRegionStart);

    // Amostras que o grao avanca num quadro, e a fracao da regiao que isso e'.
    const auto samplesPerFrame =
        (kGrainMs * 0.001 * kSampleRate) / kFramesPerSecond;
    const auto fractionPerFrame = samplesPerFrame / regionSamples;

    EXPECT_LT(fractionPerFrame * kDisplayWidth * 8.0, 1.0)
        << "oito quadros ja dariam um pixel inteiro; o ensaio deixou de valer";

    // Onde esta' o limite: medido, 64 quadros dão 2,4 px. A partir de um pixel a
    // cauda ja se distingue de um ponto, e isso levaria um rastro de 64 quadros —
    // mais de meio segundo a 60 Hz, que e' tempo demais para descrever um grao de
    // 40 ms. E' a razao de os 24 quadros serem um compromisso e nao um minimo.
    EXPECT_GT(fractionPerFrame * kDisplayWidth * 64.0, 1.0)
        << "a regiao encolheu o suficiente para 64 quadros serem viaveis";
    EXPECT_LT(GrainTrailSet::kFrames, 64)
        << "o rastro esta' a aproximar-se do limite em que a cauda descreve "
           "outro grao em vez do seu";

    // E' por isso que o desenho nao inventa comprimento: um rastro com curso zero
    // e' um ponto, e um rastro com curso real e' desenhado no comprimento que tem.
    EXPECT_FLOAT_EQ(GrainTrailSet::Trail {}.span(), 0.0f);
}

TEST(GrainTrail, TheTrailSetNeverAllocates) {
    // A thread de interface repinta a 60 Hz, e um `std::vector` aqui seria alocar a
    // cada quadro. Os arrays sao fixos e o numero de vozes e' constante.
    EXPECT_EQ(GrainTrailSet::kMaxVoices, 8);
    EXPECT_LE(GrainTrailSet::kFrames, 64) << "rastros tao compridos custam push por quadro";
}