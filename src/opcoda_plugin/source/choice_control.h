#pragma once

#include "fonts.h"
#include "palette.h"
#include "ui_animation.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace opcoda {

class ChoiceControl : public juce::Component, private juce::Slider::Listener {
public:
    enum class Mode {
        icons,   // grelha de botoes com glifo
        text,    // grelha de botoes com rotulo curto
        toggle,  // chave de dois estados
        detent,  // cursor com dentes
    };

    struct Item {
        juce::String label {};      // nome falado e tooltip
        juce::String shortLabel {}; // texto curto, quando ha' icone
        const char* icon {nullptr}; // glifo, so' no modo icons
    };

    ChoiceControl(juce::AudioProcessorValueTreeState& state,
                  const juce::String& paramId,
                  const juce::String& title,
                  const juce::String& spokenName,
                  std::vector<Item> items,
                  Mode mode,
                  int columns = 0);

    void paint(juce::Graphics& g) override;
    void setCompactSwitches(bool on) noexcept { compactSwitches_ = on; }

    void resized() override;

    bool compactSwitches_ {false};

private:
    void sliderValueChanged(juce::Slider* changed) override;
    void syncFromValue();
    [[nodiscard]] int currentIndex() const noexcept;
    void choose(int index);

    juce::Slider value_;
    std::unique_ptr<juce::SliderParameterAttachment> attachment_;

    juce::Label title_;
    juce::Label valueLabel_;

    std::vector<std::unique_ptr<juce::Button>> buttons_;
    std::vector<std::unique_ptr<PressHover>> hovers_;
    juce::StringArray labels_;
    std::vector<Item> items_;

    Mode mode_ {Mode::icons};
    int columns_ {0};
};

} // namespace opcoda
