#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "OpcodaFonts.h"

namespace opcoda {

struct Fonts {
    juce::Typeface::Ptr sansRegular;
    juce::Typeface::Ptr sansStrong;

    [[nodiscard]] bool embedded() const noexcept {
        return sansRegular != nullptr;
    }

    [[nodiscard]] juce::Font sans(float height, bool strong = false) const {
        if (const auto& face = strong ? sansStrong : sansRegular) {
            return juce::Font {juce::FontOptions {face}.withHeight(height)};
        }
        return fallbackSans(height, strong);
    }

    // coluna ASM: avanco fixo, o que faz a instrucao alinhar linha a linha.
    [[nodiscard]] juce::Font mono(float height, bool strong = false) const {
        return juce::Font {juce::FontOptions {juce::Font::getDefaultMonospacedFontName(),
                                              height,
                                              strong ? juce::Font::bold : juce::Font::plain}};
    }

private:
    [[nodiscard]] static juce::Font fallbackSans(float height, bool strong) {
        return juce::Font {juce::FontOptions {juce::Font::getDefaultSansSerifFontName(), height,
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

        return Fonts {load(BinaryData::InterDisplayMedium_ttf, BinaryData::InterDisplayMedium_ttfSize),
                       load(BinaryData::InterDisplaySemiBold_ttf,
                            BinaryData::InterDisplaySemiBold_ttfSize)};
    }();

    return loaded;
}

namespace type {
constexpr float caption {9.5f};
constexpr float body {11.0f};
constexpr float title {12.5f};
constexpr float display {14.0f};
constexpr float data {11.0f};
constexpr float trackingCaps {0.10f};
constexpr float trackingTitle {0.16f};
constexpr float trackingDisplay {-0.02f};

inline juce::Font captionFont(bool strong = true) { return fonts().sans(caption, strong); }
inline juce::Font bodyFont(bool strong = false) { return fonts().sans(body, strong); }
inline juce::Font titleFont() {
    return fonts().sans(title, true).withExtraKerningFactor(trackingTitle);
}
inline juce::Font displayFont() {
    return fonts().sans(display, true).withExtraKerningFactor(trackingDisplay);
}
inline juce::Font dataFont() { return fonts().mono(data); }
} // namespace type

} // namespace opcoda