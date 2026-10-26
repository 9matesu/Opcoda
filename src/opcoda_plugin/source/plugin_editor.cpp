#include "plugin_editor.h"

#include "factory_presets.h"

#include "opcoda_core/pe/byte_to_position.h"

namespace opcoda {
namespace {

// janela fixa. os numeros abaixo sao o layout inteiro: nao ha' ramo compacto
// porque nao ha' tamanho pequeno. a F023 pedia um minimo para o redimensionamento;
// com a janela amarrada, o minimo deixa de ter caso.
constexpr int kWindowWidth {1180};
constexpr int kWindowHeight {760};
constexpr int kHeaderHeight {64};

constexpr int kBodyMargin {16};
constexpr int kBodyTopGap {8};
constexpr int kScreenBezel {6};
constexpr int kDisplayHeight {380};

// tira vertical a' direita do ecra. teclas rectangulares com glifo e nome: 88x34
// de area util, acima do hit target do WCAG 2.5.8 (24) com folga para a mao.
constexpr int kRailWidth {96};
constexpr int kRailButtonWidth {88};
constexpr int kRailButtonHeight {34};
constexpr int kRailGap {5};

// transporte dentro do pe' do ecra. o play e o hold passaram para aqui quando a
// tira horizontal saiu: o ecra e' o centro, e o transporte vive com ele.
constexpr int kTransportButtonWidth {74};
constexpr int kTransportButtonHeight {20};
constexpr int kTransportGap {5};

constexpr int kTabHeight {28};
constexpr int kTabGap {6};
// abas de pasta: icone so', sem palavra. a cauda desce sobre o painel.
constexpr int kFolderTabWidth {44};
constexpr int kFolderTabTail {6};

constexpr int kKeyboardHeight {84};
constexpr int kMasterWidth {116};
constexpr int kMasterGap {12};
constexpr int kMaxCellWidth {156};
constexpr int kKnobContentHeight {148};
constexpr int kFooterHeight {24};
constexpr int kStatusHeight {16};

// parafuso de chapa: tres circulos e uma fenda. quatro por janela chegam para
// dizer "isto e' um aparelho" sem virar decoracao.
void paintScrew(juce::Graphics& g, juce::Point<float> centre) {
    const auto r = 3.0f;
    g.setColour(palette::alpha(palette::charcoalDeep, 0.55f));
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    g.setColour(palette::alpha(palette::faceplateDark, 0.95f));
    g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.0f);
    g.setColour(palette::alpha(palette::ivory, 0.35f));
    g.drawLine(centre.x - 1.6f, centre.y, centre.x + 1.6f, centre.y, 0.8f);
}

juce::String asInteger(float value) {
    return juce::String {juce::roundToInt(value)};
}

const auto percent = [](float v) { return asInteger(v * 100.0f) + " %"; };
const auto seconds = [](float v) { return asInteger(v * 1000.0f) + " ms"; };
const auto cutoffText = [](float v) {
    return v >= 1000.0f ? juce::String {v / 1000.0f, 1} + " kHz" : asInteger(v) + " Hz";
};

// os parametros vivem em sete modulos visiveis. os IDs dos originais nao mudam --
// sessoes salvas referenciam por ID -- so' o titulo e o nome falado passam a ingles.
const std::array<Knob::Spec, 11> kGrainSpecs {
    Knob::Spec {"grain", "SIZE", "Grain size, in milliseconds", 40.0f,
                [](float v) { return asInteger(v) + " ms"; }},
    Knob::Spec {"density", "DENSITY", "Density, in grains per second", 20.0f,
                [](float v) { return asInteger(v) + " /s"; }},
    Knob::Spec {"position", "POSITION", "Read position in the material", 0.5f, percent},
    Knob::Spec {"spray", "SPRAY", "Spray, start position spread", 0.0f, percent},
    Knob::Spec {"pitch", "PITCH", "Tuning, in semitones", 0.0f,
                [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {"window", "WINDOW", "Grain window shape", 0.0f, {},
                {"Hann", "Gaussian", "Hamming", "Blackman"}},
    Knob::Spec {"pan", "PAN", "Stereo pan, left to right", 0.0f,
                [](float v) {
                    return v == 0.0f ? juce::String {"C"}
                         : v < 0.0f  ? asInteger(-v * 100.0f) + " L"
                                     : asInteger(v * 100.0f) + " R";
                }},
    // largura stereo na saida: mid/side, 0 e' mono, 100 % o material, 200 % o
    // side dobrado. destino da matriz tambem (Stereo Width).
    Knob::Spec {"width", "STEREO", "Stereo width, mono to wide", 1.0f,
                [](float v) { return asInteger(v * 100.0f) + " %"; }},
    Knob::Spec {"grainlevel", "LEVEL", "Per-grain level", 1.0f, percent},
    Knob::Spec {"pitchrand", "P.RAND", "Random detune per grain, in semitones", 0.0f,
                [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {"scanspeed", "SCAN", "Playhead scan speed, times realtime", 1.0f,
                [](float v) { return juce::String {v, 2} + "x"; }},
};

// F020: as tres curvas ficam ao lado dos tempos que moldam. O valor diz a forma:
// -100 % exponencial, 0 linear, +100 % logaritmica.
const auto curveText = [](float v) {
    const int pct = static_cast<int>(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
    return (pct == 0) ? juce::String {"LIN"} : juce::String {pct} + " %";
};

const std::array<Knob::Spec, 7> kEnvelopeSpecs {
    Knob::Spec {"attack", "ATK", "Attack time", 0.005f, seconds},
    Knob::Spec {"decay", "DEC", "Decay time", 0.1f, seconds},
    Knob::Spec {"sustain", "SUS", "Sustain level", 1.0f, percent},
    Knob::Spec {"release", "REL", "Release time", 0.05f, seconds},
    Knob::Spec {"atkcurve", "ATK CRV", "Attack curve, exponential to logarithmic", 0.0f,
                curveText},
    Knob::Spec {"deccurve", "DEC CRV", "Decay curve, exponential to logarithmic", 0.0f,
                curveText},
    Knob::Spec {"relcurve", "REL CRV", "Release curve, exponential to logarithmic", 0.0f,
                curveText},
};

const std::array<Knob::Spec, 10> kFilterSpecs {
    Knob::Spec {"f1type", "F1 TYPE", "Filter 1 type", 0.0f, {},
                {"Low-pass", "High-pass", "Band-pass", "Notch"}},
    Knob::Spec {"f1cutoff", "F1 CUT", "Filter 1 cutoff frequency", 20000.0f, cutoffText},
    Knob::Spec {"f1q", "F1 Q", "Filter 1 resonance", 0.7071f,
                [](float v) { return juce::String {v, 2}; }},
    Knob::Spec {"f1slope", "F1 SLOPE", "Filter 1 slope", 0.0f, {}, {"12 dB", "24 dB"}},
    Knob::Spec {"f1drive", "F1 DRIVE", "Filter 1 drive, in decibels", 0.0f,
                [](float v) { return juce::String {v, 1} + " dB"; }},
    Knob::Spec {"f1keytrack", "F1 KEY", "Filter 1 key tracking", 0.0f, percent},
    Knob::Spec {"f1env", "F1 ENV", "Filter 1 envelope amount", 0.0f,
                [](float v) {
                    const int pct = static_cast<int>(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
                    return juce::String {pct} + " %";
                }},
    Knob::Spec {"f2type", "F2 TYPE", "Filter 2 type", 0.0f, {},
                {"Low-pass", "High-pass", "Band-pass", "Notch"}},
    Knob::Spec {"f2cutoff", "F2 CUT", "Filter 2 cutoff frequency", 20000.0f, cutoffText},
    Knob::Spec {"f2q", "F2 Q", "Filter 2 resonance", 0.7071f,
                [](float v) { return juce::String {v, 2}; }},
};

const std::array<Knob::Spec, 7> kModSpecs {
    Knob::Spec {"lforate", "RATE", "LFO rate, in hertz", 1.0f,
                [](float v) { return juce::String {v, 2} + " Hz"; }},
    Knob::Spec {"lfodepth", "DEPTH", "LFO depth", 0.0f, percent},
    Knob::Spec {"lfotarget", "TARGET", "LFO target parameter", 0.0f, {},
                {"Pitch", "Density", "Cutoff", "Position"}},
    Knob::Spec {"lfowave", "WAVE", "LFO waveform", 0.0f, {},
                {"Sine", "Tri", "Saw", "Square", "S&H"}},
    Knob::Spec {"lfosync", "SYNC", "LFO follows the host tempo", 0.0f, {},
                {"Free", "Sync"}},
    Knob::Spec {"lfodiv", "DIV", "LFO division when synced", 0.0f, {},
                {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/4.", "1/8.",
                 "1/4T", "1/8T", "1/16T"}},
    Knob::Spec {"lfosteps", "STEPS", "LFO steps per cycle", 16.0f,
                [](float v) { return juce::String {static_cast<int>(v + 0.5f)}; }},
};

const std::array<Knob::Spec, 3> kCloudSpecs {
    Knob::Spec {"cloudsize", "CLOUD W", "Temporal width of the grain cloud", 0.0f,
                [](float v) { return asInteger(v * 500.0f) + " ms"; }},
    Knob::Spec {"pitchspread", "SPREAD", "Pitch spread across the cloud, in semitones", 0.0f,
                [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {"grainjit", "JITTER", "Grain duration jitter", 0.0f, percent},
};

// o volume nao tem aba: e' o master, sempre visivel no canto inferior direito.
const std::array<Knob::Spec, 1> kOutSpecs {
    Knob::Spec {"volume", "VOLUME", "Output volume, in decibels", 0.0f,
                [](float v) {
                    return v <= -60.0f ? juce::String {"-inf"}
                                       : juce::String {v, 1} + " dB";
                }},
};

const std::array<Knob::Spec, 3> kKeysSpecs {
    Knob::Spec {"scale", "SCALE", "Scale applied to incoming notes", 0.0f, {},
                {"Chromatic", "Major", "Natural Minor", "Harmonic Minor", "Dorian",
                 "Phrygian", "Lydian", "Mixolydian", "Pentatonic Major",
                 "Pentatonic Minor", "Blues", "Whole Tone"}},
    Knob::Spec {"root", "ROOT", "Scale root note", 0.0f, {},
                {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}},
    Knob::Spec {"glide", "GLIDE", "Glide time between notes", 0.0f, seconds},
};

} // namespace

// trocar o texto de um Label dispara um evento de acessibilidade. A 60 Hz isso e'
// ruido no leitor de tela, por isso so' se escreve quando muda.
void setIfChanged(juce::Label& label, const juce::String& text) {
    if (label.getText() != text) {
        label.setText(text, juce::dontSendNotification);
        label.setName(text);
    }
}

PluginEditor::PluginEditor(PluginProcessor& processor)
    : juce::AudioProcessorEditor(&processor),
      owner_(processor),
      keyboard_(keyboardState_, juce::MidiKeyboardComponent::horizontalKeyboard) {
    setLookAndFeel(&lookAndFeel_);
    setOpaque(true);

    makePlainLabel(title_, "Opcoda", palette::textDark, fonts().sans(24.0f, true));
    makePlainLabel(status_, "", palette::textOnDark, fonts().sans(11.0f));

    buildHeader();
    buildNavigation();
    buildPresetSelector();
    buildParameterPanel();
    buildKeyboard();

    // a matriz e' parametro: as 24 celulas ganham attachment aqui, quando a
    // arvore do APVTS ja' existe e os knobs do painel ja' estao criados.
    grid_.attachModParameters(owner_.parameters());

    owner_.parameters().state.addListener(this);

    // 60 Hz porque a animacao tem meia-vida de 60 ms: a 20 Hz cada meia-vida tem
    // menos de dois quadros e o degrau ve-se. O texto nao vira ruido porque
    // setIfChanged so' escreve quando muda.
    startTimerHz(60);

    // **fixa.** o host nao ganha uma asa de redimensionamento e o layout nao tem
    // um segundo caminho. e' o que permite os numeros do resized() serem o desenho.
    setResizable(false, false);
    setSize(kWindowWidth, kWindowHeight);

    // **o clique nao rouba o foco.** com o foco a saltar para cada botao, as teclas
    // do computador deixavam de soar depois de trocar de vista. o teclado fica com
    // o foco e as teclas de navegacao sao encaminhadas pelo editor.
    const std::function<void(juce::Component&)> releaseFocus = [&](juce::Component& c) {
        if (&c != &keyboard_) {
            c.setMouseClickGrabsKeyboardFocus(false);
        }
        for (int i = 0; i < c.getNumChildComponents(); ++i) {
            releaseFocus(*c.getChildComponent(i));
        }
    };
    releaseFocus(*this);

    // o teclado tem o foco desde o primeiro quadro: sem isto so' soava depois de um
    // clique no proprio teclado.
    keyboard_.grabKeyboardFocus();
    addMouseListener(this, true);

    refresh();
}

PluginEditor::~PluginEditor() {
    removeMouseListener(this);
    // note-off primeiro, com o listener ainda ligado: fechar a janela com tecla
    // premida deixaria a nota presa. allNotesOff corre na thread de interface e o
    // listener empurra para a fila.
    keyboardState_.allNotesOff(1);
    keyboardState_.removeListener(this);
    owner_.parameters().state.removeListener(this);
}

void PluginEditor::buildPresetSelector() {
    // setas sem cromo e nome como aba, a mesma lingua das abas de modulo. O nome e'
    // o botao do menu: um "MENU" separado seria o quarto controle para uma funcao.
    // icones de 22 px, sem moldura: o preset e' contextual, nao um painel.
    const auto setupArrow = [this](juce::TextButton& button, const char* glyph,
                                   const char* spokenName, int direction) {
        button.setButtonText("");
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName});
        button.getProperties().set("uiIcon", glyph);
        button.getProperties().set("uiLook", "icon");
        button.onClick = [this, direction] { stepFactoryPreset(direction); };
        addAndMakeVisible(button);
    };
    setupArrow(presetPrevButton_, "chevronLeft", "Previous factory preset", -1);
    setupArrow(presetNextButton_, "chevronRight", "Next factory preset", +1);

    presetNameButton_.setButtonText(currentPresetName_);
    presetNameButton_.setName("Preset menu");
    presetNameButton_.setTooltip("Factory presets, load and save .opcoda");
    presetNameButton_.getProperties().set("uiIcon", "tab");
    presetNameButton_.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetNameButton_);
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
    // a guarda evita que os parameterChanged da aplicacao sujem o preset recem
    // aplicado: sem ela todo preset nascia com "*".
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
    // so' PARAM suja: sourcePath e a regiao mudam no ingest e nao dizem nada sobre o
    // som em relacao ao preset.
    if (changed.hasType("PARAM") && !applyingPreset_) {
        presetDirty_ = true;
    }
}

void PluginEditor::loadPresetFromFile() {
    presetChooser_ = std::make_unique<juce::FileChooser>(
        "Load .opcoda preset", juce::File {}, "*.opcoda");
    presetChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser) {
            const auto file = chooser.getResult();
            // nome primeiro, som depois: se o ficheiro falhar, o nome antigo
            // continua verdadeiro.
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
    for (auto* component : {static_cast<juce::Component*>(&title_),
                            static_cast<juce::Component*>(&loadButton_),
                            static_cast<juce::Component*>(&fileName_),
                            static_cast<juce::Component*>(&formatTag_),
                            static_cast<juce::Component*>(&fileSize_),
                            static_cast<juce::Component*>(&display_),
                            static_cast<juce::Component*>(&grid_),
                            static_cast<juce::Component*>(&status_),
                            static_cast<juce::Component*>(&positionReadout_),
                            static_cast<juce::Component*>(&offsetReadout_)}) {
        addAndMakeVisible(*component);
    }

    // a grelha escreve o POSITION. Duplo clique e tecla A alternam entre a regiao
    // exacta e a seccao PE mais proxima; quem decide o sentido e' o metodo do
    // processor, que guarda a regiao anterior.
    grid_.onAddressActivated = [this](std::uint64_t address) { activateByte(address); };
    grid_.onSnapRequested = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };
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

    for (auto* readout : {&positionReadout_, &offsetReadout_}) {
        makePlainLabel(*readout, "", palette::textOnDark, fonts().sans(10.5f));
        readout->setJustificationType(juce::Justification::centredLeft);
    }
    positionReadout_.setJustificationType(juce::Justification::centredLeft);
    offsetReadout_.setJustificationType(juce::Justification::centredLeft);

    makePlainLabel(fileName_, "Sem binario", palette::textDark,
                   fonts().sans(12.0f, true));
    makePlainLabel(formatTag_, "", palette::textSub, fonts().sans(9.0f, true));
    makePlainLabel(fileSize_, "", palette::textSub, fonts().sans(9.0f, true));
    fileName_.setJustificationType(juce::Justification::centredLeft);
    formatTag_.setJustificationType(juce::Justification::centredRight);
    fileSize_.setJustificationType(juce::Justification::centredRight);

    loadButton_.getProperties().set("uiIcon", "folder");
    loadButton_.getProperties().set("uiLook", "icon");
    loadButton_.setButtonText("");
    loadButton_.setTooltip("Escolher um binario para sintetizar (tecla L)");
    loadButton_.setName("Carregar binario");
    loadButton_.onClick = [this] { chooseFile(); };
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
        addAndMakeVisible(button);
    };

    struct ViewSpec {
        juce::TextButton* button;
        const char* icon;
        const char* label;
        const char* spokenName;
        const char* hint;
        ByteDisplay::ViewMode mode;
    };

    // quatro vistas, quatro perguntas sobre o mesmo material. a rail e' empilhada:
    // a largura do ecra fica para o ecra.
    const ViewSpec views[] = {
        {&viewWaveButton_, "wave", "WAVE", "Forma de onda do material", "Tecla 1",
         ByteDisplay::ViewMode::waveform},
        {&viewHexButton_, "hex", "HEX", "Grelha de bytes do material", "Tecla 2",
         ByteDisplay::ViewMode::hex},
        {&viewEntropyButton_, "entropy", "ENTROPY", "Curva de entropia do material",
         "Tecla 3", ByteDisplay::ViewMode::entropy},
        {&viewMatrixButton_, "matrix", "MATRIX", "Matriz de modulacao", "Tecla 4",
         ByteDisplay::ViewMode::matrix},
    };

    for (const auto& view : views) {
        setupIcon(*view.button, view.icon, "bar", view.spokenName, view.hint);
        view.button->setButtonText(view.label);
        view.button->setClickingTogglesState(true);
        const auto mode = view.mode;
        view.button->onClick = [this, mode] {
            grid_.setMode(mode);
            updateViewButtons();
            resized();
        };
    }

    setupIcon(zoomInButton_, "zoomIn", "bar", "Aproximar a forma de onda", "Tecla +");
    setupIcon(zoomOutButton_, "zoomOut", "bar", "Afastar a forma de onda", "Tecla -");
    setupIcon(zoomFitButton_, "fit", "bar", "Enquadrar a regiao inteira", "Tecla 0");
    zoomInButton_.setButtonText("IN");
    zoomOutButton_.setButtonText("OUT");
    zoomFitButton_.setButtonText("FIT");
    zoomInButton_.onClick = [this] { grid_.zoomIn(); };
    zoomOutButton_.onClick = [this] { grid_.zoomOut(); };
    zoomFitButton_.onClick = [this] { grid_.fitView(); };

    // transporte: play/pause e hold no mesmo par, separados do resto. O hold e' o
    // latch que congela a cabeca sem fechar o gate. Vivem no pe' do ecra, onde o
    // olho ja' esta', em vez de uma tira propria a roubar altura aos knobs.
    setupIcon(playButton_, "transport", "bar", "Reproduzir a regiao",
              "Tecla de espaco");
    playButton_.setButtonText("PLAY");
    playButton_.setClickingTogglesState(true);
    playButton_.onClick = [this] {
        owner_.toggleTransport();
        if (!owner_.isTransportPlaying()) {
            owner_.setTransportHold(false);
        }
        refreshPlayButton();
    };

    setupIcon(holdButton_, "hold", "bar", "Congelar a cabeca do transporte",
              "Tecla H");
    holdButton_.setButtonText("HOLD");
    holdButton_.setClickingTogglesState(true);
    holdButton_.onClick = [this] { refreshHoldButton(); };

    // paginas do hex: quatro teclas com a mesma largura, ao centro da tira. So'
    // fazem sentido na vista hex e ficam desactivadas nas outras, em vez de
    // desaparecerem e mudarem o desenho da tira.
    setupIcon(pageUpButton_, "pageUp", "icon", "Pagina anterior do hex", "Page Up");
    setupIcon(pageDownButton_, "pageDown", "icon", "Pagina seguinte do hex",
              "Page Down");
    setupIcon(homeButton_, "home", "icon", "Inicio do ficheiro", "Home");
    setupIcon(endButton_, "end", "icon", "Fim do ficheiro", "End");
    pageUpButton_.onClick = [this] { grid_.scrollHexPage(-1); };
    pageDownButton_.onClick = [this] { grid_.scrollHexPage(1); };
    homeButton_.onClick = [this] { grid_.scrollHexToStart(); };
    endButton_.onClick = [this] { grid_.scrollHexToEnd(); };

    addAndMakeVisible(meter_);
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
    mark(viewEntropyButton_, ByteDisplay::ViewMode::entropy);
    mark(viewMatrixButton_, ByteDisplay::ViewMode::matrix);

    // a navegacao de paginas e' do hex. fora do hex nao ha' o que paginar: esconder
    // e' mais honesto do que deixar quatro teclas apagadas a fingir de controlo.
    const auto hex = mode == ByteDisplay::ViewMode::hex;
    for (auto* button : {&pageUpButton_, &pageDownButton_, &homeButton_, &endButton_}) {
        button->setVisible(hex);
        button->setEnabled(hex);
    }

    // o zoom e' da onda e da entropia. na matriz nao ha' escala: esconder o trio
    // deixa a rail com as quatro vistas e o que sobra e' espaco da vista.
    const auto zoomable = mode != ByteDisplay::ViewMode::matrix;
    for (auto* button : {&zoomInButton_, &zoomFitButton_, &zoomOutButton_}) {
        button->setVisible(zoomable);
    }
}

void PluginEditor::updateHeaderVisibility() {
    const auto& info = owner_.sourceInfo();

    fileSize_.setVisible(info.sizeBytes > 0);
    formatTag_.setVisible(info.formatTag.isNotEmpty());
}

void PluginEditor::updateFooterVisibility() {
    positionReadout_.setVisible(positionReadout_.getText().isNotEmpty());
    offsetReadout_.setVisible(offsetReadout_.getText().isNotEmpty());
}

void PluginEditor::updateStatusVisibility() {
    // o botao de reproducao nunca sai: e' a unica forma de ouvir sem teclado MIDI.
    // o que muda e' estar activo, e isso e' refreshPlayButton que decide.
    playButton_.setVisible(true);
}

void PluginEditor::refreshPlayButton() {
    const bool playing = owner_.isTransportPlaying();

    if ((playButton_.getName() == "Parar a reproducao") != playing) {
        playButton_.setName(playing ? "Parar a reproducao" : "Reproduzir a regiao");
    }

    // o rotulo diz o que o clique faz: PLAY quando parado, PAUSE quando a correr.
    // o glifo segue o mesmo estado.
    const auto label = playing ? juce::String {"PAUSE"} : juce::String {"PLAY"};
    if (playButton_.getButtonText() != label) {
        playButton_.setButtonText(label);
    }

    if (playButton_.getToggleState() != playing) {
        playButton_.setToggleState(playing, juce::dontSendNotification);
    }

    // desactivado e nao invisivel, e sem fingir que esta' ativo. sem material nao ha'
    // regiao, e com um ou dois bytes nao ha' posicao que soe.
    //
    // **o playhead do host nao entra na conta.** no standalone ele alterna entre
    // "parado" e "desconhecido" ao ritmo do callback e o botao piscava entre activo
    // e apagado. o transporte que este botao comanda e' o do instrumento.
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
    holdButton_.setName(held ? "Largar a cabeca do transporte"
                             : "Congelar a cabeca do transporte");
}

void PluginEditor::refreshHoldButton() {
    owner_.setTransportHold(holdButton_.getToggleState());
    refreshPlayButton();
}

void PluginEditor::pushTransportAnchor() {
    // a ancora so' e' escrita quando o utilizador mexeu em POSITION. Escrever a cada
    // quadro subiria a geracao do nucleo a cada quadro e a cabeca nunca passaria do
    // ponto de ancoragem.
    if (!positionChangedSinceLastAnchor_) {
        return;
    }
    positionChangedSinceLastAnchor_ = false;
    lastAnchoredPosition_ = readPosition();
    owner_.seekTransportTo(lastAnchoredPosition_);
}

void PluginEditor::buildParameterPanel() {
    // ordem de construcao = ordem do enum. As fatias ficam guardadas para a aba
    // mostrar sem procurar por ID. OUT nao tem aba: o volume e' o master.
    const auto addSpecs = [this](const auto& specs) {
        moduleFirstKnob_.push_back(knobs_.size());
        for (const auto& spec : specs) {
            knobs_.push_back(std::make_unique<Knob>(owner_.parameters(), spec));
            addAndMakeVisible(*knobs_.back());
        }
        moduleKnobCount_.push_back(knobs_.size() - moduleFirstKnob_.back());
    };
    addSpecs(kGrainSpecs);
    addSpecs(kEnvelopeSpecs);
    addSpecs(kFilterSpecs);
    addSpecs(kModSpecs);
    addSpecs(kCloudSpecs);
    addSpecs(kOutSpecs);
    addSpecs(kKeysSpecs);

    masterKnobIndex_ = moduleFirstKnob_[static_cast<std::size_t>(ParamModule::out)];
    knobs_[masterKnobIndex_]->setOnDark(true);

    buildModuleTabs();
    showModule(activeModule_);
}

void PluginEditor::buildModuleTabs() {
    // abas de pasta com glifo: o nome falado continua, mas o rotulo visual e' o
    // icone. a largura encolhe porque nao ha' palavra nenhuma a defender.
    const auto setup = [this](ModuleFolderTab& button, const char* title,
                              const char* spokenName, ParamModule module) {
        button.setButtonText(title);
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName});
        button.setClickingTogglesState(true);
        button.onClick = [this, module] { showModule(module); };
        addAndMakeVisible(button);
    };
    setup(moduleGrainButton_, "GRAIN", "Grain module", ParamModule::grain);
    setup(moduleEnvelopeButton_, "ENV", "Envelope module", ParamModule::envelope);
    setup(moduleFilterButton_, "FILTER", "Filter module", ParamModule::filter);
    setup(moduleModButton_, "MOD", "Modulation module", ParamModule::mod);
    setup(moduleCloudButton_, "CLOUD", "Grain cloud module", ParamModule::cloud);
    setup(moduleKeysButton_, "KEYS", "Keyboard and tuning module", ParamModule::keys);
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
    mark(moduleCloudButton_, ParamModule::cloud);
    mark(moduleKeysButton_, ParamModule::keys);
}

void PluginEditor::showModule(ParamModule module) {
    activeModule_ = module;
    const auto index = static_cast<std::size_t>(module);
    const auto first = moduleFirstKnob_[index];
    const auto count = moduleKnobCount_[index];

    // o knob do master nunca se esconde: nao pertence a aba nenhuma e vive no
    // canto inferior direito em todas.
    for (std::size_t i = 0; i < knobs_.size(); ++i) {
        const auto inModule = i >= first && i < first + count;
        knobs_[i]->setVisible(inModule || i == masterKnobIndex_);
    }
    updateModuleTabs();
    resized();
}

void PluginEditor::buildKeyboard() {
    // sete oitavas, de C1 a C7: a leitura vai ate' onde o pitch (-48..+48)
    // alcanca a partir do centro. o nome da oitava segue a convencao do
    // teclado fisico, com o do' central em C4.
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
                        palette::accent.withAlpha(0.22f));
    keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        palette::accent.withAlpha(0.72f));
    keyboard_.setColour(juce::MidiKeyboardComponent::textLabelColourId,
                        palette::charcoalDeep);
    keyboard_.setColour(juce::MidiKeyboardComponent::shadowColourId,
                        juce::Colours::black.withAlpha(0.35f));

    addAndMakeVisible(keyboard_);
    keyboardState_.addListener(this);
}

void PluginEditor::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber,
                                float velocity) {
    owner_.pushKeyboardNote(midiNoteNumber, velocity, true);
}

void PluginEditor::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber,
                                 float velocity) {
    owner_.pushKeyboardNote(midiNoteNumber, velocity, false);
}

void PluginEditor::paint(juce::Graphics& g) {
    const auto full = getLocalBounds().toFloat();

    // chassis com profundidade: escuro em baixo, um pouco mais claro em cima. sem
    // isto a janela e' um rectangulo preto.
    juce::ColourGradient chassis {
        palette::chassis, full.getCentreX(), full.getY(), palette::chassisDark,
        full.getCentreX(), full.getBottom(), false};
    g.setGradientFill(chassis);
    g.fillRect(full);

    g.setColour(palette::chassisBorder);
    g.drawRect(full, 1.0f);
    g.setColour(palette::alpha(palette::charcoalDeep, 0.85f));
    g.drawRect(full.reduced(1.0f), 1.0f);

    // -- chapa do header --
    const auto header = getLocalBounds().withHeight(kHeaderHeight).toFloat();
    g.setColour(palette::faceplate);
    g.fillRect(header);
    g.setColour(palette::alpha(palette::ivory, 0.30f));
    g.drawHorizontalLine(0, 0.0f, header.getWidth());
    g.setColour(palette::faceplateDark);
    g.drawHorizontalLine(kHeaderHeight - 1, 0.0f, header.getWidth());
    g.setColour(palette::alpha(palette::brass, 0.60f));
    g.drawHorizontalLine(kHeaderHeight, 0.0f, static_cast<float>(getWidth()));

    // -- corpo --
    const auto body = getLocalBounds().withTrimmedTop(kHeaderHeight).toFloat();
    g.setColour(palette::faceplate);
    g.fillRect(body);
    g.setColour(palette::alpha(palette::faceplateDark, 0.45f));
    g.drawVerticalLine(kBodyMargin - 4, body.getY() + 8.0f, body.getBottom() - 8.0f);
    g.drawVerticalLine(getWidth() - kBodyMargin + 3, body.getY() + 8.0f,
                       body.getBottom() - 8.0f);

    // -- moldura do ecra --
    if (!screenBezelBounds_.isEmpty()) {
        const auto bezel = screenBezelBounds_.toFloat();
        g.setColour(palette::charcoalDeep);
        g.fillRoundedRectangle(bezel, 4.0f);
        g.setColour(palette::alpha(palette::brass, 0.45f));
        g.drawRoundedRectangle(bezel, 4.0f, 1.0f);
        g.setColour(palette::alpha(palette::ivory, 0.05f));
        g.drawRoundedRectangle(bezel.reduced(1.0f), 3.0f, 1.0f);
    }

    // -- tira de vistas: a unica superficie escura a' direita do ecra --
    if (!railBounds_.isEmpty()) {
        const auto rail = railBounds_.toFloat();
        g.setColour(palette::rail);
        g.fillRoundedRectangle(rail, 4.0f);
        g.setColour(palette::alpha(palette::knobEdge, 0.65f));
        g.drawRoundedRectangle(rail, 4.0f, 1.0f);
        g.setColour(palette::alpha(palette::ivory, 0.05f));
        g.drawRoundedRectangle(rail.reduced(1.0f), 3.0f, 1.0f);
    }

    // -- painel de parametros --
    if (!parameterPanelBounds_.isEmpty()) {
        const auto panel = parameterPanelBounds_.toFloat();
        g.setColour(palette::faceplateLight);
        g.fillRoundedRectangle(panel, 3.0f);
        g.setColour(palette::alpha(palette::faceplateDark, 0.90f));
        g.drawRoundedRectangle(panel, 3.0f, 1.0f);
        g.setColour(palette::alpha(palette::ivory, 0.28f));
        g.drawHorizontalLine(juce::roundToInt(panel.getY() + 1.0f),
                             panel.getX() + 6.0f, panel.getRight() - 6.0f);
    }

    // -- master: coluna escura, do painel ao teclado --
    if (!masterBayBounds_.isEmpty()) {
        const auto master = masterBayBounds_.toFloat();
        g.setColour(palette::charcoalDeep);
        g.fillRoundedRectangle(master, 4.0f);
        g.setColour(palette::alpha(palette::brass, 0.50f));
        g.drawRoundedRectangle(master, 4.0f, 1.0f);
        g.setColour(palette::alpha(palette::ivory, 0.05f));
        g.drawHorizontalLine(juce::roundToInt(master.getY() + 1.0f),
                             master.getX() + 8.0f, master.getRight() - 8.0f);
    }

    // -- teclado --
    if (!keyboardBayBounds_.isEmpty()) {
        const auto bay = keyboardBayBounds_.toFloat();
        g.setColour(palette::charcoalDeep);
        g.fillRect(bay);
        g.setColour(palette::alpha(palette::brass, 0.35f));
        g.drawHorizontalLine(juce::roundToInt(bay.getY()), bay.getX(), bay.getRight());
        g.setColour(palette::alpha(palette::ivory, 0.04f));
        g.drawHorizontalLine(juce::roundToInt(bay.getY()) + 1, bay.getX(),
                             bay.getRight());
    }

    // separadores do header. a chapa e' uma so', mas os grupos leem-se.
    const auto separator = [&g](int x) {
        g.setColour(palette::alpha(palette::faceplateDark, 0.50f));
        g.drawVerticalLine(x, 16.0f, static_cast<float>(kHeaderHeight) - 16.0f);
    };
    if (!title_.getBounds().isEmpty()) {
        separator(title_.getBounds().getRight() + 12);
    }
    if (!presetNextButton_.getBounds().isEmpty()) {
        separator(presetNextButton_.getBounds().getRight() + 12);
    }

    // parafusos nos cantos da janela.
    const auto w = static_cast<float>(getWidth());
    const auto h = static_cast<float>(getHeight());
    paintScrew(g, {8.0f, 8.0f});
    paintScrew(g, {w - 8.0f, 8.0f});
    paintScrew(g, {8.0f, h - 8.0f});
    paintScrew(g, {w - 8.0f, h - 8.0f});

    // -- alvo de arrasto --
    if (dragHovered_) {
        const auto target = screenBezelBounds_.toFloat().expanded(2.0f);
        g.setColour(palette::alpha(palette::accent, 0.18f));
        g.fillRoundedRectangle(target, 4.0f);
        g.setColour(palette::accent);
        g.drawRoundedRectangle(target, 4.0f, 2.0f);
        g.setFont(fonts().sans(14.0f, true));
        g.setColour(palette::ivory);
        g.drawText("SOLTE O BINARIO", display_.getBounds(),
                   juce::Justification::centred, false);
    }
}

void PluginEditor::resized() {
    // o construtor chama showModule() antes de setSize(); sem esta guarda o primeiro
    // layout corre sobre um rectangulo 0x0 e os knobs nascem com largura negativa.
    if (getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    auto area = getLocalBounds();

    // -- header: marca, preset, origem --
    auto header = area.removeFromTop(kHeaderHeight).reduced(kBodyMargin + 2, 0);
    int x = header.getX();

    title_.setBounds(x, header.getY() + 14, 182, 36);
    x += 182 + 16;

    presetPrevButton_.setBounds(x, header.getY() + 19, 22, 26);
    x += 22 + 3;
    presetNameButton_.setBounds(x, header.getY() + 17, 160, 30);
    x += 160 + 3;
    presetNextButton_.setBounds(x, header.getY() + 19, 22, 26);
    x += 22 + 18;

    loadButton_.setBounds(x, header.getY() + 19, 30, 26);
    const int nameX = x + 30 + 8;

    int stripEdge = header.getRight();
    fileSize_.setBounds(stripEdge - 62, header.getY() + 19, 62, 26);
    stripEdge -= 62 + 6;
    formatTag_.setBounds(stripEdge - 76, header.getY() + 19, 76, 26);
    stripEdge -= 76 + 8;
    fileName_.setBounds(nameX, header.getY() + 16,
                        juce::jmax(90, stripEdge - nameX), 32);

    // -- corpo --
    auto body = area.reduced(kBodyMargin, kBodyTopGap);

    // ecra e tira de vistas
    auto displayRow = body.removeFromTop(kDisplayHeight);
    railBounds_ = displayRow.removeFromRight(kRailWidth);
    displayRow.removeFromRight(8);
    screenBezelBounds_ = displayRow;
    display_.setBounds(displayRow.reduced(kScreenBezel));

    {
        auto rail = railBounds_.reduced(4, 10);
        const int bx = rail.getCentreX() - kRailButtonWidth / 2;

        int vy = rail.getY();
        for (auto* button : {&viewWaveButton_, &viewHexButton_, &viewEntropyButton_,
                             &viewMatrixButton_}) {
            button->setBounds(bx, vy, kRailButtonWidth, kRailButtonHeight);
            vy += kRailButtonHeight + kRailGap;
        }

        int zy = rail.getBottom() - (kRailButtonHeight * 3 + kRailGap * 2);
        for (auto* button : {&zoomInButton_, &zoomFitButton_, &zoomOutButton_}) {
            button->setBounds(bx, zy, kRailButtonWidth, kRailButtonHeight);
            zy += kRailButtonHeight + kRailGap;
        }
    }

    // leituras e area de desenho, por dentro do ecra
    const auto displayBounds = display_.getBounds();
    status_.setBounds(displayBounds.getX() + 18, displayBounds.getY() + 4,
                      displayBounds.getWidth() - 36, kStatusHeight);

    // pe' do ecra: o transporte no canto esquerdo, as leituras ao lado e as
    // paginas do hex a' direita. a tira horizontal deixou de existir: o que se
    // procura com o dedo vive junto ao que se esta' a ver.
    const int footerY = displayBounds.getBottom() - kFooterHeight - 4;
    const int transportY = footerY + (kFooterHeight - kTransportButtonHeight) / 2;

    int fx = displayBounds.getX() + 14;
    playButton_.setBounds(fx, transportY, kTransportButtonWidth,
                          kTransportButtonHeight);
    fx += kTransportButtonWidth + kTransportGap;
    holdButton_.setBounds(fx, transportY, kTransportButtonWidth,
                          kTransportButtonHeight);
    fx += kTransportButtonWidth + 14;

    positionReadout_.setBounds(fx, footerY, 168, kFooterHeight);
    fx += 168;
    offsetReadout_.setBounds(fx, footerY, 300, kFooterHeight);

    {
        constexpr int kPageButton {20};
        constexpr int kPageGap {3};
        int px = displayBounds.getRight() - 14 - (4 * kPageButton + 3 * kPageGap);
        for (auto* button : {&pageUpButton_, &pageDownButton_, &homeButton_,
                             &endButton_}) {
            button->setBounds(px, footerY + (kFooterHeight - kPageButton) / 2,
                              kPageButton, kPageButton);
            px += kPageButton + kPageGap;
        }
    }

    const int gridTop = displayBounds.getY() + kStatusHeight + 8;
    grid_.setBounds(juce::Rectangle<int> {
        displayBounds.getX() + 8, gridTop, displayBounds.getWidth() - 16,
        footerY - gridTop});

    body.removeFromTop(8);

    // -- abas de pasta coladas ao painel: sem fileira solta. a cauda de 6 px
    // desce sobre o painel e a activa funde-se com ele.
    moduleRowBounds_ = body.removeFromTop(kTabHeight);
    {
        int tx = moduleRowBounds_.getX() + 2;
        for (auto* button : {&moduleGrainButton_, &moduleEnvelopeButton_,
                             &moduleFilterButton_, &moduleModButton_,
                             &moduleCloudButton_, &moduleKeysButton_}) {
            button->setBounds(tx, moduleRowBounds_.getY(), kFolderTabWidth,
                              kTabHeight + kFolderTabTail);
            tx += kFolderTabWidth + kTabGap;
        }
    }

    body.removeFromTop(kFolderTabTail);

    // -- parte de baixo: knobs, teclado, master --
    auto lower = body;
    masterBayBounds_ = lower.removeFromRight(kMasterWidth);
    lower.removeFromRight(kMasterGap);
    keyboardBayBounds_ = lower.removeFromBottom(kKeyboardHeight);
    parameterPanelBounds_ = lower;

    {
        auto master = masterBayBounds_.reduced(5, 6);
        // o volume cede 18 px ao medidor: a escala de dB precisava de altura para
        // os rotulos nao se encostarem.
        auto volumeCell = master.removeFromTop(kKnobContentHeight - 18);
        knobs_[masterKnobIndex_]->setBounds(volumeCell.reduced(2, 0));
        master.removeFromTop(4);
        meter_.setBounds(master);
    }

    keyboard_.setBounds(keyboardBayBounds_.reduced(12, 6));

    // a largura da tecla sai da baia, nao de um numero fixo: 43 teclas brancas
    // entre C1 e C7 cabem todas no vidro, sem seta de scroll a cortar a ponta.
    {
        constexpr float kWhiteKeys {43.0f};
        const auto bayWidth = static_cast<float>(keyboard_.getWidth());
        keyboard_.setKeyWidth(juce::jmax(10.0f, bayWidth / kWhiteKeys));
    }

    const auto moduleIndex = static_cast<std::size_t>(activeModule_);
    const auto first = moduleFirstKnob_[moduleIndex];
    const auto count = moduleKnobCount_[moduleIndex];

    for (std::size_t i = 0; i < knobs_.size(); ++i) {
        const auto inModule = i >= first && i < first + count;
        knobs_[i]->setVisible(inModule || i == masterKnobIndex_);
    }

    if (count > 0) {
        const auto knobArea = parameterPanelBounds_.reduced(10, 6);
        const int cellWidth = juce::jmin(
            kMaxCellWidth, knobArea.getWidth() / static_cast<int>(count));
        const int totalWidth = cellWidth * static_cast<int>(count);
        const int startX = knobArea.getX() + (knobArea.getWidth() - totalWidth) / 2;

        const int contentHeight = juce::jmin(kKnobContentHeight, knobArea.getHeight());
        const int contentY = knobArea.getY() + (knobArea.getHeight() - contentHeight) / 2;

        for (std::size_t k = 0; k < count; ++k) {
            knobs_[first + k]->setBounds(
                juce::Rectangle<int> {startX + static_cast<int>(k) * cellWidth, contentY,
                                      cellWidth, contentHeight}
                    .reduced(2, 0));
        }
    }

    updateStatusVisibility();
    refreshPlayButton();
}

void PluginEditor::parentHierarchyChanged() {
    juce::AudioProcessorEditor::parentHierarchyChanged();

    // o foco pedido no construtor chega antes de haver peer. aqui a janela ja' tem
    // pai: e' o primeiro momento em que o pedido vale.
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
    // delta medido, nao assumido: assumir 1/60 faz a velocidade depender da taxa
    // real, que no Windows vai de 60 a 144 Hz conforme o monitor.
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto delta = lastTick_ > 0.0 ? static_cast<float>((now - lastTick_) / 1000.0)
                                        : 1.0f / 60.0f;
    lastTick_ = now;

    grid_.tickAnimation(delta);
    refresh();
    meter_.tick(delta);
}

bool PluginEditor::keyPressed(const juce::KeyPress& key) {
    // **notas primeiro.** se o foco estiver noutro controle (um menu aberto, o
    // display), a tecla de nota ainda tem de soar: o teclado virtual le o mapa
    // A W S E D F T G Y H U J K e nao depende do foco para isso.
    if (!keyboard_.hasKeyboardFocus(true) && keyboard_.keyPressed(key)) {
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
        if (navigation) {
            grid_.handleNavigationKey(key);
            return true;
        }
    }

    // o display ja' consumiu o que lhe competia. Aqui so' chegam teclas de
    // transporte, e so' quando o foco nao estava no display.
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        owner_.toggleTransport();
        if (!owner_.isTransportPlaying()) {
            owner_.setTransportHold(false);
        }
        refreshPlayButton();
        return true;
    }

    // letras e digitos nao tem keycode no JUCE -- ver isTypedCharacter em
    // byte_display.h.
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
        grid_.setMode(ByteDisplay::ViewMode::entropy);
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
    // rede de seguranca: o teclado virtual so' recebe o estado das teclas quando
    // tem foco. se um menu ou o display o levou, a tecla que sobe tem de continuar
    // a largar a nota -- senao fica presa.
    if (!keyboard_.hasKeyboardFocus(true)) {
        return keyboard_.keyStateChanged(isKeyDown);
    }

    return juce::AudioProcessorEditor::keyStateChanged(isKeyDown);
}

void PluginEditor::mouseDown(const juce::MouseEvent&) {
    // **tocar e' imperativo.** qualquer clique devolve o foco ao teclado. trocar de
    // vista, abrir a matriz ou arrastar um knob nao pode calar as teclas ate' o
    // utilizador clicar no teclado outra vez.
    if (keyboard_.hasKeyboardFocus(true)) {
        return;
    }
    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr) {
        return;
    }
    keyboard_.grabKeyboardFocus();
}

void PluginEditor::refresh() {
    // a thread de interface e' a dona da memoria de amostra: so' agora o ponteiro
    // devolvido pela thread de audio pode ser liberado.
    owner_.releaseReturnedBuffers();

    // os CCs sao aplicados aqui e nao na thread de audio.
    owner_.applyPendingControllerChanges();

    const auto& info = owner_.sourceInfo();

    setIfChanged(fileName_, info.name.isNotEmpty() ? info.name
                                                   : juce::String {"Sem binario"});
    setIfChanged(formatTag_, info.formatTag);
    setIfChanged(fileSize_,
                 info.sizeBytes > 0
                     ? juce::String {info.sizeBytes / (1024.0 * 1024.0), 2} + " MB"
                     : juce::String {});

    updateHeaderVisibility();

    // a grelha fica sempre visivel: sem fonte pinta o estado vazio, em vez de
    // deixar um rectangulo morto.
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
    refreshPresetName();

    // a vista tambem pode mudar por teclas dentro do display, que nao passam pelo
    // editor: a rail tem de seguir o modo real, nao o ultimo botao carregado.
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

    // a cabeca vem do parametro. Se o host a moveu por automacao, o display segue
    // o host e nao o contrario.
    const auto position = readPosition();
    const auto readHead = pe::byteForPosition(static_cast<double>(position), range);
    grid_.setReadHead(readHead);

    // a ancora so' e' escrita quando o POSITION mudou. E' o que impede o editor de
    // reancorar a cada quadro e prender a cabeca no ponto onde o knob estava.
    if (std::abs(position - lastAnchoredPosition_) > 1.0e-4f) {
        positionChangedSinceLastAnchor_ = true;
    }
    pushTransportAnchor();

    // a telemetria vem do que a thread de audio ja' publicou.
    const auto& telemetry = owner_.telemetry();
    const auto playing = telemetry.playing.load(std::memory_order_relaxed);
    const auto playhead = telemetry.playheadFraction.load(std::memory_order_relaxed);

    grid_.setPlayhead(playhead, playing);

    // medidor stereo: dois canais, cada um com o seu pico. o texto do pico saiu do
    // rodape; a barra diz o mesmo e nao gasta uma caixa de leitura.
    meter_.setLevels(telemetry.peakLeftDb.load(std::memory_order_relaxed),
                     telemetry.peakRightDb.load(std::memory_order_relaxed));

    // a cabeca em endereco. E' a leitura que responde a "o que estou a ouvir" e o
    // que o clique na grelha acabou de escolher.
    if (owner_.hasSource() && !range.empty()) {
        setIfChanged(positionReadout_,
                     "POS 0x" + juce::String::formatted(
                                    "%08X", static_cast<unsigned long long>(readHead)));
    } else {
        setIfChanged(positionReadout_, juce::String {});
    }

    if (range.empty()) {
        setIfChanged(offsetReadout_, juce::String {});
    } else {
        setIfChanged(offsetReadout_,
                     juce::String::formatted("REG 0x%08X-0x%08X",
                                             static_cast<unsigned long long>(range.start),
                                             static_cast<unsigned long long>(range.end)));
    }

    // preview dos parametros sobre a onda: spray, densidade, tamanho do grao,
    // posicao, pan, janela, pitch e nivel. Le do estado, nao dos sliders, para a
    // automacao do host aparecer tambem.
    ByteDisplay::ParamPreview preview;
    preview.position = position;
    preview.spray = juce::jlimit(0.0f, 1.0f, rawParameter("spray", 0.0f));
    preview.density = juce::jmax(0.25f, rawParameter("density", 20.0f));
    preview.grainMs = juce::jmax(1.0f, rawParameter("grain", 40.0f));
    preview.pitchSpread = juce::jmax(0.0f, rawParameter("pitchspread", 0.0f));
    preview.pan = juce::jlimit(-1.0f, 1.0f, rawParameter("pan", 0.0f));
    preview.window = juce::roundToInt(rawParameter("window", 0.0f));
    preview.pitchSemitones = rawParameter("pitch", 0.0f);
    preview.level = juce::jlimit(0.0f, 1.0f, rawParameter("grainlevel", 1.0f));
    preview.sampleRate = telemetry.sampleRate.load(std::memory_order_relaxed);
    grid_.setParamPreview(preview);

    const auto rateText =
        juce::String {telemetry.sampleRate.load(std::memory_order_relaxed) / 1000.0f, 1}
        + " kHz";
    if (title_.getTooltip() != rateText) {
        title_.setTooltip(rateText);
    }

    updateFooterVisibility();

    // graos na mesma leitura de 60 Hz. Quem publica e' a thread de audio, uma vez
    // por bloco; o display mostra o ultimo bloco publicado.
    std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices> grains {};
    const auto grainCount = owner_.grainTelemetry().read(grains);
    grid_.setGrains(grains, grainCount);
}

float PluginEditor::readPosition() const {
    // o parametro e' a fonte da verdade. Guardar a fracao no editor faz o display
    // e o host falarem em zigue-zague.
    if (const auto* parameter = owner_.parameters().getParameter("position")) {
        return parameter->getNormalisableRange().convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
}

float PluginEditor::rawParameter(const char* id, float fallback) const {
    if (const auto* parameter = owner_.parameters().getRawParameterValue(id)) {
        return parameter->load(std::memory_order_relaxed);
    }
    return fallback;
}

void PluginEditor::activateByte(std::uint64_t address) {
    // o clique pode cair fora da regiao; entao o material e' puxado para la'
    // primeiro. o POSITION e' uma fracao da regiao, e escreve-lo antes de a regiao
    // mudar apontaria para o sitio errado.
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
    // o comprimento da regiao preserva-se: mudar o inicio nao muda o tamanho da
    // janela. O Processor valida e recorta o fim se preciso.
    const auto range = owner_.byteRange();
    const auto length = juce::jmax<std::uint64_t>(range.length(), 64);
    static_cast<void>(owner_.setByteRange(address, address + length));
}

void PluginEditor::chooseFile() {
    chooser_ = std::make_unique<juce::FileChooser>(
        "Escolha um binario", juce::File::getCurrentWorkingDirectory(),
        "*.exe;*.dll;*.bin;*.sys");

    chooser_->launchAsync(
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& finished) {
            if (finished.getResults().size() > 0) {
                owner_.ingest(finished.getResults().getReference(0).getFullPathName());
                refresh();
            }
        });
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}

} // namespace opcoda
