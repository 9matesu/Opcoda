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

    // Knob estilo Ableton: disco fino, arco fino, ponteiro curto. Sem serrilhado.
    //
    // O corpo e' fixo e o valor mexe-se no arco e no ponteiro. O serrilhado saiu
    // porque era a peca que mais gritava "synth de 2006": 36 dentes a 1 px viram
    // ruido cinzento em qualquer diametro util, e um anel liso com luz de cima
    // le-se como controle moderno sem perder a pega — a pega esta' no gesto, nao
    // no desenho.
    //
    // A divisao entre o corpo e o indicador nao e' arbitraria: o corpo e' igual
    // para os seis knobs e por isso desenha-se sempre da mesma forma, enquanto o
    // arco precisa de conhecer o valor e o anel de foco precisa de saber se o
    // componente tem o foco do teclado. As duas ultimas coisas so existem em
    // codigo, e nenhuma das duas cabia numa imagem.
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

        // Raio de zero e' um rectangulo degenerado. Acontece quando a janela
        // encolhe abaixo do minimo declarado, e mesmo assim tem de pintar em vez
        // de deixar um buraco no painel.
        if (radius > 1.0f) {
            drawKnobBody(g, centre, radius);

            // Trilha do arco de valor, sempre presente: e' a escala, e uma escala
            // que so aparece preenchida obriga a adivinhar o resto.
            juce::Path track;
            track.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f,
                                rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(palette::alpha(palette::chassisBorder, 0.45f));
            g.strokePath(track, juce::PathStrokeType {1.5f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded});

            // Arco preenchido, do minimo ate ao valor.
            juce::Path valueArc;
            valueArc.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f,
                                   rotaryStartAngle, angle, true);
            g.setColour(palette::accent);
            g.strokePath(valueArc, juce::PathStrokeType {2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded});

            // Ponteiro. O indicador do valor e' uma haste curta e nao um raio
            // inteiro: sai a 30 % do centro e morre antes da borda, como no
            // Ableton. Uma haste de ponta a ponta divide o disco em dois e o
            // olho le metade; curta, ela e' um indice e nao uma divisao.
            //
            // A haste vai para fora e e' desenhada *depois* do arco, para ficar
            // por cima dele. Ao contrario, nascia por baixo do arco e o primeiro
            // troco da faixa aparecia cortado.
            const auto inner = radius * 0.30f;
            const auto outer = radius - 2.5f;
            const auto tailX = centre.x + std::sin(angle) * inner;
            const auto tailY = centre.y - std::cos(angle) * inner;
            const auto tipX = centre.x + std::sin(angle) * outer;
            const auto tipY = centre.y - std::cos(angle) * outer;

            // O ponteiro flutua sobre a face como o corpo sobre o painel: a
            // sombra de 1 px para baixo e para a direita ancora a haste, e sem
            // ela o laranja parece impresso em vez de montado.
            g.setColour(palette::alpha(juce::Colours::black, 0.30f));
            g.drawLine(tailX + 1.0f, tailY + 1.0f, tipX + 1.0f, tipY + 1.0f, 2.5f);
            g.setColour(palette::accent);
            g.drawLine(tailX, tailY, tipX, tipY, 2.5f);
        }

        // Anel de foco. Sem ele, quem navega por teclado nao sabe qual knob
        // esta' ativo, e o criterio 2.4.7 do WCAG 2.2 pede foco visivel. E' o
        // unico sinal de que o controle esta' com o foco do teclado.
        //
        // A cor e' focusRing e nao accent porque este anel nao tem leitura
        // numerica ao lado que o repita: accent sobre o chassis da 1,24:1, e o
        // 1.4.11 exige 3:1 de um indicador de foco. focusRing da 9,73:1.
        if (slider.hasKeyboardFocus(true)) {
            g.setColour(palette::focusRing);
            g.drawEllipse(bounds.reduced(-3.0f), 2.0f);
        }
    }

    // Botao sem cromo: icone ou palavra solta sobre a superficie, sem fundo, sem
    // bisel, sem sombra. O componente continua juce::TextButton — teclado, foco
    // e AccessibilityHandler de graca — so o desenho achatou.
    //
    // A superficie importa e o botao nao a conhece: `uiOnDark` diz que ele mora
    // sobre o display escuro (a linha de estado), e ai a tinta clara e' o que
    // se le. Sem a propriedade, um glifo textDark sobre o display daria 1,14:1
    // — invisivel — e o anel focusRing sumiria junto.
    //
    // O que diz qual botao e' vai na propriedade `uiIcon`: "transport" desenha
    // ▶/■, "folder" desenha a pasta, "tab" desenha a palavra. Sem a propriedade
    // nao se desenha nada, porque um botao sem icone e' um alvo invisivel e um
    // alvo invisivel nao passa nem no 2.5.8 nem no bom senso.
    //
    // Os quatro estados continuam distinguiveis sem cor: repouso e' o icone
    // quieto, sobreposto ganha o banho de alfa, premido escurece, e ligado troca
    // a forma (▶ vira ■, aba ganha o filete). A troca de palavra PLAY/STOP virou
    // troca de forma pelo mesmo motivo que a criou: o estado tem de se ler sem
    // ver cor, e forma e' o que o 1.4.1 pede.
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

        // Banho de alfa no hover e no premido: e' o unico relevo que sobrou, e
        // chega porque o icone ja tem forma propria. Preto no claro, branco no
        // escuro — o banho tem de se ver contra a superficie.
        const auto onDark = button.getProperties()["uiOnDark"];
        const auto wash = onDark ? juce::Colours::white : juce::Colours::black;
        if (shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown) {
            g.setColour(palette::alpha(wash, 0.08f));
            g.fillRoundedRectangle(reduced, 4.0f);
        }
        if (shouldDrawButtonAsDown) {
            g.setColour(palette::alpha(wash, 0.14f));
            g.fillRoundedRectangle(reduced, 4.0f);
        }

        // Aba activa: filete de acento em baixo. E' pista redundante — o texto da
        // aba activa vai em forte e escuro, e e' isso que carrega o estado — por
        // isso o 1,24:1 do laranja sobre o chassi nao conta aqui.
        if (button.getToggleState() &&
            button.getProperties()["uiIcon"].toString() == "tab") {
            g.setColour(palette::accent);
            g.fillRect(juce::Rectangle<float> {reduced.getX() + 4.0f, reduced.getBottom() - 2.5f,
                                               reduced.getWidth() - 8.0f, 2.0f});
        }

        // 2.4.7 Foco visivel. O desenho nao sabe se o botao tem o foco.
        // focusRing no claro (9,73:1), textOnDark no escuro (11,54:1): o anel
        // tem de contrastar com a superficie, e accent nas duas daria 1,24:1 e
        // 8,96:1 — a segunda ate passaria, mas o anel tem de ser um so.
        if (button.hasKeyboardFocus(true)) {
            g.setColour(button.getProperties()["uiOnDark"] ? palette::textOnDark
                                                           : palette::focusRing);
            g.drawRoundedRectangle(reduced.reduced(1.0f), 4.0f, 2.0f);
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
        // textSub no claro (6,38:1 — textMuted daria 2,87 e reprovaria),
        // textOnDarkSub no escuro (6,49:1).
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

        // Aba de vista: a palavra curta, forte e escura quando activa, normal e
        // secundaria quando nao. A mudanca de peso e cor carrega o estado; o
        // filete desenhado no fundo e' redundancia.
        const auto on = button.getToggleState();
        g.setFont(fonts().sans(10.0f, on));
        g.setColour(on ? palette::textDark : palette::textSub);
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

 private:
    // Transporte: triangulo para tocar, quadrado para parar, centrados na area
    // util do botao. O tamanho e' fracao do menor lado para nao encostar na
    // borda quando a janela encolhe o botao.
    static void paintTransportGlyph(juce::Graphics& g,
                                    const juce::Rectangle<float>& bounds,
                                    bool playing,
                                    juce::Colour ink) {
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;
        const auto centre = bounds.getCentre();

        g.setColour(ink);
        if (playing) {
            g.fillRect(juce::Rectangle<float> {centre.x - side * 0.5f, centre.y - side * 0.5f,
                                               side, side});
            return;
        }

        juce::Path triangle;
        triangle.addTriangle(centre.x - side * 0.5f, centre.y - side * 0.62f,
                             centre.x - side * 0.5f, centre.y + side * 0.62f,
                             centre.x + side * 0.62f, centre.y);
        g.fillPath(triangle);
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
    // cache para invalidar — o corpo inteiro sao tres fills e dois strokes.
    void drawKnobBody(juce::Graphics& g, juce::Point<float> centre, float radius) {
        const auto rim = juce::Rectangle<float> {centre.x - radius, centre.y - radius,
                                                  radius * 2.0f, radius * 2.0f};

        // Sombra projetada: o corpo flutua sobre o painel em vez de estar
        // colado nele. Tres elipses concentricas deslocadas 2 px para baixo, da
        // mais larga e fraca para a mais justa — um blur de verdade nao existe
        // barato no JUCE, e tres fills por knob a 60 Hz custam menos que um
        // DropShadowEffect por quadro.
        for (int i = 3; i >= 1; --i) {
            const auto spread = static_cast<float>(i);
            g.setColour(palette::alpha(juce::Colours::black, 0.10f - 0.025f * spread));
            g.fillEllipse(rim.expanded(spread).translated(0.0f, 2.0f));
        }

        // Disco, com luz suave de cima. O claro e' valBox e o escuro e' o proprio
        // painel: sem aro de chassisDark a separar, o disco assenta no modulo em
        // vez de flutuar sobre um anel.
        g.setGradientFill(juce::ColourGradient {
            palette::valBox, centre.translated(0.0f, -radius * 0.5f),
            palette::panelBg, centre.translated(0.0f, radius * 0.55f), false});
        g.fillEllipse(rim);

        // Aro fino, so para fechar a forma contra o painel.
        g.setColour(palette::alpha(palette::chassisBorder, 0.40f));
        g.drawEllipse(rim.reduced(0.5f), 1.0f);

        // Ponto de origem, no centro. Fica por baixo do arco e do ponteiro, e e'
        // por isso que a haste do valor comeca a 30 % do raio e nao no centro.
        const auto markerRadius = radius * 0.06f;
        g.setColour(palette::alpha(palette::chassisBorder, 0.65f));
        g.fillEllipse(centre.x - markerRadius, centre.y - markerRadius, markerRadius * 2.0f,
                      markerRadius * 2.0f);
    }
};

} // namespace opcoda