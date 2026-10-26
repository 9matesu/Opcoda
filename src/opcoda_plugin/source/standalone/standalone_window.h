#pragma once

// **a ordem das inclusoes conta.** o juce_StandaloneFilterWindow.h nao inclui os
// modulos de que depende: quem o inclui tem de trazer primeiro audio_devices,
// audio_utils e gui_extra, como faz o juce_audio_plugin_client_Standalone.cpp.
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

// Janela do standalone. Substitui a StandaloneFilterWindow do JUCE, que trazia o
// botao "Options" dentro da barra do sistema, um menu com Save/Load/About e o
// AudioDeviceSelectorComponent num DialogWindow.
//
// **a barra e' desenhada pelo OpcodaLookAndFeel** (drawDocumentWindowTitleBar e
// createDocumentWindowButton): grafite, marca a' esquerda, minimizar e fechar em
// teclas planas. o botao SETTINGS e' um filho da janela, como o Options era, e
// abre um ecra proprio por cima do editor -- o instrumento continua a tocar por
// baixo enquanto se escolhe a placa.
class StandaloneWindow final : public juce::DocumentWindow {
public:
    StandaloneWindow(juce::String title, juce::Colour background,
                     std::unique_ptr<juce::StandalonePluginHolder> holder);
    ~StandaloneWindow() override;

    [[nodiscard]] juce::StandalonePluginHolder& holder() noexcept { return *holder_; }

    // o icone da barra de tarefas vive no peer, nao no DocumentWindow (o
    // setIcon do JUCE so' guarda uma copia para o titulo, que este LookAndFeel
    // nem desenha). so' tem efeito depois de a janela estar no desktop.
    void applyTaskbarIcon();

    void closeButtonPressed() override;
    void resized() override;

private:
    // Area de conteudo: o editor do plugin e, por cima, o ecra de configuracao.
    // o editor e' criado pelo processador (createEditorAndMakeActive) e morre
    // antes dele, que e' a ordem que o AudioProcessor espera.
    class Body final : public juce::Component {
    public:
        explicit Body(juce::StandalonePluginHolder& holder);

        void resized() override;
        void openSettings();

        [[nodiscard]] int preferredWidth() const noexcept { return preferredWidth_; }
        [[nodiscard]] int preferredHeight() const noexcept { return preferredHeight_; }

    private:
        std::unique_ptr<juce::AudioProcessorEditor> editor_;
        std::unique_ptr<SettingsScreen> settings_;
        int preferredWidth_ {1180};
        int preferredHeight_ {760};
    };

    void restoreWindowPosition();
    void saveWindowPosition();
    [[nodiscard]] static juce::Image makeWindowIcon();

    std::unique_ptr<juce::StandalonePluginHolder> holder_;
    Body* body_ {nullptr};
    juce::TextButton settingsButton_;

    static constexpr int kTitleHeight {36};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StandaloneWindow)
};

} // namespace opcoda
