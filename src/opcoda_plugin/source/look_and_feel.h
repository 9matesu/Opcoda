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

    // Knob: peca fisica do asset harness, rodada pelo angulo do valor, com o
    // arco de escala e o anel de foco desenhados por cima em codigo.
    //
    // A divisao entre o que vem do PNG e o que e' codigo nao e' arbitraria. O
    // corpo e o marcador branco sao fisicos e ficam iguais em todos os valores.
    // O arco precisa de know o valor, e o anel de foco precisa de saber se o
    // componente tem o foco do teclado: as duas coisas so existem em codigo, e
    // um PNG nao as teria.
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

        // A peca foi recortada com o marcador em 12:00, entao rodar o
        // angulo do valor faz o marcador apontar para o sitio certo. A peca e'
        // vista de cima, por isso a rotacao le-se como o marcador a girar.
        //
        // O retangulo e' calculado ANTES de addTransform. Um drawImage com um
        // rect ja transformado e' ele proprio o rect que a rotacao despacha, e
        // nao o rect pedido: o corpo sai do sitio e cai no canto.
        const auto& body = assets::detail::knobMd();
        if (body.isValid()) {
            // A peca nao e' rodada. E' uma fotografia top-down com a luz de
            // estudio assente na peels; rodar o PNG faria o brilho andar com o
            // knob, e o realce passaria a descer de manha a norte, que e'
            // fisicamente falso. O harness diz o mesmo: a rotacao e' do
            // plugin, via filme de imagens, e `createSpriteSheet` esta'
            // reservado e nao implementado.
            //
            // O indicador do valor e' o arco laranja e o ponteiro, ambos em
            // codigo. O ponto branco do meio da face fica como marca de
            // origem, que e' o que se ve num knob real.
            const auto side = radius * 2.0f;
            g.drawImage(body,
                        juce::Rectangle<float> {centre.x - side * 0.5f, centre.y - side * 0.5f,
                                                side, side},
                        juce::RectanglePlacement::centred);
        }

        // Trilha do arco de valor, sempre presente: e' a escala, e uma escala que so
        // aparece preenchida obriga a adivinhar o resto.
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(palette::alpha(palette::chassisBorder, 0.45f));
        g.strokePath(track, juce::PathStrokeType {1.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded});

        // Arco preenchido, do minimo ate ao valor.
        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f,
                               0.0f, rotaryStartAngle, angle, true);
        g.setColour(palette::accent);
        g.strokePath(valueArc, juce::PathStrokeType {2.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});

        // Ponteiro. A peca fotografada tem um marcador branco no centro da face, e
        // um knob com dois indicadores nao diz qual e' o valor: por isso o
        // indicador do valor e' uma haste desenhada aqui.
        //
        // A haste vai do centro para fora e e' desenhada *depois* do arco, para
        // ficar por cima dele. Ao contrario, nascia por baixo do arco e o
        // primeiro troco da faixa aparecia cortado.
        const auto inner = radius * 0.16f;
        const auto outer = radius - 2.5f;
        const auto tailX = centre.x + std::sin(angle) * inner;
        const auto tailY = centre.y - std::cos(angle) * inner;
        const auto tipX = centre.x + std::sin(angle) * outer;
        const auto tipY = centre.y - std::cos(angle) * outer;

        g.setColour(palette::accent);
        g.drawLine(tailX, tailY, tipX, tipY, 2.5f);

        // Anel de foco. Sem ele, quem navega por teclado nao sabe qual knob
        // esta' ativo, e o criterio 2.4.7 do WCAG 2.2 pede foco visivel. E' o
        // unico sinal de que o controle esta' com o foco do teclado.
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
        const auto bounds = button.getLocalBounds().toFloat();
        const auto& face = assets::detail::buttonLarge();

        // A peca traz bisel, sombra e realce, por isso o estado e' um
        // deslocamento e um tint, e nao um redesenho do bisel.
        if (face.isValid()) {
            // A peca e' um botao quadrado e o LOAD e' uma faixa larga. Em vez
            // de esticar a peca, que deformaria o bisel, desenha-se um quadrado
            // do tamanho da altura no canto esquerdo e o texto segue ao lado.
            // Esticar um PNG e' sempre visivel: o bisel de 2 px passa a 1 px
            // num lado e a 4 px no outro.
            const auto side = juce::jmin(bounds.getHeight(), 28.0f);
            const auto faceRect = juce::Rectangle<float> {bounds.getX(), bounds.getCentreY() - side * 0.5f,
                                                          side, side};
            const auto target = faceRect.translated(0.0f, shouldDrawButtonAsDown ? 1.0f : 0.0f);
            g.drawImage(face, target, juce::RectanglePlacement::centred);

            if (shouldDrawButtonAsDown) {
                g.setColour(juce::Colours::black.withAlpha(0.24f));
                g.fillRoundedRectangle(target.reduced(side * 0.14f), 3.0f);
            } else if (shouldDrawButtonAsHighlighted) {
                g.setColour(juce::Colours::white.withAlpha(0.12f));
                g.fillRoundedRectangle(target.reduced(side * 0.14f), 3.0f);
            }

            // 2.4.7 Foco visivel. A peca nao sabe se o botao tem o foco.
            if (button.hasKeyboardFocus(true)) {
                g.setColour(palette::accent);
                g.drawRoundedRectangle(target.reduced(1.0f), 3.0f, 2.0f);
            }
            return;
        }

        // Sem a peca: bisel vetorial. Nao e' o caminho previsto, e' a rede para
        // o binario nao falhar em silencio se o asset faltar.
        const auto reduced = bounds.reduced(0.5f);
        const auto base = shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.12f)
                                                        : backgroundColour;

        juce::ColourGradient fill(base.brighter(0.10f), reduced.getTopLeft(),
                                  base.darker(0.06f), reduced.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(reduced, 2.0f);

        g.setColour(palette::alpha(palette::chassisBorder,
                                   shouldDrawButtonAsDown ? 0.95f : 0.65f));
        g.drawRoundedRectangle(reduced, 2.0f, 1.0f);
    }

    // O texto e' desenhado em codigo e nunca vem do PNG: o harness nao gera texto,
// porque texto gerado por modelo sai com letra errada. E' aqui que a peca
    // quadrada deixa de ocupar o botao inteiro, entao o texto passa a ocupar a
    // faixa a direita.
    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool shouldDrawButtonAsDown) override {
        const auto bounds = button.getLocalBounds().toFloat();
        const auto side = juce::jmin(bounds.getHeight(), 28.0f);

        g.setFont(BoxedLabel::sansFont(10.0f, true));
        g.setColour(button.findColour(juce::TextButton::textColourOffId)
                        .withAlpha(shouldDrawButtonAsDown ? 0.9f : 1.0f));

// Com a peca a esquerda, o texto alinha a esquerda no resto da faixa.
        // Centrar tudo sobre a faixa inteira punha o texto por cima da peca.
        const auto textLeft = bounds.getX() + side + 6.0f;
        g.drawText(button.getButtonText(),
                   juce::Rectangle<float> {textLeft, bounds.getY(),
                                           juce::jmax(0.0f, bounds.getRight() - textLeft - 2.0f),
                                           bounds.getHeight()},
                   juce::Justification::centredLeft, false);
    }
};

} // namespace opcoda