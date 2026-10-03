#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Rotulo com fundo. Serve tanto para a caixa de valor dos knobs (valBox) quanto
// para os chips da faixa de arquivo (subPanel), que e' a mesma coisa com outra
// cor.
class BoxedLabel : public juce::Label {
public:
    explicit BoxedLabel(juce::Colour background, juce::Colour outline)
        : background_(background), outline_(outline) {
        setJustificationType(juce::Justification::centred);
        setInterceptsMouseClicks(false, false);
        // Numero e' sempre monoespacado e sempre alinhado: e' o que faz a
        // leitura parecer instrumento e nao pagina web.
        setFont(monoFont(10.0f));
    }

    static juce::Font monoFont(float height) {
        auto font = juce::Font {juce::FontOptions {height, juce::Font::plain}};
        font.setTypefaceName(getDefaultMonospacedTypefaceName());
        return font;
    }

    static const juce::String& getDefaultMonospacedTypefaceName() {
        return juce::Font::getDefaultMonospacedFontName();
    }

    static juce::Font sansFont(float height, bool bold = false) {
        auto font = juce::Font {juce::FontOptions {height,
                                                   bold ? juce::Font::bold : juce::Font::plain}};
        font.setTypefaceName(juce::Font::getDefaultSansSerifFontName());
        return font;
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(background_);
        g.fillRoundedRectangle(bounds, 1.5f);
        g.setColour(palette::alpha(outline_, 0.55f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 1.5f, 1.0f);
        juce::Label::paint(g);
    }

private:
    juce::Colour background_;
    juce::Colour outline_;
};

} // namespace opcoda