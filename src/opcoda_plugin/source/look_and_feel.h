#pragma once

#include "boxed_label.h"
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

    // Knob: corpo desenhado em codigo, com o arco de escala, o ponteiro e o anel
    // de foco por cima.
    //
    // O corpo e' fixo e o valor mexe-se no arco e no ponteiro. O corpo serrilhado
    // e' fixo como na peca fotografada, e o que muda e' o arco.
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

            // Ponteiro. O indicador do valor e' uma haste de verdade e nao o ponto
            // do centro da face: um knob com dois indicadores nao diz qual e' o
            // valor.
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
        }

        // Anel de foco. Sem ele, quem navega por teclado nao sabe qual knob
        // esta' ativo, e o criterio 2.4.7 do WCAG 2.2 pede foco visivel. E' o
        // unico sinal de que o controle esta' com o foco do teclado.
        if (slider.hasKeyboardFocus(true)) {
            g.setColour(palette::accent);
            g.drawEllipse(bounds.reduced(-3.0f), 2.0f);
        }
    }

    // Botao com bisel: borda clara em cima e escura embaixo, que e' a leitura de
    // tecla fisica. Pressionado inverte o bisel e afunda 1 px.
    //
    // Os quatro estados - repouso, sobreposto, premido e ligado - sao
    // distinguiveis sem cor. O ligado ganha uma barra de 1 px no lado esquerdo
    // porque o laranja sobre o chassis claro da 2,4:1
    // (design/DESIGN-SYSTEM.md:65), e um botao cuja unica pista de estado e' essa
    // cor nao passa o 1.4.3.
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
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

        const bool isOn = button.getToggleState();

        auto base = isOn ? palette::subPanel : backgroundColour;
        if (shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown) {
            base = base.brighter(0.12f);
        }
        if (shouldDrawButtonAsDown) {
            base = base.darker(0.10f);
        }

        // Gradiente vertical: a face e' mais clara em cima do que em baixo, e o
        // premido e' o inverso. E' o que da a sensacao de tecla em vez de
        // rectangulo.
        const auto topColour = shouldDrawButtonAsDown ? base.darker(0.10f)
                                                      : base.brighter(0.10f);
        const auto bottomColour = shouldDrawButtonAsDown ? base.brighter(0.02f)
                                                         : base.darker(0.06f);
        g.setGradientFill(juce::ColourGradient {topColour, reduced.getTopLeft(), bottomColour,
                                                reduced.getBottomLeft(), false});
        g.fillRoundedRectangle(reduced, 2.0f);

        // Bisel: realce em cima, sombra em baixo. A espessura e' a mesma nos dois
        // lados, que era exactamente o que esticar a peca fotografada nao
        // permitia.
        g.setColour(palette::alpha(juce::Colours::white,
                                   shouldDrawButtonAsDown ? 0.10f : 0.35f));
        g.drawHorizontalLine(juce::roundToInt(reduced.getY() + 0.5f), reduced.getX() + 1.5f,
                             reduced.getRight() - 1.5f);
        g.setColour(palette::alpha(palette::chassisBorder,
                                   shouldDrawButtonAsDown ? 0.35f : 0.95f));
        g.drawHorizontalLine(juce::roundToInt(reduced.getBottom() - 0.5f), reduced.getX() + 1.5f,
                             reduced.getRight() - 1.5f);
        g.drawRoundedRectangle(reduced, 2.0f, 1.0f);

        // Estado ligado: barra de 1 px a toda a altura do lado esquerdo mais um
        // contorno de acento. E' o sinal que sobrevive a quem nao distingue as
        // cores, e e' o mesmo que as tabs de secao removidas usavam.
        if (isOn) {
            g.setColour(palette::accent);
            g.fillRect(juce::Rectangle<float> {reduced.getX() + 0.5f, reduced.getY() + 1.5f, 1.0f,
                                               reduced.getHeight() - 3.0f});
            g.drawRoundedRectangle(reduced, 2.0f, 1.0f);
        }

        // 2.4.7 Foco visivel. O desenho nao sabe se o botao tem o foco.
        if (button.hasKeyboardFocus(true)) {
            g.setColour(palette::accent);
            g.drawRoundedRectangle(reduced.reduced(1.0f), 2.0f, 2.0f);
        }
    }

    // O texto e' desenhado em codigo e nunca vem de uma peca.
    //
    // Centralizado, com 3 px de recuo a esquerda para nao cair em cima da barra do
    // estado ligado. E' tambem o que torna o estado legivel sem cor, porque o
    // botao troca a palavra: PLAY para STOP.
    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool shouldDrawButtonAsDown) override {
        const auto bounds = button.getLocalBounds().toFloat();

        g.setFont(BoxedLabel::sansFont(10.0f, true));
        g.setColour(button.findColour(juce::TextButton::textColourOffId)
                        .withAlpha(shouldDrawButtonAsDown ? 0.9f : 1.0f));

        g.drawText(button.getButtonText(), bounds.reduced(3.0f, 0.0f),
                   juce::Justification::centred, false);
    }

private:
    // O corpo do knob: aro serrilhado, face recuada e ponto de origem.
    //
    // A luz vem de cima e da esquerda, como numa fotografia de estudio com a luz
    // assente na peca. E' por isso que o gradiente e' diagonal e nao radial a
    // partir do centro: um gradiente radial simetrico daria um disco sem
    // direccao, e sem direccao nao se le como superficie redonda.
    //
    // Os tres circulos sao o que separa "disco" de "botao rotativo": o aro
    // serrilhado le-se como pega, a face mais escova recua, e o ponto no centro
    // marca a origem, que e' o que se ve num knob real.
    void drawKnobBody(juce::Graphics& g, juce::Point<float> centre, float radius) {
        // O serrilhado e' um Path memorizado por posicao. Trinta e seis dentes por
        // knob sao 216 chamadas de drawLine por quadro a 60 Hz, e o Path e' um
        // stroke so. Os seis knobs sao do mesmo tamanho, portanto o cache vale
        // para quase todos os quadros da sessao.
        //
        // A comparacao e' dos dois valores e nao so do raio: o mesmo raio com o
        // centro noutro sitio e' um serrilhado na posicao errada, e os knobs
        // podem mudar de celula sem mudar de tamanho.
        if (std::abs(radius - cachedKnurlRadius_) > 0.5f ||
            centre.getDistanceFrom(cachedKnurlCentre_) > 0.5f) {
            rebuildKnurl(centre, radius);
        }

        const auto rim = juce::Rectangle<float> {centre.x - radius, centre.y - radius,
                                                  radius * 2.0f, radius * 2.0f};

        // Aro, com luz de cima a esquerda.
        g.setGradientFill(juce::ColourGradient {
            palette::valBox, centre.translated(-radius * 0.45f, -radius * 0.45f),
            palette::chassisDark, centre.translated(radius * 0.6f, radius * 0.6f), true});
        g.fillEllipse(rim);

        // Serrilhado. Trinta e seis dentes: menos que isso le-se como textura e
        // nao como pega, e mais que isso fecha num anel cinzento liso quando o
        // knob esta' a 20 px de diametro.
        g.setColour(palette::alpha(palette::chassisBorder, 0.34f));
        g.strokePath(knurlPath_,
                     juce::PathStrokeType {1.0f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded});

        // Face, recuada em relacao ao aro: mais escura, e com o gradiente na
        // mesma direccao para parecer um covao e nao um botao em cima.
        const auto faceRadius = radius * 0.72f;
        const auto face = juce::Rectangle<float> {centre.x - faceRadius, centre.y - faceRadius,
                                                  faceRadius * 2.0f, faceRadius * 2.0f};
        g.setGradientFill(juce::ColourGradient {
            palette::subPanel, centre.translated(-faceRadius * 0.4f, -faceRadius * 0.4f),
            palette::chassisDark, centre.translated(faceRadius * 0.5f, faceRadius * 0.5f), true});
        g.fillEllipse(face);

        // Contorno da face, para o recuo ter uma borda e nao so uma mudanca de
        // cor.
        g.setColour(palette::alpha(palette::chassisBorder, 0.30f));
        g.drawEllipse(face.reduced(0.5f), 1.0f);

        // Ponto de origem, no centro. Fica por baixo do arco e do ponteiro, e e'
        // por isso que a haste do valor comeca a 16 % do raio e nao no centro.
        const auto markerRadius = radius * 0.075f;
        g.setColour(palette::alpha(juce::Colours::white, 0.55f));
        g.fillEllipse(centre.x - markerRadius, centre.y - markerRadius, markerRadius * 2.0f,
                      markerRadius * 2.0f);
    }

    void rebuildKnurl(juce::Point<float> centre, float radius) {
        // Trinta e seis dentes: menos que isso le-se como textura e nao como pega.
        constexpr int kTeeth {36};
        constexpr float kKnurlInner {0.84f};

        knurlPath_.clear();
        for (int tooth = 0; tooth < kTeeth; ++tooth) {
            const auto angle = static_cast<float>(tooth) *
                               juce::MathConstants<float>::twoPi /
                               static_cast<float>(kTeeth);
            const auto sinA = std::sin(angle);
            const auto cosA = std::cos(angle);

            const auto inner = juce::Point<float> {centre.x + sinA * radius * kKnurlInner,
                                                    centre.y - cosA * radius * kKnurlInner};
            const auto outer = juce::Point<float> {centre.x + sinA * (radius - 1.0f),
                                                    centre.y - cosA * (radius - 1.0f)};
            knurlPath_.startNewSubPath(inner);
            knurlPath_.lineTo(outer);
        }

        cachedKnurlCentre_ = centre;
        cachedKnurlRadius_ = radius;
    }

    juce::Path knurlPath_;
    juce::Point<float> cachedKnurlCentre_ {};
    float cachedKnurlRadius_ {-1.0f};
};

} // namespace opcoda