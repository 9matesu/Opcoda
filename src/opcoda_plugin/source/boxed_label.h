#pragma once

#include "fonts.h"
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

    // As duas fabricas de fonte de toda a interface. Tudo o que escreve texto
    // passa por aqui, e nao ha um terceiro sitio — ver fonts.h para a razao de a
    // fonte vir por Typeface::Ptr e nao por nome.
    [[nodiscard]] static juce::Font monoFont(float height, bool strong = false) {
        return fonts().monospace(height, strong);
    }

    [[nodiscard]] static juce::Font sansFont(float height, bool strong = false) {
        return fonts().sans(height, strong);
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