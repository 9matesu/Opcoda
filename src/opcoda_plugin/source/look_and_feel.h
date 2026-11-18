#pragma once

#include "fonts.h"
#include "palette.h"

#include "OpcodaBrand.h"
#include "ui_animation.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>

namespace opcoda {

namespace windowChrome {
constexpr int kTrafficSize {14};
constexpr int kTrafficGap {6};
constexpr int kTrafficPad {10};
}

class OpcodaWindowButton final : public juce::Button {
public:
    explicit OpcodaWindowButton(int type)
        : juce::Button(type == juce::DocumentWindow::closeButton ? "close" : "minimise"),
          type_(type) {}

    void paintButton(juce::Graphics& g, bool over, bool down) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto close = type_ == juce::DocumentWindow::closeButton;
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight());
        if (side < 4.0f) {
            return;
        }

        const auto ink = (over || down)
                             ? (close ? palette::error : palette::accentWarn)
                             : palette::alpha(palette::ivory, 0.55f);
        const auto c = bounds.getCentre();
        const auto arm = side * 0.26f;

        juce::Path glyph;
        if (close) {
            glyph.startNewSubPath(c.x - arm, c.y - arm);
            glyph.lineTo(c.x + arm, c.y + arm);
            glyph.startNewSubPath(c.x + arm, c.y - arm);
            glyph.lineTo(c.x - arm, c.y + arm);
        } else {
            glyph.startNewSubPath(c.x - arm, c.y);
            glyph.lineTo(c.x + arm, c.y);
        }

        g.setColour(ink);
        g.strokePath(glyph, juce::PathStrokeType {1.35f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded});
    }

private:
    int type_;
};

class OpcodaLookAndFeel : public juce::LookAndFeel_V4 {
public:
    OpcodaLookAndFeel() {
        refreshPalette();
    }

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

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width,
                     int height) override {
        const auto bounds =
            juce::Rectangle<int> {0, 0, width, height}.toFloat().reduced(1.0f);
        g.setColour(palette::charcoalDeep);
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(palette::alpha(palette::ivory, 0.14f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
        g.setFont(type::bodyFont());
        g.setColour(palette::textOnDark);
        g.drawText(text, bounds.reduced(8.0f, 5.0f), juce::Justification::centredLeft, true);
    }

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
        const auto hover = hoverOf(slider);
        const auto arcRadius = outer - (detents > 0 ? 6.5f : 2.0f);
        const auto weight = juce::jmax(1.6f, arcRadius * 0.045f) * (1.0f + 0.12f * hover);
        const auto angle =
            rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle,
                            rotaryEndAngle, true);
        g.setColour(palette::knobTrack.interpolatedWith(palette::ivory, 0.22f * hover));
        g.strokePath(track, juce::PathStrokeType {weight, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});

        if (sliderPosProportional > 0.0001f) {
            juce::Path value;
            value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                rotaryStartAngle, angle, true);
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

        const auto dotRadius = weight * 1.05f * (1.0f + 0.25f * hover);
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
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto press = pressOf(button);
        if (press > 0.01f) {
            bounds = bounds.reduced(0.6f * press, 0.6f * press);
        }
        if (bounds.getWidth() <= 1.0f || bounds.getHeight() <= 1.0f) {
            return;
        }

        const auto look = button.getProperties()["uiLook"].toString();
        if (look == "brand") {
            return;
        }
        const auto isRow = look == "row";
        const auto on = button.getToggleState();
        const auto hover = hoverOf(button);
        const auto lit = button.isEnabled()
                             ? juce::jmax(highlighted ? 1.0f : 0.0f, hover)
                             : 0.0f;

        if (isRow) {
            if (lit > 0.01f && !down) {
                g.setColour(palette::alpha(palette::ivory, 0.05f * lit));
                g.fillRoundedRectangle(bounds, 3.0f);
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
                juce::Path tick;
                tick.startNewSubPath(box.getX() + 2.6f, box.getCentreY());
                tick.lineTo(box.getX() + 4.6f, box.getBottom() - 3.2f);
                tick.lineTo(box.getRight() - 2.3f, box.getY() + 3.4f);
                g.setColour(palette::charcoalDeep);
                g.strokePath(tick, juce::PathStrokeType {1.7f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded});
            }
        } else if (lit > 0.01f && !down) {
            g.setColour(palette::alpha(palette::ivory, 0.05f * lit));
            g.fillRoundedRectangle(bounds.reduced(1.0f), 3.0f);
        }

        if (button.isEnabled() && button.hasKeyboardFocus(true)) {
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
        if (look == "brand") {
            return;
        }
        const auto on = button.getToggleState();

        juce::Colour ink = palette::textDark;
        if (!button.isEnabled()) {
            ink = palette::textMuted;
        } else if (look == "row") {
            // lista sobre creme: as duas posicoes tem de ler no mesmo fundo.
            ink = on ? palette::textDark : palette::textSub;
        } else if (look == "tab" || look == "icon" || look == "rail" || look == "chip" ||
                   look == "gbtn") {
            ink = on ? palette::accent
                     : (look == "rail" ? palette::textOnDarkSub : palette::textDark);
        } else if (look == "bar") {
            ink = on ? palette::accent : palette::textOnDarkSub;
        }

        if (look == "row") {
            g.setFont(type::bodyFont(on));
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
            // caixa de 20 px, a mesma das abas de vista e dos botoes de parametro.
            const auto glyphBox = bounds.withWidth(20.0f).reduced(2.0f);
            if (!paintGlyph(g, kind, glyphBox, ink, on)) {
                // botao so' de texto (transporte): o nome ocupa o rectangulo todo
                g.setFont(fonts().sans(9.0f, true));
                g.setColour(ink);
                g.drawText(button.getButtonText(), bounds.reduced(2.0f, 0.0f),
                           juce::Justification::centred, false);
                return;
            }
            g.setFont(type::captionFont(on));
            g.setColour(ink);
            g.drawText(button.getButtonText(), bounds.withTrimmedLeft(22.0f).reduced(2.0f, 0.0f),
                       juce::Justification::centredLeft, false);
            return;
        }

        if (kind == "hex") {
            const auto centre = bounds.getCentre();
            const auto radius =
                juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.44f;
            juce::Path hex;
            for (int i = 0; i < 6; ++i) {
                const auto angle = juce::MathConstants<float>::pi / 6.0f +
                                   static_cast<float>(i) *
                                       juce::MathConstants<float>::twoPi / 6.0f;
                const auto x = centre.x + radius * std::cos(angle);
                const auto y = centre.y + radius * std::sin(angle);
                if (i == 0) {
                    hex.startNewSubPath(x, y);
                } else {
                    hex.lineTo(x, y);
                }
            }
            hex.closeSubPath();
            g.setColour(ink);
            g.fillPath(hex);
            return;
        }

        // botao de icone ligado: banho claro atras do glifo. E' o que faz o
        // estado activo ler-se nos botoes de forma do LFO e na lista de janelas.
        if (look == "icon" && on) {
            g.setColour(palette::alpha(palette::ivory, 0.12f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), 3.0f);
            g.setColour(palette::alpha(palette::ivory, 0.30f));
            g.drawRoundedRectangle(bounds.reduced(1.5f), 3.0f, 1.0f);
        }

        if (paintGlyph(g, kind, bounds, ink, on)) {
            return;
        }

        g.setFont(type::captionFont(on || look.isEmpty()));
        g.setColour(ink);
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

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
            g.fillRoundedRectangle(area.reduced(3, 1).toFloat(), 3.0f);
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

    [[nodiscard]] juce::Font getComboBoxFont(juce::ComboBox&) override {
        return fonts().sans(11.0f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int,
                      juce::ComboBox&) override {
        g.setColour(palette::alpha(palette::ivory, 0.13f));
        g.fillRect(juce::Rectangle<float> {0.0f, static_cast<float>(height) - 1.0f,
                                           static_cast<float>(width), 1.0f});

        juce::Path chevron;
        const auto cx = static_cast<float>(width) - 9.0f;
        const auto cy = static_cast<float>(height) * 0.5f;
        chevron.startNewSubPath(cx - 3.0f, cy - 2.0f);
        chevron.lineTo(cx, cy + 1.0f);
        chevron.lineTo(cx + 3.0f, cy - 2.0f);
        g.setColour(palette::textSub);
        g.strokePath(chevron, juce::PathStrokeType {1.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded});
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setEditable(false);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(getComboBoxFont(box));
        label.setBounds(2, 0, juce::jmax(10, box.getWidth() - 20), box.getHeight());
    }

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
        g.drawRoundedRectangle(bounds, 3.0f, 3.0f);
        g.setColour(palette::focusRing);
        g.drawRoundedRectangle(bounds, 3.0f, 1.5f);
    }

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
    static juce::String hexOf(juce::Colour colour) {
        return juce::String::formatted("#%02X%02X%02X", colour.getRed(), colour.getGreen(),
                                       colour.getBlue());
    }

    static const juce::Drawable* brandMark() {
        static std::unique_ptr<juce::Drawable> cached;
        static juce::Colour cachedTint {};
        const auto tint = palette::accent;

        if (cached == nullptr || cachedTint != tint) {
            auto svg = juce::String::fromUTF8(OpcodaBrand::logolight_svg,
                                              OpcodaBrand::logolight_svgSize);
            if (svg.startsWith("\xEF\xBB\xBF")) {
                svg = svg.substring(1);
            }
            svg = svg.replace("#e8e8e8", hexOf(tint));
            if (auto xml = juce::XmlDocument::parse(svg)) {
                cached = juce::Drawable::createFromSVG(*xml);
            }
            cachedTint = tint;
        }

        return cached.get();
    }

    // o logo da editora, quando o rato passa pelo nome do instrumento.
    // silhueta branca: a arte preta vira alfa, o fundo branco desaparece.
    static void paintBloodworm(juce::Graphics& g, juce::Rectangle<float> box) {
        static std::unique_ptr<juce::Image> cached;
        if (cached == nullptr) {
            cached = std::make_unique<juce::Image>(
                juce::ImageFileFormat::loadFrom(OpcodaBrand::bloodwormtrax_png,
                                                static_cast<std::size_t>(
                                                    OpcodaBrand::bloodwormtrax_pngSize)));
            if (cached->isValid()) {
                auto mask = std::make_unique<juce::Image>(juce::Image::ARGB,
                                                          cached->getWidth(),
                                                          cached->getHeight(), true);
                juce::Image::BitmapData src(*cached, juce::Image::BitmapData::readOnly);
                juce::Image::BitmapData dest(*mask, juce::Image::BitmapData::writeOnly);
                for (int y = 0; y < src.height; ++y) {
                    for (int x = 0; x < src.width; ++x) {
                        const auto c = src.getPixelColour(x, y);
                        const auto lum = (static_cast<int>(c.getRed())
                                          + static_cast<int>(c.getGreen())
                                          + static_cast<int>(c.getBlue()))
                                         / 3;
                        dest.setPixelColour(x, y, juce::Colours::white.withAlpha(
                                                   static_cast<juce::uint8>(255 - lum)));
                    }
                }
                cached = std::move(mask);
            }
        }
        if (cached->isValid()) {
            g.drawImageWithin(*cached, juce::roundToInt(box.getX()),
                              juce::roundToInt(box.getY()),
                              juce::roundToInt(box.getWidth()),
                              juce::roundToInt(box.getHeight()),
                              juce::RectanglePlacement::centred);
        }
    }

    static void paintMark(juce::Graphics& g, juce::Rectangle<float> box) {
        if (box.getWidth() <= 1.0f || box.getHeight() <= 1.0f) {
            return;
        }
        if (const auto* drawable = brandMark()) {
            drawable->drawWithin(g, box, juce::RectanglePlacement::centred, 1.0f);
        }
    }

    static bool paintGlyph(juce::Graphics& g,
                           const juce::String& kind,
                           const juce::Rectangle<float>& bounds,
                           juce::Colour ink,
                           bool on) {
        if (kind.isEmpty()) {
            return false;
        }

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
        bool filledGlyph = false;
        if (kind == "hex") {
            move(12.0f, 2.5f);
            line(20.5f, 7.5f);
            line(20.5f, 16.5f);
            line(12.0f, 21.5f);
            line(3.5f, 16.5f);
            line(3.5f, 7.5f);
            path.closeSubPath();
            move(12.0f, 8.0f);
            line(15.5f, 10.0f);
            line(15.5f, 14.0f);
            line(12.0f, 16.0f);
            line(8.5f, 14.0f);
            line(8.5f, 10.0f);
            path.closeSubPath();
        }

        const auto stroke = [&] {
            g.setColour(ink);
            if (filledGlyph) {
                g.fillPath(path);
            } else {
                g.strokePath(path, juce::PathStrokeType {juce::jmax(1.35f, 1.6f * u),
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded});
            }
        };

        // o hexagono e' construido acima, antes desta cadeia; sem este atalho
        // caia no `else { return false; }` e o glifo nunca era desenhado.
        if (kind == "hex") {
            stroke();
            return true;
        }

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
        } else if (kind == "entropy") {
            move(3.5f, 19.0f);
            cubic(6.5f, 18.0f, 7.5f, 13.0f, 9.5f, 10.0f);
            cubic(11.5f, 7.0f, 13.0f, 7.0f, 14.5f, 9.0f);
            cubic(16.0f, 11.0f, 16.5f, 15.0f, 20.0f, 17.0f);
        } else if (kind == "zoomIn" || kind == "zoomOut") {
            circle(10.0f, 10.0f, 6.2f);
            move(14.6f, 14.6f);
            line(19.6f, 19.6f);
            move(6.8f, 10.0f);
            line(13.2f, 10.0f);
            if (kind == "zoomIn") {
                move(10.0f, 6.8f);
                line(10.0f, 13.2f);
            }
        } else if (kind == "fit") {
            box(4.0f, 5.5f, 16.0f, 13.0f);
            move(7.5f, 12.0f);
            line(12.0f, 12.0f);
            move(10.0f, 10.0f);
            line(12.0f, 12.0f);
            line(10.0f, 14.0f);
            move(16.5f, 12.0f);
            line(12.0f, 12.0f);
            move(14.0f, 10.0f);
            line(12.0f, 12.0f);
            line(14.0f, 14.0f);
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
        } else if (kind == "minus") {
            move(5.0f, 12.0f);
            line(19.0f, 12.0f);
        } else if (kind == "plus") {
            move(5.0f, 12.0f);
            line(19.0f, 12.0f);
            move(12.0f, 5.0f);
            line(12.0f, 19.0f);
        } else if (kind == "settings") {
            constexpr int kTeeth {6};
            for (int i = 0; i < kTeeth * 2; ++i) {
                const auto a = static_cast<float>(i) * juce::MathConstants<float>::pi /
                               static_cast<float>(kTeeth);
                const auto r = (i % 2 == 0) ? 9.6f : 7.0f;
                const auto px = 12.0f + std::sin(a) * r;
                const auto py = 12.0f - std::cos(a) * r;
                if (i == 0) {
                    move(px, py);
                } else {
                    line(px, py);
                }
            }
            path.closeSubPath();
            circle(12.0f, 12.0f, 2.8f);
            path.setUsingNonZeroWinding(false);
            filledGlyph = true;
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
            const auto shoulder = kind == "winHamming" ? 0.16f
                                  : kind == "winBlackman" ? 0.30f
                                  : kind == "winGaussian" ? 0.24f
                                                          : 0.36f;
            move(3.0f, 17.0f);
            cubic(3.0f + 9.0f * shoulder, 17.0f, 3.0f + 9.0f * shoulder, 6.0f, 12.0f, 6.0f);
            cubic(21.0f - 9.0f * shoulder, 6.0f, 21.0f - 9.0f * shoulder, 17.0f, 21.0f, 17.0f);
        } else if (kind == "filterLowPass" || kind == "filterHighPass" ||
                   kind == "filterBandPass" || kind == "filterNotch") {
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
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.55f;
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

        glyph.addTriangle(centre.x - side * 0.38f, centre.y - side * 0.52f,
                          centre.x - side * 0.38f, centre.y + side * 0.52f,
                          centre.x + side * 0.52f, centre.y);
        g.setColour(ink);
        g.fillPath(glyph);
    }

    static void paintLockGlyph(juce::Graphics& g,
                               const juce::Rectangle<float>& bounds,
                               juce::Colour ink) {
        const auto side = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.84f;
        const auto u = side / 24.0f;
        const auto c = bounds.getCentre();
        const auto ox = c.x - side * 0.5f;
        const auto oy = c.y - side * 0.5f;
        const auto thickness = juce::jmax(1.2f, 1.6f * u);

        juce::Path glyph;
        // argola: meia-lua acima do corpo, de 180 a 360 graus. Antes era um
        // circulo inteiro, que se sobrepunha ao corpo e distorcia a leitura.
        glyph.startNewSubPath(ox + 7.5f * u, oy + 11.5f * u);
        glyph.addCentredArc(ox + 12.0f * u, oy + 11.5f * u, 4.5f * u, 4.5f * u,
                            juce::MathConstants<float>::pi,
                            juce::MathConstants<float>::twoPi, false);
        // corpo: caixa arredondada encostada a' argola.
        glyph.addRoundedRectangle(ox + 5.0f * u, oy + 11.0f * u, 14.0f * u, 9.0f * u,
                                  1.8f * u);

        g.setColour(ink);
        g.strokePath(glyph, juce::PathStrokeType {thickness, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});
    }

};

} // namespace opcoda
