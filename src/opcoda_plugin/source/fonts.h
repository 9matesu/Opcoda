#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "OpcodaFonts.h"

namespace opcoda {

struct Fonts {
    // Switzer em tres pesos. Medium e' a voz corrente da interface: rotulos,
    // leituras e valores. Book fica para o texto secundario (dicas, estados
    // vazios) e Semibold para titulos e assinaturas.
    juce::Typeface::Ptr sansBook;
    juce::Typeface::Ptr sansRegular;
    juce::Typeface::Ptr sansStrong;

    enum class Weight { book, medium, semibold };

    // factor global: 15% acima dos valores de desenho, em todo o app.
    static constexpr float kScale {1.15f};

    [[nodiscard]] bool embedded() const noexcept {
        return sansRegular != nullptr;
    }

    [[nodiscard]] juce::Font sans(float height, Weight weight) const {
        const auto* face = &sansRegular;
        if (weight == Weight::book) {
            face = &sansBook;
        } else if (weight == Weight::semibold) {
            face = &sansStrong;
        }
        if (const auto& typeface = *face) {
            return juce::Font {juce::FontOptions {typeface}.withHeight(height * kScale)};
        }
        return fallbackSans(height, weight == Weight::semibold);
    }

    [[nodiscard]] juce::Font sans(float height, bool strong = false) const {
        return sans(height, strong ? Weight::semibold : Weight::medium);
    }

    // Enderecos, bytes e leituras: a mesma familia, sem monoespacada. Os
    // algarismos do Switzer tem avanco fixo de nascenca, e e' isso que alinha a
    // coluna de enderecos -- uma fonte monoespacada daria dois desenhos no mesmo
    // ecrã, que e' exactamente o que a interface nao quer.
    [[nodiscard]] juce::Font mono(float height, bool strong = false) const {
        return sans(height, strong);
    }

private:
    [[nodiscard]] static juce::Font fallbackSans(float height, bool strong) {
        return juce::Font {juce::FontOptions {juce::Font::getDefaultSansSerifFontName(),
                                              height * kScale,
                                              strong ? juce::Font::bold : juce::Font::plain}};
    }
};

inline const Fonts& fonts() {
    static const Fonts loaded = [] {
        const auto load = [](const char* data, int size) {
            return (data != nullptr && size > 0)
                       ? juce::Typeface::createSystemTypefaceFor(data, static_cast<std::size_t>(size))
                       : nullptr;
        };

        return Fonts {load(BinaryData::SwitzerRegular_ttf, BinaryData::SwitzerRegular_ttfSize),
                      load(BinaryData::SwitzerMedium_ttf, BinaryData::SwitzerMedium_ttfSize),
                      load(BinaryData::SwitzerSemibold_ttf, BinaryData::SwitzerSemibold_ttfSize)};
    }();

    return loaded;
}

namespace type {
constexpr float caption {9.5f};
constexpr float body {11.0f};
constexpr float title {12.5f};
constexpr float display {14.0f};
constexpr float data {11.0f};
// kerning do "Opcoda" igual ao de "bloodworm trax | Opcoda": antes estava a 0.24
// e ficava muito mais aberto que a linha de baixo.
constexpr float trackingCaps {0.18f};
constexpr float trackingTitle {0.10f};
constexpr float trackingDisplay {0.06f};

inline juce::Font captionFont(bool strong = true) { return fonts().sans(caption, strong); }
inline juce::Font bodyFont(bool strong = false) { return fonts().sans(body, strong); }
inline juce::Font titleFont() {
    return fonts().sans(title, Fonts::Weight::semibold)
        .withExtraKerningFactor(trackingTitle);
}
inline juce::Font displayFont() {
    return fonts().sans(display, Fonts::Weight::semibold)
        .withExtraKerningFactor(trackingDisplay);
}
inline juce::Font dataFont() { return fonts().mono(data); }
} // namespace type

} // namespace opcoda