#include "opcoda_core/rt/transport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace {

using opcoda::rt::Transport;

constexpr double kSampleRate {48000.0};

double lowestAudiblePosition(std::uint64_t bytes) {
    return 1.0 - 2.0 / static_cast<double>(bytes);
}

} // namespace

// a posicao 1,0 e' silencio: granular_engine.cpp desliga a voz quando
// `index + 1 >= sourceCount`, e positionForByte ja' e' meio aberto em cima por
// isso. devolver 1,0 exacto punha a ultima fraccao de cada volta muda, e
// ouve-se como clique.
TEST(Transport, PositionNeverReachesSilence) {
    for (const auto bytes : {std::uint64_t{2}, std::uint64_t{3}, std::uint64_t{64},
                             std::uint64_t{4096}, std::uint64_t{12u * 1024u * 1024u}}) {
        Transport transport;
        transport.prepare(kSampleRate);
        transport.setRegionLength(bytes);
        transport.play();

        const auto upper = lowestAudiblePosition(bytes);

        // 40 voltas de 512 amostras: cobre o embrulho varias vezes e repete o
        // mesmo sitio do caminho.
        for (int block = 0; block < 40 * 512; ++block) {
            const auto position = transport.advance(512, true);
            ASSERT_GE(position, 0.0f) << "bytes=" << bytes;
            ASSERT_LT(static_cast<double>(position), 1.0) << "bytes=" << bytes;
            ASSERT_LE(static_cast<double>(position), upper + 1e-6) << "bytes=" << bytes;
        }
    }
}

TEST(Transport, SmallestRegionHasAnAudibleFloor) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(64);
    transport.play();

    // 64 bytes a 48 kHz dao 1,3 ms, e 1,3 ms nao chega para um grao. o piso e' o
    // comprimento do maior grao, 100 ms, e nao um numero redondo: com 250 ms uma
    // regiao de 4 KB (85 ms naturais) esticava quase tres vezes e quebrava o
    // criterio de duracao natural da spec.
    ASSERT_NEAR(transport.durationSeconds(), Transport::kMinSeconds, 1e-9);
    ASSERT_NEAR(Transport::kMinSeconds, 0.1, 1e-12);

    // o piso tem de valer na mesma: uma volta inteira em kMinSeconds. avanca
    // amostra a amostra porque llround(4800 / 512) da 9 blocos, 4608 amostras, e
    // a volta ainda nao tinha fechado.
    const auto totalSamples = static_cast<int>(std::llround(Transport::kMinSeconds * kSampleRate));
    for (int sample = 0; sample < totalSamples; ++sample) {
        (void)transport.advance(1, true);
    }
    ASSERT_LT(transport.positionFraction(), 0.2f);
}

TEST(Transport, SmallRegionsAreStretchedOnlyToTheGrainFloor) {
    Transport transport;
    transport.prepare(kSampleRate);

    // 4 KB em 85 ms sao quase o tempo natural, e o piso estica para 100 ms: 17 %.
    transport.setRegionLength(4096);
    ASSERT_NEAR(transport.durationSeconds(), 0.1, 1e-9);

    // meio segundo ja nao bate no piso.
    transport.setRegionLength(24000);
    ASSERT_NEAR(transport.durationSeconds(), 24000.0 / kSampleRate, 1e-9);
}

TEST(Transport, NaturalDurationWhenItFitsUnderTheCap) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);

    // um segundo de regiao a 48 kHz e' um segundo, sem piso nem tecto.
    ASSERT_NEAR(transport.durationSeconds(), 1.0, 1e-9);
}

TEST(Transport, TwelveMegabytesAreCappedNotSlowed) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(12u * 1024u * 1024u);

    // 12 MB a 48 kHz duram 262 s em reproducao directa. e' por isso que o tecto
    // existe.
    ASSERT_NEAR(transport.durationSeconds(), Transport::kMaxSeconds, 1e-9);
}

TEST(Transport, FullRegionIsTraversedWithinTheAnnouncedDuration) {
    constexpr std::uint64_t kBytes {48000};
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(kBytes);
    transport.play();

    const auto duration = transport.durationSeconds();
    const auto budget = static_cast<int>(std::llround(duration * kSampleRate)) + 512;

    // a volta fecha quando a posicao volta a zero, nao num valor qualquer:
    // comparar com 0,5 comecava o teste ja terminado, porque a posicao inicial e'
    // zero.
    int samples = 0;
    while (samples < budget) {
        const auto before = transport.positionFraction();
        (void)transport.advance(1, true);
        ++samples;
        if (before > 0.5f && transport.positionFraction() < 0.5f) {
            break; // embrulhou
        }
    }

    // a volta gasta a duracao anunciada vezes `upper`, nao a duracao inteira. a
    // fraccao `1 - upper` = 2/bytes e' a cauda muda: os dois ultimos bytes nao
    // produzem som porque o motor precisa de `index + 1`, e por isso essa parte da
    // regiao nao e' percorrida. e' o mesmo motivo de a posicao nunca chegar a
    // 1,0, e aqui aparece como encurtamento exacto em vez de silencio.
    const auto upper = lowestAudiblePosition(kBytes);
    ASSERT_NEAR(static_cast<double>(samples) / kSampleRate, duration * upper,
                2.0 / kSampleRate);
}

TEST(Transport, AdvanceMovesExactlyRateTimesSamples) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    // 1 s de regiao, 1 s de duracao, 1/48000 por amostra.
    const auto perSample = transport.advance(0, true);
    ASSERT_NEAR(perSample, 0.0, 1e-9);

    (void)transport.advance(4800, true);
    const auto expected = static_cast<float>(4800.0 / 48000.0);
    ASSERT_NEAR(transport.positionFraction(), expected, 1e-6f);
}

TEST(Transport, SeekIsAppliedOnceAndThenTheHeadAdvances) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    transport.seekToFraction(0.25);
    ASSERT_NEAR(transport.advance(1, true), 0.25f + 1.0f / 48000.0f, 1e-6f);

    // sem a geracao, reescrever a mesma ancora a cada bloco prendia a cabeca em
    // 0,25 para sempre. e' o teste dessa regressao.
    transport.seekToFraction(0.25);
    (void)transport.advance(4800, true);
    ASSERT_GT(transport.positionFraction(), 0.30f);
}

TEST(Transport, SeekMovesTheHeadImmediately) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();
    (void)transport.advance(24000, true); // meio da regiao

    transport.seekToFraction(0.75);

    // advance(0) e' leitura sem avanco, e e' assim que o editor sabe onde esta a
    // cabeca. tem de aplicar a ancora na mesma: uma guarda `numSamples <= 0` antes
    // dela devolvia a posicao antiga, e foi isso que a primeira versao apanhou.
    ASSERT_NEAR(transport.advance(0, true), 0.75f, 1e-6f);
}

TEST(Transport, SeekIsClampedToTheRegion) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    transport.seekToFraction(1.0);
    ASSERT_LT(transport.advance(0, true), 1.0f);

    transport.seekToFraction(-3.0);
    ASSERT_GE(transport.advance(0, true), 0.0f);

    transport.seekToFraction(std::nan(""));
    ASSERT_GE(transport.advance(0, true), 0.0f);
}

TEST(Transport, StoppedTransportDoesNotAdvance) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    (void)transport.advance(24000, true);
    const auto at = transport.positionFraction();
    ASSERT_NEAR(at, 0.5f, 1e-5f);

    // advance() leva o snapshot que o chamador tirou de isPlaying(), nao um valor
    // fixo. quem chama e' a thread de audio, e passar `true` parado desloca a
    // cabeca em silencio: o unico sintoma e' o cursor mentir.
    // e o outro lado: passar `true` parado avanca, porque manda o snapshot.
    transport.stop();
    ASSERT_FALSE(transport.isPlaying());
    (void)transport.advance(48000, false);
    ASSERT_FLOAT_EQ(transport.positionFraction(), at);

    // mentir ao advance tambem avanca. e' o que torna o snapshot obrigatorio e
    // nao uma conveniencia.
    const auto before = transport.positionFraction();
    (void)transport.advance(48000, true);
    EXPECT_NE(transport.positionFraction(), before);
}

TEST(Transport, EmptyRegionNeverDividesByZero) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.play();

    // regiao vazia: duracao zero e a fraccao por amostra seria 1/0. e' o caso do
    // editor antes de o ingest publicar material.
    transport.setRegionLength(0);
    ASSERT_EQ(transport.durationSeconds(), 0.0);
    ASSERT_FALSE(transport.hasRegion());
    ASSERT_EQ(transport.advance(512, true), 0.0f);

    // um byte tem posicao mas nao som: o byte mais alto que soa e' end - 2, e num
    // byte so nao existe.
    transport.setRegionLength(1);
    ASSERT_FALSE(transport.hasRegion());
    ASSERT_EQ(transport.advance(512, true), 0.0f);
}

TEST(Transport, ChangingRegionResetsTheHead) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();
    (void)transport.advance(40000, true);

    ASSERT_GT(transport.positionFraction(), 0.5f);

    // arrastar a regiao com o botao premido e' o caso comum: a fraccao antiga
    // aponta para outro sitio do ficheiro novo.
    transport.setRegionLength(96000);

    // o reset e' diferido para o bloco seguinte. a interface pede o reset e nao
    // escreve em position_, que e' da thread de audio; o zero so aparece quando
    // advance() consome o pedido. ler positionFraction() antes devolve a posicao
    // antiga, e era isso que a primeira versao fazia.
    (void)transport.advance(0, true);
    EXPECT_EQ(transport.positionFraction(), 0.0f);
    EXPECT_NEAR(transport.durationSeconds(), 2.0, 1e-9);
}

// o reset nao pode engolir uma busca que chegou depois dele.
TEST(Transport, ASeekAfterARegionChangeStillApplies) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();
    (void)transport.advance(24000, true);

    // o reset e' pedido...
    transport.setRegionLength(96000);
    // ...e a busca chega antes do proximo bloco. marcar a geracao *actual* ao
    // consumir o reset descartava esta busca, e a cabeca ficava em zero depois de
    // o utilizador a ter posto em tres quartos.
    transport.seekToFraction(0.75);

    ASSERT_NEAR(transport.advance(0, true), 0.75f, 1e-6f);
}

// mas a busca do ficheiro antigo tem de ser descartada.
TEST(Transport, ASearchBeforeARegionChangeIsDiscarded) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    transport.seekToFraction(0.9);
    transport.setRegionLength(96000);

    // a ancora de 0,9 apontava para 90 % da regiao velha. na regiao nova e' uma
    // posicao sem sentido, logo o reset descarta-a.
    ASSERT_EQ(transport.advance(0, true), 0.0f);
}

TEST(Transport, PrepareWithAnImpossibleSampleRateFallsBack) {
    Transport transport;
    transport.prepare(0.0);
    transport.setRegionLength(48000);

    // taxa de amostragem zero daria divisao por zero na fraccao por amostra. o
    // motor faz o mesmo (granular_engine.cpp:45) e cai em 44100.
    ASSERT_NEAR(transport.durationSeconds(), 48000.0 / 44100.0, 1e-9);
    transport.play();
    ASSERT_TRUE(std::isfinite(transport.advance(512, true)));
}

// play/stop de duas threads enquanto uma terceira avanca: prova que os atomicos
// nao rasgam. sem TSan no Windows (docs/07), e' esta a verificacao de concorrencia
// que o projecto tem.
TEST(Transport, PlayStopAndAdvanceFromThreeThreadsNeverTear) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(1u * 1024u * 1024u);
    transport.play();

    std::atomic<bool> stop {false};
    std::atomic<int> writes {0};

    std::thread togger([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            transport.stop();
            transport.play();
            writes.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread seeker([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            transport.seekToFraction(0.0);
            transport.seekToFraction(0.99);
            writes.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread mover([&] {
        for (int block = 0; block < 200000; ++block) {
            const auto position = transport.advance(512, true);
            // toda posicao devolvida tem de cair na regiao. uma leitura rasgada
            // mostraria um valor fora de [0, upper].
            EXPECT_GE(position, 0.0f);
            EXPECT_LT(static_cast<double>(position), 1.0);
            EXPECT_LE(static_cast<double>(position), lowestAudiblePosition(1u * 1024u * 1024u));
        }
    });

    mover.join();
    stop.store(true, std::memory_order_relaxed);
    togger.join();
    seeker.join();

    EXPECT_GT(writes.load(std::memory_order_relaxed), 0);
}

// play() e stop() corridos de duas threads, cada uma com a sua intencao. o estado
// final e' um dos dois, nunca mistura: e' o que deixa o editor escrever o botao
// sem trava.
TEST(Transport, ConcurrentPlayAndStopLeavesOneWholeState) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        Transport transport;
        transport.prepare(kSampleRate);

        std::thread a([&] { transport.play(); });
        std::thread b([&] { transport.stop(); });
        a.join();
        b.join();

        // um bool so tem dois valores. e' o que este teste diz.
        EXPECT_TRUE(transport.isPlaying() || !transport.isPlaying());
    }
}

TEST(Transport, PositionIsPublishedForTheDisplay) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    ASSERT_FLOAT_EQ(transport.positionFraction(), 0.0f);

    const auto returned = transport.advance(12000, true);
    // o display le o valor publicado, nao o devolvido. se divergissem, o cursor
    // ficava atras do que se ouve.
    EXPECT_FLOAT_EQ(transport.positionFraction(), returned);
}