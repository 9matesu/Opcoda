#include "standalone_window.h"

#include "../look_and_feel.h"
#include "../palette.h"

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace opcoda {
namespace {

// Aplicacao do standalone. Substitui a StandaloneFilterApp do JUCE, que criava a
// StandaloneFilterWindow com a barra do sistema, o botao "Options" e o dialogo de
// audio do AudioDeviceSelectorComponent.
//
// **o LookAndFeel e' o da chapa em toda a app.** assim os menus de escolha, os
// avisos e o ecra de configuracao saem da mesma linguagem; no VST3 nada disto
// acontece, porque o LookAndFeel global e' do host e nos nao lhe tocamos.
class StandaloneApp final : public juce::JUCEApplication {
public:
    StandaloneApp() = default;

    const juce::String getApplicationName() override { return juce::CharPointer_UTF8(JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String&) override {}

    void initialise(const juce::String&) override {
        juce::PropertiesFile::Options options;
        options.applicationName = juce::CharPointer_UTF8(JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
#if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
#else
        options.folderName = "";
#endif
        appProperties_.setStorageParameters(options);

        lookAndFeel_ = std::make_unique<OpcodaLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel(lookAndFeel_.get());

        auto holder = std::make_unique<juce::StandalonePluginHolder>(
            appProperties_.getUserSettings(), false, juce::String {}, nullptr,
            juce::Array<juce::StandalonePluginHolder::PluginInOuts> {}, false);

        openMidiInputsOnFirstRun(*holder);

        window_ = std::make_unique<StandaloneWindow>(getApplicationName(), palette::chassis,
                                                     std::move(holder));
        window_->setVisible(true);
        window_->applyTaskbarIcon();
    }

    void shutdown() override {
        window_.reset();

        // a ordem importa: um LookAndFeel nao pode morrer enquanto for o padrao.
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        lookAndFeel_.reset();
        appProperties_.saveIfNeeded();
    }

    void systemRequestedQuit() override {
        if (window_ != nullptr) {
            window_->holder().savePluginState();
        }

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents()) {
            juce::Timer::callAfterDelay(100, [] {
                if (auto* app = juce::JUCEApplicationBase::getInstance()) {
                    app->systemRequestedQuit();
                }
            });
        } else {
            quit();
        }
    }

private:
    // Sem um audioSetup guardado, o JUCE nao liga nenhuma entrada MIDI e o
    // instrumento parece surdo com um teclado USB ligado. na primeira execucao
    // ficam todas ligadas; o ecra de configuracao mostra-as e desliga o que o
    // utilizador nao quiser, e a escolha persiste no .settings. e' a mesma
    // politica do autoOpenMidiDevices, que o JUCE so' usa no telemovel.
    static void openMidiInputsOnFirstRun(juce::StandalonePluginHolder& holder) {
        if (holder.settings == nullptr || holder.settings->containsKey("audioSetup")) {
            return;
        }

        for (const auto& input : juce::MidiInput::getAvailableDevices()) {
            holder.deviceManager.setMidiInputDeviceEnabled(input.identifier, true);
        }
    }

    juce::ApplicationProperties appProperties_;
    std::unique_ptr<OpcodaLookAndFeel> lookAndFeel_;
    std::unique_ptr<StandaloneWindow> window_;
};

} // namespace
} // namespace opcoda

START_JUCE_APPLICATION(opcoda::StandaloneApp)
