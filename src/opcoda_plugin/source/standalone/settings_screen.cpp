#include "settings_screen.h"

#include <array>
#include <cmath>

namespace opcoda {
namespace {

void paintSectionLabel(juce::Graphics& g, const char* text, juce::Rectangle<int> area) {
    g.setColour(palette::textSub);
    g.setFont(type::captionFont().withExtraKerningFactor(type::trackingCaps));
    g.drawText(text, area, juce::Justification::centredLeft, false);
}

juce::String formatRate(double rate) {
    return rate > 0.0 ? juce::String(rate / 1000.0, 1) + " kHz" : juce::String {"-"};
}

juce::String formatMs(double samples, double rate) {
    return rate > 0.0 ? juce::String(1000.0 * samples / rate, 1) + " ms" : juce::String {"-"};
}

juce::String formatInteger(double value) {
    return juce::String(juce::roundToInt(value));
}

} // namespace

SettingsScreen::SettingsScreen(juce::AudioDeviceManager& devices) : devices_(devices) {
    for (auto* tab : {&audioTab_, &midiTab_, &themesTab_}) {
        tab->getProperties().set("uiLook", "tab");
        tab->setClickingTogglesState(true);
        tab->setRadioGroupId(1);
        addAndMakeVisible(*tab);
    }
    audioTab_.setToggleState(true, juce::dontSendNotification);
    audioTab_.onClick = [this] { selectPage(Page::audio); };
    midiTab_.onClick = [this] { selectPage(Page::midi); };
    themesTab_.onClick = [this] { selectPage(Page::themes); };

    for (auto* row : {&darkTheme_, &classicTheme_}) {
        row->getProperties().set("uiLook", "row");
        row->setClickingTogglesState(true);
        row->setRadioGroupId(2);
        addChildComponent(*row);
    }
    darkTheme_.onClick = [this] { chooseTheme(palette::Theme::dark); };
    classicTheme_.onClick = [this] { chooseTheme(palette::Theme::classic); };

    // a cruz do cabecalho fala a lingua da rail: traco claro sobre carvao.
    closeButton_.setButtonText("");
    closeButton_.getProperties().set("uiIcon", "close");
    closeButton_.getProperties().set("uiLook", "rail");
    closeButton_.onClick = [this] { close(); };
    addAndMakeVisible(closeButton_);

    doneButton_.onClick = [this] { close(); };
    addAndMakeVisible(doneButton_);

    controlPanel_.getProperties().set("uiLook", "bar");
    controlPanel_.onClick = [this] {
        if (auto* device = devices_.getCurrentAudioDevice()) {
            if (device->hasControlPanel()) {
                device->showControlPanel();
            }
        }
    };
    addChildComponent(controlPanel_);

    for (auto* box : {&driverBox_, &outputBox_, &rateBox_, &bufferBox_}) {
        box->setColour(juce::ComboBox::textColourId, palette::textOnDark);
        box->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        box->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        box->setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
        box->setJustificationType(juce::Justification::centredLeft);
        addChildComponent(*box);
    }

    refreshPalette();

    driverBox_.onChange = [this] {
        if (applying_) {
            return;
        }

        const auto type = driverBox_.getText();
        if (type.isEmpty()) {
            return;
        }

        applying_ = true;
        devices_.setCurrentAudioDeviceType(type, true);
        applying_ = false;

        error_ = devices_.getCurrentAudioDevice() != nullptr
                     ? juce::String {}
                     : "driver opened without a device";
        refreshAudio();
        repaint();
    };

    outputBox_.onChange = [this] { applyAudio(); };
    rateBox_.onChange = [this] { applyAudio(); };
    bufferBox_.onChange = [this] { applyAudio(); };

    devices_.addChangeListener(this);
    setWantsKeyboardFocus(true);
    setVisible(false);
}

SettingsScreen::~SettingsScreen() {
    devices_.removeChangeListener(this);
}

//==============================================================================
void SettingsScreen::open() {
    refreshAudio();
    refreshMidi();
    selectPage(Page::audio);
    setVisible(true);
    toFront(true);
    grabKeyboardFocus();
    startTimerHz(4);
}

void SettingsScreen::close() {
    setVisible(false);
    stopTimer();

    if (onClosed) {
        onClosed();
    }
}

void SettingsScreen::visibilityChanged() {
    if (!isVisible()) {
        stopTimer();
    }
}

//==============================================================================
juce::Rectangle<int> SettingsScreen::panelBounds() const {
    const auto width = juce::jmin(kPanelWidth, getWidth() - 72);
    const auto height = juce::jmin(kPanelHeight, getHeight() - 48);
    return juce::Rectangle<int> {juce::jmax(320, width), juce::jmax(240, height)}
        .withCentre(getLocalBounds().getCentre());
}

juce::Rectangle<int> SettingsScreen::bodyBounds() const {
    return panel_.withTrimmedTop(120).withTrimmedBottom(68).reduced(kInset, 0);
}

void SettingsScreen::resized() {
    panel_ = panelBounds();

    closeButton_.setBounds(panel_.getRight() - kInset - 30, panel_.getY() + 11, 30, 30);
    doneButton_.setBounds(panel_.getRight() - kInset - 118, panel_.getBottom() - 48, 118, 30);

    audioTab_.setBounds(panel_.getX() + kInset, panel_.getY() + 66, 92, 28);
    midiTab_.setBounds(panel_.getX() + kInset + 96, panel_.getY() + 66, 92, 28);
    themesTab_.setBounds(panel_.getX() + kInset + 192, panel_.getY() + 66, 92, 28);

    const auto body = bodyBounds();
    auto y = body.getY() + 4;
    const auto comboWidth = juce::jmin(520, juce::jmax(180, body.getWidth() - 200));
    for (auto* box : {&driverBox_, &outputBox_, &rateBox_, &bufferBox_}) {
        box->setBounds(body.getX() + 180, y, comboWidth, 34);
        y += kRowPitch;
    }

    status_ = body.withTop(body.getBottom() - kStatusHeight);
    controlPanel_.setBounds(status_.getRight() - 160, status_.getY() + 6, 160, 28);

    auto rowY = body.getY() + 42;
    for (auto& row : midiRows_) {
        row->setBounds(body.getX(), rowY, body.getWidth(), 34);
        rowY += 34;
    }

    darkTheme_.setBounds(body.getX(), body.getY() + 42, body.getWidth(), 42);
    classicTheme_.setBounds(body.getX(), body.getY() + 100, body.getWidth(), 42);
}

//==============================================================================
void SettingsScreen::refreshPalette() {
    const auto ink = palette::currentTheme == palette::Theme::dark
                         ? palette::textOnDark
                         : palette::textDark;
    for (auto* box : {&driverBox_, &outputBox_, &rateBox_, &bufferBox_}) {
        if (box->findColour(juce::ComboBox::textColourId) != ink) {
            box->setColour(juce::ComboBox::textColourId, ink);
        }
    }
    repaint();
}

void SettingsScreen::paint(juce::Graphics& g) {
    g.fillAll(palette::alpha(juce::Colours::black, 0.62f));

    const auto panel = panel_.toFloat();
    if (panel.isEmpty()) {
        return;
    }

    // sombra em tres aneis: o painel flutua, nao esta' colado ao vidro.
    for (int i = 3; i >= 1; --i) {
        const auto spread = static_cast<float>(i) * 4.0f;
        g.setColour(palette::alpha(juce::Colours::black, 0.12f));
        g.fillRoundedRectangle(panel.expanded(spread).translated(0.0f, 8.0f), 6.0f + spread);
    }

    g.setColour(palette::faceplate);
    g.fillRoundedRectangle(panel, 6.0f);

    {
        juce::Path clipped;
        clipped.addRoundedRectangle(panel, 6.0f);

        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(clipped);
        g.setColour(palette::chassisDark);
        g.fillRect(juce::Rectangle<float> {panel.getX(), panel.getY(), panel.getWidth(),
                                           static_cast<float>(kHeaderHeight)});
        g.setColour(palette::alpha(palette::ivory, 0.08f));
        g.drawHorizontalLine(juce::roundToInt(panel.getY()) + kHeaderHeight - 1, panel.getX(),
                             panel.getRight());
    }

    g.setColour(palette::textOnDark);
    g.setFont(fonts().sans(11.0f, true).withExtraKerningFactor(0.20f));
    g.drawText("Settings",
               juce::Rectangle<int> {juce::roundToInt(panel.getX()) + kInset, panel_.getY(),
                                     240, kHeaderHeight},
               juce::Justification::centredLeft, false);

    // filete do rodape: separa a unica accao do ecra.
    g.setColour(palette::alpha(palette::ivory, 0.10f));
    g.drawHorizontalLine(panel_.getBottom() - 64, panel.getX() + kInset,
                         panel.getRight() - kInset);

    const auto body = bodyBounds();

    if (page_ == Page::audio) {
        constexpr std::array<const char*, 4> labels {"Driver", "Output", "Sample rate", "Buffer"};
        auto y = body.getY() + 4;
        for (const auto* label : labels) {
            paintSectionLabel(g, label, {body.getX(), y, 170, 30});
            y += kRowPitch;
        }

        paintSectionLabel(g, "Channels", {body.getX(), y, 170, 30});
        g.setColour(palette::textDark);
        g.setFont(fonts().sans(11.0f));
        g.drawText("2 out / 0 in", juce::Rectangle<int> {body.getX() + 180, y, 200, 30},
                   juce::Justification::centredLeft, false);

        paintStatus(g);
    } else if (page_ == Page::themes) {
        paintSectionLabel(g, "Theme", {body.getX(), body.getY() + 4, 300, 18});
        g.setColour(palette::textMuted);
        g.setFont(fonts().sans(9.5f));
        g.drawText("The whole instrument changes at once; the choice is saved.",
                   juce::Rectangle<int> {body.getX(), body.getY() + 22, body.getWidth(), 16},
                   juce::Justification::centredLeft, false);

        const auto note = [&g](juce::Rectangle<int> row, const juce::String& text) {
            g.setColour(palette::textMuted);
            g.setFont(fonts().sans(9.0f));
            g.drawText(text,
                       juce::Rectangle<int> {row.getX() + 16, row.getY() + 22,
                                             row.getWidth() - 32, 14},
                       juce::Justification::centredLeft, false);
        };
        note(darkTheme_.getBounds(), "Graphite and white. No colour anywhere.");
        note(classicTheme_.getBounds(), "Cream, charcoal and burnt orange. The 2025 face.");
    } else {
        paintSectionLabel(g, "MIDI inputs", {body.getX(), body.getY() + 4, 300, 18});
        g.setColour(palette::textMuted);
        g.setFont(fonts().sans(9.5f));
        g.drawText("Keyboards ticked here play the instrument.",
                   juce::Rectangle<int> {body.getX(), body.getY() + 22, body.getWidth(), 16},
                   juce::Justification::centredLeft, false);

        if (midiRows_.empty()) {
            g.setColour(palette::textSub);
            g.setFont(fonts().sans(11.0f));
            g.drawText("No MIDI inputs found.",
                       juce::Rectangle<int> {body.getX(), body.getY() + 58, body.getWidth(), 20},
                       juce::Justification::centredLeft, false);
        }
    }

    if (error_.isNotEmpty()) {
        g.setColour(palette::error);
        g.setFont(fonts().sans(9.5f));
        g.drawText(error_,
                   juce::Rectangle<int> {panel_.getX() + kInset, panel_.getBottom() - 48,
                                         panel_.getWidth() - 2 * kInset - 140, 30},
                   juce::Justification::centredLeft, true);
    }
}

void SettingsScreen::paintStatus(juce::Graphics& g) const {
    const auto area = status_;
    if (area.isEmpty()) {
        return;
    }

    g.setColour(palette::alpha(palette::ivory, 0.08f));
    g.drawHorizontalLine(area.getY(), static_cast<float>(area.getX()),
                         static_cast<float>(area.getRight()));

    paintSectionLabel(g, "Device", {area.getX(), area.getY() + 10, 200, 16});

    auto* device = devices_.getCurrentAudioDevice();
    g.setColour(palette::textDark);
    g.setFont(fonts().sans(11.5f, true));
    g.drawText(device != nullptr ? device->getName() : juce::String {"no device"},
               juce::Rectangle<int> {area.getX(), area.getY() + 28, area.getWidth() - 176, 20},
               juce::Justification::centredLeft, true);

    if (device != nullptr) {
        const auto rate = device->getCurrentSampleRate();
        const auto buffer = device->getCurrentBufferSizeSamples();
        const auto latency = formatMs(static_cast<double>(buffer) +
                                          device->getOutputLatencyInSamples(),
                                      rate);
        const auto line = formatRate(rate) + " / " + juce::String(buffer) + " samples / " +
                          latency;
        g.setColour(palette::textSub);
        g.setFont(fonts().sans(10.5f));
        g.drawText(line, juce::Rectangle<int> {area.getX(), area.getY() + 48,
                                               area.getWidth() - 176, 18},
                   juce::Justification::centredLeft, true);
    }

    const auto load = juce::jlimit(0.0, 1.0, devices_.getCpuUsage());
    const auto bar = juce::Rectangle<int> {area.getX() + 34, area.getY() + 80, 200, 8};

    paintSectionLabel(g, "CPU", {area.getX(), area.getY() + 74, 30, 20});

    g.setColour(palette::alpha(palette::ivory, 0.10f));
    g.fillRoundedRectangle(bar.toFloat(), 3.0f);
    g.setColour(load > 0.85 ? palette::meterRed
                            : (load > 0.60 ? palette::meterAmber : palette::meterGreen));
    g.fillRoundedRectangle(
        bar.toFloat().withWidth(static_cast<float>(bar.getWidth()) * static_cast<float>(load)),
        3.0f);

    g.setColour(palette::textDark);
    g.setFont(fonts().sans(10.5f));
    g.drawText(formatInteger(load * 100.0) + " %",
               juce::Rectangle<int> {bar.getRight() + 10, area.getY() + 74, 60, 20},
               juce::Justification::centredLeft, false);
}

//==============================================================================
void SettingsScreen::mouseDown(const juce::MouseEvent& event) {
    if (!panel_.contains(event.getPosition())) {
        close();
    }
}

bool SettingsScreen::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) {
        close();
        return true;
    }

    // tudo o resto e' do instrumento: uma janela aberta nao pode calar o teclado.
    return onUnhandledKey != nullptr ? onUnhandledKey(key) : false;
}

bool SettingsScreen::keyStateChanged(bool isKeyDown) {
    return onKeyStateChanged != nullptr ? onKeyStateChanged(isKeyDown) : false;
}

//==============================================================================
void SettingsScreen::selectPage(Page page) {
    page_ = page;

    audioTab_.setToggleState(page == Page::audio, juce::dontSendNotification);
    midiTab_.setToggleState(page == Page::midi, juce::dontSendNotification);
    themesTab_.setToggleState(page == Page::themes, juce::dontSendNotification);

    const auto audio = page == Page::audio;
    for (auto* box : {&driverBox_, &outputBox_, &rateBox_, &bufferBox_}) {
        box->setVisible(audio);
    }
    for (auto& row : midiRows_) {
        row->setVisible(page == Page::midi);
    }

    const auto themes = page == Page::themes;
    darkTheme_.setVisible(themes);
    classicTheme_.setVisible(themes);
    if (themes) {
        darkTheme_.setToggleState(palette::currentTheme == palette::Theme::dark,
                                  juce::dontSendNotification);
        classicTheme_.setToggleState(palette::currentTheme == palette::Theme::classic,
                                     juce::dontSendNotification);
    }

    if (page == Page::midi) {
        refreshMidi();
    }

    refreshStatus();
    resized();
    repaint();
}

void SettingsScreen::chooseTheme(palette::Theme theme) {
    palette::setTheme(theme);
    palette::saveTheme(theme);
    palette::applyToTree(*this);

    darkTheme_.setToggleState(theme == palette::Theme::dark, juce::dontSendNotification);
    classicTheme_.setToggleState(theme == palette::Theme::classic,
                                 juce::dontSendNotification);

    if (onThemeChanged) {
        onThemeChanged();
    }
    repaint();
}

void SettingsScreen::refreshAudio() {
    if (applying_) {
        return;
    }

    applying_ = true;

    driverBox_.clear(juce::dontSendNotification);
    const auto currentType = devices_.getCurrentAudioDeviceType();
    int driverId = 0;
    for (auto* type : devices_.getAvailableDeviceTypes()) {
        if (type == nullptr) {
            continue;
        }
        driverBox_.addItem(type->getTypeName(), driverBox_.getNumItems() + 1);
        if (type->getTypeName() == currentType) {
            driverId = driverBox_.getNumItems();
        }
    }
    if (driverBox_.getNumItems() > 0) {
        driverBox_.setSelectedId(driverId > 0 ? driverId : 1, juce::dontSendNotification);
    }

    juce::StringArray outputs;
    if (auto* type = devices_.getCurrentDeviceTypeObject()) {
        type->scanForDevices();
        outputs = type->getDeviceNames(false);
    }

    outputBox_.clear(juce::dontSendNotification);
    for (const auto& name : outputs) {
        outputBox_.addItem(name, outputBox_.getNumItems() + 1);
    }

    const auto setup = devices_.getAudioDeviceSetup();
    if (setup.outputDeviceName.isNotEmpty()) {
        outputBox_.setText(setup.outputDeviceName, juce::dontSendNotification);
    } else if (!outputs.isEmpty()) {
        outputBox_.setSelectedItemIndex(0, juce::dontSendNotification);
    }

    rateBox_.clear(juce::dontSendNotification);
    bufferBox_.clear(juce::dontSendNotification);

    if (auto* device = devices_.getCurrentAudioDevice()) {
        const auto rates = device->getAvailableSampleRates();
        const auto currentRate = device->getCurrentSampleRate();
        for (int i = 0; i < rates.size(); ++i) {
            rateBox_.addItem(formatRate(rates[i]), i + 1);
            if (std::abs(rates[i] - currentRate) < 1.0) {
                rateBox_.setSelectedId(i + 1, juce::dontSendNotification);
            }
        }

        const auto rate = currentRate > 0.0 ? currentRate : 48000.0;
        const auto sizes = device->getAvailableBufferSizes();
        for (int i = 0; i < sizes.size(); ++i) {
            bufferBox_.addItem(juce::String(sizes[i]) + " (" + formatMs(sizes[i], rate) + ")",
                               i + 1);
            if (sizes[i] == device->getCurrentBufferSizeSamples()) {
                bufferBox_.setSelectedId(i + 1, juce::dontSendNotification);
            }
        }
    }

    applying_ = false;
    refreshStatus();
}

void SettingsScreen::refreshMidi() {
    const auto inputs = juce::MidiInput::getAvailableDevices();

    bool same = static_cast<int>(midiRows_.size()) == inputs.size();
    for (int i = 0; same && i < inputs.size(); ++i) {
        same = midiRows_[static_cast<std::size_t>(i)]->getButtonText() == inputs[i].name;
    }

    if (same) {
        // a lista nao mudou: so' o estado de cada teclado pode ter mudado.
        for (int i = 0; i < inputs.size(); ++i) {
            midiRows_[static_cast<std::size_t>(i)]->setToggleState(
                devices_.isMidiInputDeviceEnabled(inputs[i].identifier),
                juce::dontSendNotification);
        }
        return;
    }

    midiRows_.clear();

    for (const auto& input : inputs) {
        auto row = std::make_unique<juce::TextButton>(input.name);
        row->getProperties().set("uiLook", "row");
        row->setClickingTogglesState(true);
        row->setToggleState(devices_.isMidiInputDeviceEnabled(input.identifier),
                            juce::dontSendNotification);

        const auto identifier = input.identifier;
        auto* raw = row.get();
        row->onClick = [this, identifier, raw] {
            devices_.setMidiInputDeviceEnabled(identifier, raw->getToggleState());
        };

        addAndMakeVisible(*row);
        row->setVisible(page_ == Page::midi);
        midiRows_.push_back(std::move(row));
    }

    resized();
    repaint();
}

void SettingsScreen::refreshStatus() {
    auto* device = devices_.getCurrentAudioDevice();
    const auto wantsPanel = page_ == Page::audio && device != nullptr && device->hasControlPanel();
    if (controlPanel_.isVisible() != wantsPanel) {
        controlPanel_.setVisible(wantsPanel);
    }
    repaint(status_);
}

void SettingsScreen::applyAudio() {
    if (applying_ || page_ != Page::audio) {
        return;
    }

    auto setup = devices_.getAudioDeviceSetup();
    if (outputBox_.getText().isNotEmpty()) {
        setup.outputDeviceName = outputBox_.getText();
    }

    if (auto* device = devices_.getCurrentAudioDevice()) {
        const auto rates = device->getAvailableSampleRates();
        const auto sizes = device->getAvailableBufferSizes();
        const auto rateId = rateBox_.getSelectedId();
        const auto bufferId = bufferBox_.getSelectedId();
        if (rateId > 0 && rateId <= rates.size()) {
            setup.sampleRate = rates[rateId - 1];
        }
        if (bufferId > 0 && bufferId <= sizes.size()) {
            setup.bufferSize = sizes[bufferId - 1];
        }
    }

    applying_ = true;
    error_ = devices_.setAudioDeviceSetup(setup, true);
    applying_ = false;

    // a taxa nova muda os milissegundos do buffer: os rotulos sao refeitos.
    refreshAudio();
    repaint();
}

void SettingsScreen::changeListenerCallback(juce::ChangeBroadcaster*) {
    if (!isVisible() || applying_) {
        return;
    }

    refreshAudio();
    refreshMidi();
    repaint();
}

void SettingsScreen::timerCallback() {
    repaint(status_);
}

} // namespace opcoda
