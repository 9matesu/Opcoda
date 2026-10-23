#include "opcoda_core/rt/transport.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

using opcoda::rt::Transport;

constexpr double kSampleRate {48000.0};

} // namespace

TEST(TransportRate, ScalesAdvance) {
    // taxa 2.0 percorre o dobro por bloco: 4800 amostras a 48 kHz dao 0,1 no tempo
    // natural e 0,2 ao dobro. longe do embrulho nos dois casos, para medir
    // velocidade e nao aritmetica de volta: o tecto e' 1 - 2/48000.
    Transport normal;
    normal.prepare(kSampleRate);
    normal.setRegionLength(48000);
    normal.play();

    Transport fast;
    fast.prepare(kSampleRate);
    fast.setRegionLength(48000);
    fast.setRate(2.0);
    fast.play();

    const auto slow = normal.advance(4800, true);
    const auto quick = fast.advance(4800, true);

    ASSERT_GT(slow, 0.05f);
    EXPECT_NEAR(static_cast<double>(quick), 2.0 * static_cast<double>(slow), 1e-6);
}

TEST(TransportRate, ChangeMidPlayAppliesImmediately) {
    // a taxa e' lida do atomico a cada bloco, nao guardada no play(). se a
    // implementacao a copiasse na partida, este ensaio falhava e o knob SCAN SPEED
    // parecia avariado: mexia e nada mudava ate parar e tocar.
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);
    transport.play();

    const auto before = transport.advance(4800, true);
    transport.setRate(2.0);
    const auto after = transport.advance(4800, true);

    ASSERT_GT(before, 0.05f);
    EXPECT_NEAR(static_cast<double>(after - before),
                2.0 * static_cast<double>(before), 1e-6);
}

TEST(TransportRate, IsClampedAndNaNSafe) {
    // a faixa e' 0,25..4,0, e NaN volta a 1,0 em vez de congelar a cabeca com o
    // gate aberto. a duracao e' o observavel, porque ja' divide pela taxa. os
    // oraculos sao as constantes da spec, nao valores derivados da implementacao.
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(48000);

    ASSERT_NEAR(transport.durationSeconds(), 1.0, 1e-9);

    transport.setRate(100.0);
    EXPECT_NEAR(transport.durationSeconds(), 0.25, 1e-9);

    transport.setRate(0.0);
    EXPECT_NEAR(transport.durationSeconds(), 4.0, 1e-9);

    transport.setRate(-3.0);
    EXPECT_NEAR(transport.durationSeconds(), 4.0, 1e-9);

    transport.setRate(std::nan(""));
    EXPECT_NEAR(transport.durationSeconds(), 1.0, 1e-9);

    transport.setRate(std::numeric_limits<double>::infinity());
    EXPECT_NEAR(transport.durationSeconds(), 1.0, 1e-9);

    // nas bordas passa direto; um epsilon para fora trunca. sem estes dois, um
    // off-by-one no clamp (</<=) passava em silencio.
    transport.setRate(0.25);
    EXPECT_NEAR(transport.durationSeconds(), 4.0, 1e-9);
    transport.setRate(4.0);
    EXPECT_NEAR(transport.durationSeconds(), 0.25, 1e-9);
}

TEST(TransportRate, FloorHoldsOnShortRegionsAtHighRate) {
    // o piso existe porque abaixo de um grao nao ha audicao: 64 bytes duram 1,3 ms
    // naturais, e a 4x dariam 25 ms, que nao completa um grao de 100 ms — o clique
    // que o piso impede. o clamp vale sobre o efectivo, logo a volta fica nos 0,1 s
    // do piso.
    Transport transport;
    transport.prepare(kSampleRate);
    transport.setRegionLength(64);
    transport.setRate(4.0);

    EXPECT_NEAR(transport.durationSeconds(), Transport::kMinSeconds, 1e-9);
}
