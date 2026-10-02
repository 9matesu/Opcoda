#include <gtest/gtest.h>

#include "opcoda_core/entropy/shannon_entropy.h"

#include <array>
#include <cstdint>
#include <vector>

using opcoda::entropy::shannonBitsPerByte;
using opcoda::entropy::shannonCurve;

TEST(ShannonEntropy, EmptyInputIsZero) {
    EXPECT_DOUBLE_EQ(shannonBitsPerByte(nullptr, 0), 0.0);
}

TEST(ShannonEntropy, SingleRepeatedByteIsZero) {
    // Alvo do artigo: bloco de 1024 bytes identicos produz H = 0, porque um
    // unico simbolo concentra toda a probabilidade.
    const std::vector<std::uint8_t> data(1024, std::uint8_t{0x5A});
    EXPECT_NEAR(shannonBitsPerByte(data.data(), data.size()), 0.0, 1e-12);
}

TEST(ShannonEntropy, UniformDistributionIsEightBits) {
    // Alvo do artigo: 256 valores equiprovaveis atingem o maximo teorico.
    std::array<std::uint8_t, 256> data {};
    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<std::uint8_t>(i);
    }
    EXPECT_NEAR(shannonBitsPerByte(data.data(), data.size()), 8.0, 1e-9);
}

TEST(ShannonEntropy, TwoEqualSymbolsIsOneBit) {
    const std::uint8_t data[] = {std::uint8_t{0}, std::uint8_t{1}};
    EXPECT_NEAR(shannonBitsPerByte(data, sizeof(data)), 1.0, 1e-12);
}

TEST(ShannonEntropy, SkewedDistributionIsBetweenExtremes) {
    // 3/4 de um simbolo, 1/4 de outro: H = -0.75*log2(0.75) - 0.25*log2(0.25)
    std::vector<std::uint8_t> data(1000, std::uint8_t{0x00});
    for (std::size_t i = 0; i < 250; ++i) {
        data[i] = std::uint8_t{0xFF};
    }
    const double h = shannonBitsPerByte(data.data(), data.size());
    EXPECT_GT(h, 0.7);
    EXPECT_LT(h, 0.9);
}

TEST(ShannonEntropy, NeverExceedsTheoreticalMaximum) {
    // propriedade de seguranca: para qualquer entrada, H fica em [0, 8]
    std::vector<std::uint8_t> data(4096);
    std::uint32_t state = 12345;
    for (auto& byte : data) {
        state = (state * 1103515245u) + 12345u;
        byte = static_cast<std::uint8_t>(state >> 16);
    }
    const double h = shannonBitsPerByte(data.data(), data.size());
    EXPECT_GE(h, 0.0);
    EXPECT_LE(h, 8.0);
}

TEST(ShannonEntropy, OffsetVersionMatchesSlice) {
    // A versao com offset tem de concordar com o recorte equivalente.
    // 16 bytes de um unico simbolo (H = 0) seguidos de 8 simbolos distintos
    // um de cada vez (H = 3, o maximo de log2(8)). O corte no offset 16 separa
    // as duas partes, entao o offset muda o resultado de 1,0 para 3,0.
    std::vector<std::uint8_t> data(16, std::uint8_t{0x7F});
    const std::uint8_t distinct[] = {1, 2, 3, 4, 5, 6, 7, 8};
    data.insert(data.end(), std::begin(distinct), std::end(distinct));

    const auto fromOffset = shannonBitsPerByte(data.data(), data.size(), 16);
    const std::vector<std::uint8_t> tail(data.begin() + 16, data.end());
    EXPECT_NEAR(fromOffset, shannonBitsPerByte(tail.data(), tail.size()), 1e-12);
    EXPECT_NEAR(fromOffset, 3.0, 1e-9);

    // 24 bytes: 16 iguais a 0x7F e 8 distintos, um de cada vez. p = 2/3 e
    // 1/12 por simbolo, o que da H = -(2/3)log2(2/3) - 8*(1/12)log2(1/12) = 1,918.
    const auto whole = shannonBitsPerByte(data.data(), data.size());
    EXPECT_NEAR(whole, 1.9182958340544896, 1e-9);
    EXPECT_GT(std::abs(whole - fromOffset), 1.0);
}

TEST(ShannonEntropy, CurveCoversWholeInput) {
    // 4096 bytes com janela de 2048 sao duas janelas, como o pipeline espera.
    const std::vector<std::uint8_t> data(4096, std::uint8_t{0x11});
    std::array<double, 8> curve {};
    const auto written = shannonCurve(data.data(), data.size(), 2048, curve.data(), curve.size());
    EXPECT_EQ(written, 2u);
}

TEST(ShannonEntropy, CurveRespectsOutputCapacity) {
    const std::vector<std::uint8_t> data(8192, std::uint8_t{0x22});
    std::array<double, 3> curve {};
    const auto written = shannonCurve(data.data(), data.size(), 512, curve.data(), curve.size());
    EXPECT_EQ(written, 3u);
}

TEST(ShannonEntropy, CurveHandlesTailShorterThanWindow) {
    // A ultima janela pode ser menor que a janela cheia; nao pode estourar.
    const std::vector<std::uint8_t> data(2500, std::uint8_t{0x33});
    std::array<double, 8> curve {};
    const auto written = shannonCurve(data.data(), data.size(), 2048, curve.data(), curve.size());
    EXPECT_EQ(written, 2u);
}

TEST(ShannonEntropy, RejectsDegenerateArguments) {
    std::array<double, 2> curve {};
    const std::vector<std::uint8_t> data(16, std::uint8_t{0});
    EXPECT_EQ(shannonCurve(nullptr, 16, 2048, curve.data(), curve.size()), 0u);
    EXPECT_EQ(shannonCurve(data.data(), 16, 0, curve.data(), curve.size()), 0u);
    EXPECT_EQ(shannonCurve(data.data(), 16, 2048, nullptr, 2), 0u);
}
