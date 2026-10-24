#pragma once

#include <algorithm>

namespace opcoda {

// Duas fileiras de oito segmentos. A constante vive aqui para o teste puro e o
// desenho nao poderem discordar sobre quantos segmentos existem.
inline constexpr int kVuSegments {8};

// Fundo da escala. Abaixo disto o medidor fica apagado.
inline constexpr float kVuFloorDb {-60.0f};

// Valor publicado pela telemetria quando o bloco nao tem sinal. Nao pode ser
// -1.0: o limitador corta em -1 dB, e um pico real ficaria indistinguivel do
// "nada a sair" — era o que acendia os oito segmentos. -100 esta' abaixo do
// fundo da escala, e o `static_assert` abaixo impede a regressao.
inline constexpr float kSilenceDb {-100.0f};

static_assert(kSilenceDb < kVuFloorDb, "o sentinela de silencio tem de cair abaixo do fundo");

// Mapeia o pico em dB para segmentos acesos, de 0 a kVuSegments.
//
// A regra e' degrau: -60 dB e abaixo da' zero, 0 dB da' os oito, e o
// arredondamento poe cada segmento a meio caminho do seguinte. O corte no fundo
// tambem apanha NaN, que nao pode acender nada.
constexpr int vuSegmentsForPeak(float peakDb) {
    if (!(peakDb >= kVuFloorDb)) {
        return 0;
    }
    const auto level = std::clamp((peakDb - kVuFloorDb) / (0.0f - kVuFloorDb), 0.0f, 1.0f);
    return static_cast<int>(level * static_cast<float>(kVuSegments) + 0.5f);
}

} // namespace opcoda
