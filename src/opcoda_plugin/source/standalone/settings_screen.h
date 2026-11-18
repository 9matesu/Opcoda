#pragma once

#include "../fonts.h"
#include "../look_and_feel.h"
#include "../palette.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace opcoda {

class SettingsScreen final : public juce::Component,
                             private juce::ChangeListener,
                             private juce::Timer {
public:
    explicit SettingsScreen(juce::AudioDeviceManager& devices);
    ~SettingsScreen() override;

    void open();
    void close();

    void refreshPalette();

    std::function<bool(const juce::KeyPress&)> onUnhandledKey;
    std::function<bool(bool)> onKeyStateChanged;
    std::function<void()> onClosed;
    std::function<void()> onThemeChanged;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    enum class Page { audio, midi, themes };

    [[nodiscard]] juce::Rectangle<int> panelBounds() const;
    [[nodiscard]] juce::Rectangle<int> bodyBounds() const;

    void selectPage(Page page);
    void chooseTheme(palette::Theme theme);
    void refreshAudio();
    void refreshMidi();
    void refreshStatus();
    void applyAudio();
    void paintStatus(juce::Graphics&) const;

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void visibilityChanged() override;

    juce::AudioDeviceManager& devices_;

    juce::TextButton audioTab_ {"Audio"};
    juce::TextButton midiTab_ {"MIDI"};
    juce::TextButton themesTab_ {"Themes"};
    juce::TextButton closeButton_;
    juce::TextButton doneButton_ {"Close"};
    juce::TextButton controlPanel_ {"Control panel"};

    juce::ComboBox driverBox_;
    juce::ComboBox outputBox_;
    juce::ComboBox rateBox_;
    juce::ComboBox bufferBox_;

    std::vector<std::unique_ptr<juce::TextButton>> midiRows_;

    // cinco chapas: grafite, classica, clara, ambar e ciano.
    juce::TextButton darkTheme_ {"Dark"};
    juce::TextButton classicTheme_ {"Classic"};
    juce::TextButton lightTheme_ {"Light"};
    juce::TextButton amberTheme_ {"Amber"};
    juce::TextButton cyanTheme_ {"Cyan"};

    Page page_ {Page::audio};
    bool applying_ {false};
    juce::String error_;

    juce::Rectangle<int> panel_;
    juce::Rectangle<int> status_;

    static constexpr int kPanelWidth {900};
    static constexpr int kPanelHeight {620};
    static constexpr int kHeaderHeight {52};
    static constexpr int kInset {28};
    static constexpr int kRowPitch {52};
    static constexpr int kStatusHeight {132};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsScreen)
};

} // namespace opcoda
