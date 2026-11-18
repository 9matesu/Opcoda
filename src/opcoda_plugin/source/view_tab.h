#pragma once

#include "sidebar_button.h"
#include "ui_animation.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Aba de vista do display: um item da sidebar. Largura fixa, glifo e rotulo
// sempre visiveis, caixa opaca que inverte quando a vista esta activa.
class ViewTab : public juce::Component {
public:
    ViewTab() = default;
    ViewTab(const char* icon, juce::String label)
        : icon_(icon), label_(std::move(label)) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setSize(sidebar::kWidth, sidebar::kHeight);
        hover_.attach(*this, [this](float, float) { repaint(); });
    }

    void setToggled(bool on) {
        if (toggled_ != on) {
            toggled_ = on;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        sidebar::paintButton(g, getLocalBounds().toFloat(), icon_, label_, hover_.hover(),
                             toggled_, true);
    }

    void mouseDown(const juce::MouseEvent&) override {
        if (onClick != nullptr) {
            onClick();
        }
    }

    std::function<void()> onClick;

private:
    const char* icon_ {nullptr};
    juce::String label_;
    bool toggled_ {false};
    PressHover hover_;
};

} // namespace opcoda
