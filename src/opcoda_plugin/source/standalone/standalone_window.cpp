#include "standalone_window.h"

#include "../plugin_editor.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace opcoda {

//==============================================================================
StandaloneWindow::Body::Body(juce::StandalonePluginHolder& holder) {
    if (auto* processor = holder.processor.get()) {
        editor_.reset(processor->hasEditor() ? processor->createEditorAndMakeActive()
                                             : new juce::GenericAudioProcessorEditor(*processor));
    }

    if (editor_ != nullptr) {
        addAndMakeVisible(*editor_);
        preferredWidth_ = editor_->getWidth();
        preferredHeight_ = editor_->getHeight();
    }

    settings_ = std::make_unique<SettingsScreen>(holder.deviceManager);

    // as teclas que o ecra de configuracao nao usa tocam no editor, e o foco
    // volta ao teclado virtual quando ele fecha: escolher a placa com uma nota
    // presa e' o caso normal, e fechar os ajustes nao pode calar o instrumento.
    settings_->onUnhandledKey = [this](const juce::KeyPress& key) {
        return editor_ != nullptr && editor_->keyPressed(key);
    };
    settings_->onKeyStateChanged = [this](bool isKeyDown) {
        return editor_ != nullptr && editor_->keyStateChanged(isKeyDown);
    };
    settings_->onClosed = [this] {
        if (auto* editor = dynamic_cast<PluginEditor*>(editor_.get())) {
            editor->focusPlayKeyboard();
        }
    };

    addChildComponent(*settings_);

    setSize(preferredWidth_, preferredHeight_);
}

void StandaloneWindow::Body::resized() {
    if (editor_ != nullptr) {
        editor_->setBounds(getLocalBounds());
    }
    if (settings_ != nullptr) {
        settings_->setBounds(getLocalBounds());
    }
}

void StandaloneWindow::Body::openSettings() {
    if (settings_ != nullptr) {
        settings_->open();
    }
}

//==============================================================================
StandaloneWindow::StandaloneWindow(juce::String title,
                                   juce::Colour background,
                                   std::unique_ptr<juce::StandalonePluginHolder> holder)
    : DocumentWindow(std::move(title), background, minimiseButton | closeButton),
      holder_(std::move(holder)) {
    setUsingNativeTitleBar(false);
    setTitleBarHeight(kTitleHeight);
    setTitleBarButtonsRequired(minimiseButton | closeButton, false);
    setResizable(false, false);

    settingsButton_.setButtonText("SETTINGS");
    settingsButton_.getProperties().set("uiIcon", "settings");
    settingsButton_.getProperties().set("uiLook", "bar");
    settingsButton_.onClick = [this] {
        if (body_ != nullptr) {
            body_->openSettings();
        }
    };
    // a janela nao e' um pai de componentes normal; o proprio StandaloneFilterWindow
    // do JUCE faz o mesmo com o botao Options.
    juce::Component::addAndMakeVisible(settingsButton_);

    auto body = std::make_unique<Body>(*holder_);
    body_ = body.get();
    setContentOwned(body.release(), false);

    // a moldura de 1 px do ResizableWindow conta para o tamanho: sem isto o
    // editor de 1180x760 ficava cortado 2 px a' direita e em baixo.
    const auto border = getBorderThickness();
    setSize(body_->preferredWidth() + border.getLeftAndRight(),
            body_->preferredHeight() + border.getTopAndBottom() + kTitleHeight);
    restoreWindowPosition();
}

StandaloneWindow::~StandaloneWindow() {
    saveWindowPosition();

    // o editor e o ecra saem primeiro: o processador (dentro do holder) tem de
    // os ver morrer.
    clearContentComponent();
    body_ = nullptr;
    holder_.reset();
}

void StandaloneWindow::resized() {
    DocumentWindow::resized();

    const auto titleBar = getTitleBarArea();
    settingsButton_.setBounds(titleBar.getRight() - 2 * 34 - 4 - 8 - 96, titleBar.getY() + 4,
                              96, titleBar.getHeight() - 8);
}

void StandaloneWindow::applyTaskbarIcon() {
    if (auto* peer = getPeer()) {
        peer->setIcon(makeWindowIcon());
    }
}

void StandaloneWindow::closeButtonPressed() {
    if (holder_ != nullptr) {
        holder_->savePluginState();
    }
    juce::JUCEApplicationBase::quit();
}

//==============================================================================
void StandaloneWindow::restoreWindowPosition() {
    const auto fallback = [this] { centreWithSize(getWidth(), getHeight()); };

    if (holder_ == nullptr || holder_->settings == nullptr) {
        fallback();
        return;
    }

    const auto x = holder_->settings->getIntValue("windowX", -1);
    const auto y = holder_->settings->getIntValue("windowY", -1);
    if (x < 0 || y < 0) {
        fallback();
        return;
    }

    // setBoundsConstrained e nao setTopLeftPosition: se o monitor onde a janela
    // estava foi desligado, a posicao guardada punha-a fora do ecra.
    setBoundsConstrained(
        juce::Rectangle<int> {x, y, getWidth(), getHeight()});
}

void StandaloneWindow::saveWindowPosition() {
    if (holder_ == nullptr || holder_->settings == nullptr) {
        return;
    }
    if (isFullScreen() || isMinimised()) {
        return;
    }

    holder_->settings->setValue("windowX", getX());
    holder_->settings->setValue("windowY", getY());
}

juce::Image StandaloneWindow::makeWindowIcon() {
    // o icone e' a marca: chapa laranja com "Op" na fonte do nome. a mesma
    // funcao desenha a barra da janela e o cabecalho dos ajustes, por isso a
    // barra de tarefas, a janela e o ecra dizem a mesma coisa.
    constexpr int kSize {256};
    juce::Image icon(juce::Image::ARGB, kSize, kSize, true);
    juce::Graphics g(icon);

    OpcodaLookAndFeel::paintMark(g, juce::Rectangle<float> {16.0f, 16.0f, 224.0f, 224.0f});

    return icon;
}

} // namespace opcoda
