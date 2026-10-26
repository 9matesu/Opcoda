#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace opcoda {

// LookAndFeel do Opcoda. knob, botao e anel de foco sao desenhados aqui; o resto
// fica no padrao do JUCE. nada vem de imagem: o corpo do knob e' um Path, e o
// arranque nao descodifica PNG.
//
// **os icones sao desenhados em codigo.** nao ha fonte de icones no repositorio e
// uma fonte so para dez glifos seria peso morto. As propriedades de cada botao
// dizem o que desenhar:
//
//   uiIcon  nome do glifo (wave, hex, entropy, zoomIn, zoomOut, fit, transport,
//           hold, pageUp, pageDown, home, end, chevron, folder)
//   uiLook  "tab"  tecla de funcao com filete
//           "icon" tecla plana de tira clara (transporte, navegacao)
//           "rail" tecla plana de tira escura (vistas, zoom)
//           ausente: tecla fisica escura
//
// os knobs sao juce::Slider de verdade: e' o que da foco por teclado, setas e
// AccessibilityHandler (docs/10-acessibilidade-w3c.md).
// Tecla da barra da janela do standalone. O V4 desenha estes botoes com as cores
// do sistema (vermelho, amarelo, verde) sobre uma barra opaca; aqui o cromo e' da
// chapa: fundo transparente, glifo de traco 1,4 px, hover em banho. o fechar fica
// ferrugem no hover, porque e' a unica tecla que faz desaparecer o instrumento.
class OpcodaWindowButton final : public juce::Button {
public:
    explicit OpcodaWindowButton(int type)
        : juce::Button(type == juce::DocumentWindow::closeButton ? "close" : "minimise"),
          type_(type) {}

    void paintButton(juce::Graphics& g, bool over, bool down) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto close = type_ == juce::DocumentWindow::closeButton;

        if (over || down) {
            g.setColour(close ? palette::alpha(palette::error, down ? 0.92f : 0.70f)
                              : palette::alpha(palette::ivory, down ? 0.16f : 0.09f));
            g.fillRoundedRectangle(bounds.reduced(3.0f, 4.0f), 3.0f);
        }

        g.setColour(close && (over || down) ? palette::charcoalDeep
                                            : palette::alpha(palette::ivory, 0.62f));
        const auto c = bounds.getCentre();
        constexpr float arm {4.5f};
        if (close) {
            g.drawLine(c.x - arm, c.y - arm, c.x + arm, c.y + arm, 1.4f);
            g.drawLine(c.x + arm, c.y - arm, c.x - arm, c.y + arm, 1.4f);
        } else {
            g.drawLine(c.x - arm, c.y, c.x + arm, c.y, 1.4f);
        }
    }

private:
    int type_;
};

class OpcodaLookAndFeel : public juce::LookAndFeel_V4 {
public:
    OpcodaLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, palette::chassis);

        setColour(juce::Slider::textBoxTextColourId, palette::textDark);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

        setColour(juce::Label::textColourId, palette::textDark);

        setColour(juce::TextButton::buttonColourId, palette::charcoal);
        setColour(juce::TextButton::buttonOnColourId, palette::accent);
        setColour(juce::TextButton::textColourOffId, palette::textOnDark);
        setColour(juce::TextButton::textColourOnId, palette::charcoalDeep);

        // menus e avisos: o JUCE trazia aqui o azul do sistema. o standalone so'
        // mostra menus de escolha e a caixa de aviso; ambos passam a ser carvao.
        setColour(juce::PopupMenu::backgroundColourId, palette::charcoalDeep);
        setColour(juce::PopupMenu::textColourId, palette::textOnDark);
        setColour(juce::PopupMenu::headerTextColourId, palette::textOnDarkSub);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,
                  palette::alpha(palette::ivory, 0.09f));
        setColour(juce::PopupMenu::highlightedTextColourId, palette::ivory);

        setColour(juce::AlertWindow::backgroundColourId, palette::charcoal);
        setColour(juce::AlertWindow::textColourId, palette::textOnDark);
        setColour(juce::AlertWindow::outlineColourId, palette::knobEdge);

        setColour(juce::TooltipWindow::backgroundColourId, palette::charcoalDeep);
        setColour(juce::TooltipWindow::textColourId, palette::textOnDark);
        setColour(juce::TooltipWindow::outlineColourId, palette::alpha(palette::ivory, 0.14f));
    }

    // knob de equipamento: corpo escuro, aro de marcas, arco de valor e ponteiro.
    // o corpo nao muda com o valor; so o arco, o ponteiro e o foco mudam.
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
        const float outer = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

        if (outer <= 8.0f) {
            return;
        }

        // o disco visual e' o que interessa: bodyRadius a 2.5 px da borda da
        // celula da' disco = S-5 num slider de 96. o recuo antigo (-5) comia 16 px
        // de knob e deixava o controlo pequeno dentro de uma celula grande.
        const float bodyRadius = outer - 2.5f;
        const float arcRadius = outer - 1.0f;
        const float angle = rotaryStartAngle +
                            sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // marcas fixas: a escala fica visivel antes de se tocar no knob.
        for (int tick = 0; tick <= 10; ++tick) {
            const auto t = static_cast<float>(tick) / 10.0f;
            const auto tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const auto inner = arcRadius - (tick % 5 == 0 ? 3.6f : 2.2f);
            const auto sx = centre.x + std::sin(tickAngle) * inner;
            const auto sy = centre.y - std::cos(tickAngle) * inner;
            const auto ex = centre.x + std::sin(tickAngle) * arcRadius;
            const auto ey = centre.y - std::cos(tickAngle) * arcRadius;
            g.setColour(palette::alpha(palette::textSub,
                                       tick % 5 == 0 ? 0.85f : 0.45f));
            g.drawLine(sx, sy, ex, ey, tick % 5 == 0 ? 1.6f : 1.0f);
        }

        drawKnobBody(g, centre, bodyRadius);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(palette::alpha(palette::charcoalDeep, 0.45f));
        g.strokePath(track, juce::PathStrokeType {1.6f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});

        juce::Path valueArc;
        valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, angle, true);
        g.setColour(palette::rust);
        g.strokePath(valueArc, juce::PathStrokeType {2.4f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});

        // ponteiro de marfim: o laranja do arco ja' carrega o valor; a agulha
        // tem de se ler sobre o corpo escuro.
        const auto inner = bodyRadius * 0.34f;
        const auto tip = bodyRadius - 1.0f;
        const auto tailX = centre.x + std::sin(angle) * inner;
        const auto tailY = centre.y - std::cos(angle) * inner;
        const auto tipX = centre.x + std::sin(angle) * tip;
        const auto tipY = centre.y - std::cos(angle) * tip;

        g.setColour(palette::alpha(juce::Colours::black, 0.45f));
        g.drawLine(tailX + 1.0f, tailY + 1.0f, tipX + 1.0f, tipY + 1.0f, 3.0f);
        g.setColour(palette::ivory);
        g.drawLine(tailX, tailY, tipX, tipY, 2.6f);

        if (slider.hasKeyboardFocus(true)) {
            drawFocusRing(g, bounds.expanded(1.5f));
        }
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool highlighted,
                              bool down) override {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        if (bounds.getWidth() <= 1.0f || bounds.getHeight() <= 1.0f) {
            return;
        }

        const auto look = button.getProperties()["uiLook"].toString();
        const auto isTab = look == "tab";
        const auto isIcon = look == "icon";
        const auto isRail = look == "rail";
        const auto isBar = look == "bar";
        const auto isRow = look == "row";
        const auto isTransport = button.getProperties()["uiIcon"].toString() == "transport";
        const auto on = button.getToggleState();

        if (isRow) {
            // linha de lista (entradas MIDI no ecra de configuracao): sem moldura.
            // o estado vive num quadrado a' esquerda e o hover e' um banho de
            // carvao a 7%, para a lista nao competir com os knobs do painel.
            if (highlighted && !down) {
                g.setColour(palette::alpha(palette::charcoalDeep, 0.07f));
                g.fillRoundedRectangle(bounds, 2.0f);
            }
            const auto box = juce::Rectangle<float> {bounds.getX() + 2.0f,
                                                    bounds.getCentreY() - 5.5f, 11.0f, 11.0f};
            if (on) {
                g.setColour(palette::accent);
                g.fillRoundedRectangle(box, 1.5f);
            }
            g.setColour(on ? palette::alpha(palette::charcoalDeep, 0.85f)
                           : palette::alpha(palette::charcoal, 0.45f));
            g.drawRoundedRectangle(box, 1.5f, 1.2f);
            if (on) {
                // visto a carvao dentro do quadrado laranja: a cor sozinha nao
                // diz "ligado" a quem nao distingue laranja de creme.
                juce::Path tick;
                tick.startNewSubPath(box.getX() + 2.6f, box.getCentreY());
                tick.lineTo(box.getX() + 4.6f, box.getBottom() - 3.2f);
                tick.lineTo(box.getRight() - 2.3f, box.getY() + 3.4f);
                g.setColour(palette::charcoalDeep);
                g.strokePath(tick, juce::PathStrokeType {1.7f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded});
            }
        } else if (isBar) {
            // tecla larga: retangulo de chapa com icone e nome. e' a lingua das
            // vistas e do transporte, onde o glifo sozinho nao chegava para dizer
            // o que a tecla faz.
            //
            // **fundo opaco.** era um banho a 45 % e a chapa de baixo aparecia
            // por tras: sobre a barra da janela o texto ficava com duas sombras.
            // a mistura passa a ser feita na cor, nao na transparencia.
            auto fill = on ? palette::charcoal.interpolatedWith(palette::accent, 0.30f)
                           : palette::charcoal;
            if (down) {
                fill = palette::charcoalDeep;
            } else if (highlighted && !on) {
                fill = palette::charcoal.interpolatedWith(palette::ivory, 0.10f);
            }
            g.setColour(fill);
            g.fillRoundedRectangle(bounds, 2.5f);
            g.setColour(palette::alpha(palette::knobEdge, on ? 0.85f : 0.45f));
            g.drawRoundedRectangle(bounds, 2.5f, 1.0f);
            if (on) {
                g.setColour(palette::accent);
                g.fillRect(juce::Rectangle<float> {bounds.getX() + 1.0f,
                                                   bounds.getY() + 4.0f, 2.0f,
                                                   bounds.getHeight() - 8.0f});
            }
        } else if (isTab || isIcon || isRail) {
            // cromo secundario, padrao Ableton: desligado e' contorno de 1 px,
            // ligado leva carvao e filete ambar de 2 px. sem halo.
            // na rail escura o carvao ligado nao se distingue do fundo: a tecla
            // activa leva um banho de laranja e um filete vertical a' esquerda, que
            // e' a lingua de uma tira empilhada. nas tiras claras fica o filete
            // horizontal de sempre.
            auto fill = on ? (isRail ? palette::alpha(palette::accent, 0.20f)
                                     : palette::charcoal)
                           : juce::Colours::transparentBlack;
            if (down) {
                fill = on ? palette::charcoalDeep
                          : palette::alpha(palette::charcoalDeep, 0.22f);
            } else if (highlighted && !on) {
                fill = isRail ? palette::alpha(palette::ivory, 0.08f)
                              : palette::alpha(palette::charcoalDeep, 0.12f);
            }

            g.setColour(fill);
            g.fillRoundedRectangle(bounds, isIcon || isRail ? 3.0f : 4.0f);
            if (on) {
                g.setColour(palette::accent);
                if (isRail) {
                    g.fillRect(juce::Rectangle<float> {bounds.getX() + 1.0f,
                                                       bounds.getY() + 6.0f, 2.0f,
                                                       bounds.getHeight() - 12.0f});
                } else {
                    g.fillRect(juce::Rectangle<float> {
                        bounds.getX() + (isIcon ? 3.0f : 5.0f),
                        bounds.getBottom() - 3.0f,
                        bounds.getWidth() - (isIcon ? 6.0f : 10.0f), 2.0f});
                }
            }
            // sem moldura no estado desligado: o icone desligado e' so o glifo, mais
            // fraco. a moldura a 1 px em setas e vistas de 30 px era o que as fazia
            // parecer botoes de web.
        } else {
            auto fill = palette::charcoal;
            if (isTransport && on) {
                fill = palette::accent;
            } else if (highlighted && !down) {
                fill = palette::panelBg;
            }
            if (down) {
                fill = palette::charcoalDeep;
            }

            g.setColour(fill);
            g.fillRoundedRectangle(bounds, 3.0f);
            g.setColour(isTransport && on ? palette::accentSoft : palette::knobEdge);
            g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
            g.setColour(palette::alpha(palette::ivory, 0.10f));
            g.drawHorizontalLine(juce::roundToInt(bounds.getY() + 1.0f),
                                 bounds.getX() + 3.0f, bounds.getRight() - 3.0f);
        }

        if (button.hasKeyboardFocus(true)) {
            drawFocusRing(g, bounds.reduced(1.0f));
        }
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool) override {
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        if (bounds.getWidth() <= 1.0f || bounds.getHeight() <= 1.0f) {
            return;
        }

        const auto kind = button.getProperties()["uiIcon"].toString();
        const auto look = button.getProperties()["uiLook"].toString();
        const auto on = button.getToggleState();

        juce::Colour ink = palette::textOnDark;
        if (!button.isEnabled()) {
            ink = palette::textMuted;
        } else if (look == "row") {
            // lista sobre creme: as duas posicoes tem de ler no mesmo fundo.
            ink = on ? palette::textDark : palette::textSub;
        } else if (look == "tab" || look == "icon" || look == "rail") {
            ink = on ? palette::ivory : (look == "rail" ? palette::textOnDarkSub
                                                        : palette::textSub);
        } else if (look == "bar") {
            ink = on ? palette::ivory : palette::textOnDarkSub;
        } else if (kind == "transport" && on) {
            ink = palette::charcoalDeep;
        }

        if (look == "row") {
            // nome a' esquerda depois do quadrado de estado, com o mesmo recuo
            // em todas as linhas da lista.
            g.setFont(fonts().sans(11.0f, on));
            g.setColour(ink);
            g.drawText(button.getButtonText(), bounds.withTrimmedLeft(20.0f).reduced(2.0f, 0.0f),
                       juce::Justification::centredLeft, true);
            if (button.hasKeyboardFocus(true)) {
                drawFocusRing(g, bounds.reduced(1.0f));
            }
            return;
        }

        if (look == "bar") {
            // glifo a' esquerda, nome a' direita: duas leituras no mesmo rectangulo.
            const auto glyphBox = bounds.withWidth(18.0f).reduced(2.0f);
            if (!paintGlyph(g, kind, glyphBox, ink, on)) {
                // botao so' de texto (transporte): o nome ocupa o rectangulo todo
                g.setFont(fonts().sans(9.0f, true));
                g.setColour(ink);
                g.drawText(button.getButtonText(), bounds.reduced(2.0f, 0.0f),
                           juce::Justification::centred, false);
                return;
            }
            g.setFont(fonts().sans(9.0f, on));
            g.setColour(ink);
            g.drawText(button.getButtonText(), bounds.withTrimmedLeft(20.0f).reduced(2.0f, 0.0f),
                       juce::Justification::centredLeft, false);
            return;
        }

        if (paintGlyph(g, kind, bounds, ink, on)) {
            return;
        }

        g.setFont(fonts().sans(look == "icon" ? 9.5f : 10.5f, on || look.isEmpty()));
        g.setColour(ink);
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

    // -- cromo da janela do standalone --
    // a barra nao e' do JUCE: grafite um tom abaixo do chassis, a marca a'
    // esquerda e duas teclas de janela sem o fundo cinzento do V4.
    //
    // **a assinatura mudou de sitio.** o nome estava no header do editor, na
    // mesma fila do preset e do ficheiro, e empurrava esse bloco para o canto.
    // aqui a barra diz o nome com a fonte do titulo, e o editor fica so' com o
    // que e' operavel.
    void drawDocumentWindowTitleBar(juce::DocumentWindow&, juce::Graphics& g, int w, int h,
                                    int, int, const juce::Image*, bool) override {
        g.fillAll(palette::chassisDark);
        g.setColour(palette::alpha(palette::ivory, 0.08f));
        g.drawHorizontalLine(h - 1, 0.0f, static_cast<float>(w));

        constexpr float kSide {22.0f};
        const auto mark = juce::Rectangle<float> {12.0f,
                                                  (static_cast<float>(h) - kSide) * 0.5f,
                                                  kSide, kSide};
        paintMark(g, mark);

        g.setColour(palette::textOnDark);
        g.setFont(fonts().sans(17.0f, true));
        g.drawText("Opcoda",
                   juce::Rectangle<int> {juce::roundToInt(mark.getRight()) + 9, 0, 170, h},
                   juce::Justification::centredLeft, false);
    }

    juce::Button* createDocumentWindowButton(int buttonType) override {
        return new OpcodaWindowButton(buttonType);
    }

    void positionDocumentWindowButtons(juce::DocumentWindow&, int titleBarX, int titleBarY,
                                       int titleBarW, int titleBarH, juce::Button* minimiseButton,
                                       juce::Button* maximiseButton, juce::Button* closeButton,
                                       bool positionTitleBarButtonsOnLeft) override {
        // empilhadas a' direita, 34 px cada e 3 px de folga ao canto (as teclas
        // da interface medem o mesmo). o lado esquerdo existe para nao deixar a
        // assinatura da funcao a mentir.
        constexpr int kButton {34};
        constexpr int kPad {4};
        auto x = positionTitleBarButtonsOnLeft ? titleBarX + kPad
                                               : titleBarX + titleBarW - kButton - kPad;

        const auto place = [&](juce::Button* b) {
            if (b == nullptr) {
                return;
            }
            b->setBounds(x, titleBarY + 3, kButton, titleBarH - 6);
            x += positionTitleBarButtonsOnLeft ? kButton : -kButton;
        };

        if (positionTitleBarButtonsOnLeft) {
            place(closeButton);
            place(minimiseButton);
            place(maximiseButton);
        } else {
            place(closeButton);
            place(maximiseButton);
            place(minimiseButton);
        }
    }

    // -- menus: a ultima superficie que ainda vinha do sistema --
    // o menu de escolha do WINDOW e a lista de dispositivos passam por aqui. e'
    // carvao de cima a baixo, com o visto em laranja em vez do risco do V4.
    juce::Font getPopupMenuFont() override {
        return fonts().sans(11.5f);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override {
        g.fillAll(palette::charcoalDeep);
        g.setColour(palette::alpha(palette::ivory, 0.14f));
        g.drawRect(0, 0, width, height, 1);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                           bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable*, const juce::Colour* textColour) override {
        if (isSeparator) {
            g.setColour(palette::alpha(palette::ivory, 0.10f));
            g.fillRect(area.withSizeKeepingCentre(area.getWidth() - 12, 1));
            return;
        }

        if (isHighlighted) {
            g.setColour(palette::alpha(palette::ivory, 0.09f));
            g.fillRoundedRectangle(area.reduced(3, 1).toFloat(), 2.0f);
        }

        if (isTicked) {
            // quadrado laranja, a mesma marca das listas de dispositivos.
            g.setColour(palette::accent);
            g.fillRoundedRectangle(juce::Rectangle<float> {static_cast<float>(area.getX() + 7),
                                                          static_cast<float>(area.getCentreY()) -
                                                              2.5f,
                                                          5.0f, 5.0f},
                                   1.0f);
        }

        auto ink = isActive ? (isHighlighted ? palette::ivory : palette::textOnDark)
                            : palette::textMuted;
        if (textColour != nullptr) {
            ink = *textColour;
        }

        g.setColour(ink);
        g.setFont(fonts().sans(11.5f, isTicked));
        g.drawText(text, area.withTrimmedLeft(20).withTrimmedRight(hasSubMenu ? 18 : 8),
                   juce::Justification::centredLeft, true);

        if (hasSubMenu) {
            const auto x = static_cast<float>(area.getRight() - 12);
            const auto y = static_cast<float>(area.getCentreY());
            juce::Path chevron;
            chevron.addTriangle(x - 2.0f, y - 3.5f, x + 2.0f, y, x - 2.0f, y + 3.5f);
            g.setColour(ink);
            g.fillPath(chevron);
        }

        if (shortcutKeyText.isNotEmpty()) {
            g.setFont(fonts().sans(9.5f));
            g.setColour(palette::textOnDarkSub);
            g.drawText(shortcutKeyText, area.withTrimmedRight(10),
                       juce::Justification::centredRight, true);
        }
    }

private:
    // combo da matriz: chapa escura, sem moldura, caret proprio. o texto usa a
    // cor que a vista poe em textColourId.
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override {
        const auto bounds = juce::Rectangle<float> {0.0f, 0.0f,
                                                    static_cast<float>(width),
                                                    static_cast<float>(height)};
        g.setColour(palette::alpha(palette::charcoal, 0.65f));
        g.fillRoundedRectangle(bounds, 3.0f);

        const auto c = bounds.getCentre();
        juce::Path chevron;
        chevron.addTriangle(c.x - 3.5f, c.y - 1.5f, c.x + 3.5f, c.y - 1.5f, c.x, c.y + 2.5f);
        g.setColour(palette::alpha(palette::textOnDark, 0.55f));
        g.fillPath(chevron);

        if (box.hasKeyboardFocus(true)) {
            drawFocusRing(g, bounds.reduced(1.0f));
        }
    }

    [[nodiscard]] juce::Font getComboBoxFont(juce::ComboBox&) override {
        return fonts().sans(11.5f);
    }

    // barra bipolar do amount da matriz: trilho de 3 px, fill laranja do centro
    // para o thumb, handle de 4 px. o centro e' nada, e o desenho diz isso sem
    // texto.
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override {
        if (style != juce::Slider::LinearHorizontal) {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.0f, 1.0f,
                                              style, slider);
            return;
        }

        const auto track = juce::Rectangle<float> {
            static_cast<float>(x), static_cast<float>(y) + static_cast<float>(height) * 0.5f - 1.5f,
            static_cast<float>(width), 3.0f};
        g.setColour(palette::alpha(palette::charcoalDeep, 0.92f));
        g.fillRoundedRectangle(track, 1.5f);

        const auto centreX = track.getCentreX();
        const auto from = juce::jmin(centreX, sliderPos);
        const auto to = juce::jmax(centreX, sliderPos);
        g.setColour(palette::accent);
        g.fillRoundedRectangle(juce::Rectangle<float> {from, track.getY(),
                                                       juce::jmax(2.0f, to - from),
                                                       track.getHeight()},
                               1.5f);

        g.setColour(palette::ivory);
        g.fillRoundedRectangle(
            juce::Rectangle<float> {sliderPos - 2.0f, track.getY() - 4.0f, 4.0f,
                                    track.getHeight() + 8.0f},
            1.5f);

        if (slider.hasKeyboardFocus(true)) {
            drawFocusRing(g, juce::Rectangle<float> {static_cast<float>(x),
                                                     static_cast<float>(y),
                                                     static_cast<float>(width),
                                                     static_cast<float>(height)});
        }
    }

    static void drawFocusRing(juce::Graphics& g, juce::Rectangle<float> bounds) {
        g.setColour(palette::charcoalDeep);
        g.drawRoundedRectangle(bounds, 2.5f, 3.0f);
        g.setColour(palette::focusRing);
        g.drawRoundedRectangle(bounds, 2.5f, 1.5f);
    }

public:
    // a marca: a caixa laranja com as duas letras da casa, na fonte do nome.
    //
    // **o icone e' o texto.** tres graos num quadrado de 16 px nao se leem como
    // nada; "Op" le-se, e' a mesma fonte do titulo e aguenta o tamanho do icone
    // da barra de tarefas sem virar borrao. o tamanho da letra vem da caixa, para
    // a mesma funcao servir a barra (22 px), o ecra de ajustes (16) e o icone da
    // janela (256).
    static void paintMark(juce::Graphics& g, juce::Rectangle<float> box) {
        g.setColour(palette::accent);
        g.fillRoundedRectangle(box, box.getWidth() * 0.17f);
        g.setColour(palette::chassisDark);
        g.setFont(fonts().sans(box.getHeight() * 0.56f, true));
        g.drawText("Op", box, juce::Justification::centred, false);
    }

    // devolve true quando desenhou um glifo. o texto so' fica para as teclas que
    // nao tem icone. publico porque as abas de pasta desenham o proprio glifo.
    static bool paintGlyph(juce::Graphics& g,
                           const juce::String& kind,
                           const juce::Rectangle<float>& bounds,
                           juce::Colour ink,
                           bool on) {
        if (kind.isEmpty()) {
            return false;
        }

        const auto c = bounds.getCentre();
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());

        if (kind == "transport") {
            paintTransportGlyph(g, bounds, on ? 1 : 0, ink);
            return true;
        }
        if (kind == "hold") {
            paintLockGlyph(g, bounds, ink);
            return true;
        }
        if (kind == "folder") {
            paintFolderGlyph(g, bounds, ink);
            return true;
        }
        if (kind == "chevron") {
            juce::Path chevron;
            chevron.addTriangle(c.x - side * 0.16f, c.y - side * 0.08f,
                                c.x + side * 0.16f, c.y - side * 0.08f,
                                c.x, c.y + side * 0.12f);
            g.setColour(ink);
            g.fillPath(chevron);
            return true;
        }
        if (kind == "wave") {
            // onda: linha sinusoidal cheia de picos, a leitura da vista 1.
            juce::Path wave;
            const auto w = side * 0.58f;
            const auto h = side * 0.30f;
            const auto left = c.x - w * 0.5f;
            constexpr int kSteps {24};
            for (int i = 0; i <= kSteps; ++i) {
                const auto t = static_cast<float>(i) / static_cast<float>(kSteps);
                const auto x = left + t * w;
                const auto y = c.y - std::sin(t * juce::MathConstants<float>::twoPi * 1.5f) *
                                           h * (0.35f + 0.65f * std::sin(t * 3.1f));
                if (i == 0) {
                    wave.startNewSubPath(x, y);
                } else {
                    wave.lineTo(x, y);
                }
            }
            g.setColour(ink);
            g.strokePath(wave, juce::PathStrokeType {1.7f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});
            return true;
        }
        if (kind == "hex") {
            // grelha de bytes: quatro por tres pontos.
            const auto step = side * 0.17f;
            const auto dot = juce::jmax(1.6f, side * 0.055f);
            g.setColour(ink);
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 4; ++col) {
                    g.fillRoundedRectangle(
                        juce::Rectangle<float> {c.x - step * 1.5f +
                                                    static_cast<float>(col) * step -
                                                    dot * 0.5f,
                                                c.y - step +
                                                    static_cast<float>(row) * step -
                                                    dot * 0.5f,
                                                dot, dot},
                        1.0f);
                }
            }
            return true;
        }
        if (kind == "entropy") {
            // curva de entropia: quatro colunas ascendentes.
            const auto w = side * 0.12f;
            const auto gap = side * 0.07f;
            const auto base = c.y + side * 0.24f;
            const float heights[] {0.16f, 0.30f, 0.46f, 0.34f};
            g.setColour(ink);
            for (int i = 0; i < 4; ++i) {
                g.fillRoundedRectangle(
                    juce::Rectangle<float> {c.x - (w + gap) * 2.0f +
                                                static_cast<float>(i) * (w + gap),
                                            base - side * heights[i], w,
                                            side * heights[i]},
                    1.0f);
            }
            return true;
        }
        if (kind == "zoomIn" || kind == "zoomOut") {
            const auto r = side * 0.22f;
            const auto lens = juce::Point<float> {c.x - side * 0.04f, c.y - side * 0.04f};
            g.setColour(ink);
            g.drawEllipse(juce::Rectangle<float> {lens.x - r, lens.y - r, r * 2.0f, r * 2.0f},
                          1.7f);
            g.drawLine(lens.x + r * 0.72f, lens.y + r * 0.72f,
                       lens.x + r * 1.55f, lens.y + r * 1.55f, 1.9f);
            g.drawLine(lens.x - r * 0.45f, lens.y, lens.x + r * 0.45f, lens.y, 1.5f);
            if (kind == "zoomIn") {
                g.drawLine(lens.x, lens.y - r * 0.45f, lens.x, lens.y + r * 0.45f, 1.5f);
            }
            return true;
        }
        if (kind == "fit") {
            // quatro cantos de mira.
            const auto w = side * 0.46f;
            const auto h = side * 0.36f;
            const auto arm = side * 0.13f;
            const auto left = c.x - w * 0.5f;
            const auto right = c.x + w * 0.5f;
            const auto top = c.y - h * 0.5f;
            const auto bottom = c.y + h * 0.5f;
            g.setColour(ink);
            g.drawLine(left, top, left + arm, top, 1.7f);
            g.drawLine(left, top, left, top + arm, 1.7f);
            g.drawLine(right, top, right - arm, top, 1.7f);
            g.drawLine(right, top, right, top + arm, 1.7f);
            g.drawLine(left, bottom, left + arm, bottom, 1.7f);
            g.drawLine(left, bottom, left, bottom - arm, 1.7f);
            g.drawLine(right, bottom, right - arm, bottom, 1.7f);
            g.drawLine(right, bottom, right, bottom - arm, 1.7f);
            return true;
        }
        if (kind == "pageUp" || kind == "pageDown") {
            // up: base em baixo, ponta em cima e barra acima. down e' o espelho.
            const auto dir = kind == "pageUp" ? 1.0f : -1.0f;
            const auto w = side * 0.20f;
            const auto h = side * 0.12f;
            juce::Path chevron;
            chevron.addTriangle(c.x - w, c.y + dir * h, c.x + w, c.y + dir * h,
                                c.x, c.y - dir * h * 0.4f);
            g.setColour(ink);
            g.fillPath(chevron);
            g.fillRect(juce::Rectangle<float> {c.x - w, c.y - dir * h * 1.8f, w * 2.0f, 1.6f});
            return true;
        }
        if (kind == "home" || kind == "end") {
            // home aponta para a esquerda com a barra a' esquerda (|◀); end e' o
            // espelho (▶|). a barra fica do lado da ponta, como nos transportes.
            const auto dir = kind == "home" ? -1.0f : 1.0f;
            const auto w = side * 0.15f;
            const auto h = side * 0.20f;
            juce::Path arrow;
            arrow.addTriangle(c.x - dir * w, c.y - h, c.x - dir * w, c.y + h,
                              c.x + dir * w, c.y);
            g.setColour(ink);
            g.fillPath(arrow);
            const auto barX = c.x + dir * w * 1.35f;
            g.fillRect(juce::Rectangle<float> {barX - 1.0f, c.y - h, 2.0f, h * 2.0f});
            return true;
        }

        if (kind == "matrix") {
            // duas rotas: no, no, seta. a leitura e' a vista que liga fontes a
            // destinos, e nao uma grelha de bytes.
            const auto r = juce::jmax(1.6f, side * 0.045f);
            const auto leftX = c.x - side * 0.26f;
            const auto rightX = c.x + side * 0.26f;
            const auto y1 = c.y - side * 0.13f;
            const auto y2 = c.y + side * 0.13f;
            g.setColour(ink);
            g.drawEllipse(leftX - r, y1 - r, r * 2.0f, r * 2.0f, 1.4f);
            g.drawEllipse(leftX - r, y2 - r, r * 2.0f, r * 2.0f, 1.4f);
            g.drawEllipse(rightX - r, c.y - r, r * 2.0f, r * 2.0f, 1.4f);
            g.drawLine(leftX + r, y1, rightX - r, c.y, 1.3f);
            g.drawLine(leftX + r, y2, rightX - r, c.y, 1.3f);
            return true;
        }

        if (kind == "close") {
            // cruz de 9 px, o mesmo traco das teclas da barra da janela.
            const auto arm = side * 0.16f;
            g.setColour(ink);
            g.drawLine(c.x - arm, c.y - arm, c.x + arm, c.y + arm, 1.5f);
            g.drawLine(c.x + arm, c.y - arm, c.x - arm, c.y + arm, 1.5f);
            return true;
        }

        if (kind == "settings") {
            // tres cursos com o puxador em posicoes diferentes: a leitura de
            // "ajustes" num aparelho, sem engrenagem. o espaco entre os cursos e'
            // um terco do lado: a versao anterior encostava-os e a 14 px os tres
            // puxadores fundiam-se num borrao.
            const auto w = side * 0.40f;
            const auto gap = side * 0.32f;
            const float handles[] {-0.5f, 0.15f, -0.3f};
            for (int i = 0; i < 3; ++i) {
                const auto y = c.y + (static_cast<float>(i) - 1.0f) * gap;
                g.setColour(palette::alpha(ink, 0.80f));
                g.drawLine(c.x - w, y, c.x + w, y, 1.1f);
                g.setColour(ink);
                g.fillRoundedRectangle(juce::Rectangle<float> {c.x + handles[i] * w - 1.3f,
                                                               y - 1.6f, 2.6f, 3.2f},
                                       0.8f);
            }
            return true;
        }

        // -- abas de pasta: um glifo por modulo, 12 px de leitura --
        if (kind == "grainMod") {
            // tres rajadas de altura variavel: grao e' rajada curta.
            const float heights[] {0.34f, 0.62f, 0.42f};
            const auto w = side * 0.10f;
            const auto gap = side * 0.09f;
            const auto base = c.y + side * 0.26f;
            g.setColour(ink);
            for (int i = 0; i < 3; ++i) {
                g.fillRoundedRectangle(
                    juce::Rectangle<float> {c.x - (w + gap) * 1.5f +
                                                static_cast<float>(i) * (w + gap),
                                            base - side * heights[i], w,
                                            side * heights[i]},
                    1.0f);
            }
            return true;
        }
        if (kind == "envMod") {
            // ADSR: sobe, cai, patamar, desce.
            const auto w = side * 0.56f;
            const auto h = side * 0.40f;
            const auto left = c.x - w * 0.5f;
            const auto base = c.y + h * 0.5f;
            juce::Path env;
            env.startNewSubPath(left, base);
            env.lineTo(left + w * 0.18f, base - h);
            env.lineTo(left + w * 0.38f, base - h * 0.30f);
            env.lineTo(left + w * 0.72f, base - h * 0.30f);
            env.lineTo(left + w, base);
            g.setColour(ink);
            g.strokePath(env, juce::PathStrokeType {1.6f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded});
            return true;
        }
        if (kind == "filterMod") {
            // resposta passa-baixa: plano e queda, com o corte marcado.
            const auto w = side * 0.56f;
            const auto h = side * 0.36f;
            const auto left = c.x - w * 0.5f;
            const auto top = c.y - h * 0.5f;
            juce::Path response;
            response.startNewSubPath(left, top);
            response.lineTo(left + w * 0.52f, top);
            response.quadraticTo(left + w * 0.78f, top, left + w * 0.88f, top + h);
            response.lineTo(left + w, top + h);
            g.setColour(ink);
            g.strokePath(response, juce::PathStrokeType {1.6f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded});
            return true;
        }
        if (kind == "modMod") {
            // senoide: o lfo.
            juce::Path sine;
            const auto w = side * 0.56f;
            const auto h = side * 0.30f;
            const auto left = c.x - w * 0.5f;
            constexpr int kSteps {20};
            for (int i = 0; i <= kSteps; ++i) {
                const auto t = static_cast<float>(i) / static_cast<float>(kSteps);
                const auto x = left + t * w;
                const auto y =
                    c.y - std::sin(t * juce::MathConstants<float>::twoPi) * h;
                if (i == 0) {
                    sine.startNewSubPath(x, y);
                } else {
                    sine.lineTo(x, y);
                }
            }
            g.setColour(ink);
            g.strokePath(sine, juce::PathStrokeType {1.6f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});
            return true;
        }
        if (kind == "cloudMod") {
            // dispersao: cinco pontos, o grao solto na nuvem.
            const juce::Point<float> dots[] {
                {c.x - side * 0.20f, c.y - side * 0.10f},
                {c.x - side * 0.04f, c.y - side * 0.22f},
                {c.x + side * 0.14f, c.y - side * 0.04f},
                {c.x - side * 0.10f, c.y + side * 0.14f},
                {c.x + side * 0.18f, c.y + side * 0.20f}};
            const auto r = juce::jmax(1.5f, side * 0.05f);
            g.setColour(ink);
            for (const auto& dot : dots) {
                g.fillEllipse(dot.x - r, dot.y - r, r * 2.0f, r * 2.0f);
            }
            return true;
        }
        if (kind == "keysMod") {
            // piano de tres teclas: branca, preta, branca.
            const auto w = side * 0.16f;
            const auto h = side * 0.34f;
            const auto left = c.x - w * 1.5f;
            const auto base = c.y + h * 0.5f;
            g.setColour(ink);
            for (int i = 0; i < 3; ++i) {
                g.drawRoundedRectangle(
                    juce::Rectangle<float> {left + static_cast<float>(i) * w,
                                            base - h, w, h},
                    1.0f, 1.3f);
            }
            g.fillRect(juce::Rectangle<float> {left + w * 0.5f + 1.0f, base - h,
                                               w * 0.6f, h * 0.55f});
            return true;
        }
        if (kind == "chevronLeft" || kind == "chevronRight") {
            // seta solida de preset, sem barra: aponta para o vizinho.
            const auto dir = kind == "chevronLeft" ? -1.0f : 1.0f;
            const auto w = side * 0.14f;
            const auto h = side * 0.18f;
            juce::Path chevron;
            chevron.addTriangle(c.x + dir * w, c.y - h, c.x + dir * w, c.y + h,
                                c.x - dir * w, c.y);
            g.setColour(ink);
            g.fillPath(chevron);
            return true;
        }

        return false;
    }

    static void paintTransportGlyph(juce::Graphics& g,
                                    const juce::Rectangle<float>& bounds,
                                    int state,
                                    juce::Colour ink) {
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.42f;
        const auto centre = bounds.getCentre();

        juce::Path glyph;
        if (state == 1) {
            // a tocar: duas barras, o pause. e' o par do triangulo e le-se sem cor.
            const auto bar = side * 0.28f;
            const auto gap = side * 0.16f;
            glyph.addRectangle(centre.x - bar - gap * 0.5f, centre.y - side * 0.5f, bar,
                               side);
            glyph.addRectangle(centre.x + gap * 0.5f, centre.y - side * 0.5f, bar, side);
        } else {
            glyph.addTriangle(centre.x - side * 0.42f, centre.y - side * 0.56f,
                              centre.x - side * 0.42f, centre.y + side * 0.56f,
                              centre.x + side * 0.56f, centre.y);
        }

        g.setColour(palette::alpha(ink, 0.16f));
        juce::Path halo;
        halo.addRoundedRectangle(glyph.getBounds().expanded(2.5f), 2.5f);
        g.fillPath(halo);

        g.setColour(ink);
        g.fillPath(glyph);
    }

    // hold: um cadeado, a mesma ideia de latch que o transporte tem.
    static void paintLockGlyph(juce::Graphics& g,
                               const juce::Rectangle<float>& bounds,
                               juce::Colour ink) {
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const auto c = bounds.getCentre();
        const auto w = side * 0.34f;
        const auto h = side * 0.26f;
        const auto body = juce::Rectangle<float> {c.x - w * 0.5f, c.y - h * 0.10f, w, h};

        g.setColour(ink);
        g.drawRoundedRectangle(body, 1.2f, 1.6f);

        juce::Path shackle;
        shackle.addCentredArc(c.x, c.y - h * 0.10f, w * 0.26f, h * 0.42f, 0.0f,
                              -juce::MathConstants<float>::halfPi,
                              juce::MathConstants<float>::halfPi, true);
        g.strokePath(shackle, juce::PathStrokeType {1.6f});

        g.fillEllipse(c.x - 1.2f, c.y + h * 0.03f, 2.4f, 2.4f);
    }

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

    void drawKnobBody(juce::Graphics& g, juce::Point<float> centre, float radius) {
        const auto rim = juce::Rectangle<float> {centre.x - radius, centre.y - radius,
                                                  radius * 2.0f, radius * 2.0f};

        for (int i = 2; i >= 1; --i) {
            const auto spread = static_cast<float>(i);
            g.setColour(palette::alpha(juce::Colours::black, 0.14f - 0.04f * spread));
            g.fillEllipse(rim.expanded(spread).translated(0.0f, 2.0f));
        }

        g.setGradientFill(juce::ColourGradient {
            palette::knobTop, centre.translated(0.0f, -radius * 0.55f),
            palette::knobBottom, centre.translated(0.0f, radius * 0.60f), false});
        g.fillEllipse(rim);

        g.setColour(palette::alpha(palette::knobEdge, 0.85f));
        g.drawEllipse(rim.reduced(0.5f), 1.2f);
        g.setColour(palette::alpha(palette::ivory, 0.08f));
        g.drawEllipse(rim.reduced(3.0f), 1.0f);

        const auto markerRadius = radius * 0.055f;
        g.setColour(palette::alpha(palette::ivory, 0.42f));
        g.fillEllipse(centre.x - markerRadius, centre.y - markerRadius,
                      markerRadius * 2.0f, markerRadius * 2.0f);
    }
};

} // namespace opcoda
