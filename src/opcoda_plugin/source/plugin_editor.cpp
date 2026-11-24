#include "plugin_editor.h"

#include "factory_presets.h"
#include "ui_draw.h"
#include "ui_tokens.h"

#include "opcoda_core/pe/byte_to_position.h"

namespace opcoda {
namespace {

dsp::ModDest destForParam(const juce::String& id) {
    if (id == "pitch") return dsp::ModDest::kPitch;
    if (id == "density") return dsp::ModDest::kDensity;
    if (id == "position") return dsp::ModDest::kPosition;
    if (id == "spray") return dsp::ModDest::kSpray;
    if (id == "grain") return dsp::ModDest::kGrainSize;
    if (id == "f1cutoff") return dsp::ModDest::kCutoff1;
    if (id == "f1q") return dsp::ModDest::kQ1;
    if (id == "f2cutoff") return dsp::ModDest::kCutoff2;
    if (id == "f2q") return dsp::ModDest::kQ2;
    if (id == "pan") return dsp::ModDest::kPan;
    if (id == "grainlevel") return dsp::ModDest::kGrainLevel;
    if (id == "f1drive" || id == "f2drive") return dsp::ModDest::kDrive;
    if (id == "width") return dsp::ModDest::kWidth;
    return dsp::ModDest::kNone;
}

constexpr int kDesignWidth {1180};
constexpr int kHeaderHeight {48};

// Uma grelha so' para o chassi: margem 16, vao 16, alturas em multiplos de 8.
// Nao ha' 12, 10 nem 6 px espalhados pelo layout a partir daqui.
constexpr int kMargin {tokens::space3};
constexpr int kGap {tokens::space3};

constexpr int kDisplayHeight {396};
constexpr int kParameterHeight {176};
constexpr int kKeyboardHeight {80};
constexpr int kKeyToggleHeight {32};
constexpr int kMasterWidth {216};

// cabecalho + margem + ecra + vao + painel: o chassi fecha aqui.
constexpr int kChassisBottom {
    kHeaderHeight + kMargin + kDisplayHeight + kGap + kParameterHeight};
constexpr int kDesignHeightClosed {kChassisBottom + kGap + kKeyToggleHeight + kMargin};
constexpr int kDesignHeightOpen {kChassisBottom + kGap + kKeyboardHeight + kMargin};

constexpr int kTransportButtonWidth {32};
constexpr int kTransportButtonHeight {28};
constexpr int kTransportGap {tokens::space2};
constexpr int kLegendWidth {132};

// sidebar de vistas: 16 de padding + 72 de botao + 8 de folga ate' ao fio. O
// conteudo do ecra comeca 8 px depois do fio, na mesma coluna dos modulos.
constexpr int kRailWidth {kMargin + sidebar::kWidth + tokens::space2};
constexpr int kContentX {kRailWidth + tokens::space2};

constexpr int kAdsrStripWidth {280};
constexpr int kKnobContentHeight {90};
constexpr int kFooterHeight {28};
constexpr int kStatusHeight {16};

// padding do painel de parametros: o que ha' entre a chapa e o primeiro knob.
constexpr int kPanelPaddingX {kMargin};
constexpr int kPanelPaddingY {kMargin};
// o grafico do modulo ocupa a esquerda; os controles ficam a' direita.
constexpr int kGraphGap {kGap};
constexpr int kModGraphWidth {230};

constexpr std::uint32_t kGestureGapMs {400};
constexpr std::size_t kMaxUndoSteps {32};

float scaleForDisplay() {
    float scale = 1.0f;
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
        const auto area = display->userBounds;
        const auto sx = (area.getWidth() - 24.0f) / static_cast<float>(kDesignWidth);
        const auto sy = (area.getHeight() - 72.0f) / static_cast<float>(kDesignHeightOpen);
        scale = juce::jlimit(0.62f, 1.0f, juce::jmin(sx, sy));
    }
    return scale;
}

juce::String asInteger(float value) {
    return juce::String {juce::roundToInt(value)};
}

const auto percent = [](float v) { return asInteger(v * 100.0f) + " %"; };
const auto seconds = [](float v) {
    return v < 0.9995f ? asInteger(v * 1000.0f) + " ms" : juce::String {v, 2} + " s";
};
const auto cutoffText = [](float v) {
    return v >= 1000.0f ? juce::String {v / 1000.0f, 1} + " kHz" : asInteger(v) + " Hz";
};

const std::array<Knob::Spec, 6> kGrainRow1Specs {
    Knob::Spec {"grain", "Size", "Grain size, in milliseconds", 40.0f,
                [](float v) { return asInteger(v) + " ms"; }},
    Knob::Spec {"density", "Density", "Density, in grains per second", 20.0f,
                [](float v) { return asInteger(v) + " /s"; }},
    Knob::Spec {"position", "Position", "Read position in the material", 0.5f, percent},
    Knob::Spec {"spray", "Spray", "Spray, start position spread", 0.0f, percent},
    Knob::Spec {"pitch", "Pitch", "Tuning, in semitones", 0.0f,
                [](float v) { return juce::String {v, 1} + " st"; }},
    // o glide e' o unico parametro que sobrou do teclado: afina entre notas.
    Knob::Spec {"glide", "Glide", "Glide time between notes", 0.0f, seconds},
};

const std::array<Knob::Spec, 3> kMasterSideSpecs {
    Knob::Spec {"pan", "PAN", "Stereo pan, left to right", 0.0f,
                [](float v) {
                    return v == 0.0f ? juce::String {"C"}
                         : v < 0.0f  ? asInteger(-v * 100.0f) + " L"
                                     : asInteger(v * 100.0f) + " R";
                }},
    Knob::Spec {"width", "Stereo", "Stereo width, mono to wide", 1.0f,
                [](float v) { return asInteger(v * 100.0f) + " %"; }},
    Knob::Spec {"grainlevel", "Level", "Per-grain level", 1.0f, percent},
};

const std::vector<ChoiceControl::Item> kWindowItems {
    {"Hann window", "Hann", "winHann"},
    {"Gaussian window", "Gauss", "winGaussian"},
    {"Hamming window", "Hamm", "winHamming"},
    {"Blackman window", "Black", "winBlackman"},
};

// a forma do LFO: cinco botoes com a onda desenhada.
const std::vector<ChoiceControl::Item> kLfoWaveItems {
    {"Sine wave", "Sine", "waveSine"},
    {"Triangle wave", "Tri", "waveTri"},
    {"Sawtooth wave", "Saw", "waveSaw"},
    {"Square wave", "Square", "waveSquare"},
    {"Sample and hold", "S&H", "waveHold"},
};

// o tipo do filtro: quatro respostas em frequency.
const std::vector<ChoiceControl::Item> kFilterTypeItems {
    {"Low-pass filter", "Low-pass", "filterLowPass"},
    {"High-pass filter", "High-pass", "filterHighPass"},
    {"Band-pass filter", "Band-pass", "filterBandPass"},
    {"Notch filter", "Notch", "filterNotch"},
};

// o sync do LFO: um parametro booleano, sem itens proprios. Os nomes ficam
// aqui porque a ChoiceControl so' le AudioParameterChoice -- sem esta lista o
// botao existia e nao tinha texto, e era como se nao existisse.
const std::vector<ChoiceControl::Item> kLfoSyncItems {
    {"Free running", "Free", nullptr},
    {"Follow host tempo", "Sync", nullptr},
};

// divisões do LFO: o knob do DIV usa esta lista (12 posições).
const juce::StringArray kLfoDivLabels {
    "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
    "1/64", "1/4.", "1/8.", "1/4T", "1/8T", "1/16T"};

// o alvo do LFO: quatro botoes com rotulo curto.
const std::vector<ChoiceControl::Item> kLfoTargetItems {
    {"Modulate pitch", "PITCH", nullptr},
    {"Modulate density", "DENS", nullptr},
    {"Modulate cutoff", "CUT", nullptr},
    {"Modulate position", "POS", nullptr},
};

const auto curveText = [](float v) {
    const int pct = static_cast<int>(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
    return (pct == 0) ? juce::String {"LIN"} : juce::String {pct} + " %";
};

const std::array<Knob::Spec, 7> kEnvelopeSpecs {
    Knob::Spec {"attack", "ATK", "Attack time", 0.005f, seconds},
    Knob::Spec {"decay", "DEC", "Decay time", 0.1f, seconds},
    Knob::Spec {"sustain", "SUS", "Sustain level", 1.0f, percent},
    Knob::Spec {"release", "REL", "Release time", 0.05f, seconds},
    Knob::Spec {"atkcurve", "Atk curve", "Attack curve, exponential to logarithmic", 0.0f,
                curveText},
    Knob::Spec {"deccurve", "Dec curve", "Decay curve, exponential to logarithmic", 0.0f,
                curveText},
    Knob::Spec {"relcurve", "Rel curve", "Release curve, exponential to logarithmic", 0.0f,
                curveText},
};

const std::array<Knob::Spec, 5> kFilterRow1Specs {
    Knob::Spec {"f1cutoff", "F1 Cut", "Filter 1 cutoff frequency", 20000.0f, cutoffText},
    Knob::Spec {"f1q", "F1 Q", "Filter 1 resonance", 0.7071f,
                [](float v) { return juce::String {v, 2}; }},
    Knob::Spec {"f1drive", "F1 Drive", "Filter 1 drive, in decibels", 0.0f,
                [](float v) { return juce::String {v, 1} + " dB"; }},
    Knob::Spec {"f1keytrack", "F1 Key", "Filter 1 key tracking", 0.0f, percent},
    Knob::Spec {"f1env", "F1 Env", "Filter 1 envelope amount", 0.0f,
                [](float v) {
                    const int pct = static_cast<int>(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
                    return juce::String {pct} + " %";
                }},
};

const std::array<Knob::Spec, 2> kFilterRow2Specs {
    Knob::Spec {"f2cutoff", "F2 Cut", "Filter 2 cutoff frequency", 20000.0f, cutoffText},
    Knob::Spec {"f2q", "F2 Q", "Filter 2 resonance", 0.7071f,
                [](float v) { return juce::String {v, 2}; }},
};

const std::array<Knob::Spec, 3> kModRow1Specs {
    Knob::Spec {"lforate", "Rate", "LFO rate, in hertz", 1.0f,
                [](float v) { return juce::String {v, 2} + " Hz"; }},
    Knob::Spec {"lfodepth", "Depth", "LFO depth", 0.0f, percent},
    Knob::Spec {"lfosteps", "Steps", "LFO steps per cycle", 16.0f,
                [](float v) { return juce::String {static_cast<int>(v + 0.5f)}; }},
};

const std::array<Knob::Spec, 1> kOutSpecs {
    Knob::Spec {"volume", "Volume", "Output volume, in decibels", 0.0f,
                [](float v) {
                    return v <= -60.0f ? juce::String {"-inf"}
                                       : juce::String {v, 1} + " dB";
                }},
};

} // namespace

void setIfChanged(juce::Label& label, const juce::String& text) {
    if (label.getText() != text) {
        label.setText(text, juce::dontSendNotification);
        label.setName(text);
    }
}

PluginEditor::PluginEditor(PluginProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
      owner_(processor),
      keyboard_(keyboardState_, juce::MidiKeyboardComponent::horizontalKeyboard),
      envelopeView_(processor.parameters()),
      filterView_(processor.parameters()),
      lfoView_(processor.parameters()),
      pitchRandSlider_(processor.parameters(),
                       {"pitchrand", "P.RAND", "Random detune per grain, in semitones", 0.0f,
                        [](float v) { return juce::String {v, 1} + " st"; }}),
      scanSpeedSlider_(processor.parameters(),
                       {"scanspeed", "Scan", "Playhead scan speed, times realtime", 1.0f,
                        [](float v) { return juce::String {v, 2} + "x"; }}) {
    palette::setTheme(palette::loadSavedTheme());

    setLookAndFeel(&lookAndFeel_);
    setOpaque(true);

    uiScale_ = scaleForDisplay();
    canvas_.onPaint = [this](juce::Graphics& g) { paintDesign(g); };
    canvas_.setBounds(0, 0, kDesignWidth, designHeight());
    canvas_.setTransform(juce::AffineTransform::scale(uiScale_));
    addAndMakeVisible(canvas_);

    makePlainLabel(status_, "", palette::textOnDark, fonts().sans(11.0f));

    canvas_.addChildComponent(envelopeView_);
    canvas_.addChildComponent(filterView_);
    canvas_.addChildComponent(lfoView_);

    buildHeader();
    buildNavigation();
    buildPresetSelector();
    buildParameterPanel();
    buildKeyboard();
    filterView_.setSpectrum(owner_.telemetry().spectrum,
                            PluginProcessor::Telemetry::kSpectrumBins);
    animateButtons();

    grid_.attachModParameters(owner_.parameters());

    owner_.parameters().state.addListener(this);

    startTimerHz(60);

    setResizable(false, false);
    applyWindowSize();
    const std::function<void(juce::Component&)> releaseFocus = [&](juce::Component& c) {
        if (&c != &keyboard_) {
            c.setMouseClickGrabsKeyboardFocus(false);
        }
        for (int i = 0; i < c.getNumChildComponents(); ++i) {
            releaseFocus(*c.getChildComponent(i));
        }
    };
    releaseFocus(*this);

    keyboard_.grabKeyboardFocus();
    addMouseListener(this, true);

    refresh();

    settledState_ = owner_.stateXml();
}

PluginEditor::~PluginEditor() {
    buttonHovers_.clear();
    removeMouseListener(this);
    keyboardState_.allNotesOff(1);
    keyboardState_.removeListener(this);
    owner_.parameters().state.removeListener(this);
}

void PluginEditor::buildPresetSelector() {
    const auto setupArrow = [this](juce::TextButton& button, const char* glyph,
                                   const char* spokenName, int direction) {
        button.setButtonText("");
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName});
        button.getProperties().set("uiIcon", glyph);
        button.getProperties().set("uiLook", "icon");
        button.onClick = [this, direction] { stepFactoryPreset(direction); };
        canvas_.addAndMakeVisible(button);
    };
    setupArrow(presetPrevButton_, "chevronLeft", "Previous factory preset", -1);
    setupArrow(presetNextButton_, "chevronRight", "Next factory preset", +1);

    presetNameButton_.setButtonText(currentPresetName_);
    presetNameButton_.setName("Preset menu");
    presetNameButton_.setTooltip("Factory presets, load and save .opcoda");
    presetNameButton_.getProperties().set("uiIcon", "tab");
    presetNameButton_.onClick = [this] { showPresetMenu(); };
    canvas_.addAndMakeVisible(presetNameButton_);
}

void PluginEditor::showPresetMenu() {
    juce::PopupMenu menu;
    const auto& presets = factoryPresets();
    for (std::size_t i = 0; i < presets.size(); ++i) {
        menu.addItem(static_cast<int>(i) + 1, presets[i].name, true,
                     static_cast<int>(i) == currentFactoryIndex_ && !presetDirty_);
    }
    menu.addSeparator();
    menu.addItem(101, "Load .opcoda...");
    menu.addItem(102, "Save .opcoda...");

    menu.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this),
                       [this](int result) {
                           const auto count = static_cast<int>(factoryPresets().size());
                           if (result >= 1 && result <= count) {
                               applyFactoryPresetAndTrack(result - 1);
                           } else if (result == 101) {
                               loadPresetFromFile();
                           } else if (result == 102) {
                               savePresetToFile();
                           }
                       });
}

void PluginEditor::applyFactoryPresetAndTrack(int index) {
    applyingPreset_ = true;
    owner_.applyFactoryPreset(index);
    applyingPreset_ = false;
    currentFactoryIndex_ = index;
    currentPresetName_ = factoryPresets()[static_cast<std::size_t>(index)].name;
    presetDirty_ = false;
    refreshPresetName();
}

void PluginEditor::stepFactoryPreset(int direction) {
    const auto count = static_cast<int>(factoryPresets().size());
    applyFactoryPresetAndTrack((currentFactoryIndex_ + direction + count) % count);
}

void PluginEditor::refreshPresetName() {
    const auto text = presetDirty_ ? currentPresetName_ + " *" : currentPresetName_;
    if (presetNameButton_.getButtonText() != text) {
        presetNameButton_.setButtonText(text);
        presetNameButton_.setName("Preset: " + text);
    }
}

void PluginEditor::valueTreePropertyChanged(juce::ValueTree& changed,
                                            const juce::Identifier&) {
    if (!changed.hasType("PARAM")) {
        return;
    }

    if (!applyingPreset_) {
        presetDirty_ = true;
    }

    if (!restoringUndo_) {
        pushUndoSnapshot();
    }
}

void PluginEditor::pushUndoSnapshot() {
    if (!juce::MessageManager::existsAndIsCurrentThread()) {
        undoFromAudio_.store(true, std::memory_order_relaxed);
        return;
    }

    const auto now = juce::Time::getMillisecondCounter();
    const bool newGesture = !undoPending_ || now - lastChangeMs_ > kGestureGapMs;

    if (newGesture && settledState_ != nullptr) {
        undo_.push_back(std::make_unique<juce::XmlElement>(*settledState_));
        if (undo_.size() > kMaxUndoSteps) {
            undo_.erase(undo_.begin());
        }
    }

    lastChangeMs_ = now;
    undoPending_ = true;
}

void PluginEditor::settleUndo() {
    if (!undoPending_ || juce::Time::getMillisecondCounter() - lastChangeMs_ < kGestureGapMs) {
        return;
    }
    undoPending_ = false;
    settledState_ = owner_.stateXml();
}

void PluginEditor::applyUndoSnapshot(const juce::XmlElement& xml) {
    const auto& range = owner_.byteRange();
    const bool sameMaterial = xml.getStringAttribute("sourcePath") == owner_.sourcePath()
                              && xml.getStringAttribute("byteStart")
                                     == juce::String(range.start)
                              && xml.getStringAttribute("byteEnd") == juce::String(range.end);

    restoringUndo_ = true;
    if (sameMaterial) {
        owner_.parameters().replaceState(juce::ValueTree::fromXml(xml));
    } else {
        static_cast<void>(owner_.loadStateXml(xml));
    }
    restoringUndo_ = false;
}

void PluginEditor::undoLast() {
    if (undo_.empty()) {
        return;
    }

    auto step = std::move(undo_.back());
    undo_.pop_back();
    if (step == nullptr) {
        return;
    }

    applyUndoSnapshot(*step);

    settledState_ = std::move(step);
    undoPending_ = false;
    refresh();
}

void PluginEditor::loadPresetFromFile() {
    presetChooser_ = std::make_unique<juce::FileChooser>(
        "Load .opcoda preset", juce::File {}, "*.opcoda");
    presetChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser) {
            const auto file = chooser.getResult();
            if (file.existsAsFile() && owner_.loadPresetFromFile(file)) {
                currentPresetName_ = file.getFileNameWithoutExtension();
                currentFactoryIndex_ = -1;
                presetDirty_ = false;
                refreshPresetName();
            }
        });
}

void PluginEditor::savePresetToFile() {
    presetChooser_ = std::make_unique<juce::FileChooser>(
        "Save .opcoda preset", juce::File {}, "*.opcoda");
    presetChooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser) {
            auto file = chooser.getResult();
            if (file.getFullPathName().isEmpty()) {
                return;
            }
            if (file.getFileExtension().isEmpty()) {
                file = file.withFileExtension(".opcoda");
            }
            if (owner_.savePresetToFile(file)) {
                currentPresetName_ = file.getFileNameWithoutExtension();
                currentFactoryIndex_ = -1;
                presetDirty_ = false;
                refreshPresetName();
            }
        });
}

void PluginEditor::makePlainLabel(juce::Label& label,
                                  const juce::String& text,
                                  juce::Colour colour,
                                  juce::Font font) {
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, colour);
    label.setFont(font);
    label.setInterceptsMouseClicks(false, false);
}

void PluginEditor::buildHeader() {
    for (auto* component : {static_cast<juce::Component*>(&fileField_),
                            static_cast<juce::Component*>(&display_),
                            static_cast<juce::Component*>(&grid_),
                            static_cast<juce::Component*>(&status_)}) {
        canvas_.addAndMakeVisible(*component);
    }

    fileField_.onClick = [this] { chooseFile(); };

    grid_.onAddressActivated = [this](std::uint64_t address) { activateByte(address); };
    // clique na regua: a cabeca de leitura trava no byte e o transporte salta
    // para a mesma fracao da regiao.
    grid_.onAddressPinned = [this](std::uint64_t address) { activateByte(address); };
    grid_.onTransportSeek = [this](float fraction) {
        owner_.seekTransportTo(fraction);
        refresh();
    };
    grid_.onEmptyClick = [this] { chooseFile(); };
    canvas_.addAndMakeVisible(entropyLegend_);

    // arrasto 2D da mira: x = posicao (via activateByte), y = pitch (semitons).
    grid_.onCrosshairMoved = [this](float xFraction, float yFraction) {
        juce::ignoreUnused(xFraction);
        if (auto* parameter = owner_.parameters().getParameter("pitch")) {
            constexpr auto kMinSt = -48.0f;
            constexpr auto kMaxSt = 48.0f;
            const auto target = juce::jlimit(kMinSt, kMaxSt,
                                             kMinSt + yFraction * (kMaxSt - kMinSt));
            parameter->setValueNotifyingHost(parameter->convertTo0to1(target));
        }
        pushUndoSnapshot();
    };
    grid_.onSnapRequested = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };
    grid_.onRegionSelected = [this](std::uint64_t start, std::uint64_t end) {
        if (end <= start) {
            return;
        }
        pushUndoSnapshot();
        static_cast<void>(owner_.setByteRange(start, end));
    };
    grid_.onToggleTransport = [this] {
        owner_.toggleTransport();
        if (!owner_.isTransportPlaying()) {
            owner_.setTransportHold(false);
        }
        refreshPlayButton();
    };
    grid_.onSnapRequestedFromKey = [this] {
        static_cast<void>(owner_.snapByteRangeToSection());
    };

}

void PluginEditor::buildNavigation() {
    const auto setupIcon = [this](juce::TextButton& button, const char* icon,
                                  const char* look, const char* spokenName,
                                  const char* hint) {
        button.setButtonText("");
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName} + ". " + hint);
        button.getProperties().set("uiIcon", icon);
        button.getProperties().set("uiLook", look);
        canvas_.addAndMakeVisible(button);
    };

    struct ViewSpec {
        juce::TextButton* button;
        const char* icon;
        const char* label;
        const char* spokenName;
        const char* hint;
        ByteDisplay::ViewMode mode;
    };

    const ViewSpec views[] = {
        {&viewWaveButton_, "wave", "WAVE", "Waveform of the material", "Key 1",
         ByteDisplay::ViewMode::waveform},
        {&viewHexButton_, "hex", "HEX", "Byte grid of the material", "Key 2",
         ByteDisplay::ViewMode::hex},
        {&viewMatrixButton_, "matrix", "MATRIX", "Modulation matrix", "Key 3",
         ByteDisplay::ViewMode::matrix},
    };

    for (const auto& view : views) {
        setupIcon(*view.button, view.icon, "rail", view.spokenName, view.hint);
        view.button->setClickingTogglesState(true);
        const auto mode = view.mode;
        view.button->onClick = [this, mode] {
            grid_.setMode(mode);
            updateViewButtons();
            resized();
        };
    }

    // sidebar das vistas: largura fixa, rotulo sempre visivel, um abaixo do outro.
    viewWaveTab_ = std::make_unique<ViewTab>("wave", "WAVE");
    viewHexTab_ = std::make_unique<ViewTab>("hex", "HEX");
    viewMatrixTab_ = std::make_unique<ViewTab>("matrix", "MATRIX");
    viewWaveTab_->onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::waveform);
        updateViewButtons();
        resized();
    };
    viewHexTab_->onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::hex);
        updateViewButtons();
        resized();
    };
    viewMatrixTab_->onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::matrix);
        updateViewButtons();
        resized();
    };
    canvas_.addChildComponent(*viewWaveTab_);
    canvas_.addChildComponent(*viewHexTab_);
    canvas_.addChildComponent(*viewMatrixTab_);

    setupIcon(playButton_, "transport", "icon", "Play the region",
              "Space key");
    playButton_.setButtonText("Play");
    playButton_.setClickingTogglesState(true);
    playButton_.onClick = [this] {
        owner_.toggleTransport();
        if (!owner_.isTransportPlaying()) {
            owner_.setTransportHold(false);
        }
        refreshPlayButton();
    };

    setupIcon(holdButton_, "hold", "icon", "Freeze the transport head",
              "H key");
    holdButton_.setButtonText("Hold");
    holdButton_.setClickingTogglesState(true);
    holdButton_.onClick = [this] { refreshHoldButton(); };

    canvas_.addAndMakeVisible(pitchRandSlider_);
    canvas_.addAndMakeVisible(scanSpeedSlider_);

    setupIcon(pageUpButton_, "pageUp", "icon", "Previous hex page", "Page Up");
    setupIcon(pageDownButton_, "pageDown", "icon", "Next hex page",
              "Page Down");
    setupIcon(homeButton_, "home", "icon", "Start of file", "Home");
    setupIcon(endButton_, "end", "icon", "End of file", "End");
    pageUpButton_.onClick = [this] { grid_.scrollHexPage(-1); };
    pageDownButton_.onClick = [this] { grid_.scrollHexPage(1); };
    homeButton_.onClick = [this] { grid_.scrollHexToStart(); };
    endButton_.onClick = [this] { grid_.scrollHexToEnd(); };

    themeButton_.setButtonText("");
    themeButton_.setName("Themes and settings");
    themeButton_.setTooltip("Themes and audio/MIDI settings");
    themeButton_.getProperties().set("uiIcon", "settings");
    themeButton_.getProperties().set("uiLook", "icon");
    themeButton_.setVisible(false);
    themeButton_.onClick = [this] {
        if (onThemeRequested_ != nullptr) {
            onThemeRequested_();
        }
    };
    canvas_.addAndMakeVisible(themeButton_);
    canvas_.addAndMakeVisible(midiLed_);

    brandButton_.setButtonText("");
    brandButton_.setName("Bloodworm Trax");
    brandButton_.setTooltip("bloodworm trax | Opcoda");
    brandButton_.getProperties().set("uiLook", "brand");
    canvas_.addAndMakeVisible(brandButton_);

    keyboardToggle_.setButtonText("");
    keyboardToggle_.setName("Hide the MIDI keyboard");
    keyboardToggle_.setTooltip("Open or close the MIDI keyboard");
    keyboardToggle_.getProperties().set("uiIcon", "minus");
    keyboardToggle_.getProperties().set("uiLook", "icon");
    keyboardToggle_.onClick = [this] {
        keyboardOpen_ = !keyboardOpen_;
        keyboardToggle_.getProperties().set("uiIcon", keyboardOpen_ ? "minus" : "plus");
        keyboardToggle_.setName(keyboardOpen_ ? "Hide the MIDI keyboard"
                                              : "Show the MIDI keyboard");
        keyboard_.setVisible(keyboardOpen_);
        resized();
        applyWindowSize();
    };
    canvas_.addAndMakeVisible(keyboardToggle_);

    canvas_.addAndMakeVisible(meter_);
    updateViewButtons();
}

void PluginEditor::updateViewButtons() {
    const auto mode = grid_.mode();

    const auto mark = [&mode](juce::TextButton& button, ByteDisplay::ViewMode candidate) {
        const auto on = mode == candidate;
        if (button.getToggleState() != on) {
            button.setToggleState(on, juce::dontSendNotification);
        }
    };

    mark(viewWaveButton_, ByteDisplay::ViewMode::waveform);
    mark(viewHexButton_, ByteDisplay::ViewMode::hex);
    mark(viewMatrixButton_, ByteDisplay::ViewMode::matrix);

    viewWaveTab_->setToggled(mode == ByteDisplay::ViewMode::waveform);
    viewHexTab_->setToggled(mode == ByteDisplay::ViewMode::hex);
    viewMatrixTab_->setToggled(mode == ByteDisplay::ViewMode::matrix);

    const auto hex = mode == ByteDisplay::ViewMode::hex;
    for (auto* button : {&pageUpButton_, &pageDownButton_, &homeButton_, &endButton_}) {
        button->setVisible(hex);
        button->setEnabled(hex);
    }

}


void PluginEditor::updateStatusVisibility() {
    playButton_.setVisible(true);
}

void PluginEditor::refreshPlayButton() {
    const bool playing = owner_.isTransportPlaying();

    const auto spoken = playing ? juce::String {"Pause playback"}
                                 : juce::String {"Play the region"};
    if (playButton_.getName() != spoken) {
        playButton_.setName(spoken);
        playButton_.setTooltip(spoken);
    }

    const auto label = playing ? juce::String {"Pause"} : juce::String {"Play"};
    if (playButton_.getButtonText() != label) {
        playButton_.setButtonText(label);
    }

    if (playButton_.getToggleState() != playing) {
        playButton_.setToggleState(playing, juce::dontSendNotification);
    }

    const auto canPlay = owner_.hasSource() && owner_.hasPlayableRegion();
    playButton_.setEnabled(canPlay);
    holdButton_.setEnabled(canPlay && playing);

    // parar larga o latch: voltar a tocar nao pode nascer congelado.
    if (!playing && owner_.isTransportHeld()) {
        owner_.setTransportHold(false);
    }

    const auto held = owner_.isTransportHeld();
    if (holdButton_.getToggleState() != held) {
        holdButton_.setToggleState(held, juce::dontSendNotification);
    }
    holdButton_.setName(held ? "Release the transport head"
                             : "Freeze the transport head");
}

void PluginEditor::refreshHoldButton() {
    owner_.setTransportHold(holdButton_.getToggleState());
    refreshPlayButton();
}

void PluginEditor::pushTransportAnchor() {
    if (!positionChangedSinceLastAnchor_) {
        return;
    }
    positionChangedSinceLastAnchor_ = false;
    lastAnchoredPosition_ = readPosition();
    owner_.seekTransportTo(lastAnchoredPosition_);
}

void PluginEditor::buildParameterPanel() {
    const auto addKnob = [this](const Knob::Spec& spec) {
        knobs_.push_back(std::make_unique<Knob>(owner_.parameters(), spec));
        canvas_.addAndMakeVisible(*knobs_.back());
        return knobs_.back().get();
    };

    const auto addChoice = [this](const char* paramId, const char* title,
                                  const char* spokenName, ChoiceControl::Mode mode,
                                  int columns, std::vector<ChoiceControl::Item> items) {
        controls_.push_back(std::make_unique<ChoiceControl>(
            owner_.parameters(), paramId, title, spokenName, std::move(items), mode,
            columns));
        canvas_.addAndMakeVisible(*controls_.back());
        return controls_.back().get();
    };

    const auto rowOf = [](std::initializer_list<Cell> cells) {
        return std::vector<Cell> {cells};
    };

    moduleGrids_.resize(static_cast<std::size_t>(ParamModule::out));

    {
        ModuleGrid grid;
        grid.columns = 7;
        grid.maxCellWidth = 0;
        grid.rows.push_back(rowOf({
            {addKnob(kGrainRow1Specs[0]), nullptr, 1},
            {addKnob(kGrainRow1Specs[1]), nullptr, 1},
            {addKnob(kGrainRow1Specs[2]), nullptr, 1},
            {addKnob(kGrainRow1Specs[3]), nullptr, 1},
            {addKnob(kGrainRow1Specs[4]), nullptr, 1},
            {addKnob(kGrainRow1Specs[5]), nullptr, 1},
            {nullptr, [this, &addChoice] {
                 auto* choice = addChoice("window", "Window", "Grain window shape",
                                          ChoiceControl::Mode::icons, 1, kWindowItems);
                 return choice;
             }(), 1},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::grain)] = std::move(grid);
    }

    // -- ENV: sete knobs numa fila, ao lado da curva.
    {
        ModuleGrid grid;
        grid.columns = 7;
        grid.maxCellWidth = 92;
        grid.rows.push_back(rowOf({
            {addKnob(kEnvelopeSpecs[0]), nullptr, 1},
            {addKnob(kEnvelopeSpecs[1]), nullptr, 1},
            {addKnob(kEnvelopeSpecs[2]), nullptr, 1},
            {addKnob(kEnvelopeSpecs[3]), nullptr, 1},
            {addKnob(kEnvelopeSpecs[4]), nullptr, 1},
            {addKnob(kEnvelopeSpecs[5]), nullptr, 1},
            {addKnob(kEnvelopeSpecs[6]), nullptr, 1},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::envelope)] = std::move(grid);
    }

    {
        ModuleGrid grid;
        grid.columns = 5;
        grid.maxCellWidth = 118;
        grid.rows.push_back(rowOf({
            {nullptr, addChoice("f1type", "F1 Type", "Filter 1 type",
                                 ChoiceControl::Mode::icons, 4, kFilterTypeItems), 1},
            {addKnob(kFilterRow1Specs[0]), nullptr, 1},
            {addKnob(kFilterRow1Specs[1]), nullptr, 1},
            {addKnob(kFilterRow1Specs[2]), nullptr, 1},
            {addKnob(kFilterRow1Specs[3]), nullptr, 1},
        }));
        grid.rows.push_back(rowOf({
            {nullptr, addChoice("f2type", "F2 Type", "Filter 2 type",
                                 ChoiceControl::Mode::icons, 4, kFilterTypeItems), 1},
            {addKnob(kFilterRow2Specs[0]), nullptr, 1},
            {addKnob(kFilterRow2Specs[1]), nullptr, 1},
            {addKnob(kFilterRow1Specs[4]), nullptr, 1},
            {nullptr, addChoice("f1slope", "Slope", "Filter 1 slope",
                                 ChoiceControl::Mode::text, 2,
                                 {{"12 dB", "12 dB", nullptr},
                                  {"24 dB", "24 dB", nullptr}}), 1},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::filter)] = std::move(grid);
    }

    {
        ModuleGrid grid;
        grid.columns = 4;
        grid.maxCellWidth = 150;
        // o sync fica ao lado do knob de ritmo (e do DIV, quando o sync esta
        // ligado): e' a chave que manda no resto da linha.
        grid.rows.push_back(rowOf({
            {modRateKnob_ = addKnob(kModRow1Specs[0]), nullptr, 1},
            {nullptr,
             addChoice("lfosync", "Sync", "LFO follows the host tempo",
                       ChoiceControl::Mode::toggle, 1, kLfoSyncItems),
             1},
            {addKnob(kModRow1Specs[1]), nullptr, 1},
            {addKnob(kModRow1Specs[2]), nullptr, 1},
        }));
        grid.rows.push_back(rowOf({
            {nullptr, addChoice("lfotarget", "Target", "LFO target parameter",
                                 ChoiceControl::Mode::text, 4, kLfoTargetItems), 2},
            {nullptr, addChoice("lfowave", "Wave", "LFO waveform",
                                 ChoiceControl::Mode::icons, 5, kLfoWaveItems), 2},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::mod)] = std::move(grid);

        // o DIV so' existe quando o sync esta ligado; vive na celula do Rate.
        // E' um knob, nao um slider: usa o mesmo caminho dos outros knobs, que
        // ja' funcionam, e a lista de 12 divisoes aparece no rotulo.
        modDivKnob_ = addKnob(Knob::Spec {"lfodiv", "DIV", "LFO division when synced",
                                          2.0f, nullptr, kLfoDivLabels});
        modDivKnob_->setVisible(false);
    }

    masterKnob_ = addKnob(kOutSpecs[0]);
    masterKnob_->setOnDark(true);
    masterPanKnob_ = addKnob(kMasterSideSpecs[0]);
    masterPanKnob_->setOnDark(true);
    masterStereoKnob_ = addKnob(kMasterSideSpecs[1]);
    masterStereoKnob_->setOnDark(true);
    masterLevelKnob_ = addKnob(kMasterSideSpecs[2]);
    masterLevelKnob_->setOnDark(true);

    buildModuleTabs();
    showModule(activeModule_);
}

void PluginEditor::buildModuleTabs() {
    const auto setup = [this](ModuleFolderTab& button, const char* label,
                              const char* spokenName, ParamModule module) {
        button.setButtonText(label);
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName});
        button.setClickingTogglesState(true);
        button.onClick = [this, module] { showModule(module); };
        canvas_.addAndMakeVisible(button);
    };
    const auto setupModule = [&setup](ModuleFolderTab& button, const char* icon,
                                       const char* label, const char* spokenName,
                                       ParamModule module) {
        setup(button, label, spokenName, module);
        button.getProperties().set("uiIcon", icon);
    };
    setupModule(moduleGrainButton_, "grainMod", "GRAIN", "Grain module", ParamModule::grain);
    setupModule(moduleEnvelopeButton_, "envMod", "ENV", "Envelope module",
                ParamModule::envelope);
    setupModule(moduleFilterButton_, "filterMod", "FILTER", "Filter module",
                ParamModule::filter);
    setupModule(moduleModButton_, "modMod", "MOD", "Modulation module", ParamModule::mod);
}

void PluginEditor::updateModuleTabs() {
    const auto mark = [this](ModuleFolderTab& button, ParamModule candidate) {
        const auto on = activeModule_ == candidate;
        if (button.getToggleState() != on) {
            button.setToggleState(on, juce::dontSendNotification);
        }
    };
    mark(moduleGrainButton_, ParamModule::grain);
    mark(moduleEnvelopeButton_, ParamModule::envelope);
    mark(moduleFilterButton_, ParamModule::filter);
    mark(moduleModButton_, ParamModule::mod);
}

void PluginEditor::showModule(ParamModule module) {
    activeModule_ = module;
    const auto index = static_cast<std::size_t>(module);

    for (std::size_t m = 0; m < moduleGrids_.size(); ++m) {
        const bool on = m == index;
        for (const auto& row : moduleGrids_[m].rows) {
            for (const auto& cell : row) {
                if (cell.knob != nullptr) {
                    cell.knob->setVisible(on);
                }
                if (cell.control != nullptr) {
                    cell.control->setVisible(on);
                }
            }
        }
    }
    if (masterKnob_ != nullptr) {
        masterKnob_->setVisible(true);
    }
    for (Knob* knob : {masterPanKnob_, masterStereoKnob_, masterLevelKnob_}) {
        if (knob != nullptr) {
            knob->setVisible(true);
        }
    }

    updateModuleTabs();
    resized();
}

void PluginEditor::buildKeyboard() {
    keyboard_.setAvailableRange(24, 96);
    keyboard_.setLowestVisibleKey(24);
    keyboard_.setOctaveForMiddleC(4);
    keyboard_.setName("Virtual keyboard: play with the mouse or the computer keys "
                      "A W S E D F T G Y H U J K");
    keyboard_.setWantsKeyboardFocus(true);

    keyboard_.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, palette::ivory);
    keyboard_.setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                        palette::charcoalDeep);
    keyboard_.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                        palette::faceplateDark);
    keyboard_.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                        juce::Colours::transparentBlack);
    keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        juce::Colours::transparentBlack);
    keyboard_.setColour(juce::MidiKeyboardComponent::textLabelColourId,
                        palette::charcoalDeep);
    keyboard_.setColour(juce::MidiKeyboardComponent::shadowColourId,
                        juce::Colours::transparentBlack);

    canvas_.addAndMakeVisible(keyboard_);
    keyboardState_.addListener(this);
}

void PluginEditor::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber,
                                float velocity) {
    if (mirroringNotes_) {
        return;
    }
    setPendingNote(midiNoteNumber, true);
    owner_.pushKeyboardNote(midiNoteNumber, velocity, true);
}

void PluginEditor::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber,
                                 float velocity) {
    if (mirroringNotes_) {
        return;
    }
    setPendingNote(midiNoteNumber, false);
    owner_.pushKeyboardNote(midiNoteNumber, velocity, false);
}

void PluginEditor::setPendingNote(int note, bool on) {
    const auto bit = std::uint64_t {1} << (note % 64);
    auto& onBits = note < 64 ? pendingOnLow_ : pendingOnHigh_;
    auto& offBits = note < 64 ? pendingOffLow_ : pendingOffHigh_;
    if (on) {
        onBits |= bit;
        offBits &= ~bit;
    } else {
        offBits |= bit;
        onBits &= ~bit;
    }
}

void PluginEditor::mirrorIncomingNotes() {
    const auto pressed = owner_.pressedNotes();

    // confirma as notas locais: o audio e' a verdade, mas so' depois de as ver.
    for (int note = 0; note < 128; ++note) {
        const auto bit = std::uint64_t {1} << (note % 64);
        const auto bits = note < 64 ? pressed.low : pressed.high;
        const bool down = (bits & bit) != 0;
        const auto onBits = note < 64 ? pendingOnLow_ : pendingOnHigh_;
        const auto offBits = note < 64 ? pendingOffLow_ : pendingOffHigh_;
        if ((onBits & bit) != 0 && down) {
            (note < 64 ? pendingOnLow_ : pendingOnHigh_) &= ~bit;
        } else if ((offBits & bit) != 0 && !down) {
            (note < 64 ? pendingOffLow_ : pendingOffHigh_) &= ~bit;
        }
    }

    if (pressed.low == mirroredLow_ && pressed.high == mirroredHigh_) {
        return;
    }
    mirroredLow_ = pressed.low;
    mirroredHigh_ = pressed.high;

    mirroringNotes_ = true;
    for (int note = 0; note < 128; ++note) {
        const auto bit = std::uint64_t {1} << (note % 64);
        const auto onBits = note < 64 ? pendingOnLow_ : pendingOnHigh_;
        const auto offBits = note < 64 ? pendingOffLow_ : pendingOffHigh_;
        if ((onBits & bit) != 0 || (offBits & bit) != 0) {
            continue;   // a intencao local manda enquanto o audio nao confirma
        }

        const auto bits = note < 64 ? pressed.low : pressed.high;
        const bool down = (bits & bit) != 0;
        const bool shown = keyboardState_.isNoteOn(1, note);
        if (down && !shown) {
            keyboardState_.noteOn(1, note, 0.9f);
        } else if (!down && shown) {
            keyboardState_.noteOff(1, note, 0.0f);
        }
    }
    mirroringNotes_ = false;
}

int PluginEditor::designHeight() const {
    return keyboardOpen_ ? kDesignHeightOpen : kDesignHeightClosed;
}

void PluginEditor::applyWindowSize() {
    setSize(juce::roundToInt(static_cast<float>(kDesignWidth) * uiScale_),
            juce::roundToInt(static_cast<float>(designHeight()) * uiScale_));
}

void PluginEditor::paint(juce::Graphics& g) {
    // o canvas cobre a janela toda; isto e' so' o fundo por baixo do transform.
    g.fillAll(palette::chassisDark);
}

void PluginEditor::paintDesign(juce::Graphics& g) {
    const auto design = juce::Rectangle<int> {0, 0, kDesignWidth, designHeight()};
    const auto full = design.toFloat();

    // sombra 3D do chassi: projectada para baixo-direita, visivel porque o
    // plugin e' desenhado com uma margem escura a volta (paint() fillAll).
    {
        const juce::Graphics::ScopedSaveState save(g);
        for (int i = 6; i >= 1; --i) {
            const auto offset = static_cast<float>(i) * 1.6f;
            g.setColour(palette::alpha(juce::Colour {0xff000000}, 0.055f));
            g.fillRect(full.translated(offset, offset));
        }
    }

    juce::ColourGradient chassis {
        palette::chassis, full.getCentreX(), full.getY(), palette::chassisDark,
        full.getCentreX(), full.getBottom(), false};
    g.setGradientFill(chassis);
    g.fillRect(full);

    // volume: topo iluminado, base escurecida (efeito 3D por dentro).
    g.setColour(palette::alpha(palette::ivory, 0.07f));
    g.fillRect(juce::Rectangle<float> {full.getX() + 1.0f, full.getY() + 1.0f,
                                       full.getWidth() - 2.0f, 2.0f});
    g.setColour(palette::alpha(juce::Colour {0xff000000}, 0.22f));
    g.fillRect(juce::Rectangle<float> {full.getX() + 1.0f,
                                       full.getBottom() - 3.0f,
                                       full.getWidth() - 2.0f, 2.0f});

    // borda fina do chassi
    g.setColour(palette::chassisBorder);
    g.drawRect(full.reduced(0.5f), 1.0f);

    const auto header = design.withHeight(kHeaderHeight).toFloat();
    g.setGradientFill(juce::ColourGradient {palette::toolbarTop, header.getCentreX(),
                                            header.getY(), palette::toolbarBottom,
                                            header.getCentreX(), header.getBottom(), false});
    g.fillRect(header);
    ui::hairLine(g, 0.0f, static_cast<float>(kHeaderHeight) - 1.0f, header.getWidth(),
                 static_cast<float>(kHeaderHeight) - 1.0f, uiScale_, palette::charcoalDeep);

    const auto brandX = header.getX() + static_cast<float>(kMargin) +
                        static_cast<float>(chromeInset_);
    const auto brandHover = hoverOf(brandButton_);
    const auto brandApproach = 1.0f + 0.10f * brandHover;
    const auto brandCentreY = header.getCentreY();

    // simbolo: a marca sai na primeira metade, o bicho entra na segunda — nunca
    // os dois ao mesmo tempo, para nao se sobreporem a meio da animacao.
    const auto markAlpha = juce::jlimit(0.0f, 1.0f, 1.0f - 2.0f * brandHover);
    const auto wormAlpha = juce::jlimit(0.0f, 1.0f, 2.0f * brandHover - 1.0f);
    const auto markSide = 17.0f * brandApproach;
    const auto markBox = juce::Rectangle<float> {brandX + (17.0f - markSide) * 0.5f,
                                                 brandCentreY - markSide * 0.5f, markSide,
                                                 markSide};
    if (markAlpha > 0.001f) {
        const juce::Graphics::ScopedSaveState save(g);
        g.setOpacity(markAlpha);
        OpcodaLookAndFeel::paintMark(g, markBox);
    }
    if (wormAlpha > 0.001f) {
        const juce::Graphics::ScopedSaveState save(g);
        g.setOpacity(wormAlpha);
        OpcodaLookAndFeel::paintBloodworm(g, markBox.expanded(1.5f * brandHover));
    }

    // texto: "Opcoda" sai na primeira metade, o nome da editora entra na segunda.
    {
        const juce::Graphics::ScopedSaveState save(g);
        const auto anchorX = brandX + 23.0f;
        g.addTransform(juce::AffineTransform::translation(-anchorX, -brandCentreY)
                           .followedBy(juce::AffineTransform::scale(brandApproach))
                           .followedBy(juce::AffineTransform::translation(anchorX,
                                                                         brandCentreY)));
        if (markAlpha > 0.001f) {
            g.setOpacity(markAlpha);
            g.setFont(type::titleFont());
            g.setColour(palette::textDark);
            g.drawText("Opcoda",
                       juce::Rectangle<float> {anchorX, header.getY(), 90.0f,
                                               header.getHeight()},
                       juce::Justification::centredLeft, false);
        }
        if (wormAlpha > 0.001f) {
            g.setOpacity(wormAlpha);
            g.setFont(fonts().sans(10.5f, true));
            g.setColour(palette::textDark);
            g.drawText("bloodworm trax | Opcoda",
                       juce::Rectangle<float> {anchorX, header.getY(), 230.0f,
                                               header.getHeight()},
                       juce::Justification::centredLeft, false);
        }
    }

    // motion graphics: o filete corre por baixo do nome.
    if (brandHover > 0.01f) {
        g.setColour(palette::alpha(palette::ivory, 0.50f * brandHover));
        g.fillRect(juce::Rectangle<float> {brandX + 23.0f, brandCentreY + 11.0f,
                                           64.0f * brandHover, 1.0f});
    }

    // filete do header: separa o preset (do instrumento) do ficheiro (o material).
    if (headerDividerX_ > 0) {
        ui::hairLine(g, static_cast<float>(headerDividerX_), header.getY() + 10.0f,
                     static_cast<float>(headerDividerX_), header.getBottom() - 10.0f, uiScale_,
                     palette::alpha(palette::knobEdge, 0.55f));
    }

    const auto body = design.withTrimmedTop(kHeaderHeight).toFloat();
    // o corpo pinta-se ate' ao fundo do desenho: e' o que separa o teclado do
    // vazio. Antes parava no fim do painel e o teclado assentava num rectangulo
    // preto -- era esse o "fundo preto" da seccao MIDI.
    g.setColour(palette::faceplate);
    g.fillRect(body);

    // -- painel de parametros: chapa, raio 6, fio de 1 px --
    if (!parameterPanelBounds_.isEmpty()) {
        const auto panel = parameterPanelBounds_.toFloat();
        const auto radius = static_cast<float>(tokens::radiusPanel);
        g.setColour(palette::faceplateLight);
        g.fillRoundedRectangle(panel, radius);
        g.setColour(palette::alpha(palette::ivory, tokens::borderAlpha));
        g.drawRoundedRectangle(panel.reduced(0.5f), radius, 1.0f);
    }

    // -- grelha do painel: filetes de 1 px entre as celulas, como no preview --
    if (gridCellWidth_ > 0 && gridRowHeight_ > 0) {
        const auto area = gridArea_.toFloat();
        for (int c = 1; c < gridColumns_; ++c) {
            const auto lineIndex = static_cast<std::size_t>(c - 1);
            if (lineIndex < gridColumnLines_.size() && gridColumnLines_[lineIndex] == 0) {
                continue;
            }
            const auto x = area.getX() + static_cast<float>(c) * static_cast<float>(gridCellWidth_);
            ui::hairLine(g, x, area.getY() + static_cast<float>(tokens::space2), x,
                         area.getBottom() - static_cast<float>(tokens::space2), uiScale_,
                         palette::hair);
        }
        for (int r = 1; r < gridRows_; ++r) {
            const auto y = area.getY() + static_cast<float>(r) * static_cast<float>(gridRowHeight_);
            ui::hairLine(g, area.getX() + static_cast<float>(tokens::space2), y,
                         area.getRight() - static_cast<float>(tokens::space2), y, uiScale_,
                         palette::hair);
        }
    }

    // -- master: painel secundario, um degrau abaixo do principal --
    if (!masterBayBounds_.isEmpty()) {
        const auto master = masterBayBounds_.toFloat();
        const auto radius = static_cast<float>(tokens::radiusPanel);
        g.setColour(palette::subPanel);
        g.fillRoundedRectangle(master, radius);
        g.setColour(palette::alpha(palette::ivory, tokens::borderAlpha));
        g.drawRoundedRectangle(master.reduced(0.5f), radius, 1.0f);
    }

    // -- teclado: um card do mesmo material do chassi, alinhado ao conteudo --
    if (!keyboardBayBounds_.isEmpty()) {
        const auto bay = keyboardBayBounds_.toFloat();
        const auto radius = static_cast<float>(tokens::radiusPanel);
        g.setColour(palette::charcoal);
        g.fillRoundedRectangle(bay, radius);
        g.setColour(palette::alpha(palette::ivory, tokens::borderAlpha));
        g.drawRoundedRectangle(bay.reduced(0.5f), radius, 1.0f);

        if (!keyboardOpen_) {
            g.setFont(fonts().mono(9.0f, true));
            g.setColour(palette::textOnDarkSub);
            g.drawText("MIDI",
                       bay.withTrimmedLeft(static_cast<float>(kMargin))
                           .withWidth(80.0f),
                       juce::Justification::centredLeft, false);
        }
    }


    // -- alvo de arrasto --
    if (dragHovered_) {
        const auto target = screenBezelBounds_.toFloat().expanded(2.0f);
        g.setColour(palette::alpha(palette::accent, 0.18f));
        g.fillRoundedRectangle(target, 6.0f);
        g.setColour(palette::accent);
        g.drawRoundedRectangle(target, 6.0f, 2.0f);
        g.setFont(fonts().sans(14.0f, true));
        g.setColour(palette::ivory);
        g.drawText("Drop the binary", display_.getBounds(),
                   juce::Justification::centred, false);
    }
}

void PluginEditor::resized() {
    if (getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    // o canvas pensa em 1180x820 e o transform faz a janela que cabe no ecra.
    canvas_.setBounds(0, 0, kDesignWidth, designHeight());
    canvas_.setTransform(juce::AffineTransform::scale(uiScale_));

    auto area = canvas_.getLocalBounds();

    // -- header: marca, preset, origem --
    auto header = area.removeFromTop(kHeaderHeight);

    {
        constexpr int kArrow {22};
        constexpr int kName {168};
        constexpr int kField {470};
        constexpr int kFieldHeight {32};
        constexpr int kArrowGap {2};

        constexpr int kGear {22};
        constexpr int kGearGap {8};

        const int presetWidth = kArrow + kArrowGap + kName + kArrowGap + kArrow;
        int x = header.getRight() - kMargin - chromeRightInset_ - presetWidth;

        const auto cy = header.getCentreY();
        const auto rowY = [cy](int height) { return cy - height / 2; };

        presetPrevButton_.setBounds(x, rowY(22), kArrow, 22);
        x += kArrow + kArrowGap;
        presetNameButton_.setBounds(x, rowY(28), kName, 28);
        x += kName + kArrowGap;
        presetNextButton_.setBounds(x, rowY(22), kArrow, 22);

        headerDividerX_ = -1;
        const int fieldX = header.getCentreX() - kField / 2;
        // a engrenagem fica a esquerda do campo de ficheiro, colada a ele.
        themeButton_.setBounds(fieldX - kGearGap - kGear, rowY(kGear), kGear, kGear);
        fileField_.setBounds(fieldX, rowY(kFieldHeight), kField, kFieldHeight);
        brandButton_.setBounds(header.getX() + kMargin + chromeInset_, header.getY() + 10,
                               240, 28);
    }

    // -- corpo: margem 16 em cima, aos lados e em baixo. Tudo o que vem a
    // seguir sai desta caixa; nao ha' desvios de 2 px.
    auto body = area.reduced(kMargin, 0);
    body.removeFromTop(kMargin);
    body.removeFromBottom(kMargin);

    // -- ecra: largura toda do corpo --
    const auto displayRow = body.removeFromTop(kDisplayHeight);
    screenBezelBounds_ = displayRow;
    display_.setBounds(displayRow);
    display_.setRailWidth(kRailWidth);

    // as vistas sao a sidebar (largura fixa, rotulo sempre visivel); os botoes
    // originais ficam invisiveis.
    {
        for (auto* button : {&viewWaveButton_, &viewHexButton_, &viewMatrixButton_}) {
            button->setVisible(false);
        }

        // mesma caixa das abas de modulo: 72x36, coladas, a 16 do ecra.
        int y = displayRow.getY() + kMargin;
        const int x = displayRow.getX() + kMargin;
        for (auto* tab : {viewWaveTab_.get(), viewHexTab_.get(), viewMatrixTab_.get()}) {
            tab->setBounds(x, y, sidebar::kWidth, sidebar::kHeight);
            tab->setVisible(true);
            y += sidebar::kHeight;
        }
    }

    // -- conteudo do ecra: rail a' esquerda, 16 de padding nos outros lados --
    const auto content = displayRow.withTrimmedLeft(kContentX)
                             .withTrimmedRight(kMargin)
                             .withTrimmedTop(kMargin)
                             .withTrimmedBottom(kMargin);

    // a leitura alinha com a régua: mesma margem interna de 8 px que o display usa.
    status_.setBounds(content.getX() + tokens::space2, content.getY(),
                      content.getWidth() - tokens::space2, kStatusHeight);

    // -- rodape: legenda a esquerda, transporte ao centro, P.RAND e Scan a
    // direita dos icones, paginacao do hex no canto direito --
    const int footerY = content.getBottom() - kFooterHeight;
    const int transportY = footerY + (kFooterHeight - kTransportButtonHeight) / 2;

    entropyLegend_.setBounds(content.getX(), footerY + 6, kLegendWidth, 16);

    const int pairWidth = 2 * kTransportButtonWidth + kTransportGap;
    const int pairLeft = content.getCentreX() - pairWidth / 2;
    playButton_.setBounds(pairLeft, transportY, kTransportButtonWidth,
                          kTransportButtonHeight);
    holdButton_.setBounds(pairLeft + kTransportButtonWidth + kTransportGap, transportY,
                          kTransportButtonWidth, kTransportButtonHeight);

    {
        constexpr int kMiniWidth {168};
        constexpr int kMiniHeight {20};
        const int my = footerY + (kFooterHeight - kMiniHeight) / 2;
        const int slidersX = pairLeft + pairWidth + kGap;
        pitchRandSlider_.setBounds(slidersX, my, kMiniWidth, kMiniHeight);
        scanSpeedSlider_.setBounds(slidersX + kMiniWidth + kGap, my, kMiniWidth,
                                   kMiniHeight);
    }

    {
        constexpr int kPageButton {18};
        int px = content.getRight() - (4 * kPageButton + 3 * tokens::space1);
        for (auto* button : {&pageUpButton_, &pageDownButton_, &homeButton_,
                             &endButton_}) {
            button->setBounds(px, footerY + (kFooterHeight - kPageButton) / 2,
                              kPageButton, kPageButton);
            px += kPageButton + tokens::space1;
        }
    }

    const int gridTop = content.getY() + kStatusHeight + tokens::space2;
    grid_.setBounds(content.getX(), gridTop, content.getWidth(),
                    footerY - tokens::space2 - gridTop);

    // -- painel de parametros + master: uma linha so', 176 de altura --
    body.removeFromTop(kGap);
    auto panelRow = body.removeFromTop(kParameterHeight);

    masterBayBounds_ = panelRow.removeFromRight(kMasterWidth);
    panelRow.removeFromRight(kGap);
    parameterPanelBounds_ = panelRow;

    // sidebar dos modulos: mesma caixa das vistas, dentro da chapa do painel.
    // A altura sai da area util e o grupo centra-se nela, para a coluna nunca
    // encostar nas bordas do painel nem transbordar.
    auto moduleArea = parameterPanelBounds_.reduced(kPanelPaddingX, kPanelPaddingY);
    const auto moduleTabsBounds = moduleArea.removeFromLeft(sidebar::kWidth);
    {
        constexpr int kTabCount {4};
        const int tabHeight = sidebar::kHeight;
        const int totalHeight = kTabCount * tabHeight;
        int y = moduleTabsBounds.getY() +
                juce::jmax(0, (moduleTabsBounds.getHeight() - totalHeight) / 2);
        const int x = moduleTabsBounds.getX();
        for (auto* button : {&moduleGrainButton_, &moduleEnvelopeButton_,
                             &moduleFilterButton_, &moduleModButton_}) {
            button->setBounds(x, y, sidebar::kWidth, tabHeight);
            y += tabHeight;
        }
    }

    {
        // sem rotulo: a coluna e' so' os quatro knobs e o medidor, centrados
        // na altura e na largura da chapa.
        auto master = masterBayBounds_.reduced(kPanelPaddingX, kPanelPaddingY);
        auto meterCol = master.removeFromRight(tokens::space4);
        master.removeFromRight(tokens::space2);
        meter_.setBounds(meterCol);

        const int cellWidth = master.getWidth() / 2;
        const int cellHeight = master.getHeight() / 2;
        const auto cellAt = [&](int column, int row) {
            return juce::Rectangle<int> {master.getX() + column * cellWidth,
                                         master.getY() + row * cellHeight, cellWidth,
                                         cellHeight};
        };
        masterKnob_->setBounds(cellAt(0, 0));
        masterPanKnob_->setBounds(cellAt(1, 0));
        masterStereoKnob_->setBounds(cellAt(0, 1));
        masterLevelKnob_->setBounds(cellAt(1, 1));
    }

    // -- teclado: card na largura do corpo, com 8 de respiro para as teclas --
    {
        body.removeFromTop(kGap);
        const auto bay =
            body.removeFromTop(keyboardOpen_ ? kKeyboardHeight : kKeyToggleHeight);
        keyboardBayBounds_ = bay;

        constexpr int kToggle {20};
        constexpr int kKeyGutter {kMargin + kToggle + tokens::space2};
        keyboardToggle_.setBounds(bay.getRight() - kMargin - kToggle,
                                  bay.getCentreY() - kToggle / 2, kToggle, kToggle);
        keyboardToggle_.getProperties().set("uiIcon", keyboardOpen_ ? "minus" : "plus");

        // LED MIDI: acima do botao de colapsar, dentro do cartao. Fechado, a
        // tira nao tem altura para cima e a luz passa para o lado.
        if (keyboardOpen_) {
            midiLed_.setBounds(keyboardToggle_.getX() + (kToggle - 9) / 2,
                               keyboardToggle_.getY() - 9 - tokens::space1, 9, 9);
        } else {
            const auto toggle = keyboardToggle_.getBounds();
            midiLed_.setBounds(toggle.getX() - 9 - tokens::space1,
                               toggle.getCentreY() - 4, 9, 9);
        }

        if (keyboardOpen_) {
            auto keys = bay.reduced(tokens::space2);
            keys.removeFromRight(kKeyGutter - tokens::space2);
            keyboard_.setBounds(keys);

            constexpr float kWhiteKeys {43.0f};
            keyboard_.setKeyWidth(juce::jmax(
                10.0f, static_cast<float>(keys.getWidth()) / kWhiteKeys));
        }
    }

    const auto moduleIndex = static_cast<std::size_t>(activeModule_);

    envelopeView_.setVisible(activeModule_ == ParamModule::envelope);
    filterView_.setVisible(activeModule_ == ParamModule::filter);
    lfoView_.setVisible(activeModule_ == ParamModule::mod);

    const bool hasVisual =
        envelopeView_.isVisible() || filterView_.isVisible() || lfoView_.isVisible();

    auto knobArea = parameterPanelBounds_.reduced(kPanelPaddingX, kPanelPaddingY);
    knobArea.removeFromLeft(sidebar::kWidth + kPanelPaddingX);

    if (hasVisual) {
        if (activeModule_ == ParamModule::envelope) {
            const auto strip = knobArea.removeFromLeft(kAdsrStripWidth);
            knobArea.removeFromLeft(kGraphGap);
            envelopeView_.setBounds(strip);
        } else {
            const int graphWidth = activeModule_ == ParamModule::mod
                                       ? kModGraphWidth
                                       : juce::jlimit(300, 460,
                                                      static_cast<int>(knobArea.getWidth() * 0.42f));
            const auto graph = knobArea.removeFromLeft(graphWidth);
            knobArea.removeFromLeft(kGraphGap);
            if (activeModule_ == ParamModule::mod) {
                lfoView_.setBounds(graph);
            } else {
                filterView_.setBounds(graph);
            }
        }
    }

    if (moduleIndex < moduleGrids_.size()) {
        layoutModuleGrid(moduleGrids_[moduleIndex], knobArea);
    }

    updateModSyncLayout();

    updateStatusVisibility();
    refreshPlayButton();
    canvas_.repaint();
}

void PluginEditor::layoutModuleGrid(const ModuleGrid& grid, juce::Rectangle<int> area) {
    if (grid.rows.empty() || area.isEmpty()) {
        return;
    }

    auto gridArea = area;
    const int columns = juce::jmax(1, grid.columns);
    int cellWidth = gridArea.getWidth() / columns;
    if (grid.maxCellWidth > 0 && cellWidth > grid.maxCellWidth) {
        cellWidth = grid.maxCellWidth;
        const int total = cellWidth * columns;
        gridArea.setX(gridArea.getX() + (gridArea.getWidth() - total) / 2);
        gridArea.setWidth(total);
    }

    const int rowHeight = juce::jmin(
        gridArea.getHeight() / juce::jmax(1, static_cast<int>(grid.rows.size())),
        kKnobContentHeight);

    gridArea.setY(gridArea.getY()
                  + (gridArea.getHeight() - rowHeight * static_cast<int>(grid.rows.size()))
                        / 2);
    gridArea.setHeight(rowHeight * static_cast<int>(grid.rows.size()));

    gridArea_ = gridArea;
    gridCellWidth_ = cellWidth;
    gridRowHeight_ = rowHeight;
    gridColumns_ = columns;
    gridRows_ = static_cast<int>(grid.rows.size());

    // fronteiras de coluna que ficam dentro de uma celula fundida nao levam
    // filete: o botao ocupa as duas colunas e a linha atravessava-o.
    gridColumnLines_.assign(static_cast<std::size_t>(juce::jmax(0, columns - 1)), 1);
    for (const auto& row : grid.rows) {
        int column = 0;
        for (const auto& cell : row) {
            const int span = juce::jmax(1, cell.span);
            for (int b = column + 1; b < column + span; ++b) {
                if (b - 1 < static_cast<int>(gridColumnLines_.size())) {
                    gridColumnLines_[static_cast<std::size_t>(b - 1)] = 0;
                }
            }
            column += span;
        }
    }

    for (std::size_t r = 0; r < grid.rows.size(); ++r) {
        int x = gridArea.getX();
        const int y = gridArea.getY() + static_cast<int>(r) * rowHeight;
        for (const auto& cell : grid.rows[r]) {
            const int width = cellWidth * juce::jmax(1, cell.span);
            if (cell.knob != nullptr) {
                const int contentHeight = juce::jmin(kKnobContentHeight, rowHeight);
                cell.knob->setBounds(juce::Rectangle<int> {
                    x, y + (rowHeight - contentHeight) / 2, width, contentHeight}
                    .reduced(2, 0));
            } else if (cell.control != nullptr) {
                cell.control->setBounds(
                    juce::Rectangle<int> {x, y, width, rowHeight}.reduced(2, 0));
            }
            x += width;
        }
    }
}

void PluginEditor::updateModSyncLayout() {
    const auto* syncParam = owner_.parameters().getParameter("lfosync");
    const bool sync = syncParam != nullptr && syncParam->getValue() > 0.5f;
    modSyncOn_ = sync;

    const bool modActive = activeModule_ == ParamModule::mod;
    if (modRateKnob_ != nullptr) {
        modRateKnob_->setVisible(modActive && !sync);
    }
    if (modDivKnob_ == nullptr) {
        return;
    }

    const bool showDiv = modActive && sync;
    modDivKnob_->setVisible(showDiv);
    if (showDiv && gridCellWidth_ > 0 && gridRowHeight_ > 0) {
        modDivKnob_->setBounds(
            juce::Rectangle<int> {gridArea_.getX(), gridArea_.getY(), gridCellWidth_,
                                  gridRowHeight_}
                .reduced(2, 0));
    }
}

void PluginEditor::parentHierarchyChanged() {
    juce::AudioProcessorEditor::parentHierarchyChanged();

    if (isShowing() && !keyboard_.hasKeyboardFocus(true)) {
        keyboard_.grabKeyboardFocus();
    }
}

void PluginEditor::visibilityChanged() {
    juce::AudioProcessorEditor::visibilityChanged();

    if (isShowing() && !keyboard_.hasKeyboardFocus(true)) {
        keyboard_.grabKeyboardFocus();
    }
}

void PluginEditor::timerCallback() {
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto delta = lastTick_ > 0.0 ? static_cast<float>((now - lastTick_) / 1000.0)
                                        : 1.0f / 60.0f;
    lastTick_ = now;

    grid_.tickAnimation(delta);
    refresh();
    mirrorIncomingNotes();

    // LED de entrada MIDI: acende com o evento e decai.
    {
        const auto activity = owner_.midiActivity();
        if (activity != lastMidiActivity_) {
            lastMidiActivity_ = activity;
            midiLedLevel_ = 1.0f;
        } else if (midiLedLevel_ > 0.0f) {
            midiLedLevel_ = juce::jmax(0.0f, midiLedLevel_ - static_cast<float>(delta) * 3.2f);
        }
        midiLed_.setLit(midiLedLevel_);
    }

    {
        const auto* syncParam = owner_.parameters().getParameter("lfosync");
        const bool sync = syncParam != nullptr && syncParam->getValue() > 0.5f;
        if (sync != modSyncOn_) {
            resized();
        }
    }

    {
        const auto brandHover = hoverOf(brandButton_);
        if (std::abs(brandHover - lastBrandHover_) > 0.001f) {
            lastBrandHover_ = brandHover;
            canvas_.repaint();
        }
    }

    if (undoFromAudio_.exchange(false, std::memory_order_relaxed)) {
        pushUndoSnapshot();
    }
    settleUndo();
    meter_.tick(delta);
}

void PluginEditor::focusPlayKeyboard() {
    keyboard_.grabKeyboardFocus();
}

void PluginEditor::refreshTheme() {
    palette::applyToTree(*this);
    lookAndFeel_.refreshPalette();
    repaint();
}

bool PluginEditor::keyPressed(const juce::KeyPress& key) {
    if (!keyboard_.hasKeyboardFocus(true) && keyboard_.keyPressed(key)) {
        return true;
    }

    if (key.getKeyCode() == 'Z' && key.getModifiers().isCommandDown()) {
        undoLast();
        return true;
    }

    // o cursor do display responde as setas em qualquer vista, com ou sem foco.
    {
        const auto code = key.getKeyCode();
        const auto navigation =
            code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey ||
            code == juce::KeyPress::upKey || code == juce::KeyPress::downKey ||
            code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey ||
            code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey ||
            code == juce::KeyPress::returnKey;
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        const bool editingKnob =
            focused != nullptr && focused->findParentComponentOfClass<Knob>() != nullptr;
        if (navigation && !editingKnob) {
            grid_.handleNavigationKey(key);
            return true;
        }
    }

    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        owner_.toggleTransport();
        if (!owner_.isTransportPlaying()) {
            owner_.setTransportHold(false);
        }
        refreshPlayButton();
        return true;
    }

    if (isTypedCharacter(key, 'K')) {
        keyboard_.grabKeyboardFocus();
        return true;
    }

    if (isTypedCharacter(key, 'H')) {
        if (owner_.isTransportPlaying()) {
            owner_.setTransportHold(!owner_.isTransportHeld());
            refreshPlayButton();
        }
        return true;
    }

    if (isTypedCharacter(key, 'A')) {
        static_cast<void>(owner_.snapByteRangeToSection());
        return true;
    }

    if (isTypedCharacter(key, 'L')) {
        chooseFile();
        return true;
    }

    if (isDigitKey(key, '1')) {
        grid_.setMode(ByteDisplay::ViewMode::waveform);
        updateViewButtons();
        return true;
    }

    if (isDigitKey(key, '2')) {
        grid_.setMode(ByteDisplay::ViewMode::hex);
        updateViewButtons();
        return true;
    }

    if (isDigitKey(key, '3')) {
        grid_.setMode(ByteDisplay::ViewMode::matrix);
        updateViewButtons();
        return true;
    }

    if (isTypedCharacter(key, '+') || isTypedCharacter(key, '=')) {
        grid_.zoomIn();
        return true;
    }

    if (isTypedCharacter(key, '-')) {
        grid_.zoomOut();
        return true;
    }

    if (isTypedCharacter(key, '0')) {
        grid_.fitView();
        return true;
    }

    return false;
}

bool PluginEditor::keyStateChanged(bool isKeyDown) {
    if (!keyboard_.hasKeyboardFocus(true)) {
        return keyboard_.keyStateChanged(isKeyDown);
    }

    return juce::AudioProcessorEditor::keyStateChanged(isKeyDown);
}

void PluginEditor::mouseDown(const juce::MouseEvent& event) {
    const auto headerBottom =
        getScreenBounds().getY() + juce::roundToInt(static_cast<float>(kHeaderHeight) * uiScale_);
    if (windowDraggable_ && event.mods.isLeftButtonDown() &&
        event.getMouseDownScreenPosition().getY() < headerBottom) {
        if (auto* top = getTopLevelComponent()) {
            windowDragOffset_ = event.getScreenPosition() - top->getPosition();
            draggingWindow_ = true;
        }
    }

    if (keyboard_.hasKeyboardFocus(true)) {
        return;
    }
    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr) {
        return;
    }
    if (auto* hit = getComponentAt(event.getPosition());
        hit != nullptr && hit->findParentComponentOfClass<Knob>() != nullptr) {
        return;
    }
    keyboard_.grabKeyboardFocus();
}

void PluginEditor::mouseDrag(const juce::MouseEvent& event) {
    if (draggingWindow_) {
        if (auto* top = getTopLevelComponent()) {
            top->setTopLeftPosition(event.getScreenPosition() - windowDragOffset_);
        }
        return;
    }
    juce::AudioProcessorEditor::mouseDrag(event);
}

void PluginEditor::mouseUp(const juce::MouseEvent& event) {
    draggingWindow_ = false;
    juce::AudioProcessorEditor::mouseUp(event);
}

void PluginEditor::refresh() {
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent()) {
        if (auto* knob = focused->findParentComponentOfClass<Knob>()) {
            grid_.setSelectedModDest(
                destForParam(knob->getProperties()["paramId"].toString()));
        }
    }

    owner_.releaseReturnedBuffers();

    // os CCs sao aplicados aqui e nao na thread de audio.
    owner_.applyPendingControllerChanges();

    const auto& info = owner_.sourceInfo();

    grid_.setDisassemblyEnabled(info.formatTag.contains("64-bit"));
    fileField_.setFile(info.name.isNotEmpty() ? info.name : juce::String {},
                       info.sizeBytes > 0
                           ? juce::String {info.sizeBytes / (1024.0 * 1024.0), 2} + " MB"
                           : juce::String {},
                       info.formatTag);

    grid_.setVisible(true);

    // o estado cabe num booleano: o status so' aparece avaria.
    enum class State { ok, fault };
    State state = State::ok;
    juce::String message;

    if (owner_.lastError().isNotEmpty()) {
        state = State::fault;
        message = "RECUSADO / " + owner_.lastError();
    }

    status_.setVisible(state == State::fault);
    setIfChanged(status_, state == State::fault ? message : juce::String {});
    status_.setColour(juce::Label::textColourId, palette::errorLight);

    updateStatusVisibility();
    refreshPlayButton();

    // o analisador e' tempo real: so' repinta quando e' a vista visivel.
    if (activeModule_ == ParamModule::filter) {
        filterView_.repaint();
    }
    refreshPresetName();

    if (grid_.mode() != lastViewMode_) {
        lastViewMode_ = grid_.mode();
        updateViewButtons();
        resized();
    }

    refreshTelemetry(info);
}

void PluginEditor::refreshTelemetry(const PluginProcessor::SourceInfo& info) {
    const auto signature = info.name + juce::String {info.sizeBytes};
    if (loadedSignature_ != signature) {
        loadedSignature_ = signature;

        grid_.setSource(&owner_.sourceBytes(),
                        static_cast<std::uint64_t>(info.sizeBytes));
        grid_.setSections(info.sections);
    }

    const auto range = owner_.byteRange();
    grid_.setRegion(range.start, range.end);

    grid_.setLiveLevel(owner_.telemetry().envelopeLevel.load(std::memory_order_relaxed));

    const auto& telemetry = owner_.telemetry();
    const auto playing = telemetry.playing.load(std::memory_order_relaxed);
    const auto playhead = telemetry.playheadFraction.load(std::memory_order_relaxed);

    const auto position = readPosition();
    const auto headPosition = playing ? playhead : position;
    const auto readHead = pe::byteForPosition(static_cast<double>(headPosition), range);
    grid_.setReadHead(readHead);

    if (std::abs(position - lastAnchoredPosition_) > 1.0e-4f) {
        positionChangedSinceLastAnchor_ = true;
    }
    pushTransportAnchor();

    // a telemetria vem do que a thread de audio ja' publicou.
    grid_.setPlayhead(playhead, playing);

    meter_.setLevels(telemetry.peakLeftDb.load(std::memory_order_relaxed),
                     telemetry.peakRightDb.load(std::memory_order_relaxed));


    ByteDisplay::ParamPreview preview;
    preview.position = position;
    preview.spray = juce::jlimit(0.0f, 1.0f, rawParameter("spray", 0.0f));
    preview.density = juce::jlimit(0.0f, 100.0f,
                                   (rawParameter("density", 20.0f) - 1.0f) / 1.99f);
    preview.grainMs = juce::jmax(1.0f, rawParameter("grain", 40.0f));
    preview.pitchSpread = juce::jmax(0.0f, rawParameter("pitchrand", 0.0f));
    preview.pan = juce::jlimit(-1.0f, 1.0f, rawParameter("pan", 0.0f));
    preview.window = juce::roundToInt(rawParameter("window", 0.0f));
    preview.pitchSemitones = rawParameter("pitch", 0.0f);
    preview.level = juce::jlimit(0.0f, 1.0f, rawParameter("grainlevel", 1.0f));
    preview.sampleRate = telemetry.sampleRate.load(std::memory_order_relaxed);
    grid_.setParamPreview(preview);

    // a mira segue o pitch: o eixo Y e' o mesmo parametro.
    {
        constexpr auto kMinSt = -48.0f;
        constexpr auto kMaxSt = 48.0f;
        const auto norm = juce::jlimit(0.0f, 1.0f,
                                       (preview.pitchSemitones - kMinSt) / (kMaxSt - kMinSt));
        grid_.setCrosshairY(norm);
    }

    const auto rateText =
        juce::String {telemetry.sampleRate.load(std::memory_order_relaxed) / 1000.0f, 1}
        + " kHz";
    if (status_.getTooltip() != rateText) {
        status_.setTooltip(rateText);
    }

    std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices> grains {};
    const auto grainCount = owner_.grainTelemetry().read(grains);
    grid_.setGrains(grains, grainCount);

    if (envelopeView_.isVisible()) {
        envelopeView_.setLive(telemetry.envelopeLevel.load(std::memory_order_relaxed),
                              telemetry.envelopeStage.load(std::memory_order_relaxed),
                              telemetry.envelopePhase.load(std::memory_order_relaxed));
    }
    if (filterView_.isVisible()) {
        filterView_.setLive(telemetry.cutoff1Hz.load(std::memory_order_relaxed),
                            telemetry.cutoff2Hz.load(std::memory_order_relaxed));
    }
    if (lfoView_.isVisible()) {
        lfoView_.setLive(telemetry.lfoPhase.load(std::memory_order_relaxed),
                         telemetry.lfoValue.load(std::memory_order_relaxed));
    }
}

float PluginEditor::readPosition() const {
    if (const auto* parameter = owner_.parameters().getParameter("position")) {
        return parameter->getNormalisableRange().convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
}

float PluginEditor::rawParameter(const char* id, float fallback) const {
    if (const auto* parameter = owner_.parameters().getParameter(id)) {
        return parameter->convertFrom0to1(parameter->getValue());
    }
    return fallback;
}

void PluginEditor::animateButtons() {
    for (auto* button : {static_cast<juce::Button*>(&presetPrevButton_),
                         static_cast<juce::Button*>(&presetNameButton_),
                         static_cast<juce::Button*>(&presetNextButton_),
                         static_cast<juce::Button*>(&viewWaveButton_),
                         static_cast<juce::Button*>(&viewHexButton_),
                         static_cast<juce::Button*>(&viewMatrixButton_),
                         static_cast<juce::Button*>(&playButton_),
                         static_cast<juce::Button*>(&holdButton_),
                         static_cast<juce::Button*>(&pageUpButton_),
                         static_cast<juce::Button*>(&pageDownButton_),
                         static_cast<juce::Button*>(&homeButton_),
                         static_cast<juce::Button*>(&endButton_),
                         static_cast<juce::Button*>(&themeButton_),
                         static_cast<juce::Button*>(&keyboardToggle_),
                         static_cast<juce::Button*>(&brandButton_)}) {
        attachButtonHover(*button, buttonHovers_);
    }
}

void PluginEditor::activateByte(std::uint64_t address) {
    const auto range = owner_.byteRange();
    if (address < range.start || address >= range.end) {
        moveRegionTo(address);
    }

    const auto updated = owner_.byteRange();

    if (auto* parameter = owner_.parameters().getParameter("position")) {
        parameter->setValueNotifyingHost(
            static_cast<float>(pe::positionForByte(address, updated)));
    }
}

void PluginEditor::moveRegionTo(std::uint64_t address) {
    const auto range = owner_.byteRange();
    const auto length = juce::jmax(range.length(), std::uint64_t {64});
    pushUndoSnapshot();
    static_cast<void>(owner_.setByteRange(address, address + length));
}

void PluginEditor::chooseFile() {
    chooser_ = std::make_unique<juce::FileChooser>(
        "Choose a binary", juce::File::getCurrentWorkingDirectory(),
        "*.exe;*.dll;*.bin;*.sys");

    chooser_->launchAsync(
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& finished) {
            if (finished.getResults().size() > 0) {
                pushUndoSnapshot();
                owner_.ingest(finished.getResults().getReference(0).getFullPathName());
                refresh();
            }
        });
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}

} // namespace opcoda
