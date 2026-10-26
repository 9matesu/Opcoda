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

// Ecra de configuracao do standalone, aberto pelo botao SETTINGS da barra da
// janela. Substitui o AudioDeviceSelectorComponent que o JUCE trazia dentro de um
// DialogWindow: aquele era uma grelha de etiquetas cinzentas com sliders do
// sistema, e aparecia sempre que se ia escolher a placa.
//
// **o veu nao tapa o instrumento.** o painel flutua sobre o ecra com 62% de
// carvao por baixo, para a chapa continuar a ser o contexto; fechar e' um clique
// fora, a tecla Escape ou o CLOSE.
//
// **as mudancas aplicam-se ao escolher.** nao ha botao APPLY: um pedido de
// dispositivo que falhe deixa a razao escrita a' esquerda do CLOSE, em ferrugem,
// e o dispositivo anterior continua a tocar.
class SettingsScreen final : public juce::Component,
                             private juce::ChangeListener,
                             private juce::Timer {
public:
    explicit SettingsScreen(juce::AudioDeviceManager& devices);
    ~SettingsScreen() override;

    // abre com os valores que o gestor tem agora e rouba o foco para o Escape
    // chegar aqui antes de chegar ao editor.
    void open();
    void close();

    // o instrumento continua a tocar por baixo do veu. as teclas que o ecra nao
    // usa (o mapa A W S E...) vao para o editor, para se poder escolher a placa
    // com uma nota presa; e quando o ecra fecha o foco volta ao teclado virtual.
    std::function<bool(const juce::KeyPress&)> onUnhandledKey;
    std::function<bool(bool)> onKeyStateChanged;
    std::function<void()> onClosed;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    enum class Page { audio, midi };

    [[nodiscard]] juce::Rectangle<int> panelBounds() const;
    [[nodiscard]] juce::Rectangle<int> bodyBounds() const;

    void selectPage(Page page);
    void refreshAudio();
    void refreshMidi();
    void refreshStatus();
    void applyAudio();
    void paintStatus(juce::Graphics&) const;

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void visibilityChanged() override;

    juce::AudioDeviceManager& devices_;

    juce::TextButton audioTab_ {"AUDIO"};
    juce::TextButton midiTab_ {"MIDI"};
    juce::TextButton closeButton_;
    juce::TextButton doneButton_ {"CLOSE"};
    juce::TextButton controlPanel_ {"CONTROL PANEL"};

    juce::ComboBox driverBox_;
    juce::ComboBox outputBox_;
    juce::ComboBox rateBox_;
    juce::ComboBox bufferBox_;

    std::vector<std::unique_ptr<juce::TextButton>> midiRows_;

    Page page_ {Page::audio};
    bool applying_ {false};
    juce::String error_;

    juce::Rectangle<int> panel_;
    juce::Rectangle<int> status_;

    static constexpr int kPanelWidth {820};
    static constexpr int kPanelHeight {580};
    static constexpr int kHeaderHeight {52};
    static constexpr int kInset {28};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsScreen)
};

} // namespace opcoda
