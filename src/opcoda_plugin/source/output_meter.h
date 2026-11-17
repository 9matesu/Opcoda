#pragma once

#include "animator.h"
#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>

namespace opcoda {

class OutputMeter : public juce::Component {
public:
    OutputMeter() {
        setName("Output level, left and right");
        setHelpText("Nivel de saida do instrumento, em decibeis, por canal.");
        setInterceptsMouseClicks(false, false);
    }

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
        const auto beforeLeft = left_.value();
        const auto beforeRight = right_.value();
        const auto beforePeakL = peakLeft_;
        const auto beforePeakR = peakRight_;

        left_.tick(deltaSeconds, kFallHalfLife);
        right_.tick(deltaSeconds, kFallHalfLife);
        peakLeft_ = juce::jmax(0.0f, peakLeft_ - deltaSeconds * kPeakFallPerSecond);
        peakRight_ = juce::jmax(0.0f, peakRight_ - deltaSeconds * kPeakFallPerSecond);

        constexpr float kEpsilon {0.0005f};
        if (std::abs(left_.value() - beforeLeft) > kEpsilon ||
            std::abs(right_.value() - beforeRight) > kEpsilon ||
            std::abs(peakLeft_ - beforePeakL) > kEpsilon ||
            std::abs(peakRight_ - beforePeakR) > kEpsilon) {
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        constexpr float kBarWidth {9.0f};
        constexpr float kGap {6.0f};
        constexpr float kLabelHeight {10.0f};

        const auto labelTop = bounds.getBottom() - kLabelHeight;
        const auto barHeight = juce::jmax(kBarWidth * 2.0f, labelTop - bounds.getY() - 2.0f);
        const auto left = bounds.getCentreX() - (kBarWidth * 2.0f + kGap) * 0.5f;

        const auto drawBar = [&](float x, float level, float peak, const char* label) {
            const auto bed = juce::Rectangle<float> {x, bounds.getY(), kBarWidth, barHeight};
            g.setColour(palette::alpha(palette::ivory, 0.12f));
            g.fillRoundedRectangle(bed, 3.0f);

            const auto height = juce::jmax(kBarWidth, barHeight * juce::jlimit(0.0f, 1.0f, level));
            g.setColour(palette::accent);
            g.fillRoundedRectangle(
                juce::Rectangle<float> {x, bed.getBottom() - height, kBarWidth, height}, 3.0f);

            const auto peakY = bed.getBottom() - barHeight * juce::jlimit(0.0f, 1.0f, peak);
            if (barHeight * peak > 2.0f) {
                g.setColour(peak > 0.88f ? palette::meterRed : palette::meterAmber);
                g.fillRect(juce::Rectangle<float> {x - 1.0f, peakY - 0.75f, kBarWidth + 2.0f, 1.5f});
            }

            g.setFont(type::captionFont());
            g.setColour(palette::textOnDarkSub);
            g.drawText(label,
                       juce::Rectangle<float> {x - 2.0f, labelTop, kBarWidth + 4.0f, kLabelHeight},
                       juce::Justification::centred, false);
        };

        drawBar(left, left_.value(), peakLeft_, "L");
        drawBar(left + kBarWidth + kGap, right_.value(), peakRight_, "R");
    }

private:
    static constexpr float kFloorDb {-48.0f};
    static constexpr float kFallHalfLife {0.24f};
    static constexpr float kPeakFallPerSecond {0.55f};
    Eased left_;
    Eased right_;
    float peakLeft_ {0.0f};
    float peakRight_ {0.0f};
    bool primed_ {false};
};

} // namespace opcoda
