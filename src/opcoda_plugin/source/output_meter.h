#pragma once

#include "animator.h"
#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>

namespace opcoda {

// medidor de saida stereo, no canto inferior direito, como o mestre de uma mesa.
//
// dois lados, escada de segmentos: verde no corpo, ambar perto do topo, vermelho
// no tecto. a leitura e' a mesma do pico que o rodape mostrava em texto, mas com
// forma e sem ocupar uma caixa de leitura.
//
// **sobe depressa e cai devagar.** um medidor simetrico pisca ao ritmo dos graos e
// nao se le; a queda lenta e' o que da a sensacao de instrumento. a meia-vida de
// queda e' medida em segundos, por isso o medidor nao muda de velocidade com o
// monitor.
class OutputMeter : public juce::Component {
public:
    OutputMeter() {
        setName("Output level, left and right");
        setHelpText("Nivel de saida do instrumento, em decibeis, por canal.");
        setInterceptsMouseClicks(false, false);
    }

    // niveis em dB do ultimo bloco. -48 e' o piso da escala: e' o fundo que
    // interessa num instrumento, e a escala fica legivel sem rotulos a chocar.
    void setLevels(float leftDb, float rightDb) {
        const auto toUnit = [](float db) {
            return std::clamp((db - kFloorDb) / -kFloorDb, 0.0f, 1.0f);
        };

        const auto left = toUnit(leftDb);
        const auto right = toUnit(rightDb);

        if (!primed_) {
            left_.jumpTo(left);
            right_.jumpTo(right);
            primed_ = true;
        } else {
            left_.set(left);
            right_.set(right);
        }

        peakLeft_ = juce::jmax(peakLeft_, left);
        peakRight_ = juce::jmax(peakRight_, right);
    }

    // avanca a queda. chamado pelo timer do editor, nunca do paint.
    void tick(float deltaSeconds) {
        left_.tick(deltaSeconds, kFallHalfLife);
        right_.tick(deltaSeconds, kFallHalfLife);
        peakLeft_ = juce::jmax(0.0f, peakLeft_ - deltaSeconds * kPeakFallPerSecond);
        peakRight_ = juce::jmax(0.0f, peakRight_ - deltaSeconds * kPeakFallPerSecond);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();

        // chapa do mestre: carvao afundado, moldura de latao.
        g.setColour(palette::charcoalDeep);
        g.fillRoundedRectangle(bounds.reduced(0.5f), 4.0f);
        g.setColour(palette::alpha(palette::knobEdge, 0.55f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

        constexpr float titleHeight {14.0f};
        g.setFont(fonts().sans(8.5f, true));
        g.setColour(palette::textOnDarkSub);
        g.drawText("OUTPUT", bounds.reduced(8.0f, 0.0f).withHeight(titleHeight + 2.0f),
                   juce::Justification::centredLeft, false);

        // duas colunas de segmentos, com as letras por baixo e a escala a' direita.
        constexpr float labelHeight {12.0f};
        const auto track = bounds.reduced(8.0f, 4.0f)
                               .withTrimmedTop(titleHeight)
                               .withTrimmedBottom(labelHeight);
        constexpr float columnWidth {20.0f};
        constexpr float columnGap {6.0f};
        const auto columnsWidth = columnWidth * 2.0f + columnGap;
        const auto leftX = track.getX() + 2.0f;

        const auto drawColumn = [&](float x, float level, float peak) {
            const auto lit = static_cast<int>(std::ceil(level * kSegments));
            for (int i = 0; i < kSegments; ++i) {
                const auto segmentBottom = track.getBottom() -
                                           static_cast<float>(i) * track.getHeight() /
                                               static_cast<float>(kSegments);
                const auto segmentTop = segmentBottom - track.getHeight() /
                                                          static_cast<float>(kSegments) +
                                                          1.5f;
                const auto r = juce::Rectangle<float> {
                    x, segmentTop, columnWidth, segmentBottom - segmentTop};

                // i conta de baixo para cima. o vermelho e' o tecto, nao o fundo:
                // a primeira versao comparava pelo indice invertido e acendia a
                // base em vermelho, o que lia como um medidor ao contrario.
                juce::Colour litColour = palette::meterGreen;
                if (i >= kSegments - kRedSegments) {
                    litColour = palette::meterRed;
                } else if (i >= kSegments - kRedSegments - kAmberSegments) {
                    litColour = palette::meterAmber;
                }

                const auto isLit = i < lit;
                g.setColour(isLit ? litColour
                                  : palette::alpha(litColour, 0.14f));
                g.fillRoundedRectangle(r, 1.0f);
            }

            // pico: um filete branco que cai. e' a unica marca que nao depende de
            // cor, para o tecto se ler tambem sem distinguir verde de vermelho.
            if (peak > 0.01f) {
                const auto y = track.getBottom() - peak * track.getHeight();
                g.setColour(palette::alpha(palette::ivory, 0.85f));
                g.fillRect(juce::Rectangle<float> {x, y - 1.0f, columnWidth, 1.5f});
            }
        };

        drawColumn(leftX, left_.value(), peakLeft_);
        drawColumn(leftX + columnWidth + columnGap, right_.value(), peakRight_);

        // escala de dB a' direita: 0 no tecto, -48 em baixo. cada rotulo assenta no
        // seu traco e o ultimo de cima e' empurrado para dentro da pista -- era o
        // "0" a sair por cima da caixa que fazia a escala parecer desalinhada.
        g.setFont(fonts().sans(8.0f));
        const auto scaleX = leftX + columnsWidth + 7.0f;
        constexpr float kLabelHeight {9.0f};
        for (const auto db : {0.0f, -12.0f, -24.0f, -36.0f, -48.0f}) {
            const auto unit = std::clamp((db - kFloorDb) / -kFloorDb, 0.0f, 1.0f);
            const auto y = track.getBottom() - unit * track.getHeight();
            const auto labelY = juce::jlimit(track.getY(), track.getBottom() - kLabelHeight,
                                             y - kLabelHeight * 0.5f);

            g.setColour(palette::alpha(palette::textOnDarkSub, 0.7f));
            g.fillRect(juce::Rectangle<float> {scaleX, y - 0.5f, 3.0f, 1.0f});
            g.setColour(palette::alpha(palette::textOnDarkSub, 0.9f));
            g.drawText(juce::String {static_cast<int>(db)},
                       juce::Rectangle<float> {scaleX + 5.0f, labelY, 24.0f,
                                               kLabelHeight},
                       juce::Justification::centredLeft, false);
        }

        g.setFont(fonts().sans(8.0f, true));
        g.setColour(palette::textOnDarkSub);
        const auto labelsY = bounds.getBottom() - labelHeight;
        g.drawText("L", juce::Rectangle<float> {leftX, labelsY, columnWidth, labelHeight},
                   juce::Justification::centred, false);
        g.drawText("R", juce::Rectangle<float> {leftX + columnWidth + columnGap, labelsY,
                                                columnWidth, labelHeight},
                   juce::Justification::centred, false);
    }

private:
    static constexpr float kFloorDb {-48.0f};
    static constexpr float kFallHalfLife {0.24f};
    static constexpr float kPeakFallPerSecond {0.55f};
    static constexpr int kSegments {16};
    static constexpr int kRedSegments {2};
    static constexpr int kAmberSegments {3};

    Eased left_;
    Eased right_;
    float peakLeft_ {0.0f};
    float peakRight_ {0.0f};
    bool primed_ {false};
};

} // namespace opcoda
