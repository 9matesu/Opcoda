#pragma once

#include "assets.h"
#include "boxed_label.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// LookAndFeel do Opcoda. Existe para dois desenhos: o knob e o botao. Todo o
// resto fica no LookAndFeel padrao.
//
// Os knobs sao juce::Slider de verdade, nao desenho customizado. Isso nao e'
// padrao: e o que da foco por teclado, ajuste com setas e
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

        setColour(juce::TextButton::buttonColourId, palette::metalButton);
        setColour(juce::TextButton::buttonOnColourId, palette::accent);
        setColour(juce::TextButton::textColourOffId, palette::textOnDark);
        setColour(juce::TextButton::textColourOnId, palette::textOnDark);
    }

// Knob: corpo creme do mock, desenhado em vetor. O mock nao tem arco de valor:
    // e' um disco de marfim com uma haste escura e uma tampa central, e e' o que
    // se ve na faixa de macros. Desenhar o PNG do harness aqui dava um knob
    // escuro, que contradiz a peca do Stitch.
    //
    // O corpo, a haste e a tampa sao fixos; o que muda com o valor e' so' o
    // angulo da haste. O anel de foco continua em codigo porque so ele sabe se
    // o componente tem o foco do teclado.
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

        // Sombra projetada. Sem blur no JUCE, duas elipses de alpha baixo dao a
        // mesma queda suave que o box-shadow do mock.
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillEllipse(bounds.translated(0.0f, 2.5f).expanded(0.5f));
        g.setColour(juce::Colours::black.withAlpha(0.10f));
        g.fillEllipse(bounds.translated(0.0f, 4.5f).expanded(1.5f));

        // Corpo: gradiente radial com o foco em 35%/30% da caixa, como o
        // radial-gradient(circle at 35% 30%) do mock. Os tres stops sao os do
        // CSS: #f7f5ed, #beb6a0 a 60% e #827b68 na borda.
        const auto focus = juce::Point<float> {bounds.getX() + bounds.getWidth() * 0.35f,
                                               bounds.getY() + bounds.getHeight() * 0.30f};
        const auto edge = centre.translated(radius * 1.15f, radius * 1.15f);
        juce::ColourGradient body {juce::Colour {0xfff7f5ed}, focus,
                                   juce::Colour {0xff827b68}, edge, true};
        body.addColour(0.55, juce::Colour {0xffbeb6a0});
        g.setGradientFill(body);
        g.fillEllipse(bounds);

        g.setColour(juce::Colour {0xff9d9580});
        g.drawEllipse(bounds.reduced(0.5f), 1.0f);

        // Realce interno no topo: o inset 0 1px 2px branco do mock. Um arco e
        // nao um anel inteiro, porque a luz vem de cima.
        juce::Path sheen;
        sheen.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                            0.0f, -2.55f, -0.75f, true);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.strokePath(sheen, juce::PathStrokeType {1.0f});

        // Haste do valor: 1.5 x 7 no mock, escura (#252830), do centro para a
        // borda. O comprimento e' proporcional ao raio para a peca encolher sem
        // perder a leitura.
        const auto inner = radius * 0.22f;
        const auto outer = radius * 0.80f;
        const auto tailX = centre.x + std::sin(angle) * inner;
        const auto tailY = centre.y - std::cos(angle) * inner;
        const auto tipX = centre.x + std::sin(angle) * outer;
        const auto tipY = centre.y - std::cos(angle) * outer;

        g.setColour(juce::Colour {0xff252830});
        g.drawLine(tailX, tailY, tipX, tipY, 2.6f);

        // Tampa central de 12 px no mock, em #3a372f a 40%.
        const auto capRadius = radius * 0.19f;
        g.setColour(juce::Colour {0xff3a372f}.withAlpha(0.40f));
        g.fillEllipse(juce::Rectangle<float> {capRadius * 2.0f, capRadius * 2.0f}
                          .withCentre(centre));

        // Anel de foco. Sem ele, quem navega por teclado nao sabe qual knob
        // esta' ativo, e o criterio 2.4.7 do WCAG 2.2 pede foco visivel. E' o
        // unico sinal de que o controle esta' com o foco do teclado.
        if (slider.hasKeyboardFocus(true)) {
            g.setColour(palette::accent);
            g.drawEllipse(bounds.expanded(2.5f), 1.8f);
        }
    }

    // Botao chapado do mock: retangulo escuro arredondado com borda tingida
    // pelo texto. O LOAD e' #282c35 com borda ambar; o ALINHAR do display e'
    // a mesma peca em cinza. Nao ha bisel de tecla fisica porque o mock usa
    // uma tecla de painel, nao um botao de hardware.
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        constexpr float radius {3.5f};

        auto base = backgroundColour;
        if (base.isTransparent()) {
            base = palette::metalButton;
        }
        if (shouldDrawButtonAsDown) {
            base = base.darker(0.28f);
        } else if (shouldDrawButtonAsHighlighted) {
            base = base.brighter(0.14f);
        }

        g.setColour(base);
        g.fillRoundedRectangle(bounds, radius);

        // Borda do mock: border-amber-900/30 no LOAD. Usa a cor do texto em
        // alpha para o mesmo desenho servir o botao cinza sem mais um token.
        const auto tint = button.findColour(juce::TextButton::textColourOffId);
        g.setColour(tint.withAlpha(shouldDrawButtonAsDown ? 0.55f : 0.32f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);

        // Realce de 1 px no topo: e' o que separa uma tecla de um retangulo.
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawHorizontalLine(1, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);

        // 2.4.7 Foco visivel.
        if (button.hasKeyboardFocus(true)) {
            g.setColour(palette::accent);
            g.drawRoundedRectangle(bounds.expanded(1.5f), radius + 1.0f, 2.0f);
        }
    }

    // O texto e' sempre desenhado em codigo: o harness nao gera texto, porque
    // texto gerado por modelo sai com letra errada. No mock o rotulo ocupa a
    // tecla inteira, centrado e em maiusculas.
    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool shouldDrawButtonAsDown) override {
        g.setFont(BoxedLabel::sansFont(10.0f, true));
        g.setColour(button.findColour(juce::TextButton::textColourOffId)
                        .withAlpha(shouldDrawButtonAsDown ? 0.90f : 1.0f));
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred, false);
    }
};

} // namespace opcoda