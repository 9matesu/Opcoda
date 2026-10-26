#pragma once

#include "fonts.h"
#include "look_and_feel.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// tecla de navegacao de modulo, sentada por cima do painel de parametros.
//
// **icone e nome, rectangular.** era uma pasta de 44 px com o glifo so', e o nome
// vivia no tooltip: com seis modulos, um glifo de 13 px nao dizia "envelope" a
// ninguem. agora a tecla divide a largura toda com as vizinhas e le-se como o que
// e' -- navegacao, na mesma lingua das teclas da rail.
//
// a lingua continua a das pastas: a activa e' da cor do painel e funde-se com ele
// pela cauda de 6 px; as inactivas ficam rebaixadas, com a linha de pe e a tinta
// fraca.
class ModuleFolderTab : public juce::Button {
public:
    ModuleFolderTab(const char* icon, const juce::String& spokenName)
        : juce::Button(spokenName) {
        setName(spokenName);
        setTooltip(spokenName);
        getProperties().set("uiIcon", icon);
        setClickingTogglesState(true);
    }

    void resized() override {
        // a largura do nome depende da fonte e do texto, nao do rectangulo: o
        // par glifo+nome e' centrado como um bloco. mede-se aqui e nao em paint,
        // que corre a cada rato por cima.
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(nameFont(), getButtonText(), 0.0f, 0.0f);
        textWidth_ = glyphs.getBoundingBox(0, -1, false).getWidth();
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto on = getToggleState();

        // corpo: so' os cantos de cima arredondam; o pe encosta no painel. a
        // activa desce 2 px a mais, e' ela que esta' da frente.
        const auto height = on ? bounds.getHeight() : bounds.getHeight() - 2.0f;
        juce::Path shape;
        shape.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), height,
                                  5.0f, 5.0f, true, true, false, false);

        auto fill = on ? palette::faceplateLight
                       : palette::alpha(palette::faceplateDark, 0.24f);
        if (down) {
            fill = on ? palette::faceplate : palette::alpha(palette::faceplateDark, 0.40f);
        } else if (highlighted && !on) {
            fill = palette::alpha(palette::faceplateDark, 0.36f);
        }
        g.setColour(fill);
        g.fillPath(shape);

        // vinco do pe das inactivas: e' o que as le como pastas atras da activa.
        if (!on) {
            g.setColour(palette::alpha(palette::faceplateDark, 0.70f));
            g.drawLine(bounds.getX() + 3.0f, bounds.getY() + height - 0.5f,
                       bounds.getRight() - 3.0f, bounds.getY() + height - 0.5f, 1.0f);
        } else {
            // filete da activa: o mesmo ambar das teclas, agora na base da aba.
            g.setColour(palette::alpha(palette::accent, 0.85f));
            g.drawLine(bounds.getX() + 4.0f, bounds.getBottom() - 1.0f,
                       bounds.getRight() - 4.0f, bounds.getBottom() - 1.0f, 2.0f);
        }

        juce::Colour ink = on ? palette::charcoalDeep : palette::textSub;
        if (!on && (highlighted || down)) {
            ink = palette::textDark;
        }

        // glifo em cima do nome? nao: lado a lado, centrados em bloco. a altura
        // da tecla da' para os dois e a leitura faz-se de uma vez.
        constexpr float kGlyph {15.0f};
        constexpr float kGap {7.0f};
        const auto total = kGlyph + kGap + textWidth_;
        const auto left = bounds.getCentreX() - total * 0.5f;
        const auto glyphBox =
            juce::Rectangle<float> {left, bounds.getCentreY() - kGlyph * 0.5f, kGlyph, kGlyph};
        const auto kind = getProperties()["uiIcon"].toString();
        if (!OpcodaLookAndFeel::paintGlyph(g, kind, glyphBox, ink, on)) {
            // sem glifo desenhado, o nome toma conta: nenhuma tecla fica vazia.
            g.setFont(nameFont());
            g.setColour(ink);
            g.drawText(getButtonText(),
                       juce::Rectangle<float> {left, bounds.getY(), total, height},
                       juce::Justification::centredLeft, false);
        } else {
            g.setFont(nameFont());
            g.setColour(ink);
            g.drawText(getButtonText(),
                       juce::Rectangle<float> {left + kGlyph + kGap, bounds.getY(),
                                               textWidth_, height},
                       juce::Justification::centredLeft, false);
        }

        if (hasKeyboardFocus(true)) {
            g.setColour(palette::charcoalDeep);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 3.0f);
            g.setColour(palette::focusRing);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 1.5f);
        }
    }

private:
    static juce::Font nameFont() {
        return fonts().sans(9.5f, true).withExtraKerningFactor(0.14f);
    }

    float textWidth_ {0.0f};
};

} // namespace opcoda
