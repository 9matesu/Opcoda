#pragma once

#include "fonts.h"
#include "look_and_feel.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace opcoda {

class FileField final : public juce::Component {
public:
    std::function<void()> onClick;

    FileField() {
        setName("Campo do ficheiro");
        setHelpText("Ficheiro em uso. Clique para escolher outro.");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
        hover_.attach(*this, [this](float, float) { repaint(); });
    }

    void setFile(const juce::String& name, const juce::String& size,
                 const juce::String& format) {
        if (name_ != name || size_ != size || format_ != format) {
            name_ = name;
            size_ = size;
            format_ = format;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        if (bounds.getWidth() < 20.0f || bounds.getHeight() < 8.0f) {
            return;
        }

        const auto hover = hover_.hover();

        auto fill = palette::charcoalDeep;
        if (hover > 0.01f) {
            fill = fill.interpolatedWith(palette::charcoal, 0.55f * hover);
        }

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(palette::alpha(palette::knobEdge, 0.45f + 0.45f * hover));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        const auto font = fonts().sans(10.5f);
        const auto metaFont = fonts().sans(9.0f);
        const auto glyph = 12.0f;
        const auto cy = bounds.getCentreY();

        auto textLeft = bounds.getX() + 8.0f;
        OpcodaLookAndFeel::paintGlyph(g, "folder",
                                      juce::Rectangle<float> {textLeft, cy - glyph * 0.5f, glyph,
                                                              glyph},
                                      palette::textMuted, false);
        textLeft += glyph + 6.0f;

        auto textRight = bounds.getRight() - 20.0f;

        if (format_.isNotEmpty()) {
            g.setFont(metaFont);
            g.setColour(palette::textMuted);
            g.drawText(format_,
                       juce::Rectangle<float> {textRight - 62.0f, bounds.getY(), 62.0f,
                                               bounds.getHeight()},
                       juce::Justification::centredRight, false);
        }
        if (size_.isNotEmpty()) {
            g.setFont(metaFont);
            g.setColour(palette::textMuted);
            g.drawText(size_,
                       juce::Rectangle<float> {textRight - 122.0f, bounds.getY(), 58.0f,
                                               bounds.getHeight()},
                       juce::Justification::centredRight, false);
        }

        auto nameArea = juce::Rectangle<float> {textLeft, bounds.getY(),
                                                juce::jmax(20.0f, textRight - 128.0f - textLeft),
                                                bounds.getHeight()};

        auto size = 10.5f;
        auto fontFit = font;
        const auto widthOf = [&fontFit](const juce::String& value) {
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText(fontFit, value, 0.0f, 0.0f);
            return glyphs.getBoundingBox(0, -1, false).getWidth();
        };
        while (size > 8.0f && widthOf(name_) > nameArea.getWidth()) {
            size -= 0.5f;
            fontFit = fonts().sans(size);
        }

        g.setFont(fontFit);
        g.setColour(name_.isEmpty() ? palette::textMuted : palette::textDark);
        g.drawText(name_.isEmpty() ? juce::String {"Sem binario"} : name_, nameArea,
                   juce::Justification::centredLeft, true);

        OpcodaLookAndFeel::paintGlyph(
            g, "chevron",
            juce::Rectangle<float> {bounds.getRight() - 16.0f, cy - 5.0f, 10.0f, 10.0f},
            palette::alpha(palette::textSub, 0.55f + 0.45f * hover), false);

        if (hasKeyboardFocus(true)) {
            g.setColour(palette::charcoalDeep);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 3.0f, 3.0f);
            g.setColour(palette::focusRing);
            g.drawRoundedRectangle(bounds.reduced(1.0f), 3.0f, 1.5f);
        }
    }

    void mouseDown(const juce::MouseEvent&) override {
        grabKeyboardFocus();
        if (onClick != nullptr) {
            onClick();
        }
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
        if (onClick != nullptr) {
            onClick();
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey) {
            if (onClick != nullptr) {
                onClick();
            }
            return true;
        }
        return false;
    }

private:
    juce::String name_ {"Sem binario"};
    juce::String size_;
    juce::String format_;
    PressHover hover_;
};

} // namespace opcoda