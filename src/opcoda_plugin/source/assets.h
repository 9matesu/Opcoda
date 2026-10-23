#pragma once

#include "OpcodaAssets.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda::assets {

// Fontes do mock. Chakra Petch assina o wordmark; JetBrains Mono carrega os
// numeros do dump e das leituras; Inter Display (fonts.h) fica nos rotulos.
//
// O Typeface e' criado uma vez e partilhado. O createSystemTypefaceFor faz o
// parse do TTF na primeira chamada; repeti-lo por knob seria parse do mesmo
// ficheiro seis vezes.
namespace fonts {
namespace detail {
inline juce::Typeface::Ptr load(const char* data, int size) {
    return juce::Typeface::createSystemTypefaceFor(data, static_cast<std::size_t>(size));
}
} // namespace detail

inline const juce::Typeface::Ptr& tech() {
    static const juce::Typeface::Ptr typeface {
        detail::load(BinaryData::ChakraPetchBold_ttf, BinaryData::ChakraPetchBold_ttfSize)};
    return typeface;
}

inline const juce::Typeface::Ptr& monoRegular() {
    static const juce::Typeface::Ptr typeface {
        detail::load(BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize)};
    return typeface;
}

inline const juce::Typeface::Ptr& monoBold() {
    static const juce::Typeface::Ptr typeface {
        detail::load(BinaryData::JetBrainsMonoBold_ttf, BinaryData::JetBrainsMonoBold_ttfSize)};
    return typeface;
}

// FontOptions com Typeface ignora o nome e os flags de estilo: o peso ja' vem
// do ficheiro. Pedir bold a um Typeface regular nao engrossa, cai no regular.
inline juce::Font tech(float height) {
    return juce::Font {juce::FontOptions {tech()}.withHeight(height)};
}

inline juce::Font mono(float height, bool bold = false) {
    return juce::Font {juce::FontOptions {bold ? monoBold() : monoRegular()}.withHeight(height)};
}

} // namespace fonts

} // namespace opcoda::assets
