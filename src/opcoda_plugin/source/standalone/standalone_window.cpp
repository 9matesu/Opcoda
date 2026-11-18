#include "standalone_window.h"

#include "../plugin_editor.h"

#include "OpcodaAssets.h"

#include <juce_audio_utils/juce_audio_utils.h>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace opcoda {

//==============================================================================
StandaloneWindow::Body::Body(juce::StandalonePluginHolder& holder) {
    if (auto* processor = holder.processor.get()) {
        editor_.reset(processor->hasEditor() ? processor->createEditorAndMakeActive()
                                             : new juce::GenericAudioProcessorEditor(*processor));
    }

    if (editor_ != nullptr) {
        addAndMakeVisible(*editor_);
        if (auto* pluginEditor = dynamic_cast<PluginEditor*>(editor_.get())) {
            pluginEditor->setStandaloneChrome(true);
        }
        preferredWidth_ = editor_->getWidth();
        preferredHeight_ = editor_->getHeight();
    }

    settings_ = std::make_unique<SettingsScreen>(holder.deviceManager);

    settings_->onThemeChanged = [this] {
        if (auto* editor = dynamic_cast<PluginEditor*>(editor_.get())) {
            editor->refreshTheme();
        }
        if (settings_ != nullptr) {
            settings_->refreshPalette();
        }
        if (auto* lnf =
                dynamic_cast<OpcodaLookAndFeel*>(&juce::LookAndFeel::getDefaultLookAndFeel())) {
            lnf->refreshPalette();
        }
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>()) {
            window->setBackgroundColour(palette::chassis);
            window->repaint();
        }
        repaint();
    };

    settings_->onUnhandledKey = [this](const juce::KeyPress& key) {
        return editor_ != nullptr && editor_->keyPressed(key);
    };
    settings_->onKeyStateChanged = [this](bool isKeyDown) {
        return editor_ != nullptr && editor_->keyStateChanged(isKeyDown);
    };
    if (auto* pluginEditor = dynamic_cast<PluginEditor*>(editor_.get())) {
        pluginEditor->setOnThemeRequested([this] { openSettings(); });
    }

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

void StandaloneWindow::Body::childBoundsChanged(juce::Component* child) {
    if (child != editor_.get() || editor_ == nullptr) {
        return;
    }
    const auto width = editor_->getWidth();
    const auto height = editor_->getHeight();
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width == getWidth() && height == getHeight()) {
        return;
    }
    preferredWidth_ = width;
    preferredHeight_ = height;
    setSize(width, height);
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>()) {
        const auto border = window->getBorderThickness();
        window->setSize(width + border.getLeftAndRight(),
                        height + border.getTopAndBottom());
        auto bounds = window->getBounds();
        if (auto* display = juce::Desktop::getInstance()
                                .getDisplays()
                                .getDisplayForRect(bounds)) {
            const auto area = display->userBounds;
            if (bounds.getBottom() > area.getBottom()) {
                bounds.setY(area.getBottom() - bounds.getHeight());
            }
            if (bounds.getRight() > area.getRight()) {
                bounds.setX(area.getRight() - bounds.getWidth());
            }
            window->setBounds(bounds);
        }
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
    : DocumentWindow(std::move(title), background, 0),
      holder_(std::move(holder)) {
    setUsingNativeTitleBar(false);
    setTitleBarHeight(0);
    setResizable(false, false);

    closeButton_.setName("Close");
    closeButton_.setTooltip("Close Opcoda");
    closeButton_.onClick = [this] { closeButtonPressed(); };
    minimiseButton_.setName("Minimise");
    minimiseButton_.setTooltip("Minimise");
    minimiseButton_.onClick = [this] { minimiseButtonPressed(); };
    addAndMakeVisible(closeButton_);
    addAndMakeVisible(minimiseButton_);

    auto body = std::make_unique<Body>(*holder_);
    body_ = body.get();
    setContentOwned(body.release(), false);

    closeButton_.toFront(false);
    minimiseButton_.toFront(false);

    const auto border = getBorderThickness();
    setSize(body_->preferredWidth() + border.getLeftAndRight(),
            body_->preferredHeight() + border.getTopAndBottom());
    restoreWindowPosition();
}

StandaloneWindow::~StandaloneWindow() {
    saveWindowPosition();

    clearContentComponent();
    body_ = nullptr;
    holder_.reset();
}

void StandaloneWindow::resized() {
    DocumentWindow::resized();

    const auto place = [this](juce::Button& button, int x, int size) {
        // centrado no cabecalho de 48 px, encostado a' direita.
        button.setBounds(x, (48 - size) / 2, size, size);
    };
    const int closeX =
        getWidth() - windowChrome::kTrafficPad - windowChrome::kTrafficSize;
    place(closeButton_, closeX, windowChrome::kTrafficSize);
    place(minimiseButton_, closeX - windowChrome::kTrafficSize -
                               windowChrome::kTrafficGap,
          windowChrome::kTrafficSize);
    applyRoundedCorners();
}

void StandaloneWindow::applyRoundedCorners() {
#if JUCE_WINDOWS
    auto* peer = getPeer();
    if (peer == nullptr) {
        return;
    }
    auto* hwnd = static_cast<HWND>(peer->getNativeHandle());
    if (hwnd == nullptr || getWidth() < 32 || getHeight() < 32) {
        return;
    }
    constexpr int kRadius {12};
    if (HRGN region = ::CreateRoundRectRgn(0, 0, getWidth() + 1, getHeight() + 1,
                                            kRadius * 2, kRadius * 2)) {
        ::SetWindowRgn(hwnd, region, TRUE);
    }
#endif
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
    int iconSize {0};
    if (auto* data = OpcodaAssets::getNamedResource("opcodaicon256_png", iconSize)) {
        const auto image = juce::ImageFileFormat::loadFrom(
            data, static_cast<std::size_t>(iconSize));
        if (image.isValid()) {
            return image;
        }
    }

    // o PNG nao embarcou: a marca segura o lugar.
    constexpr int kSize {256};
    juce::Image icon(juce::Image::ARGB, kSize, kSize, true);
    juce::Graphics g(icon);

    OpcodaLookAndFeel::paintMark(g, juce::Rectangle<float> {16.0f, 16.0f, 224.0f, 224.0f});

    return icon;
}

} // namespace opcoda
