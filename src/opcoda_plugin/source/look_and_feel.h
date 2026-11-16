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
        refreshPalette();
    }

    // as cores fixadas no LookAndFeel sao lidas nos construtores dos controlos.
    // uma troca de tema a meio da sessao tem de as voltar a escrever, senao os
    // menus e as caixas ficavam com a tinta da chapa anterior.
    void refreshPalette() {
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

    // knob: arco e ponto. nada de corpo, nada de marcas fixas.
    //
    // **o corpo saiu.** o disco com gradiente, o aro e as onze marcas eram o que
    // dava ao painel o ar de pedalboard; a 96 px eles pesavam mais que o valor.
    // o que fica e' a leitura: um arco de fundo fino, um arco de valor mais
    // grosso e o ponto onde o dedo esta'. e' a lingua do knob de plugin do
    // Logic, que tambem nao desenha peca.
    //
    // com a propriedade `uiDetents` > 0 o knob vira dial de detentes: as
    // posicoes aparecem por fora do arco, em traco fino, e a ativa vira ponto
    // cheio. so' vale a pena ate' cinco posicoes -- acima disso a lista do
    // dropdown le-se melhor, e o Knob decide qual usar.
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

        const auto detents = static_cast<int>(slider.getProperties()["uiDetents"]);
        // o anel de posicoes vive por fora do arco; o arco recolhe para lhe dar
        // lugar em vez de se cruzar com ele.
        const auto arcRadius = outer - (detents > 0 ? 6.5f : 2.0f);
        const auto weight = juce::jmax(1.6f, arcRadius * 0.045f);
        const auto angle =
            rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle,
                            rotaryEndAngle, true);
        g.setColour(palette::alpha(palette::knobEdge, 0.45f));
        g.strokePath(track, juce::PathStrokeType {weight, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});

        if (sliderPosProportional > 0.0001f) {
            juce::Path value;
            value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, angle, true);
            // o valor e' o traco mais grosso do knob, mas nao o dobro: 1,3x ja'
            // separa os dois sem virar bisel.
            g.setColour(palette::accent);
            g.strokePath(value, juce::PathStrokeType {weight * 1.3f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded});
        }

        if (detents > 0) {
            const auto inner = arcRadius + weight * 1.6f;
            const auto outerTick = inner + weight * 2.4f;
            const auto step = detents > 1 ? 1.0f / static_cast<float>(detents - 1) : 0.0f;
            for (int i = 0; i < detents; ++i) {
                const auto t = detents > 1 ? static_cast<float>(i) / static_cast<float>(detents - 1)
                                           : 0.0f;
                const auto a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
                // **o dente ativo e' um ponto, nao um risco mais grosso.** a
                // forma diz qual esta' ativa mesmo sem cor, que e' o que o
                // criterio 1.4.1 pede.
                if (std::abs(t - sliderPosProportional) <= step * 0.5f) {
                    const auto r = (inner + outerTick) * 0.5f;
                    g.setColour(palette::accent);
                    g.fillEllipse(centre.x + std::sin(a) * r - weight * 1.1f,
                                  centre.y - std::cos(a) * r - weight * 1.1f, weight * 2.2f,
                                  weight * 2.2f);
                } else {
                    g.setColour(palette::alpha(palette::textMuted, 0.7f));
                    g.drawLine(centre.x + std::sin(a) * inner, centre.y - std::cos(a) * inner,
                               centre.x + std::sin(a) * outerTick,
                               centre.y - std::cos(a) * outerTick, weight);
                }
            }
        }

        const auto dotRadius = weight * 1.05f;
        g.setColour(palette::accent);
        g.fillEllipse(centre.x + std::sin(angle) * arcRadius - dotRadius,
                      centre.y - std::cos(angle) * arcRadius - dotRadius, dotRadius * 2.0f,
                      dotRadius * 2.0f);

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
                g.setColour(palette::alpha(palette::ivory, 0.06f));
                g.fillRoundedRectangle(bounds, 2.0f);
            }
            const auto box = juce::Rectangle<float> {bounds.getX() + 2.0f,
                                                    bounds.getCentreY() - 5.5f, 11.0f, 11.0f};
            if (on) {
                g.setColour(palette::accent);
                g.fillRoundedRectangle(box, 1.5f);
            }
            g.setColour(on ? palette::charcoalDeep
                           : palette::alpha(palette::ivory, 0.28f));
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
            g.fillRoundedRectangle(bounds, 5.0f);
            g.setColour(palette::alpha(palette::knobEdge, on ? 0.85f : 0.45f));
            g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
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
            g.fillRoundedRectangle(bounds, 4.5f);
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
            g.fillRoundedRectangle(bounds, 5.0f);
            g.setColour(isTransport && on ? palette::accentSoft : palette::knobEdge);
            g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
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
            // a tecla de opcao vive sobre creme: o glifo apagado tem de ser
            // tinta cheia, nao cinza -- a 34 px um traco fino em cinza medio
            // desaparece e os quatro icones do filtro liam-se como um borrao.
            ink = on ? palette::ivory
                     : (look == "rail" ? palette::textOnDarkSub : palette::textDark);
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
    // a barra nao e' do JUCE: grafite um tom abaixo do chassis, o nome a'
    // esquerda e duas teclas de janela sem o fundo cinzento do V4.
    //
    // **a assinatura mudou de sitio.** o nome estava no header do editor, na
    // mesma fila do preset e do ficheiro, e empurrava esse bloco para o canto.
    // aqui a barra diz o nome com a fonte do titulo, e o editor fica so' com o
    // que e' operavel.
    //
    // **sem marca.** a caixa laranja com "Op" saiu da barra: a identidade vem do
    // icone da janela e do nome escrito, e a caixa era uma segunda assinatura.
    void drawDocumentWindowTitleBar(juce::DocumentWindow&, juce::Graphics& g, int w, int h,
                                    int, int, const juce::Image*, bool) override {
        g.fillAll(palette::chassisDark);
        g.setColour(palette::alpha(palette::ivory, 0.08f));
        g.drawHorizontalLine(h - 1, 0.0f, static_cast<float>(w));

        g.setColour(palette::textOnDark);
        g.setFont(fonts().sans(17.0f, true));
        g.drawText("Opcoda", juce::Rectangle<int> {14, 0, 220, h},
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
        if (slider.getProperties()["uiLook"].toString() == "detent") {
            drawDetentSlider(g, x, y, width, height, sliderPos, slider);
            return;
        }

        if (style != juce::Slider::LinearHorizontal) {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.0f, 1.0f,
                                              style, slider);
            return;
        }

        const auto track = juce::Rectangle<float> {
            static_cast<float>(x), static_cast<float>(y) + static_cast<float>(height) * 0.5f - 1.5f,
            static_cast<float>(width), 3.0f};
        // o trilho precisa de se ver no fundo preto: a carvao a 92 % sumia e a
        // barra parecia um tracinho solto no meio da linha.
        g.setColour(palette::alpha(palette::ivory, 0.16f));
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
            juce::Rectangle<float> {sliderPos - 2.5f, track.getY() - 5.0f, 5.0f,
                                    track.getHeight() + 10.0f},
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

    // cursor com dentes: a lista comprida (a divisao do LFO) tem doze posicoes
    // e nenhum knob as mostra todas. o trilho e' o eixo, os dentes sao as
    // posicoes e o nome do valor fica em baixo, escrito pelo controle.
    static void drawDetentSlider(juce::Graphics& g,
                                 int x,
                                 int y,
                                 int width,
                                 int height,
                                 float sliderPos,
                                 juce::Slider& slider) {
        const auto bounds = juce::Rectangle<float> {static_cast<float>(x), static_cast<float>(y),
                                                    static_cast<float>(width),
                                                    static_cast<float>(height)};
        const auto track = juce::Rectangle<float> {bounds.getX(), bounds.getCentreY() - 1.5f,
                                                    bounds.getWidth(), 3.0f};
        g.setColour(palette::alpha(palette::charcoalDeep, 0.95f));
        g.fillRoundedRectangle(track, 1.5f);

        // os dentes sao as posicoes do cursor: o intervalo e' o passo do
        // parametro e o alcance e' o da lista.
        const auto interval = juce::jmax(1.0, slider.getInterval());
        const auto range = slider.getRange();
        const auto divisions = juce::jmax(
            1, juce::roundToInt((range.getEnd() - range.getStart()) / interval));
        for (int i = 0; i <= divisions; ++i) {
            const auto t = static_cast<float>(i) / static_cast<float>(divisions);
            const auto tx = track.getX() + t * track.getWidth();
            g.setColour(palette::alpha(palette::textSub, 0.45f));
            g.fillRect(juce::Rectangle<float> {tx - 0.5f, track.getBottom() + 2.0f, 1.0f, 3.0f});
        }

        const auto handleX = track.getX() + juce::jlimit(0.0f, 1.0f, sliderPos) * track.getWidth();
        g.setColour(palette::accent);
        g.fillRoundedRectangle(juce::Rectangle<float> {track.getX(), track.getY(),
                                                        juce::jmax(2.0f, handleX - track.getX()),
                                                        track.getHeight()},
                               1.5f);

        g.setColour(palette::ivory);
        g.fillRoundedRectangle(juce::Rectangle<float> {handleX - 3.0f, track.getY() - 5.0f,
                                                        6.0f, track.getHeight() + 10.0f},
                               2.0f);

        if (slider.hasKeyboardFocus(true)) {
            drawFocusRing(g, bounds.reduced(1.0f));
        }
    }

public:
    // a marca: o desenho do logo, em Path.
    //
    // o "Op" era texto e pedia a fonte do titulo em todos os tamanhos. o logo e'
    // geometrico -- bloco com o canto cortado e um ponto ao lado -- por isso
    // escala de 16 a 256 px sem perder o desenho e sem depender de fonte.
    //
    // **o ponto e' a segunda marca.** quem ve sem cor ve um bloco e um circulo,
    // e continua a ler a mesma coisa.
    static void paintMark(juce::Graphics& g, juce::Rectangle<float> box) {
        if (box.getWidth() <= 1.0f || box.getHeight() <= 1.0f) {
            return;
        }

        const auto side = juce::jmin(box.getWidth(), box.getHeight());
        const auto left = box.getCentreX() - side * 0.5f;
        const auto top = box.getCentreY() - side * 0.5f;
        const auto p = [&](float nx, float ny) {
            return juce::Point<float> {left + nx * side, top + ny * side};
        };

        juce::Path mark;
        // bloco: retangulo com o canto inferior direito cortado na diagonal
        mark.startNewSubPath(p(0.24f, 0.0f));
        mark.lineTo(p(0.76f, 0.0f));
        mark.quadraticTo(p(1.0f, 0.0f), p(1.0f, 0.24f));
        mark.lineTo(p(1.0f, 0.60f));
        mark.lineTo(p(0.60f, 1.0f));
        mark.lineTo(p(0.24f, 1.0f));
        mark.quadraticTo(p(0.0f, 1.0f), p(0.0f, 0.76f));
        mark.lineTo(p(0.0f, 0.24f));
        mark.quadraticTo(p(0.0f, 0.0f), p(0.24f, 0.0f));
        mark.closeSubPath();

        const auto dotCentre = p(0.80f, 0.79f);
        const auto dotRadius = side * 0.185f;

        g.setColour(palette::accent);
        g.fillPath(mark);
        g.fillEllipse(dotCentre.x - dotRadius, dotCentre.y - dotRadius, dotRadius * 2.0f,
                      dotRadius * 2.0f);
    }

    // devolve true quando desenhou um glifo. o texto so' fica para as teclas que
    // nao tem icone. publico porque as abas de pasta desenham o proprio glifo.
    //
    // **uma grelha, um peso.** todos os glifos nascem na mesma grelha de 24 e
    // sao tracados com o mesmo peso, pontas e juncoes redondas. antes cada um
    // tinha o seu traco (1,5 a 1,9) e o seu recuo e o conjunto lia-se como
    // trinta desenhos diferentes; agora le-se como um sistema.
    //
    // o desenho e' nosso e nao do SF Symbols: a licenca da Apple nao permite
    // usar os originais fora de plataforma Apple. a lingua e' a mesma -- traco
    // fino, canto redondo, peso unico -- e o resultado acompanha o resto da
    // interface sem arrastar um binario de fonte de icones.
    static bool paintGlyph(juce::Graphics& g,
                           const juce::String& kind,
                           const juce::Rectangle<float>& bounds,
                           juce::Colour ink,
                           bool on) {
        if (kind.isEmpty()) {
            return false;
        }

        // o transporte e' preenchido de proposito: um triangulo de contorno a
        // 16 px vira um risco.
        if (kind == "transport") {
            paintTransportGlyph(g, bounds, on ? 1 : 0, ink);
            return true;
        }
        if (kind == "hold") {
            paintLockGlyph(g, bounds, ink);
            return true;
        }

        const auto c = bounds.getCentre();
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.84f;
        const auto u = side / 24.0f;  // uma unidade da grelha
        const auto ox = c.x - side * 0.5f;
        const auto oy = c.y - side * 0.5f;

        juce::Path path;
        const auto move = [&](float x, float y) {
            path.startNewSubPath(ox + x * u, oy + y * u);
        };
        const auto line = [&](float x, float y) { path.lineTo(ox + x * u, oy + y * u); };
        const auto cubic = [&](float x1, float y1, float x2, float y2, float x, float y) {
            path.cubicTo(ox + x1 * u, oy + y1 * u, ox + x2 * u, oy + y2 * u, ox + x * u,
                         oy + y * u);
        };
        const auto circle = [&](float cx, float cy, float r) {
            path.addEllipse(juce::Rectangle<float> {ox + (cx - r) * u, oy + (cy - r) * u,
                                                    2.0f * r * u, 2.0f * r * u});
        };
        const auto box = [&](float x, float y, float w, float h) {
            path.addRectangle(juce::Rectangle<float> {ox + x * u, oy + y * u, w * u, h * u});
        };
        const auto stroke = [&] {
            // o peso tem piso em pixeis: a 11 px um traco proporcional cai
            // abaixo de 1 px e o antialiasing apaga-o.
            g.setColour(ink);
            g.strokePath(path, juce::PathStrokeType {juce::jmax(1.15f, 1.5f * u),
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});
        };

        if (kind == "folder") {
            move(3.5f, 7.5f);
            line(3.5f, 6.5f);
            line(4.5f, 6.5f);
            line(8.5f, 6.5f);
            line(10.1f, 8.5f);
            line(19.5f, 8.5f);
            line(20.5f, 8.5f);
            line(20.5f, 17.5f);
            line(19.5f, 17.5f);
            line(4.5f, 17.5f);
            line(3.5f, 17.5f);
            path.closeSubPath();
        } else if (kind == "chevron") {
            move(7.0f, 10.0f);
            line(12.0f, 15.0f);
            line(17.0f, 10.0f);
        } else if (kind == "chevronLeft" || kind == "chevronRight") {
            const auto dir = kind == "chevronLeft" ? -1.0f : 1.0f;
            move(12.0f - dir * 2.0f, 6.0f);
            line(12.0f + dir * 4.0f, 12.0f);
            line(12.0f - dir * 2.0f, 18.0f);
        } else if (kind == "wave") {
            move(3.0f, 12.0f);
            cubic(4.6f, 8.0f, 6.4f, 8.0f, 7.8f, 12.0f);
            cubic(9.2f, 16.0f, 11.0f, 16.0f, 12.6f, 12.0f);
            cubic(14.2f, 8.0f, 16.0f, 8.0f, 17.4f, 12.0f);
            cubic(18.6f, 15.2f, 19.8f, 14.4f, 21.0f, 12.0f);
        } else if (kind == "hex") {
            move(8.5f, 4.5f);
            line(15.5f, 4.5f);
            line(19.0f, 12.0f);
            line(15.5f, 19.5f);
            line(8.5f, 19.5f);
            line(5.0f, 12.0f);
            path.closeSubPath();
            move(9.5f, 9.5f);
            line(14.5f, 9.5f);
            move(9.5f, 14.5f);
            line(14.5f, 14.5f);
        } else if (kind == "entropy") {
            move(3.5f, 19.0f);
            cubic(6.5f, 18.0f, 7.5f, 13.0f, 9.5f, 10.0f);
            cubic(11.5f, 7.0f, 13.0f, 7.0f, 14.5f, 9.0f);
            cubic(16.0f, 11.0f, 16.5f, 15.0f, 20.0f, 17.0f);
        } else if (kind == "zoomIn" || kind == "zoomOut") {
            circle(10.5f, 10.5f, 6.5f);
            move(15.2f, 15.2f);
            line(20.0f, 20.0f);
            move(8.0f, 10.5f);
            line(13.0f, 10.5f);
            if (kind == "zoomIn") {
                move(10.5f, 8.0f);
                line(10.5f, 13.0f);
            }
        } else if (kind == "fit") {
            move(4.0f, 9.0f);
            line(4.0f, 5.5f);
            line(5.5f, 5.5f);
            line(8.0f, 5.5f);
            move(16.0f, 5.5f);
            line(18.5f, 5.5f);
            line(20.0f, 5.5f);
            line(20.0f, 9.0f);
            move(20.0f, 15.0f);
            line(20.0f, 18.5f);
            line(18.5f, 18.5f);
            line(16.0f, 18.5f);
            move(8.0f, 18.5f);
            line(5.5f, 18.5f);
            line(4.0f, 18.5f);
            line(4.0f, 15.0f);
        } else if (kind == "pageUp" || kind == "pageDown") {
            move(6.0f, 5.0f);
            line(18.0f, 5.0f);
            move(6.0f, 19.0f);
            line(18.0f, 19.0f);
            if (kind == "pageUp") {
                move(12.0f, 18.0f);
                line(12.0f, 9.0f);
                move(8.5f, 12.5f);
                line(12.0f, 9.0f);
                line(15.5f, 12.5f);
            } else {
                move(12.0f, 6.0f);
                line(12.0f, 15.0f);
                move(8.5f, 11.5f);
                line(12.0f, 15.0f);
                line(15.5f, 11.5f);
            }
        } else if (kind == "home") {
            move(4.0f, 11.5f);
            line(12.0f, 4.5f);
            line(20.0f, 11.5f);
            move(6.5f, 9.5f);
            line(6.5f, 19.5f);
            line(17.5f, 19.5f);
            line(17.5f, 9.5f);
        } else if (kind == "end") {
            move(4.0f, 12.0f);
            line(20.0f, 12.0f);
            move(16.0f, 8.0f);
            line(20.0f, 12.0f);
            line(16.0f, 16.0f);
        } else if (kind == "matrix") {
            box(3.5f, 3.5f, 17.0f, 17.0f);
            move(3.5f, 9.0f);
            line(20.5f, 9.0f);
            move(3.5f, 15.0f);
            line(20.5f, 15.0f);
            move(9.5f, 3.5f);
            line(9.5f, 20.5f);
            move(15.0f, 3.5f);
            line(15.0f, 20.5f);
        } else if (kind == "close") {
            move(6.0f, 6.0f);
            line(18.0f, 18.0f);
            move(18.0f, 6.0f);
            line(6.0f, 18.0f);
        } else if (kind == "settings") {
            circle(12.0f, 12.0f, 3.4f);
            for (int i = 0; i < 8; ++i) {
                const auto a = juce::MathConstants<float>::halfPi * 0.25f +
                               static_cast<float>(i) * juce::MathConstants<float>::halfPi * 0.5f;
                const auto dx = std::sin(a);
                const auto dy = -std::cos(a);
                move(12.0f + dx * 6.4f, 12.0f + dy * 6.4f);
                line(12.0f + dx * 8.6f, 12.0f + dy * 8.6f);
            }
        } else if (kind == "grainMod") {
            move(6.0f, 18.5f);
            line(6.0f, 9.0f);
            move(12.0f, 18.5f);
            line(12.0f, 4.5f);
            move(18.0f, 18.5f);
            line(18.0f, 12.0f);
        } else if (kind == "cloudMod") {
            move(7.5f, 18.0f);
            cubic(4.2f, 18.0f, 3.0f, 14.0f, 5.6f, 12.4f);
            cubic(5.0f, 8.6f, 9.4f, 6.4f, 11.8f, 9.2f);
            cubic(15.6f, 7.4f, 18.0f, 11.4f, 16.4f, 14.4f);
            cubic(18.8f, 15.4f, 18.4f, 18.0f, 15.6f, 18.0f);
            line(7.5f, 18.0f);
        } else if (kind == "keysMod") {
            box(3.5f, 6.5f, 17.0f, 11.0f);
            move(7.0f, 6.5f);
            line(7.0f, 10.5f);
            move(11.0f, 6.5f);
            line(11.0f, 10.5f);
            move(15.0f, 6.5f);
            line(15.0f, 10.5f);
            move(8.5f, 14.5f);
            line(15.5f, 14.5f);
        } else if (kind == "envMod") {
            move(3.5f, 17.5f);
            line(6.0f, 6.0f);
            line(10.0f, 10.5f);
            line(15.0f, 10.5f);
            line(20.5f, 17.5f);
        } else if (kind == "filterMod") {
            move(4.0f, 5.0f);
            line(20.0f, 5.0f);
            line(14.0f, 12.5f);
            line(14.0f, 20.0f);
            line(10.0f, 18.0f);
            line(10.0f, 12.5f);
            path.closeSubPath();
        } else if (kind == "modMod") {
            move(3.0f, 12.0f);
            line(6.0f, 12.0f);
            cubic(8.0f, 12.0f, 8.0f, 7.0f, 10.0f, 7.0f);
            cubic(12.0f, 7.0f, 12.0f, 17.0f, 14.0f, 17.0f);
            cubic(16.0f, 17.0f, 16.0f, 12.0f, 18.0f, 12.0f);
            line(21.0f, 12.0f);
        } else if (kind == "waveSine") {
            move(3.0f, 12.0f);
            cubic(5.2f, 6.2f, 7.3f, 6.2f, 9.5f, 12.0f);
            cubic(11.7f, 17.8f, 13.8f, 17.8f, 16.0f, 12.0f);
            cubic(18.2f, 6.2f, 19.4f, 8.2f, 21.0f, 10.4f);
        } else if (kind == "waveTri") {
            move(3.0f, 16.5f);
            line(7.5f, 7.5f);
            line(12.0f, 16.5f);
            line(16.5f, 7.5f);
            line(21.0f, 16.5f);
        } else if (kind == "waveSaw") {
            move(3.0f, 16.5f);
            line(7.5f, 7.5f);
            line(7.5f, 16.5f);
            line(12.0f, 7.5f);
            line(12.0f, 16.5f);
            line(16.5f, 7.5f);
            line(16.5f, 16.5f);
            line(21.0f, 7.5f);
        } else if (kind == "waveSquare") {
            move(3.0f, 15.5f);
            line(3.0f, 8.5f);
            line(7.5f, 8.5f);
            line(7.5f, 15.5f);
            line(12.0f, 15.5f);
            line(12.0f, 8.5f);
            line(16.5f, 8.5f);
            line(16.5f, 15.5f);
            line(21.0f, 15.5f);
        } else if (kind == "waveHold") {
            move(3.0f, 15.0f);
            line(3.0f, 9.0f);
            line(7.5f, 9.0f);
            line(7.5f, 15.0f);
            line(10.5f, 15.0f);
            line(10.5f, 9.0f);
            line(15.0f, 9.0f);
            line(15.0f, 15.0f);
            line(18.0f, 15.0f);
            line(18.0f, 9.0f);
            line(21.0f, 9.0f);
        } else if (kind == "winHann" || kind == "winGaussian" || kind == "winHamming" ||
                   kind == "winBlackman") {
            // as quatro janelas: o que muda e' a largura do lobo, e e' isso que
            // se desenha. um sino mais estreito diz qual delas e'.
            const auto shoulder = kind == "winHamming" ? 0.16f
                                  : kind == "winBlackman" ? 0.30f
                                  : kind == "winGaussian" ? 0.24f
                                                          : 0.36f;
            move(3.0f, 17.0f);
            cubic(3.0f + 9.0f * shoulder, 17.0f, 3.0f + 9.0f * shoulder, 6.0f, 12.0f, 6.0f);
            cubic(21.0f - 9.0f * shoulder, 6.0f, 21.0f - 9.0f * shoulder, 17.0f, 21.0f, 17.0f);
        } else if (kind == "filterLowPass" || kind == "filterHighPass" ||
                   kind == "filterBandPass" || kind == "filterNotch") {
            // a resposta do filtro: patamar, joelho e queda. uma diagonal so'
            // nao diz qual dos quatro e'.
            if (kind == "filterLowPass") {
                move(3.0f, 8.0f);
                line(11.0f, 8.0f);
                cubic(14.0f, 8.0f, 14.0f, 18.0f, 21.0f, 18.0f);
            } else if (kind == "filterHighPass") {
                move(3.0f, 18.0f);
                line(10.0f, 18.0f);
                cubic(17.0f, 18.0f, 17.0f, 8.0f, 20.0f, 8.0f);
                line(21.0f, 8.0f);
            } else if (kind == "filterBandPass") {
                move(3.0f, 18.0f);
                line(5.0f, 18.0f);
                cubic(8.0f, 18.0f, 8.0f, 7.0f, 11.0f, 7.0f);
                line(13.0f, 7.0f);
                cubic(16.0f, 7.0f, 16.0f, 18.0f, 19.0f, 18.0f);
                line(21.0f, 18.0f);
            } else {
                move(3.0f, 7.0f);
                line(5.0f, 7.0f);
                cubic(8.0f, 7.0f, 8.0f, 18.0f, 11.0f, 18.0f);
                line(13.0f, 18.0f);
                cubic(16.0f, 18.0f, 16.0f, 7.0f, 19.0f, 7.0f);
                line(21.0f, 7.0f);
            }
        } else {
            return false;
        }

        stroke();
        return true;
    }

    static void paintTransportGlyph(juce::Graphics& g,
                                    const juce::Rectangle<float>& bounds,
                                    int state,
                                    juce::Colour ink) {
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.34f;
        const auto centre = bounds.getCentre();

        juce::Path glyph;
        if (state == 1) {
            // a tocar: duas barras, o pause. e' o par do triangulo e le-se sem cor.
            const auto bar = side * 0.30f;
            const auto gap = side * 0.20f;
            glyph.addRoundedRectangle(centre.x - bar - gap * 0.5f, centre.y - side * 0.5f,
                                      bar, side, 0.8f);
            glyph.addRoundedRectangle(centre.x + gap * 0.5f, centre.y - side * 0.5f,
                                      bar, side, 0.8f);
            g.setColour(ink);
            g.fillPath(glyph);
            return;
        }

        // o triangulo do play e' um contorno, nao um bloco: a 13 px o cheio
        // pesava mais que o texto ao lado. o halo saiu com ele.
        glyph.addTriangle(centre.x - side * 0.36f, centre.y - side * 0.50f,
                          centre.x - side * 0.36f, centre.y + side * 0.50f,
                          centre.x + side * 0.50f, centre.y);
        g.setColour(ink);
        g.strokePath(glyph, juce::PathStrokeType {1.4f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});
    }

    // hold: um cadeado, a mesma ideia de latch que o transporte tem. desenhado
    // na grelha de 24 como o resto, para nao ser o unico glifo fora do peso.
    static void paintLockGlyph(juce::Graphics& g,
                               const juce::Rectangle<float>& bounds,
                               juce::Colour ink) {
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.84f;
        const auto u = side / 24.0f;
        const auto c = bounds.getCentre();
        const auto ox = c.x - side * 0.5f;
        const auto oy = c.y - side * 0.5f;
        const auto thickness = juce::jmax(1.15f, 1.5f * u);

        juce::Path glyph;
        glyph.startNewSubPath(ox + 7.0f * u, oy + 11.0f * u);
        glyph.lineTo(ox + 7.0f * u, oy + 8.5f * u);
        glyph.addCentredArc(ox + 12.0f * u, oy + 8.5f * u, 5.0f * u, 5.0f * u, 0.0f,
                            juce::MathConstants<float>::pi,
                            juce::MathConstants<float>::twoPi, false);
        glyph.lineTo(ox + 17.0f * u, oy + 11.0f * u);
        glyph.addRoundedRectangle(ox + 5.5f * u, oy + 11.0f * u, 13.0f * u, 8.5f * u,
                                  1.6f * u);

        g.setColour(ink);
        g.strokePath(glyph, juce::PathStrokeType {thickness, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});
    }

};

} // namespace opcoda
