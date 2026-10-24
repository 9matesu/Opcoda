#pragma once

#include "palette.h"
#include "vu_scale.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// ===== Faixa de fundo =====
//
// Encerra a faixa escura do rodape entre as supericies de metal. Componente
// opaco a mais para o editor nao repetir a formula do layout dentro de paint()
// — o paint do editor continua a pintar so o metal.
class BandStrip : public juce::Component {
public:
    BandStrip(juce::Colour background, juce::Colour topEdge)
        : background_(background), topEdge_(topEdge) {
        setOpaque(true);
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(background_);
        if (topEdge_.isTransparent()) {
            return;
        }
        g.setColour(topEdge_);
        g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
    }

private:
    juce::Colour background_;
    juce::Colour topEdge_;
};

// ===== Medidor de saida =====
//
// Dois fileiras de oito segmentos, verde ate' o penultimo e ambar no topo,
// apagados com fundo #334155.
//
// O nivel vem da telemetria de pico (peakDb) do Processor — o mesmo dado que o
// PEAK do rodape do display escreve. O Opcoda e' mono no nucleo; as duas linhas
// mostram o mesmo pico e por isso o rotulo e' "OUT", nao "L/R": duas cores
// diferentes para a mesma leitura seria fachada.
class VuMeter : public juce::Component {
public:
    VuMeter() { setInterceptsMouseClicks(false, false); }

    void setLevelDb(float peakDb) {
        // A escala (silencio -> 0, -60 dB -> 0, 0 dB -> 8) mora em vu_scale.h,
        // sem JUCE, para o teste a poder chamar. Quantizar antes do repaint
        // evita acordar o paint a cada fraqueza de dB da leitura.
        const auto lit = vuSegmentsForPeak(peakDb);
        if (lit != lit_) {
            lit_ = lit;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        // w-1.5 h-2 do mock: 6 x 8 px, com 2 px entre segmentos e 4 entre as
        // duas fileiras.
        constexpr float kSegmentWidth {6.0f};
        constexpr float kSegmentHeight {8.0f};
        constexpr float kGapX {2.0f};
        constexpr float kGapY {4.0f};

        const auto totalWidth =
            static_cast<float>(kVuSegments) * kSegmentWidth +
            static_cast<float>(kVuSegments - 1) * kGapX;
        const auto totalHeight = kSegmentHeight * 2.0f + kGapY;

        const auto originX = juce::jmax(bounds.getX(), (bounds.getWidth() - totalWidth) * 0.5f);
        const auto originY = juce::jmax(bounds.getY(),
                                        (bounds.getHeight() - totalHeight) * 0.5f);

        for (int row = 0; row < 2; ++row) {
            for (int segment = 0; segment < kVuSegments; ++segment) {
                const auto x = originX +
                               static_cast<float>(segment) * (kSegmentWidth + kGapX);
                const auto y = originY + static_cast<float>(row) * (kSegmentHeight + kGapY);
                const auto cell = juce::Rectangle<float> {x, y, kSegmentWidth, kSegmentHeight};

                g.setColour(juce::Colour {0xff334155});
                if (segment < lit_) {
                    g.setColour(segment >= kVuSegments - 1 ? palette::accentWarn : palette::ok);
                }
                g.fillRoundedRectangle(cell, 1.0f);
            }
        }
    }

private:
    int lit_ {0};
};

} // namespace opcoda
