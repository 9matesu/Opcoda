#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// LookAndFeel do Opcoda. Existe para uma coisa so: o desenho do knob e do
// botao.
//
// Os knobs sao juce::Slider de verdade, nao desenho customizado. Isso nao e
// Repository: e o que da foco por teclado, ajuste com setas e
// AccessibilityHandler de graca, e sao exatamente os criterios que a
// docs/10-acessibilidade-w3c.md exige.
class OpcodaLookAndFeel : public juce::LookAndFeel_V4 {
public:
    OpcodaLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, palette::chassis);

        // O valor fica em um Label proprio, nao na TextBox do Slider, para o
        // mock poder ter a caixa valBox com fonte monoespacada.
        setColour(juce::Slider::textBoxTextColourId, palette::textDark);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

        setColour(juce::Label::textColourId, palette::textDark);

        setColour(juce::TextButton::buttonColourId, palette::panelBg);
        setColour(juce::TextButton::buttonOnColourId, palette::accent);
        setColour(juce::TextButton::textColourOffId, palette::textDark);
        setColour(juce::TextButton::textColourOnId, palette::textDark);
    }

    // Knob: anel de 3 px com gradiente vertical, face com realce no topo, sombra
    // projetada e indicador laranja. O arco de valor fica como trilha fina
    // atras do ponteiro, nao como anel grosso: o mock usa indicador e nao
    // anel preenchido.
    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float angle = rotaryStartAngle +
                            sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Trilha do arco de valor, para dar escala sem roubar o desenho.
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - 3.0f, radius - 3.0f,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(palette::alpha(palette::chassisBorder, 0.45f));
        g.strokePath(track, juce::PathStrokeType {1.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded});

        // Sombra: um disco escuro deslocado, antes do corpo. E o que da a
        // sensacao de altura em vez de circulo chapado.
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillEllipse(bounds.reduced(radius * 0.12f).translated(0.0f, 1.5f));

        // Corpo: gradiente vertical de claro em cima para escuro embaixo, que
        // e' a leitura de "cone de luz" de um knob de plastico.
        juce::ColourGradient body(palette::chassis, centre.x, centre.y - radius,
                                  palette::chassisDark, centre.x, centre.y + radius, false);
        g.setGradientFill(body);
        g.fillEllipse(bounds);

        // Realce especular: um arco fino claro no topo da face.
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        const float inset = radius * 0.18f;
        juce::Path specular;
        specular.addCentredArc(centre.x, centre.y + inset, radius - inset, radius - inset,
                               0.0f, juce::MathConstants<float>::pi * 0.15f,
                               juce::MathConstants<float>::pi * 0.85f, true);
        g.strokePath(specular, juce::PathStrokeType {1.0f});

        // Borda de 1 px: e o que separa o knob do painel em fundo claro.
        g.setColour(palette::alpha(palette::chassisBorder, 0.85f));
        g.drawEllipse(bounds, 1.0f);

        // Indicador. Arredondado nas pontas para nao parecer um risco.
        const auto inner = juce::jmax(2.0f, radius * 0.18f);
        const auto outer = radius - 2.5f;
        const auto tipX = centre.x + std::sin(angle) * outer;
        const auto tipY = centre.y - std::cos(angle) * outer;
        const auto tailX = centre.x + std::sin(angle) * inner;
        const auto tailY = centre.y - std::cos(angle) * inner;

        g.setColour(palette::accent);
        g.drawLine(tailX, tailY, tipX, tipY, 2.5f);

        // Anel de foco. Sem ele, quem navega por teclado nao sabe qual knob
        // esta' ativo, e o criterio 2.4.7 do WCAG 2.2 pede foco visivel.
        if (slider.hasKeyboardFocus(true)) {
            g.setColour(palette::accent);
            g.drawEllipse(bounds.reduced(-3.0f), 2.0f);
        }
    }

    // Botao com bisel: borda clara em cima e escura embaixo, que e' a leitura
    // de tecla fisica. Pressionado inverte o bisel e afunda 1 px.
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto base = shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.12f)
                                                        : backgroundColour;

        juce::ColourGradient fill(base.brighter(0.10f), bounds.getTopLeft(),
                                  base.darker(0.06f), bounds.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 2.0f);

        g.setColour(palette::alpha(palette::chassisBorder,
                                       shouldDrawButtonAsDown ? 0.95f : 0.65f));
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

        if (shouldDrawButtonAsDown) {
            g.setColour(juce::Colours::black.withAlpha(0.12f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), 1.5f);
        }
    }

    // A assinatura nao leva cor: o texto vem do TextButton, que le a cor no
    // LookAndFeel. Encaixar a largura nao e' necessario porque o botao LOAD tem
    // largura fixa.
    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool shouldDrawButtonAsDown) override {
        g.setFont(juce::Font {juce::FontOptions {11.0f, juce::Font::bold}});
        g.setColour(button.findColour(juce::TextButton::textColourOffId)
                        .withAlpha(shouldDrawButtonAsDown ? 0.9f : 1.0f));
        g.drawText(button.getButtonText(),
                   button.getLocalBounds().reduced(2, 0),
                   juce::Justification::centred, false);
    }
};

} // namespace opcoda