#pragma once

#include "assets.h"
#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Rotulo com fundo. Serve tanto para os chips da faixa de ficheiro (chipBg)
// quanto para as leituras do display (posChipBg), que sao a mesma coisa com
// outra cor e outro raio.
class BoxedLabel : public juce::Label {
public:
    explicit BoxedLabel(juce::Colour background,
                        juce::Colour outline,
                        float radius = 1.5f,
                        float outlineOpacity = 0.55f)
        : background_(background), outline_(outline), radius_(radius),
          outlineOpacity_(outlineOpacity) {
        setJustificationType(juce::Justification::centred);
        setInterceptsMouseClicks(false, false);
        // Numero e' sempre monoespacado e sempre alinhado: e' o que faz a
        // leitura parecer instrumento e nao pagina web. A fonte e' a JetBrains
        // Mono do mock, embutida no binario.
        setFont(monoFont(10.0f));
    }

    static juce::Font monoFont(float height, bool bold = false) {
        return assets::fonts::mono(height, bold);
    }

    static juce::Font sansFont(float height, bool bold = false) {
        return fonts().sans(height, bold);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        if (!background_.isTransparent()) {
            g.setColour(background_);
            g.fillRoundedRectangle(bounds, radius_);
        }

        // isTransparent() antes do alpha: `withAlpha` define o alpha em vez de
        // multiplicar, e um outline transparentBlack transformado com alpha()
        // vira um contorno preto a 55%. Era isso que punha uma caixa a volta de
        // cada rotulo sem caixa.
        if (!outline_.isTransparent()) {
            g.setColour(palette::alpha(outline_, outlineOpacity_));
            g.drawRoundedRectangle(bounds.reduced(0.5f), radius_, 1.0f);
        }
        juce::Label::paint(g);
    }

private:
    juce::Colour background_;
    juce::Colour outline_;
    float radius_;
    float outlineOpacity_;
};

} // namespace opcoda