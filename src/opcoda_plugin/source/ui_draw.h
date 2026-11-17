#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace opcoda::ui {

// O canvas vive sob uma transform de escala fracionaria. Um rectangulo de 1 px
// logico cai a meio de um pixel fisico e o JUCE antialiasa: a linha fica mais
// fraca, mais grossa ou duplicada. Estas duas funcoes alinham a linha com a
// grelha fisica e dao-lhe exactamente 1 pixel fisico de espessura.
inline float snap(float logical, float scale) noexcept {
    return scale > 0.0f ? std::round(logical * scale) / scale : logical;
}

inline float hairlineWidth(float scale) noexcept {
    return scale > 0.0f ? 1.0f / scale : 1.0f;
}

inline void hairLine(juce::Graphics& g,
                     float x1,
                     float y1,
                     float x2,
                     float y2,
                     float scale,
                     juce::Colour colour) noexcept {
    const auto ax = snap(x1, scale);
    const auto ay = snap(y1, scale);
    const auto bx = snap(x2, scale);
    const auto by = snap(y2, scale);
    const auto w = hairlineWidth(scale);

    g.setColour(colour);
    if (std::abs(ay - by) < 0.01f) {
        const auto left = juce::jmin(ax, bx);
        const auto right = juce::jmax(ax, bx);
        g.fillRect(juce::Rectangle<float> {left, ay - w * 0.5f,
                                           juce::jmax(1.0f, right - left), w});
    } else {
        const auto top = juce::jmin(ay, by);
        const auto bottom = juce::jmax(ay, by);
        g.fillRect(juce::Rectangle<float> {ax - w * 0.5f, top, w,
                                           juce::jmax(1.0f, bottom - top)});
    }
}

} // namespace opcoda::ui
