#pragma once

#include "animator.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace opcoda {

class PressHover final : private juce::Timer, private juce::MouseListener {
public:
    using Change = std::function<void(float hover, float press)>;

    void attach(juce::Component& component, Change change) {
        component_ = &component;
        change_ = std::move(change);
        component.addMouseListener(this, true);
    }

    [[nodiscard]] float hover() const noexcept { return hover_.value(); }
    [[nodiscard]] float press() const noexcept { return press_.value(); }

    void jump(float hoverValue, float pressValue) noexcept {
        hover_.jumpTo(hoverValue);
        press_.jumpTo(pressValue);
    }

private:
    void mouseEnter(const juce::MouseEvent&) override { wake(1.0f, press_.target()); }
    void mouseExit(const juce::MouseEvent&) override { wake(0.0f, 0.0f); }
    void mouseDown(const juce::MouseEvent&) override { wake(hover_.target(), 1.0f); }
    void mouseUp(const juce::MouseEvent&) override { wake(hover_.target(), 0.0f); }

    void wake(float hoverTarget, float pressTarget) {
        hover_.set(hoverTarget);
        press_.set(pressTarget);
        startTimerHz(kHz);
        notify();
    }

    void timerCallback() override {
        hover_.tick(kDelta, kHalfLife);
        press_.tick(kDelta, kPressHalfLife);
        notify();
        if (hover_.settled(0.002f) && press_.settled(0.002f)) {
            stopTimer();
        }
    }

    void notify() {
        if (change_ != nullptr) {
            change_(hover_.value(), press_.value());
        }
    }

    static constexpr int kHz {90};
    static constexpr float kDelta {1.0f / static_cast<float>(kHz)};
    static constexpr float kHalfLife {0.045f};
    static constexpr float kPressHalfLife {0.030f};

    Eased hover_ {};
    Eased press_ {};
    juce::Component* component_ {nullptr};
    Change change_;
};

inline void attachButtonHover(juce::Button& button,
                              std::vector<std::unique_ptr<PressHover>>& store) {
    button.getProperties().set("uiHover", 0.0);
    button.getProperties().set("uiPress", 0.0);

    auto hover = std::make_unique<PressHover>();
    hover->attach(button, [&button](float h, float p) {
        button.getProperties().set("uiHover", static_cast<double>(h));
        button.getProperties().set("uiPress", static_cast<double>(p));
        button.repaint();
    });
    store.push_back(std::move(hover));
}

[[nodiscard]] inline float hoverOf(const juce::Component& button) {
    const auto value = button.getProperties()["uiHover"];
    if (!value.isVoid()) {
        return juce::jlimit(0.0f, 1.0f, static_cast<float>(static_cast<double>(value)));
    }
    return button.isMouseOver() ? 1.0f : 0.0f;
}

[[nodiscard]] inline float pressOf(const juce::Component& button) {
    const auto value = button.getProperties()["uiPress"];
    if (!value.isVoid()) {
        return juce::jlimit(0.0f, 1.0f, static_cast<float>(static_cast<double>(value)));
    }
    return 0.0f;
}

} // namespace opcoda