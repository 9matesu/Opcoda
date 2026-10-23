#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace opcoda {

// LookAndFeel do Opcoda. Existe para dois desenhos: o knob e o botao. Todo o
// resto fica no LookAndFeel padrao.
//
// **As pecas sao desenhadas em codigo, e nao vem nenhuma imagem do binario.**
// Foi uma decisao, e nao uma falta de arte: knob-md.png eram 186x192 pixels para
// pintar um disco de 62, e button-large.png obrigava a desenhar uma peca
// quadrada ao lado do texto porque esticar o bisel de 2 px o deixava com 1 px de
// um lado e 4 px do outro. Com Path o corpo escala, o bisel tem a espessura que
// se pede, e nao ha um descodificador de PNG a correr no arranque.
//
// O aviso que vivia em assets.h fica aqui: este ficheiro usa
// juce::ColourGradient, que e' de juce_graphics. O modulo chega por
// juce_gui_basics e juce_audio_processors e nao por um include explicito, o que
// significa que a falha, se alguma vez acontecer, aparece neste ficheiro e nao
// onde a falta e'.
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

        setColour(juce::TextButton::buttonColourId, palette::panelBg);
        setColour(juce::TextButton::buttonOnColourId, palette::accent);
        setColour(juce::TextButton::textColourOffId, palette::textDark);
        setColour(juce::TextButton::textColourOnId, palette::textDark);
    }

    // Knob do mock: corpo creme, haste escura e tampa central. O valor vive no
    // Label por baixo, por isso o desenho nao precisa de arco.
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

        if (radius <= 1.0f) {
            return;
        }

        drawKnobBody(g, centre, radius);

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
        // esta' ativo, e o criterio 2.4.7 do WCAG 2.2 pede foco visivel.
        if (slider.hasKeyboardFocus(true)) {
            g.setColour(palette::accent);
            g.drawEllipse(bounds.expanded(2.5f), 1.8f);
        }
    }

    // Botao compacto de workstation: retangulo de raio minimo, borda fina,
    // inativo neutro, ativo com filete ambar. O componente continua
    // juce::TextButton ÔÇö teclado, foco e AccessibilityHandler de graca.
    //
    // A superficie importa e o botao nao a conhece: `uiOnDark` diz que ele mora
    // sobre o display escuro (a linha de estado). Como o chassis tambem e'
    // escuro agora, a tinta e' a mesma dos dois lados; a propriedade fica
    // porque o wash do hover precisa dela.
    //
    // O que diz qual botao e' vai na propriedade `uiIcon`: "transport" desenha
    // ÔûÂ/Ôûá, "folder" desenha a pasta, "tab" desenha a palavra numa caixa de
    // borda fina. Sem a propriedade nao se desenha nada, porque um botao sem
    // icone e' um alvo invisivel e um alvo invisivel nao passa nem no 2.5.8
    // nem no bom senso.
    //
    // Os quatro estados continuam distinguiveis sem cor: repouso e' o icone
    // quieto, sobreposto ganha o banho de alfa, premido escurece, e ligado
    // troca a forma (ÔûÂ vira Ôûá, aba ganha o filete) e o peso do texto. A troca
    // de palavra PLAY/STOP virou troca de forma pelo mesmo motivo que a criou:
    // o estado tem de se ler sem ver cor, e forma e' o que o 1.4.1 pede.
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        // Os limites sao os do botao e nao os deste LookAndFeel. Os dois derives
        // de Component, e `getLocalBounds()` aqui dentro devolveria a area do
        // LookAndFeel, que e' zero: o botao seria desenhado com 1 px de altura.
        const auto bounds = button.getLocalBounds().toFloat();
        const auto reduced = bounds.reduced(0.5f);
        if (reduced.getWidth() <= 1.0f || reduced.getHeight() <= 1.0f) {
            return;
        }

        const auto isTab = button.getProperties()["uiIcon"].toString() == "tab";
        const auto on = button.getToggleState();

        // Caixa da aba: borda fina sempre, preenchimento so quando ativa.
        // Inativa neutra (fundo do pai), ativa com subPanel para separar do
        // fundo sem gritar. Raio 2: workstation, nao pill.
        if (isTab) {
            if (on) {
                g.setColour(palette::metalBottom.darker(0.04f));
                g.fillRoundedRectangle(reduced, 2.0f);
            }
            g.setColour(palette::alpha(palette::metalBorder, on ? 0.95f : 0.55f));
            g.drawRoundedRectangle(reduced, 2.0f, 1.0f);
        } else if (!static_cast<bool>(button.getProperties()["uiOnDark"])) {
            g.setColour(shouldDrawButtonAsDown ? palette::metalButton.darker(0.28f)
                                               : palette::metalButton);
            g.fillRoundedRectangle(reduced, 3.5f);
            g.setColour(palette::alpha(palette::amberText, 0.32f));
            g.drawRoundedRectangle(reduced.reduced(0.5f), 3.5f, 1.0f);
        }

        // Banho de alfa no hover e no premido: e' o unico relevo que sobrou, e
        // chega porque o icone ja tem forma propria. Branco no escuro ÔÇö o
        // chassis e o display sao ambos escuros agora.
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

        // Aba activa: filete de acento em baixo, com halo fraco por tras. E'
        // pista redundante ÔÇö o texto da aba activa vai em forte e claro, e e'
        // isso que carrega o estado. O halo e' a mesma lingua do glow da onda:
        // a vista activa tem luz, as outras nao.
        if (on && isTab) {
            const auto bar = juce::Rectangle<float> {reduced.getX() + 4.0f,
                                                     reduced.getBottom() - 2.5f,
                                                     reduced.getWidth() - 8.0f, 2.0f};
            g.setColour(palette::alpha(palette::accent, 0.25f));
            g.fillRect(bar.expanded(3.0f, 2.0f));
            g.setColour(palette::accent);
            g.fillRect(bar);
        }

        // 2.4.7 Foco visivel. O desenho nao sabe se o botao tem o foco.
        // focusRing e' ambar (~7:1 sobre o fundo escuro): o anel tem de
        // contrastar com a superficie, e a forma (anel de 2 px) e' a segunda
        // pista que o 1.4.1 pede.
        if (button.hasKeyboardFocus(true)) {
            g.setColour(palette::focusRing);
            g.drawRoundedRectangle(reduced.reduced(1.0f), isTab ? 2.0f : 4.0f, 2.0f);
        }
    }

    // O icone e' desenhado em codigo e nunca vem de uma peca nem de uma fonte de
    // icones, porque nao ha nenhuma no repositorio e uma fonte so para tres
    // glifos seria peso morto.
    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool) override {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        if (bounds.getWidth() <= 1.0f || bounds.getHeight() <= 1.0f) {
            return;
        }

        // Desligado usa o apagado da superficie em vez de sumir: um icone que
        // some quando o transporte do host para parece que o botao foi embora, e
        // um botao que recusa o toque tem de continuar la para recusar.
        // Secundario (~7:1 sobre o fundo escuro) chega para icone desligado,
        // que nao e' texto de leitura.
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

        // Aba de vista: a palavra curta, forte e clara quando activa, normal e
        // secundaria quando nao. A mudanca de peso e cor carrega o estado; o
        // filete e a caixa desenhados no fundo sao redundancia.
        const auto on = button.getToggleState();
        g.setFont(fonts().sans(9.0f, on));
        g.setColour(on ? palette::textDark : palette::textSub);
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

 private:
    // Transporte: triangulo para tocar, quadrado para parar, centrados na area
    // util do botao. O tamanho e' fracao do menor lado para nao encostar na
    // borda quando a janela encolhe o botao.
    //
    // Halo branco por tras do glifo: o transporte mora no display escuro, e um
    // halo claro ali e' a mesma luz dos graios ÔÇö o botao que comanda o som tem a
    // mesma energia do som a acontecer. Pilha de alfa e nao blur, como em todo o
    // glow da casa.
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

        // O halo e' o glifo dilatado: sem blur no renderizador de software, um
        // passe alargado e fraco por baixo do passe cheio da a leitura de luz
        // sem custo que apareca no perfil.
        g.setColour(palette::alpha(juce::Colours::white, 0.12f));
        juce::Path haloPath;
        haloPath.addRoundedRectangle(glyph.getBounds().expanded(3.0f), 3.0f);
        g.fillPath(haloPath);

        g.setColour(ink);
        g.fillPath(glyph);
    }

    // Pasta: costas com aba e frente por cima, so com contorno. Cheia seria um
    // borrao a 14 px; o contorno le-se como pasta e deixa o chassi respirar.
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

    // O corpo do knob: disco liso, aro fino e ponto de origem.
    //
    // A luz vem de cima, quase de frente, como num controle do Ableton: o
    // gradiente e' curto, do claro para o tom do painel, e nao ha aro escuro a
    // separar o disco do fundo. Sem o serrilhado nao ha Path para memorizar nem
    // cache para invalidar ÔÇö o corpo inteiro sao tres fills e dois strokes.
    void drawKnobBody(juce::Graphics& g, juce::Point<float> centre, float radius) {
        const auto rim = juce::Rectangle<float> {centre.x - radius, centre.y - radius,
                                                  radius * 2.0f, radius * 2.0f};

        // Sombra projetada: o corpo flutua sobre o painel em vez de estar
        // colado nele. Duas elipses concentricas deslocadas 2 px para baixo ÔÇö
        // um blur de verdade nao existe barato no JUCE, e dois fills por knob
        // a 60 Hz custam menos que um DropShadowEffect por quadro.
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.fillEllipse(rim.translated(0.0f, 2.5f).expanded(0.5f));
        g.setColour(juce::Colours::black.withAlpha(0.10f));
        g.fillEllipse(rim.translated(0.0f, 4.5f).expanded(1.5f));

        // Disco, com luz suave de cima. O claro e' valBox e o escuro e' o proprio
        // painel: sem aro de chassisDark a separar, o disco assenta no modulo em
        // vez de flutuar sobre um anel.
        const auto focus = juce::Point<float> {rim.getX() + rim.getWidth() * 0.35f,
                                               rim.getY() + rim.getHeight() * 0.30f};
        const auto edge = centre.translated(radius * 1.15f, radius * 1.15f);
        juce::ColourGradient body {juce::Colour {0xfff7f5ed}, focus,
                                   juce::Colour {0xff827b68}, edge, true};
        body.addColour(0.55, juce::Colour {0xffbeb6a0});
        g.setGradientFill(body);
        g.fillEllipse(rim);

        // Aro fino, so para fechar a forma contra o painel.
        g.setColour(juce::Colour {0xff9d9580});
        g.drawEllipse(rim.reduced(0.5f), 1.0f);

        juce::Path sheen;
        sheen.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                            0.0f, -2.55f, -0.75f, true);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.strokePath(sheen, juce::PathStrokeType {1.0f});
    }
};

} // namespace opcoda
