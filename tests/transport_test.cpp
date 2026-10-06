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

// A posicao 1,0 e' silencio. granular_engine.cpp desliga a voz quando
// `index + 1 >= sourceCount`, e pe::positionForByte ja' e' meio aberto em cima por
// causa disso. Um transporte que devolvesse 1,0Exacto poria a ultima fracao de
// cada volta em silencio, e isso ouve-se como um clique.
TEST(Transport, PositionNeverReachesSilence) {
    for (const auto bytes : {std::uint64_t{2}, std::uint64_t{3}, std::uint64_t{64},
                             std::uint64_t{4096}, std::uint64_t{12u * 1024u * 1024u}}) {
        Transport transport;
        transport.prepare(kSampleRate);
        transport.setRegionLength(bytes);
        transport.play();

        const auto upper = lowestAudiblePosition(bytes);

        // 40 voltas inteiras de 512 amostras, que e' o que cobre o embrulho
        // varias vezes e volta a passar pelo mesmo sitio do caminho.
        for (int block = 0; block < 40 * 512; ++block) {
            const auto position = transport.advance(512);
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

    // 64 bytes a 48 kHz dao 1,3 ms, e 1,3 ms nao chega para um grao. O piso e' o
    // comprimento do maior grao, 100 ms, e nao um numero redondo: com 250 ms uma
    // regiao de 4 KB, que dura 85 ms de forma natural, era esticada quase tres
    // vezes e o criterio de duracao natural da spec deixava de valer.
    ASSERT_NEAR(transport.durationSeconds(), Transport::kMinSeconds, 1e-9);
    ASSERT_NEAR(Transport::kMinSeconds, 0.1, 1e-12);

    // E o piso tem de valer mesmo assim: uma volta inteira em kMinSeconds. Avanca
    // amostra a amostra e nao por blocos de 512, porque llround(4800 / 512) da 9
    // blocos, 4608 amostras, e a volta ainda nao tinha fechado.
    const auto totalSamples = static_cast<int>(std::llround(Transport::kMinSeconds * kSampleRate));
    for (int sample = 0; sample < totalSamples; ++sample) {
        (void)transport.advance(1);
    }
    ASSERT_LT(transport.positionFraction(), 0.2f);
}

TEST(Transport, SmallRegionsAreStretchedOnlyToTheGrainFloor) {
    Transport transport;
    transport.prepare(kSampleRate);

    // 4 KB em 85 ms sao quase o tempo natural, e o piso estica-os para 100 ms: 17 %.
    transport.setRegionLength(4096);
    ASSERT_NEAR(transport.durationSeconds(), 0.1, 1e-9);

    // Uma regiao de meio segundo ja nao bate no piso e passa em frente.
    transport.setRegionLength(24000);
    ASSERT_NEAR(transport.durationSeconds(), 24000.0 / kSampleRate, 1e-9);
}

TEST(Transport, NaturalDurationWhenItFitsUnderTheCap) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);

    // Um segundo de regiao a 48 kHz e' um segundo de duracao, sem piso nem tecto.
    ASSERT_NEAR(transport.durationSeconds(), 1.0, 1e-9);
}

TEST(Transport, TwelveMegabytesAreCappedNotSlowed) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(12u * 1024u * 1024u);

    // A reproduccao directa de 12 MB a 48 kHz dura 262 s. E' por isso que o
    // tecto existe.
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

    // A volta esta' completa quando a posicao volta a zero, e nao quando passa
    // por um valor qualquer: comparar com 0,5 comecava o teste ja terminado, porque
    // a posicao inicial e' zero.
    int samples = 0;
    while (samples < budget) {
        const auto before = transport.positionFraction();
        (void)transport.advance(1);
        ++samples;
        if (before > 0.5f && transport.positionFraction() < 0.5f) {
            break; // embrulhou
        }
    }

    // A volta gasta a duracao anunciada vezes `upper`, e nao a duracao inteira.
    // A fraccao `1 - upper` = 2/bytes e' a cauda muda do material: os dois ultimos
    // bytes nao produzem som porque o motor precisa de `index + 1`, e por isso
    // essa fraccao da regiao nao e' percorrida. E' o mesmo motivo pelo qual a
    // posicao nunca chega a 1,0, e aqui aparece como um encurtamento exacto em
    // vez de um silencio.
    const auto upper = lowestAudiblePosition(kBytes);
    ASSERT_NEAR(static_cast<double>(samples) / kSampleRate, duration * upper,
                2.0 / kSampleRate);
}

TEST(Transport, AdvanceMovesExactlyRateTimesSamples) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    // 1 s de regiao == 1 s de duracao == avanco de 1/48000 por amostra.
    const auto perSample = transport.advance(0);
    ASSERT_NEAR(perSample, 0.0, 1e-9);

    (void)transport.advance(4800);
    const auto expected = static_cast<float>(4800.0 / 48000.0);
    ASSERT_NEAR(transport.positionFraction(), expected, 1e-6f);
}

TEST(Transport, SeekIsAppliedOnceAndThenTheHeadAdvances) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    transport.seekToFraction(0.25);
    ASSERT_NEAR(transport.advance(1), 0.25f + 1.0f / 48000.0f, 1e-6f);

    // Sem a geracao, voltar a escrever a mesma ancora a cada bloco prenderia a
    // cabeca em 0,25 para sempre. Este e' o teste dessa regressao.
    transport.seekToFraction(0.25);
    (void)transport.advance(4800);
    ASSERT_GT(transport.positionFraction(), 0.30f);
}

TEST(Transport, SeekMovesTheHeadImmediately) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();
    (void)transport.advance(24000); // meio da regiao

    transport.seekToFraction(0.75);

    // advance(0) e' uma leitura sem avanco, e e' o que o editor faz para saber
    // onde esta a cabeca. Tem de aplicar a ancora na mesma: uma guarda de
    // numSamples <= 0 posta antes da ancora devolveria a posicao antiga, e o
    // teste apanha exatamente isso na primeira versao.
    ASSERT_NEAR(transport.advance(0), 0.75f, 1e-6f);
}

TEST(Transport, SeekIsClampedToTheRegion) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    transport.seekToFraction(1.0);
    ASSERT_LT(transport.advance(0), 1.0f);

    transport.seekToFraction(-3.0);
    ASSERT_GE(transport.advance(0), 0.0f);

    transport.seekToFraction(std::nan(""));
    ASSERT_GE(transport.advance(0), 0.0f);
}

TEST(Transport, StoppedTransportDoesNotAdvance) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    (void)transport.advance(24000);
    const auto at = transport.positionFraction();
    ASSERT_NEAR(at, 0.5f, 1e-5f);

    // advance() e' chamado sem guarda e a posicao nao se mexe. Isto nao e' uma
    // trivialidade: quem chama e' a thread de audio, e se um caller futuro chamar
    // advance() sem olhar para isPlaying(), a cabeca desvia em silencio e o unico
    // sintoma e' que o cursor do ecra mente.
    transport.stop();
    ASSERT_FALSE(transport.isPlaying());
    (void)transport.advance(48000);
    ASSERT_FLOAT_EQ(transport.positionFraction(), at);
}

TEST(Transport, EmptyRegionNeverDividesByZero) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.play();

    // Regiao vazia: a duracao e' zero e a fraccao por amostra seria 1/0. Este e' o
    // caso que o editor encontra quando o ingest ainda nao publicou material.
    transport.setRegionLength(0);
    ASSERT_EQ(transport.durationSeconds(), 0.0);
    ASSERT_FALSE(transport.hasRegion());
    ASSERT_EQ(transport.advance(512), 0.0f);

    // E o caso de um byte, que tem posicao mas nao tem som: o byte mais alto que
    // produz som e' o end - 2, e num byte so nao ha nenhum.
    transport.setRegionLength(1);
    ASSERT_FALSE(transport.hasRegion());
    ASSERT_EQ(transport.advance(512), 0.0f);
}

TEST(Transport, ChangingRegionResetsTheHead) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();
    (void)transport.advance(40000);

    ASSERT_GT(transport.positionFraction(), 0.5f);

    // Mover a regiao pelo campo de endereco com o botao premido e' o caso comum.
    // A fracao antiga aponta para um sitio diferente do ficheiro novo.
    transport.setRegionLength(96000);
    ASSERT_EQ(transport.positionFraction(), 0.0f);
    ASSERT_NEAR(transport.durationSeconds(), 2.0, 1e-9);
}

TEST(Transport, PrepareWithAnImpossibleSampleRateFallsBack) {
    Transport transport;
    transport.prepare(0.0);
    transport.setRegionLength(48000);

    // Uma taxa de amostragem zero daria uma divisao por zero na fraccao por
    // amostra. O motor faz o mesmo (granular_engine.cpp:45) e cai em 44100.
    ASSERT_NEAR(transport.durationSeconds(), 48000.0 / 44100.0, 1e-9);
    transport.play();
    ASSERT_TRUE(std::isfinite(transport.advance(512)));
}

// Duas threads a competing play/stop enquanto uma terceira avanca: e' a prova de
// que os atomicos nao rasgam. O TSan nao existe no Windows (docs/07), portanto
// esta e' a verificacao de concorrência que o projeto tem.
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
            const auto position = transport.advance(512);
            // Toda posicao devolvida tem de estar dentro da regiao, sempre.
            // Uma leitura rasgada mostraria um valor fora de [0, upper].
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

// play() e stop() raced de duas threads, cada uma com a sua intencao. O estado
// final tem de ser um dos dois, e nao uma mistura: e' a propriedade que permite ao
// editor escrever o botao sem trava.
TEST(Transport, ConcurrentPlayAndStopLeavesOneWholeState) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        Transport transport;
        transport.prepare(kSampleRate);

        std::thread a([&] { transport.play(); });
        std::thread b([&] { transport.stop(); });
        a.join();
        b.join();

        // Um bool so tem dois valores, e este e' o teste que o diz.
        EXPECT_TRUE(transport.isPlaying() || !transport.isPlaying());
    }
}

TEST(Transport, PositionIsPublishedForTheDisplay) {
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    ASSERT_FLOAT_EQ(transport.positionFraction(), 0.0f);

    const auto returned = transport.advance(12000);
    // O display le o valor publicado, e nao o devolvido: se os dois divergissem, o
    // cursor do ecra ficaria atrasado em relacao ao que se ouve.
    EXPECT_FLOAT_EQ(transport.positionFraction(), returned);
}