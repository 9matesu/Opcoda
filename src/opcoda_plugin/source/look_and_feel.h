#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace opcoda {

// LookAndFeel do Opcoda: so o knob e o botao sao desenhados aqui, o resto fica
// no padrao do JUCE. nada vem de imagem -- o corpo do knob e' um Path que
// escala, e nao ha PNG a descodificar no arranque.
//
// os knobs sao juce::Slider de verdade: e' o que da foco por teclado, setas e
// AccessibilityHandler (docs/10-acessibilidade-w3c.md).
//
// nota: este ficheiro usa juce::ColourGradient, que e' de juce_graphics. o
// modulo chega por juce_gui_basics, nao por um include explicito, por isso a
// falha, se acontecer, aparece aqui e nao onde a falta e'.
class OpcodaLookAndFeel : public juce::LookAndFeel_V4 {
public:
    OpcodaLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, palette::chassis);

        // as cores da textBox ficam definidas sem textBox visivel: o valor vive
        // num Label proprio.
        setColour(juce::Slider::textBoxTextColourId, palette::textDark);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

        setColour(juce::Label::textColourId, palette::textDark);

        setColour(juce::TextButton::buttonColourId, palette::panelBg);
        setColour(juce::TextButton::buttonOnColourId, palette::accent);
        setColour(juce::TextButton::textColourOffId, palette::textDark);
        setColour(juce::TextButton::textColourOnId, palette::textDark);
    }

    // knob estilo Ableton: disco liso, arco fino, ponteiro curto. O corpo nao
    // muda com o valor; so o arco e o ponteiro mudam, e o anel de foco depende
    // do teclado -- por isso isto e' codigo e nao imagem.
    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override {
        const auto bounds =
            juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float angle = rotaryStartAngle +
                            sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // raio zero e' um rectangulo degenerado: acontece quando a janela
        // encolhe abaixo do minimo e tem de pintar na mesma.
        if (radius > 1.0f) {
            drawKnobBody(g, centre, radius);

            // trilha do arco: a escala fica sempre visivel, senao o resto
            // adivinha-se.
            juce::Path track;
            track.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f,
                                rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(palette::alpha(palette::chassisBorder, 0.45f));
            g.strokePath(track, juce::PathStrokeType {1.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded});

            // arco de valor com halo curto por baixo. pilha de alfa e nao blur,
            // como o glow da onda -- o arco carrega o valor, nao decora.
            juce::Path valueArc;
            valueArc.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f,
                                   rotaryStartAngle, angle, true);
            g.setColour(palette::alpha(palette::accent, 0.10f));
            g.strokePath(valueArc, juce::PathStrokeType {4.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded});
            g.setColour(palette::accent);
            g.strokePath(valueArc, juce::PathStrokeType {2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded});

            // ponteiro: haste curta, de 30 % do raio ate antes da borda. vai
            // depois do arco, para ficar por cima; ao contrario, o primeiro troco
            // nascia cortado.
            const auto inner = radius * 0.30f;
            const auto outer = radius - 2.5f;
            const auto tailX = centre.x + std::sin(angle) * inner;
            const auto tailY = centre.y - std::cos(angle) * inner;
            const auto tipX = centre.x + std::sin(angle) * outer;
            const auto tipY = centre.y - std::cos(angle) * outer;

            // sombra de 1 px para baixo e para a direita: ancora a haste na
            // face, senao o laranja parece impresso.
            g.setColour(palette::alpha(juce::Colours::black, 0.30f));
            g.drawLine(tailX + 1.0f, tailY + 1.0f, tipX + 1.0f, tipY + 1.0f, 2.5f);
            g.setColour(palette::accent);
            g.drawLine(tailX, tailY, tipX, tipY, 2.5f);
        }

        // anel de foco: unico sinal de que o knob tem o foco de teclado (2.4.7).
        // focusRing e nao accent porque accent sobre o chassis da 1,24:1, e o
        // 1.4.11 pede 3:1 para um indicador de foco.
        if (slider.hasKeyboardFocus(true)) {
            g.setColour(palette::focusRing);
            g.drawEllipse(bounds.reduced(-3.0f), 2.0f);
        }
    }

    // botao compacto: rectangulo de raio minimo, borda fina, inativo neutro e
    // ativo com filete ambar. o componente continua juce::TextButton -- teclado,
    // foco e AccessibilityHandler de graca.
    //
    // a propriedade `uiIcon` diz o desenho: "transport" faz play/stop, "folder"
    // a pasta, "tab" a palavra numa caixa. sem ela nao se desenha nada.
    //
    // os estados distinguem-se sem cor: hover ganha banho de alfa, premido
    // escurece, ligado troca a forma (play vira stop, aba ganha filete).
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        // os limites sao os do botao, nao os deste LookAndFeel: getLocalBounds()
        // aqui dentro devolveria a area do LookAndFeel, que e' zero, e o botao
        // sairia com 1 px.
        const auto bounds = button.getLocalBounds().toFloat();
        const auto reduced = bounds.reduced(0.5f);
        if (reduced.getWidth() <= 1.0f || reduced.getHeight() <= 1.0f) {
            return;
        }

        const auto isTab = button.getProperties()["uiIcon"].toString() == "tab";
        const auto on = button.getToggleState();

        // caixa da aba: borda fina sempre, preenchimento so quando ativa. raio
        // 2, workstation e nao pill.
        if (isTab) {
            g.setColour(on ? palette::subPanel
                           : palette::alpha(palette::subPanel, 0.25f));
            g.fillRoundedRectangle(reduced, 2.0f);
            g.setColour(palette::alpha(palette::chassisBorder, on ? 0.95f : 0.55f));
            g.drawRoundedRectangle(reduced, 2.0f, 1.0f);
        }

        // banho de alfa no hover e no premido: e' o unico relevo que sobrou.
        // `uiOnDark` so serve ao wash, e chassis e display sao ambos escuros, por
        // isso o branco e' branco nos dois casos.
        const auto onDark = button.getProperties()["uiOnDark"];
        const auto wash = onDark ? juce::Colours::white : juce::Colours::white;
        if (shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown) {
            g.setColour(palette::alpha(wash, 0.08f));
            g.fillRoundedRectangle(reduced, isTab ? 2.0f : 4.0f);
        }
        if (shouldDrawButtonAsDown) {
            g.setColour(palette::alpha(wash, 0.14f));
            g.fillRoundedRectangle(reduced, isTab ? 2.0f : 4.0f);
        }

        // aba activa: filete de acento em baixo, com halo fraco por tras. pista
        // redundante -- o texto da aba activa ja vai em forte e claro, e carrega
        // o estado sozinho.
        if (on && isTab) {
            const auto bar = juce::Rectangle<float> {reduced.getX() + 4.0f,
                                                     reduced.getBottom() - 2.5f,
                                                     reduced.getWidth() - 8.0f, 2.0f};
            g.setColour(palette::alpha(palette::accent, 0.25f));
            g.fillRect(bar.expanded(3.0f, 2.0f));
            g.setColour(palette::accent);
            g.fillRect(bar);
        }

        // 2.4.7 foco visivel. focusRing e' ambar (~7:1 sobre o escuro), e o anel
        // de 2 px e' a forma que o 1.4.1 pede.
        if (button.hasKeyboardFocus(true)) {
            g.setColour(palette::focusRing);
            g.drawRoundedRectangle(reduced.reduced(1.0f), isTab ? 2.0f : 4.0f, 2.0f);
        }
    }

    // icone desenhado em codigo: nao ha fonte de icones no repositorio, e uma
    // fonte so para tres glifos seria peso morto.
    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool) override {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        if (bounds.getWidth() <= 1.0f || bounds.getHeight() <= 1.0f) {
            return;
        }

        // desligado usa o apagado da superficie em vez de sumir: um botao que
        // recusa o toque tem de continuar la para recusar.
        const auto onDark = static_cast<bool>(button.getProperties()["uiOnDark"]);
        const auto ink = button.isEnabled()
                             ? (onDark ? palette::textOnDark : palette::textDark)
                             : (onDark ? palette::textOnDarkSub : palette::textSub);
        const auto kind = button.getProperties()["uiIcon"].toString();

        if (kind == "transport") {
            paintTransportGlyph(g, bounds, button.getToggleState(), ink);
            return;
        }
        if (kind == "folder") {
            paintFolderGlyph(g, bounds, ink);
            return;
        }

        // aba: palavra curta, forte e clara quando activa, normal e secundaria
        // quando nao. o peso e a cor carregam o estado.
        const auto on = button.getToggleState();
        g.setFont(fonts().sans(9.0f, on));
        g.setColour(on ? palette::textDark : palette::textSub);
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

 private:
    // transporte: triangulo para tocar, quadrado para parar, centrados na area
    // util. o lado e' fracao do menor para nao encostar quando a janela encolhe.
    //
    // halo branco por tras do glifo: o transporte mora no display escuro, e a
    // luz ali e' a mesma dos graos. pilha de alfa e nao blur.
    static void paintTransportGlyph(juce::Graphics& g,
                                    const juce::Rectangle<float>& bounds,
                                    bool playing,
                                    juce::Colour ink) {
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;
        const auto centre = bounds.getCentre();

        juce::Path glyph;
        if (playing) {
            glyph.addRectangle(centre.x - side * 0.5f, centre.y - side * 0.5f, side, side);
        } else {
            glyph.addTriangle(centre.x - side * 0.5f, centre.y - side * 0.62f,
                              centre.x - side * 0.5f, centre.y + side * 0.62f,
                              centre.x + side * 0.62f, centre.y);
        }

        // halo: o glifo dilatado. sem blur no software, um passe alargado e fraco
        // por baixo do cheio da a leitura de luz barata.
        g.setColour(palette::alpha(juce::Colours::white, 0.12f));
        juce::Path haloPath;
        haloPath.addRoundedRectangle(glyph.getBounds().expanded(3.0f), 3.0f);
        g.fillPath(haloPath);

        g.setColour(ink);
        g.fillPath(glyph);
    }

    // pasta so com contorno: cheia seria um borrao a 14 px.
    static void paintFolderGlyph(juce::Graphics& g,
                                 const juce::Rectangle<float>& bounds,
                                 juce::Colour ink) {
        const auto w = juce::jmin(bounds.getWidth() * 0.52f, 20.0f);
        const auto h = w * 0.72f;
        const auto x = bounds.getCentreX() - w * 0.5f;
        const auto y = bounds.getCentreY() - h * 0.5f;

        g.setColour(ink);
        juce::Path folder;
        folder.addRoundedRectangle(x, y + h * 0.22f, w, h * 0.78f, 1.5f);
        folder.startNewSubPath(x, y + h * 0.22f);
        folder.lineTo(x, y);
        folder.lineTo(x + w * 0.38f, y);
        folder.lineTo(x + w * 0.48f, y + h * 0.22f);
        g.strokePath(folder, juce::PathStrokeType {1.6f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});
    }

    // corpo do knob: disco liso, aro fino e ponto de origem. luz de cima,
    // gradiente curto do claro para o tom do painel, sem aro escuro a separar.
    void drawKnobBody(juce::Graphics& g, juce::Point<float> centre, float radius) {
        const auto rim = juce::Rectangle<float> {centre.x - radius, centre.y - radius,
                                                  radius * 2.0f, radius * 2.0f};

        // sombra projetada: duas elipses deslocadas 2 px para baixo. um blur de
        // verdade nao existe barato no JUCE.
        for (int i = 2; i >= 1; --i) {
            const auto spread = static_cast<float>(i);
            g.setColour(palette::alpha(juce::Colours::black, 0.10f - 0.025f * spread));
            g.fillEllipse(rim.expanded(spread).translated(0.0f, 2.0f));
        }

        // disco com luz de cima: claro valBox em cima, panelBg em baixo, sem aro
        // escuro a separar.
        g.setGradientFill(juce::ColourGradient {
            palette::valBox, centre.translated(0.0f, -radius * 0.5f),
            palette::panelBg, centre.translated(0.0f, radius * 0.55f), false});
        g.fillEllipse(rim);

        // aro fino, so para fechar a forma contra o painel.
        g.setColour(palette::alpha(palette::chassisBorder, 0.40f));
        g.drawEllipse(rim.reduced(0.5f), 1.0f);

        // ponto de origem no centro. fica por baixo do arco e do ponteiro, por
        // isso a haste comeca a 30 % do raio.
        const auto markerRadius = radius * 0.06f;
        g.setColour(palette::alpha(palette::chassisBorder, 0.65f));
        g.fillEllipse(centre.x - markerRadius, centre.y - markerRadius, markerRadius * 2.0f,
                      markerRadius * 2.0f);
    }
};

} // namespace opcoda