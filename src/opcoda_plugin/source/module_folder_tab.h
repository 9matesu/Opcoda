#pragma once

#include "fonts.h"
#include "look_and_feel.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// aba de pasta: senta por cima do painel de parametros, e a activa funde-se com
// o fundo dele. icone em vez de palavra — o nome vive no tooltip e no leitor de
// tela, que e' onde um rotulo de 8 px seria inutil.
//
// a lingua e' a das pastas de arquivo: a activa e' da cor do painel, um pixel
// mais alta que as inactivas, sem vinco no pe; as inactivas ficam rebaixadas,
// com a linha de pe e a tinta fraca. o cromo em segundo plano fica por conta
// disto, sem animacao.
class ModuleFolderTab : public juce::Button {
public:
    ModuleFolderTab(const char* icon, const juce::String& spokenName)
        : juce::Button(spokenName) {
        setName(spokenName);
        setTooltip(spokenName);
        getProperties().set("uiIcon", icon);
        setClickingTogglesState(true);
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

        // glifo: o desenho e' o rotulo. activa em carvao sobre a chapa clara;
        // inactiva na tinta sub, que e' o que ela ja' era em palavra.
        juce::Colour ink = on ? palette::charcoalDeep : palette::textSub;
        if (!on && (highlighted || down)) {
            ink = palette::textDark;
        }
        const auto kind = getProperties()["uiIcon"].toString();
        const auto iconArea =
            juce::Rectangle<float> {bounds.getX(), bounds.getY(), bounds.getWidth(),
                                    height}
                .withSizeKeepingCentre(bounds.getWidth() * 0.42f, height * 0.42f);
        if (!OpcodaLookAndFeel::paintGlyph(g, kind, iconArea, ink, on)) {
            g.setFont(fonts().sans(10.0f, on));
            g.setColour(ink);
            g.drawText(getButtonText(), bounds, juce::Justification::centred, false);
        }

        if (hasKeyboardFocus(true)) {
            g.setColour(palette::charcoalDeep);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 3.0f);
            g.setColour(palette::focusRing);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 5.0f, 1.5f);
        }
    }
};

} // namespace opcoda
