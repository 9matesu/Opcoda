#include "plugin_editor.h"

#include "factory_presets.h"

#include "opcoda_core/pe/byte_to_position.h"

namespace opcoda {
namespace {

// janela fixa. o desenho pensa em 1180x760 e o canvas encolhe-o 10 %: o
// resultado e' uma janela de 1062x684, mais discreta no ecra sem mexer numa
// coordenada de layout. a F023 pedia um minimo para o redimensionamento; com a
// janela amarrada, o minimo deixou de ter caso.
constexpr int kDesignWidth {1180};
constexpr int kDesignHeight {760};
constexpr float kUiScale {0.9f};
constexpr int kWindowWidth {static_cast<int>(kDesignWidth * kUiScale)};
constexpr int kWindowHeight {static_cast<int>(kDesignHeight * kUiScale)};
constexpr int kHeaderHeight {60};

// margens da chapa. o painel nao encosta em nada: o respiro entre as pecas e'
// o que faz a interface parecer desenhada e nao colada.
constexpr int kBodyMargin {22};
constexpr int kBodyTopGap {10};
constexpr int kBodyBottomGap {16};
constexpr int kScreenBezel {6};
constexpr int kDisplayHeight {304};

// respiro entre o ecra e as abas, e entre as abas e o painel: 0, porque a cauda
// de 6 px desce ate' a chapa e a activa funde-se com ele.
constexpr int kGapBelowDisplay {16};
constexpr int kGapBelowTabs {0};

// tira vertical a' direita do ecra. teclas simples: so' o glifo, 44x38, acima
// do hit target do WCAG 2.5.8 (24) com folga para a mao. o nome vive no
// tooltip e no leitor de ecra; a tira nao alarga para o escrever.
constexpr int kRailWidth {54};
constexpr int kRailButtonWidth {38};
constexpr int kRailButtonHeight {32};
constexpr int kRailGap {10};

// transporte dentro do pe' do ecra. o play e o hold passaram para aqui quando a
// tira horizontal saiu: o ecra e' o centro, e o transporte vive com ele.
constexpr int kTransportButtonWidth {74};
constexpr int kTransportButtonHeight {20};
constexpr int kTransportGap {5};

constexpr int kTabHeight {28};
constexpr int kTabGap {6};
// a cauda desce sobre o painel: e' o que faz da tecla activa uma pasta aberta.
constexpr int kFolderTabTail {6};

constexpr int kKeyboardHeight {74};
constexpr int kGapAboveKeyboard {10};
constexpr int kMasterWidth {116};
constexpr int kMasterGap {12};
constexpr int kMaxCellWidth {156};
constexpr int kKnobContentHeight {148};
constexpr int kFooterHeight {24};
constexpr int kStatusHeight {16};

// padding do painel de parametros: o que ha' entre a chapa e o primeiro knob.
constexpr int kPanelPaddingX {18};
constexpr int kPanelPaddingY {10};
// o grafico do modulo ocupa a esquerda; os controles ficam a' direita.
constexpr int kGraphGap {12};
// o LFO e' largo, mas o grafico dele e' baixo: o modulo MOD da mais espaco aos
// botoes do que aos outros.
constexpr int kModGraphWidth {300};

// desfazer: o silencio que separa dois gestos. abaixo disto, as mudancas sao o
// mesmo arrasto ou o mesmo clique e valem um passo so'.
constexpr std::uint32_t kGestureGapMs {400};
constexpr std::size_t kMaxUndoSteps {32};

juce::String asInteger(float value) {
    return juce::String {juce::roundToInt(value)};
}

const auto percent = [](float v) { return asInteger(v * 100.0f) + " %"; };
const auto seconds = [](float v) {
    // abaixo de um segundo o numero em ms le-se melhor; acima disso, 2000 ms
    // vira "2.00 s" e a casa decimal aparece onde importa.
    return v < 0.9995f ? asInteger(v * 1000.0f) + " ms" : juce::String {v, 2} + " s";
};
const auto cutoffText = [](float v) {
    return v >= 1000.0f ? juce::String {v / 1000.0f, 1} + " kHz" : asInteger(v) + " Hz";
};

// os parametros vivem em quatro modulos visiveis. os IDs dos originais nao mudam --
// sessoes salvas referenciam por ID -- so' o titulo e o nome falado passam a ingles.
//
// **nem todo parametro e' knob.** os que sao escolha -- a janela do grao, a onda
// do LFO, o tipo e ainclinacao do filtro, o alvo do LFO, a divisao -- ficam em
// botoes, chaves e cursores: um disco para escolher entre quatro nomes e' um
// controlo que esconde o que escolhe.
const std::array<Knob::Spec, 5> kGrainRow1Specs {
    Knob::Spec {"grain", "SIZE", "Grain size, in milliseconds", 40.0f,
                [](float v) { return asInteger(v) + " ms"; }},
    Knob::Spec {"density", "DENSITY", "Density, in grains per second", 20.0f,
                [](float v) { return asInteger(v) + " /s"; }},
    Knob::Spec {"position", "POSITION", "Read position in the material", 0.5f, percent},
    Knob::Spec {"spray", "SPRAY", "Spray, start position spread", 0.0f, percent},
    Knob::Spec {"pitch", "PITCH", "Tuning, in semitones", 0.0f,
                [](float v) { return juce::String {v, 1} + " st"; }},
};

const std::array<Knob::Spec, 6> kGrainRow2Specs {
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
    // o glide era o unico parametro do modulo KEYS que valia a pena guardar.
    Knob::Spec {"glide", "GLIDE", "Glide time between notes", 0.0f, seconds},
};

// a nuvem: os tres controles que abrem o grao num enxame. zero em todos devolve
// o motor de um grao, por isso sao controles de profundidade, nao de ligar.
const std::array<Knob::Spec, 3> kCloudSpecs {
    Knob::Spec {"cloudsize", "CLOUD", "Cloud size, grains spread around the playhead",
                0.0f, percent},
    Knob::Spec {"pitchspread", "SPREAD", "Pitch spread across the cloud, in semitones",
                0.0f, [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {"grainjit", "JITTER", "Grain timing jitter", 0.0f, percent},
};

// a janela do grao: quatro botoes com o desenho do envelope. o nome da janela
// aparece em baixo, no valor, e o tooltip diz o resto.
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

// o alvo do LFO: quatro botoes com rotulo curto.
const std::vector<ChoiceControl::Item> kLfoTargetItems {
    {"Modulate pitch", "PITCH", nullptr},
    {"Modulate density", "DENS", nullptr},
    {"Modulate cutoff", "CUT", nullptr},
    {"Modulate position", "POS", nullptr},
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

const std::array<Knob::Spec, 5> kFilterRow1Specs {
    Knob::Spec {"f1cutoff", "F1 CUT", "Filter 1 cutoff frequency", 20000.0f, cutoffText},
    Knob::Spec {"f1q", "F1 Q", "Filter 1 resonance", 0.7071f,
                [](float v) { return juce::String {v, 2}; }},
    Knob::Spec {"f1drive", "F1 DRIVE", "Filter 1 drive, in decibels", 0.0f,
                [](float v) { return juce::String {v, 1} + " dB"; }},
    Knob::Spec {"f1keytrack", "F1 KEY", "Filter 1 key tracking", 0.0f, percent},
    Knob::Spec {"f1env", "F1 ENV", "Filter 1 envelope amount", 0.0f,
                [](float v) {
                    const int pct = static_cast<int>(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
                    return juce::String {pct} + " %";
                }},
};

const std::array<Knob::Spec, 2> kFilterRow2Specs {
    Knob::Spec {"f2cutoff", "F2 CUT", "Filter 2 cutoff frequency", 20000.0f, cutoffText},
    Knob::Spec {"f2q", "F2 Q", "Filter 2 resonance", 0.7071f,
                [](float v) { return juce::String {v, 2}; }},
};

const std::array<Knob::Spec, 3> kModRow1Specs {
    Knob::Spec {"lforate", "RATE", "LFO rate, in hertz", 1.0f,
                [](float v) { return juce::String {v, 2} + " Hz"; }},
    Knob::Spec {"lfodepth", "DEPTH", "LFO depth", 0.0f, percent},
    Knob::Spec {"lfosteps", "STEPS", "LFO steps per cycle", 16.0f,
                [](float v) { return juce::String {static_cast<int>(v + 0.5f)}; }},
};

const std::array<Knob::Spec, 1> kOutSpecs {
    Knob::Spec {"volume", "VOLUME", "Output volume, in decibels", 0.0f,
                [](float v) {
                    return v <= -60.0f ? juce::String {"-inf"}
                                       : juce::String {v, 1} + " dB";
                }},
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
      keyboard_(keyboardState_, juce::MidiKeyboardComponent::horizontalKeyboard),
      envelopeView_(processor.parameters()),
      filterView_(processor.parameters()),
      lfoView_(processor.parameters()) {
    setLookAndFeel(&lookAndFeel_);
    setOpaque(true);

    // o canvas e' o unico filho directo: vive em 1180x760 e o transform encolhe-o
    // para a janela de 1062x684. o paint do editor fica so' com o fundo.
    canvas_.onPaint = [this](juce::Graphics& g) { paintDesign(g); };
    canvas_.setBounds(0, 0, kDesignWidth, kDesignHeight);
    canvas_.setTransform(juce::AffineTransform::scale(kUiScale));
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

    // o estado de partida do primeiro gesto. sem ele o primeiro Ctrl+Z nao tinha
    // para onde voltar.
    settledState_ = owner_.stateXml();
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
    // **nem sempre vem da thread de interface.** a automacao do host escreve por
    // `setValueNotifyingHost` e o listener do ValueTree corre na thread de quem
    // escreveu. tirar o retrato fora da interface alocava XML no caminho de quem
    // nao pode esperar: fica a marcacao, e o timer tira o retrato onde e' seguro.
    if (!juce::MessageManager::existsAndIsCurrentThread()) {
        undoFromAudio_.store(true, std::memory_order_relaxed);
        return;
    }

    // a mudanca chegou: vale como continuacao do gesto se o ultimo evento foi ha'
    // menos de 400 ms. o retrato guardado e' o de antes do gesto comecar.
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
    // o gesto acabou quando os eventos param: so' entao o estado corrente e' o
    // ponto de partida do proximo desfazer.
    if (!undoPending_ || juce::Time::getMillisecondCounter() - lastChangeMs_ < kGestureGapMs) {
        return;
    }
    undoPending_ = false;
    settledState_ = owner_.stateXml();
}

void PluginEditor::applyUndoSnapshot(const juce::XmlElement& xml) {
    // o binario e a regiao viajam no retrato. quando sao os mesmos, repor so' os
    // parametros chega e nao toca no disco; quando mudaram, tem de se recarregar
    // pela mesma via do arrasto.
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

    // o passo que acabou de ser reposto passa a ser o "antes" do proximo gesto;
    // sem isto, o Ctrl+Z seguinte guardava outra vez o estado ja' desfeito e a
    // pilha andava para tras so' uma vez.
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
    for (auto* component : {static_cast<juce::Component*>(&loadButton_),
                            static_cast<juce::Component*>(&fileName_),
                            static_cast<juce::Component*>(&formatTag_),
                            static_cast<juce::Component*>(&fileSize_),
                            static_cast<juce::Component*>(&display_),
                            static_cast<juce::Component*>(&grid_),
                            static_cast<juce::Component*>(&status_),
                            static_cast<juce::Component*>(&positionReadout_),
                            static_cast<juce::Component*>(&offsetReadout_)}) {
        canvas_.addAndMakeVisible(*component);
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
        canvas_.addAndMakeVisible(button);
    };

    struct ViewSpec {
        juce::TextButton* button;
        const char* icon;
        const char* spokenName;
        const char* hint;
        ByteDisplay::ViewMode mode;
    };

    // quatro vistas, quatro perguntas sobre o mesmo material. a rail e' empilhada:
    // a largura do ecra fica para o ecra.
    const ViewSpec views[] = {
        {&viewWaveButton_, "wave", "Forma de onda do material", "Tecla 1",
         ByteDisplay::ViewMode::waveform},
        {&viewHexButton_, "hex", "Grelha de bytes do material", "Tecla 2",
         ByteDisplay::ViewMode::hex},
        {&viewEntropyButton_, "entropy", "Curva de entropia do material", "Tecla 3",
         ByteDisplay::ViewMode::entropy},
        {&viewMatrixButton_, "matrix", "Matriz de modulacao", "Tecla 4",
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

    setupIcon(zoomInButton_, "zoomIn", "rail", "Aproximar a forma de onda", "Tecla +");
    setupIcon(zoomOutButton_, "zoomOut", "rail", "Afastar a forma de onda", "Tecla -");
    setupIcon(zoomFitButton_, "fit", "rail", "Enquadrar a regiao inteira", "Tecla 0");
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

    // -- GRAIN: seis por fila, doze no total. a janela do grao e' um bloco de
    // quatro icones na sexta casa da primeira fila.
    {
        ModuleGrid grid;
        grid.columns = 6;
        grid.maxCellWidth = kMaxCellWidth;
        grid.rows.push_back(rowOf({
            {addKnob(kGrainRow1Specs[0]), nullptr, 1},
            {addKnob(kGrainRow1Specs[1]), nullptr, 1},
            {addKnob(kGrainRow1Specs[2]), nullptr, 1},
            {addKnob(kGrainRow1Specs[3]), nullptr, 1},
            {addKnob(kGrainRow1Specs[4]), nullptr, 1},
            {nullptr, addChoice("window", "WINDOW", "Grain window shape",
                                 ChoiceControl::Mode::icons, 2, kWindowItems), 1},
        }));
        grid.rows.push_back(rowOf({
            {addKnob(kGrainRow2Specs[0]), nullptr, 1},
            {addKnob(kGrainRow2Specs[1]), nullptr, 1},
            {addKnob(kGrainRow2Specs[2]), nullptr, 1},
            {addKnob(kGrainRow2Specs[3]), nullptr, 1},
            {addKnob(kGrainRow2Specs[4]), nullptr, 1},
            {addKnob(kGrainRow2Specs[5]), nullptr, 1},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::grain)] = std::move(grid);
    }

    // -- CLOUD: os tres controles que abrem o grao numa nuvem. ficam numa fila
    // propria porque mudam o motor, nao o grao.
    {
        ModuleGrid grid;
        grid.columns = 3;
        grid.maxCellWidth = kMaxCellWidth;
        grid.rows.push_back(rowOf({
            {addKnob(kCloudSpecs[0]), nullptr, 1},
            {addKnob(kCloudSpecs[1]), nullptr, 1},
            {addKnob(kCloudSpecs[2]), nullptr, 1},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::cloud)] = std::move(grid);
    }

    // -- KEYS: escala e tonica. doze notas e oito modos nao cabem num disco nem
    // numa fila de botoes: vao num cursor com dentes, que mostra o nome do valor
    // e aceita as setas.
    {
        ModuleGrid grid;
        grid.columns = 2;
        grid.maxCellWidth = kMaxCellWidth;
        grid.rows.push_back(rowOf({
            {nullptr, addChoice("scale", "SCALE", "Scale", ChoiceControl::Mode::detent, 1, {}), 1},
            {nullptr, addChoice("root", "ROOT", "Root note", ChoiceControl::Mode::detent, 1, {}), 1},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::keys)] = std::move(grid);
    }

    // -- ENV: sete knobs numa fila, ao lado da curva.
    {
        ModuleGrid grid;
        grid.columns = 7;
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

    // -- FILTER: cinco colunas iguais em duas filas cheias. o tipo e' uma tira de
    // quatro icones dentro de uma celula (uma fila, sem legenda) e a inclinacao
    // e' uma chave de dois estados -- nenhum controle ocupa duas colunas, por
    // isso o compasso e' o mesmo em toda a grelha. os rotulos ja' dizem F1 e F2,
    // e o nome da tecla nao precisa de uma calha com numero ao lado.
    {
        ModuleGrid grid;
        grid.columns = 5;
        grid.rows.push_back(rowOf({
            {nullptr, addChoice("f1type", "F1 TYPE", "Filter 1 type",
                                 ChoiceControl::Mode::icons, 4, kFilterTypeItems), 1},
            {addKnob(kFilterRow1Specs[0]), nullptr, 1},
            {addKnob(kFilterRow1Specs[1]), nullptr, 1},
            {nullptr, addChoice("f1slope", "F1 SLOPE", "Filter 1 slope",
                                 ChoiceControl::Mode::toggle, 1, {}), 1},
            {addKnob(kFilterRow1Specs[2]), nullptr, 1},
        }));
        grid.rows.push_back(rowOf({
            {nullptr, addChoice("f2type", "F2 TYPE", "Filter 2 type",
                                 ChoiceControl::Mode::icons, 4, kFilterTypeItems), 1},
            {addKnob(kFilterRow2Specs[0]), nullptr, 1},
            {addKnob(kFilterRow2Specs[1]), nullptr, 1},
            {addKnob(kFilterRow1Specs[3]), nullptr, 1},
            {addKnob(kFilterRow1Specs[4]), nullptr, 1},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::filter)] = std::move(grid);
    }

    // -- MOD: a forma do LFO e' larga, por isso vive numa fila so' e ocupa
    // duas colunas. o alvo sao quatro botoes com rotulo; a divisao e' um cursor
    // com dentes, porque doze divisoes nao cabem num disco.
    {
        ModuleGrid grid;
        grid.columns = 4;
        grid.rows.push_back(rowOf({
            {addKnob(kModRow1Specs[0]), nullptr, 1},
            {addKnob(kModRow1Specs[1]), nullptr, 1},
            {nullptr, addChoice("lfotarget", "TARGET", "LFO target parameter",
                                 ChoiceControl::Mode::text, 2, kLfoTargetItems), 1},
            {addKnob(kModRow1Specs[2]), nullptr, 1},
        }));
        grid.rows.push_back(rowOf({
            {nullptr, addChoice("lfowave", "WAVE", "LFO waveform",
                                 ChoiceControl::Mode::icons, 5, kLfoWaveItems), 2},
            {nullptr, addChoice("lfosync", "SYNC", "LFO follows the host tempo",
                                 ChoiceControl::Mode::toggle, 1, {}), 1},
            {nullptr, addChoice("lfodiv", "DIV", "LFO division when synced",
                                 ChoiceControl::Mode::detent, 1, {}), 1},
        }));
        moduleGrids_[static_cast<std::size_t>(ParamModule::mod)] = std::move(grid);
    }

    masterKnob_ = addKnob(kOutSpecs[0]);
    masterKnob_->setOnDark(true);

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
        canvas_.addAndMakeVisible(button);
    };
    setup(moduleGrainButton_, "GRAIN", "Grain module", ParamModule::grain);
    setup(moduleCloudButton_, "CLOUD", "Cloud module", ParamModule::cloud);
    setup(moduleEnvelopeButton_, "ENV", "Envelope module", ParamModule::envelope);
    setup(moduleFilterButton_, "FILTER", "Filter module", ParamModule::filter);
    setup(moduleModButton_, "MOD", "Modulation module", ParamModule::mod);
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
    mark(moduleCloudButton_, ParamModule::cloud);
    mark(moduleEnvelopeButton_, ParamModule::envelope);
    mark(moduleFilterButton_, ParamModule::filter);
    mark(moduleModButton_, ParamModule::mod);
    mark(moduleKeysButton_, ParamModule::keys);
}

void PluginEditor::showModule(ParamModule module) {
    activeModule_ = module;
    const auto index = static_cast<std::size_t>(module);

    // cada modulo mostra as suas celulas e esconde as dos outros. o knob do
    // master nunca se esconde: nao pertence a aba nenhuma e vive no canto
    // inferior direito em todas.
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

    canvas_.addAndMakeVisible(keyboard_);
    keyboardState_.addListener(this);
}

void PluginEditor::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber,
                                float velocity) {
    // a nota veio do MIDI do host espelhada para o teclado: o motor ja' a recebeu
    // pelo MidiBuffer, e empurra-la outra vez disparava-a duas vezes.
    if (mirroringNotes_) {
        return;
    }
    owner_.pushKeyboardNote(midiNoteNumber, velocity, true);
}

void PluginEditor::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber,
                                 float velocity) {
    if (mirroringNotes_) {
        return;
    }
    owner_.pushKeyboardNote(midiNoteNumber, velocity, false);
}

void PluginEditor::mirrorIncomingNotes() {
    const auto pressed = owner_.pressedNotes();
    if (pressed.low == mirroredLow_ && pressed.high == mirroredHigh_) {
        return;
    }
    mirroredLow_ = pressed.low;
    mirroredHigh_ = pressed.high;

    mirroringNotes_ = true;
    for (int note = 0; note < 128; ++note) {
        const auto bits = note < 64 ? pressed.low : pressed.high;
        const bool down = ((bits >> (note % 64)) & std::uint64_t {1}) != 0;
        const bool shown = keyboardState_.isNoteOn(1, note);
        if (down && !shown) {
            keyboardState_.noteOn(1, note, 0.9f);
        } else if (!down && shown) {
            keyboardState_.noteOff(1, note, 0.0f);
        }
    }
    mirroringNotes_ = false;
}

void PluginEditor::paint(juce::Graphics& g) {
    // o canvas cobre a janela toda; isto e' so' o fundo por baixo do transform.
    g.fillAll(palette::chassisDark);
}

void PluginEditor::paintDesign(juce::Graphics& g) {
    const auto design = juce::Rectangle<int> {0, 0, kDesignWidth, kDesignHeight};
    const auto full = design.toFloat();

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
    const auto header = design.withHeight(kHeaderHeight).toFloat();
    g.setColour(palette::faceplate);
    g.fillRect(header);
    g.setColour(palette::alpha(palette::ivory, 0.30f));
    g.drawHorizontalLine(0, 0.0f, header.getWidth());
    g.setColour(palette::faceplateDark);
    g.drawHorizontalLine(kHeaderHeight - 1, 0.0f, header.getWidth());
    g.setColour(palette::alpha(palette::brass, 0.60f));
    g.drawHorizontalLine(kHeaderHeight, 0.0f, static_cast<float>(kDesignWidth));

    // -- corpo --
    const auto body = design.withTrimmedTop(kHeaderHeight).toFloat();
    g.setColour(palette::faceplate);
    g.fillRect(body);
    g.setColour(palette::alpha(palette::faceplateDark, 0.45f));
    g.drawVerticalLine(kBodyMargin - 4, body.getY() + 8.0f, body.getBottom() - 8.0f);
    g.drawVerticalLine(kDesignWidth - kBodyMargin + 3, body.getY() + 8.0f,
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

    // separadores do header sairam com o titulo: o bloco do preset e da origem e'
    // um grupo unico e centrado, e um filete no meio dele so' o partia.

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

    // o canvas pensa em 1180x760 e o transform faz os 1062x684 da janela.
    canvas_.setBounds(0, 0, kDesignWidth, kDesignHeight);
    canvas_.setTransform(juce::AffineTransform::scale(kUiScale));

    auto area = canvas_.getLocalBounds();

    // -- header: marca, preset, origem --
    auto header = area.removeFromTop(kHeaderHeight);

    // -- header: preset e origem num bloco unico, centrado --
    // a marca nao vive aqui: quem diz o nome e' a barra da janela (standalone) e
    // um titulo repetido a' esquerda era o que empurrava este bloco para o canto,
    // deixando um vazio grande a' direita.
    {
        constexpr int kArrow {22};
        constexpr int kName {172};
        constexpr int kLoad {30};
        constexpr int kFile {250};
        constexpr int kFormat {76};
        constexpr int kSize {62};
        constexpr int kPresetToFile {22};

        const int presetWidth = kArrow + 3 + kName + 3 + kArrow;
        const int fileWidth = kLoad + 8 + kFile + 8 + kFormat + 6 + kSize;
        int x = header.getX() + (header.getWidth() - (presetWidth + kPresetToFile + fileWidth)) / 2;

        presetPrevButton_.setBounds(x, header.getY() + 19, kArrow, 26);
        x += kArrow + 3;
        presetNameButton_.setBounds(x, header.getY() + 17, kName, 30);
        x += kName + 3;
        presetNextButton_.setBounds(x, header.getY() + 19, kArrow, 26);
        x += kArrow + kPresetToFile;

        loadButton_.setBounds(x, header.getY() + 19, kLoad, 26);
        x += kLoad + 8;
        // a largura do nome e' fixa: com um ficheiro carregado ou sem ele, o bloco
        // fica no mesmo sitio. a linha nao salta a' passagem de cada ingest.
        fileName_.setBounds(x, header.getY() + 16, kFile, 32);
        x += kFile + 8;
        formatTag_.setBounds(x, header.getY() + 19, kFormat, 26);
        x += kFormat + 6;
        fileSize_.setBounds(x, header.getY() + 19, kSize, 26);
    }

    // -- corpo --
    auto body = area.reduced(kBodyMargin, kBodyTopGap);
    body.removeFromBottom(kBodyBottomGap);

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

    body.removeFromTop(kGapBelowDisplay);

    // -- parte de baixo: knobs, teclado, master --
    auto lower = body;
    masterBayBounds_ = lower.removeFromRight(kMasterWidth);
    lower.removeFromRight(kMasterGap);
    keyboardBayBounds_ = lower.removeFromBottom(kKeyboardHeight);
    lower.removeFromBottom(kGapAboveKeyboard);

    // -- abas de pasta: a faixa acima do painel, com a largura do painel. a
    // faixa e' reservada no fluxo vertical -- o que se ve e' o que foi medido,
    // e a aba nunca invade o ecra.
    const auto tabStrip = lower.removeFromTop(kTabHeight + kFolderTabTail);
    parameterPanelBounds_ = lower;

    moduleRowBounds_ = juce::Rectangle<int> {tabStrip.getX(), tabStrip.getY(),
                                              parameterPanelBounds_.getWidth(), kTabHeight};
    {
        // quatro teclas rectangulares a dividir a largura do painel: icone e
        // nome, como as teclas da rail. o compasso entre elas e' o respiro.
        juce::Button* tabs[] = {&moduleGrainButton_, &moduleCloudButton_,
                                &moduleEnvelopeButton_, &moduleFilterButton_,
                                &moduleModButton_, &moduleKeysButton_};
        constexpr int kCount {6};
        const int width = (moduleRowBounds_.getWidth() - kTabGap * (kCount - 1)) / kCount;
        int tx = moduleRowBounds_.getX();
        for (auto* button : tabs) {
            button->setBounds(tx, moduleRowBounds_.getY(), width, kTabHeight + kFolderTabTail);
            tx += width + kTabGap;
        }
    }

    {
        auto master = masterBayBounds_.reduced(5, 6);
        // o volume cede 18 px ao medidor: a escala de dB precisava de altura para
        // os rotulos nao se encostarem.
        auto volumeCell = master.removeFromTop(kKnobContentHeight - 18);
        masterKnob_->setBounds(volumeCell.reduced(2, 0));
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

    // ENV, FILTER e MOD trazem o visualizador: o grafico fica com a esquerda e
    // os controles recuam para a direita. e' a hierarquia que o Ableton usa no
    // dispositivo -- a forma primeiro, os numeros ao lado.
    envelopeView_.setVisible(activeModule_ == ParamModule::envelope);
    filterView_.setVisible(activeModule_ == ParamModule::filter);
    lfoView_.setVisible(activeModule_ == ParamModule::mod);

    const bool hasVisual =
        envelopeView_.isVisible() || filterView_.isVisible() || lfoView_.isVisible();

    auto knobArea = parameterPanelBounds_.reduced(kPanelPaddingX, kPanelPaddingY);

    if (hasVisual) {
        // o grafico leva 42 % da largura, entre 300 e 460 px: abaixo de 300 a
        // curva do filtro perde a leitura das decadas, acima de 460 nao sobra
        // celula de controle que caiba o nome. o MOD e' a excecao: o grafico do
        // LFO e' baixo, e os botoes da forma e da divisao precisam de espaco.
        const int graphWidth = activeModule_ == ParamModule::mod
                                   ? kModGraphWidth
                                   : juce::jlimit(300, 460,
                                                  static_cast<int>(knobArea.getWidth() * 0.42f));
        const auto graph = knobArea.removeFromLeft(graphWidth);
        knobArea.removeFromLeft(kGraphGap);

        if (activeModule_ == ParamModule::envelope) {
            envelopeView_.setBounds(graph);
        } else if (activeModule_ == ParamModule::mod) {
            lfoView_.setBounds(graph);
        } else {
            filterView_.setBounds(graph);
        }
    }

    // o filtro nao leva calha com numero nem veu de fila: os rotulos dos knobs
    // ja' dizem F1 e F2, e uma placa por baixo so' somava uma caixa ao ecra.

    if (moduleIndex < moduleGrids_.size()) {
        layoutModuleGrid(moduleGrids_[moduleIndex], knobArea);
    }

    updateStatusVisibility();
    refreshPlayButton();
}

void PluginEditor::layoutModuleGrid(const ModuleGrid& grid, juce::Rectangle<int> area) {
    if (grid.rows.empty() || area.isEmpty()) {
        return;
    }

    // a celula pode ter um teto de largura: acima dele a grelha encolhe e
    // alinha-se ao centro, para os knobs nao ficarem esbarrados nas bordas.
    auto gridArea = area;
    const int columns = juce::jmax(1, grid.columns);
    int cellWidth = gridArea.getWidth() / columns;
    if (grid.maxCellWidth > 0 && cellWidth > grid.maxCellWidth) {
        cellWidth = grid.maxCellWidth;
        const int total = cellWidth * columns;
        gridArea.setX(gridArea.getX() + (gridArea.getWidth() - total) / 2);
        gridArea.setWidth(total);
    }

    const int rowHeight = gridArea.getHeight() / juce::jmax(1, static_cast<int>(grid.rows.size()));

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
    mirrorIncomingNotes();

    // retrato que ficou pendente de uma mudanca vinda da thread de audio: sai
    // agora, antes do settle, para o "antes" guardado ser o estado certo.
    if (undoFromAudio_.exchange(false, std::memory_order_relaxed)) {
        pushUndoSnapshot();
    }
    settleUndo();
    meter_.tick(delta);
}

void PluginEditor::focusPlayKeyboard() {
    keyboard_.grabKeyboardFocus();
}

bool PluginEditor::keyPressed(const juce::KeyPress& key) {
    // **notas primeiro.** se o foco estiver noutro controle (um menu aberto, o
    // display), a tecla de nota ainda tem de soar: o teclado virtual le o mapa
    // A W S E D F T G Y H U J K e nao depende do foco para isso.
    if (!keyboard_.hasKeyboardFocus(true) && keyboard_.keyPressed(key)) {
        return true;
    }

    // desfazer. vem depois das notas e antes de tudo o resto: Ctrl+Z nao e' nota
    // e nao pode cair no mapa do teclado.
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
    // a coluna ASM do hex so' desmonta material de 64 bits: em PE32 a leitura
    // seria mentira comprimento afora.
    grid_.setDisassemblyEnabled(info.formatTag.contains("64-bit"));
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

    // a cabeca vem do parametro, ou do transporte quando ele esta' a andar: no
    // hex/ASCII/ASM a agulha tem de seguir a leitura, nao ficar parada no knob
    // enquanto o audio varre o material.
    const auto& telemetry = owner_.telemetry();
    const auto playing = telemetry.playing.load(std::memory_order_relaxed);
    const auto playhead = telemetry.playheadFraction.load(std::memory_order_relaxed);

    const auto position = readPosition();
    const auto headPosition = playing ? playhead : position;
    const auto readHead = pe::byteForPosition(static_cast<double>(headPosition), range);
    grid_.setReadHead(readHead);

    // a ancora so' e' escrita quando o POSITION mudou. E' o que impede o editor de
    // reancorar a cada quadro e prender a cabeca no ponto onde o knob estava.
    if (std::abs(position - lastAnchoredPosition_) > 1.0e-4f) {
        positionChangedSinceLastAnchor_ = true;
    }
    pushTransportAnchor();

    // a telemetria vem do que a thread de audio ja' publicou.
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

    // o ecra do material mostra so' a onda: o preview dos parametros sobre ela
    // saiu, porque era o instrumento a desenhar por cima do audio.

    const auto rateText =
        juce::String {telemetry.sampleRate.load(std::memory_order_relaxed) / 1000.0f, 1}
        + " kHz";
    if (status_.getTooltip() != rateText) {
        status_.setTooltip(rateText);
    }

    updateFooterVisibility();

    // graos na mesma leitura de 60 Hz. Quem publica e' a thread de audio, uma vez
    // por bloco; o display mostra o ultimo bloco publicado.
    std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices> grains {};
    const auto grainCount = owner_.grainTelemetry().read(grains);
    grid_.setGrains(grains, grainCount);

    // os visualizadores de ENV e FILTER mostram o que o audio esta' a fazer: o
    // nivel e a fase do envelope, e o corte ja' modulado por keytrack, envelope e
    // lfo. so' se alimenta o que esta' a' vista.
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
    // o parametro e' a fonte da verdade. Guardar a fracao no editor faz o display
    // e o host falarem em zigue-zague.
    if (const auto* parameter = owner_.parameters().getParameter("position")) {
        return parameter->getNormalisableRange().convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
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
    pushUndoSnapshot();
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
