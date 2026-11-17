#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "../fonts.h"
#include "../look_and_feel.h"
#include "../palette.h"
#include "settings_screen.h"

#include <memory>

namespace opcoda {

class StandaloneWindow final : public juce::DocumentWindow {
public:
    StandaloneWindow(juce::String title, juce::Colour background,
                     std::unique_ptr<juce::StandalonePluginHolder> holder);
    ~StandaloneWindow() override;

    [[nodiscard]] juce::StandalonePluginHolder& holder() noexcept { return *holder_; }

    void applyTaskbarIcon();

    void closeButtonPressed() override;
    void resized() override;

private:
    class Body final : public juce::Component {
    public:
        explicit Body(juce::StandalonePluginHolder& holder);

        void resized() override;
        void childBoundsChanged(juce::Component*) override;
        void openSettings();

        [[nodiscard]] int preferredWidth() const noexcept { return preferredWidth_; }
        [[nodiscard]] int preferredHeight() const noexcept { return preferredHeight_; }

    private:
        std::unique_ptr<juce::AudioProcessorEditor> editor_;
        std::unique_ptr<SettingsScreen> settings_;
        int preferredWidth_ {1180};
        int preferredHeight_ {760};
    };

    void applyRoundedCorners();

    void restoreWindowPosition();
    void saveWindowPosition();
    [[nodiscard]] static juce::Image makeWindowIcon();

    std::unique_ptr<juce::StandalonePluginHolder> holder_;
    Body* body_ {nullptr};

    OpcodaWindowButton closeButton_ {juce::DocumentWindow::closeButton};
    OpcodaWindowButton minimiseButton_ {juce::DocumentWindow::minimiseButton};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneWindow)
};

} // namespace opcoda
