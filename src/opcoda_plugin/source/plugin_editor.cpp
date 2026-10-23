#include "plugin_editor.h"

#include "factory_presets.h"

#include "opcoda_core/entropy/shannon_entropy.h"
#include "opcoda_core/pe/byte_to_position.h"

namespace opcoda {
namespace {

// 44 px: sem caixa no nome nem palavra nos botoes, a faixa nao precisa de mais.
// o LOAD de 28 entra a y+10, o nome a y+14 com 24. folga de 6 em baixo.
constexpr int kHeaderHeight {44};

// 200-320 px de display. A grelha precisa de 14 px de cabecalho, seis linhas de
// 24 e a faixa de estado; abaixo disso o fim do ficheiro fica fora do ecra.
constexpr int kMinDisplayHeight {200};
constexpr int kMaxDisplayHeight {320};

// 480x434 e' o minimo. A altura leva a fileira de abas (22 + 2); a largura desce
// a 480 porque o chip de formato colidia com o LOAD. A esta largura a grelha larga
// a coluna ASCII, e o criterio 1.4.10 passa a ter um caso real.
constexpr int kMinWidth {480};
constexpr int kMinHeight {434};
constexpr int kAddressFieldWidth {132};
constexpr int kPlayButtonWidth {60};
constexpr int kStatusHeight {14};
constexpr int kFooterHeight {16};
// 56 px: tecla branca legivel e o painel de knobs com o mesmo espaco. O custo sai
// da metade do display, nao da faixa de parametros, que ja esta' no osso.
constexpr int kKeyboardHeight {56};

// formatacao inteira, partilhada pelas tabelas de specs. STATE FILTER e MOD &
// OUTPUT do mock ficaram de fora: biquad, envelope e dry/wet nao existem no nucleo.
//
// juce::String {v, 0} nao arredonda — writeDouble so formata com numDecPlaces > 0,
// e com zero sai "28.756" onde se queria "29". O roundToInt fica explicito.
juce::String asInteger(float value) {
    return juce::String {juce::roundToInt(value)};
}

const auto percent = [](float v) { return asInteger(v * 100.0f) + " %"; };
const auto seconds = [](float v) { return asInteger(v * 1000.0f) + " ms"; };
const auto cutoffText = [](float v) {
    return v >= 1000.0f ? juce::String {v / 1000.0f, 1} + " kHz" : asInteger(v) + " Hz";
};

// os parametros vivem em sete modulos. Os IDs dos originais nao mudam — sessoes
// salvas referenciam por ID — so' o titulo e o nome falado passam a ingles. As abas
// trocam a faixa visivel: a 480 dez knobs ja ficam a 46 px por celula, e seis
// fileiras pediam 700 px.
const std::array<Knob::Spec, 10> kGrainSpecs {
    Knob::Spec {"grain", "SIZE", "Grain size, in milliseconds", 40.0f,
                [](float v) { return asInteger(v) + " ms"; }},
    Knob::Spec {"density", "DENSITY", "Density, in grains per second", 20.0f,
                [](float v) { return asInteger(v) + " /s"; }},
    Knob::Spec {"position", "POSITION", "Read position in the material", 0.5f, percent},
    Knob::Spec {"spray", "SPRAY", "Spray, start position spread", 0.0f, percent},
// com uma casa decimal o writeDouble entra no ramo fixo. PITCH e P.RAND ficam
// como estavam: ja estavam certos.
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
// F020: slope e' a inclinacao, drive satura antes do filtro, keytrack segue a nota
// e env abre com o envelope.
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
// F019: em Sync o RATE cala-se e a divisao manda. Os dois ficam no mesmo modulo:
// sao a mesma pergunta em duas unidades.
    Knob::Spec {"lfosync", "SYNC", "LFO follows the host tempo", 0.0f, {},
                {"Free", "Sync"}},
    Knob::Spec {"lfodiv", "DIV", "LFO division when synced", 0.0f, {},
                {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/4.", "1/8.",
                 "1/4T", "1/8T", "1/16T"}},
    Knob::Spec {"lfosteps", "STEPS", "LFO steps per cycle", 16.0f,
                [](float v) { return juce::String {static_cast<int>(v + 0.5f)}; }},
};

// F021: mesmo motor, tres graus de liberdade no nascimento do grao. Modulo
// separado para o GRAIN nao passar de dez celulas — abaixo disso o knob encolhe.
const std::array<Knob::Spec, 3> kCloudSpecs {
    Knob::Spec {"cloudsize", "WIDTH", "Temporal width of the grain cloud", 0.0f,
                [](float v) { return asInteger(v * 500.0f) + " ms"; }},
    Knob::Spec {"pitchspread", "SPREAD", "Pitch spread across the cloud, in semitones", 0.0f,
                [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {"grainjit", "JITTER", "Grain duration jitter", 0.0f, percent},
};

const std::array<Knob::Spec, 1> kOutSpecs {
    Knob::Spec {"volume", "VOLUME", "Output volume, in decibels", 0.0f,
                [](float v) {
                    return v <= -60.0f ? juce::String {"-inf"}
                                       : juce::String {v, 1} + " dB";
                }},
};

// F018: teclado e afinacao. A nota MIDI vira transposicao ja quantizada pela
// escala; o glide interpola. Tres parametros novos, nenhum dos antigos mudou de
// sitio.
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

    makePlainLabel(title_, "Opcoda", palette::textDark, fonts().sans(13.0f, true));
// o subtitulo, o contador de vozes do cabecalho, a dica de arrasto e o titulo do
// modulo sairam: cada um repetia algo escrito noutro sitio. Ficam o LED, o nome, o
// LOAD, o nome do ficheiro, o formato e o tamanho.
    makePlainLabel(status_, "", palette::textOnDark, fonts().sans(10.0f));

    buildHeader();
    buildViewButtons();
    buildPresetSelector();
    buildParameterPanel();
    buildKeyboard();

// a sujeira do preset vem por Listener: qualquer PARAM mexido fora de uma
// aplicacao de preset marca. O desenho do "*" mora no refresh.
    owner_.parameters().state.addListener(this);

// a thread de interface e' a dona da memoria de amostra; e' ela que libera o que
// a thread de audio devolveu. 60 Hz porque a animacao tem meia-vida de 60 ms: a 20
// hz cada meia-vida tem menos de dois quadros e o degrau ve-se. O texto nao vira
// ruido porque setIfChanged so' escreve quando muda.
    startTimerHz(60);

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, 4096, 4096);
    setSize(900, 540);
    refresh();
}

PluginEditor::~PluginEditor() {
// note-off primeiro, com o listener ainda ligado: fechar a janela com tecla
// premida deixaria a nota presa. allNotesOff corre na thread de interface e o
// listener empurra para a fila.
    keyboardState_.allNotesOff(1);
    keyboardState_.removeListener(this);
    owner_.parameters().state.removeListener(this);
}

void PluginEditor::buildPresetSelector() {
// setas sem cromo e nome como aba, a mesma lingua das abas de modulo. O nome e' o
// botao do menu: um "MENU" separado seria o quarto controle para uma funcao.
    const auto setupArrow = [this](juce::TextButton& button, const char* glyph,
                                   const char* spokenName, int direction) {
        button.setButtonText(glyph);
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName});
        button.getProperties().set("uiIcon", "tab");
        button.onClick = [this, direction] { stepFactoryPreset(direction); };
        addAndMakeVisible(button);
    };
    setupArrow(presetPrevButton_, "<", "Previous factory preset", -1);
    setupArrow(presetNextButton_, ">", "Next factory preset", +1);

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

// showMenuAsync como o FileChooser: menu modal trava a message thread. O item
// marcado so' aparece com o preset limpo; com "*" nenhum e' o som atual.
    menu.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this),
                       [this](int result) {
                           if (result >= 1 && result <= 6) {
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
    const auto text =
        presetDirty_ ? currentPresetName_ + " *" : currentPresetName_;
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
// nome primeiro, som depois: se o ficheiro falhar, o nome antigo continua
// verdadeiro. O som so' troca dentro do if.
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
// extensao forcada: o dialogo de save nao a poe, e um preset sem .opcoda nao abre
// por duplo-clique nem pelo filtro de load.
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
    for (auto* component : {static_cast<juce::Component*>(&powerLed_),
                            static_cast<juce::Component*>(&title_),
                            static_cast<juce::Component*>(&loadButton_),
                            static_cast<juce::Component*>(&fileName_),
                            static_cast<juce::Component*>(&formatTag_),
                            static_cast<juce::Component*>(&fileSize_),
                            static_cast<juce::Component*>(&display_),
                            static_cast<juce::Component*>(&statusLed_),
                            static_cast<juce::Component*>(&status_)}) {
        addAndMakeVisible(*component);
    }

    for (auto* component : {static_cast<juce::Component*>(&grid_),
                            static_cast<juce::Component*>(&address_),
                            static_cast<juce::Component*>(&playButton_),
                            static_cast<juce::Component*>(&voicesLed_)}) {
        addAndMakeVisible(*component);
    }

// a grelha escreve o POSITION. Duplo clique e tecla A alternam entre a regiao
// exacta e a seccao PE mais proxima; quem decide o sentido e' o metodo do
// processor, que guarda a regiao anterior.
    grid_.onAddressActivated = [this](std::uint64_t address) { activateByte(address); };
    grid_.onSnapRequested = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };
    address_.onAddressEntered = [this](std::uint64_t address) { moveRegionTo(address); };

// o display fica com o foco quando o campo de endereco nao tem, e responde ao
// espaco e ao A. O editor responde tambem, para quando o foco esta' num botao.
    grid_.onToggleTransport = [this] {
        owner_.toggleTransport();
        refreshPlayButton();
    };
    grid_.onSnapRequestedFromKey = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };

// sem botao de alinhar: o terceiro controle empurrava o texto para debaixo do
// campo. A dica vive no help text do display e em docs/10 — setHelpText e nao
// setTooltip, porque o Component so' expoe ajuda pelo AccessibilityHandler.
    grid_.setHelpText("Duplo-clique ou tecla A: alinha a regiao a secao PE mais proxima.");

// toggle, e a forma troca com o estado (▶/■): a segunda pista, a que serve quem
// nao distingue cores. So' glifo, desenhado pelo LookAndFeel.
    playButton_.getProperties().set("uiIcon", "transport");
// sobre o display escuro, tinta clara. uiOnDark ja' e' redundante com o chassis
// escuro; fica porque o LookAndFeel ainda a le para o wash do hover.
    playButton_.getProperties().set("uiOnDark", true);
    playButton_.setButtonText("");
    playButton_.setClickingTogglesState(true);
    playButton_.setTooltip(
        "Percorre a regiao seleccionada de inicio a fim, sem nota MIDI. Tambem na "
        "barra de espaco.");
    playButton_.setName("Reproduzir a regiao");
    playButton_.onClick = [this] {
        owner_.toggleTransport();
        refreshPlayButton();
    };

    for (auto* readout : {&entropyReadout_, &positionReadout_, &offsetReadout_,
                           &transportReadout_, &peakReadout_, &voicesReadout_}) {
        addAndMakeVisible(*readout);
        makePlainLabel(*readout, "", palette::textOnDark, fonts().sans(10.0f));
// centrado como quando havia caixa: a posicao do texto nao muda, so' some a
// moldura. Mudar o alinhamento junto seria duas mudancas visuais numa.
        readout->setJustificationType(juce::Justification::centred);
        readout->setInterceptsMouseClicks(false, false);
// a cor de texto por omissao do Label e' quase preta e desaparece no display.
// textOnDark mantem a leitura acima de 4,5:1 (criterio 1.4.3). O rodape vive no
// display, nao no chassis claro.
    }

    makePlainLabel(fileName_, "-", palette::textDark, fonts().sans(10.0f));
// formato e tamanho sao metadado tecnico: texto secundario, para o nome da fonte
// carregar a hierarquia sozinho.
    makePlainLabel(formatTag_, "", palette::textSub, fonts().sans(10.0f));
    makePlainLabel(fileSize_, "", palette::textSub, fonts().sans(10.0f));
    fileName_.setJustificationType(juce::Justification::centred);
    formatTag_.setJustificationType(juce::Justification::centred);
    fileSize_.setJustificationType(juce::Justification::centred);

    loadButton_.getProperties().set("uiIcon", "folder");
    loadButton_.setButtonText("");
    loadButton_.setTooltip("Escolher um binario para sintetizar (tecla L)");
    loadButton_.setName("Carregar binario");
    loadButton_.onClick = [this] { chooseFile(); };
}

// botoes de vista e zoom, montados a partir da tabela de specs. O rotulo e'
// curto; o nome acessivel e o tooltip levam a descricao e a tecla.
void PluginEditor::buildViewButtons() {
// rotulos curtos de proposito: o nome acessivel e' a descricao completa e o
// tooltip explica a tecla. 15 caracteres num botao de 60 px sairiam cortados.
    struct Spec {
        juce::TextButton* button;
        const char* shortLabel;
        const char* spokenName;
        const char* hint;
        ByteDisplay::ViewMode mode;
    };

    const Spec specs[] = {
        {&viewWaveButton_, "WAV", "Forma de onda do material", "Tecla 1", ByteDisplay::ViewMode::waveform},
        {&viewHexButton_, "HEX", "Grelha de bytes do material", "Tecla 2", ByteDisplay::ViewMode::hex},
        {&viewEntropyButton_, "ENT", "Curva de entropia do material", "Tecla 3", ByteDisplay::ViewMode::entropy},
    };

    for (auto* button : {static_cast<juce::Component*>(&viewWaveButton_),
                         static_cast<juce::Component*>(&viewHexButton_),
                         static_cast<juce::Component*>(&viewEntropyButton_),
                         static_cast<juce::Component*>(&zoomOutButton_),
                         static_cast<juce::Component*>(&zoomInButton_),
                         static_cast<juce::Component*>(&zoomFitButton_)}) {
        addAndMakeVisible(*button);
    }

    for (const auto& spec : specs) {
        spec.button->setButtonText(spec.shortLabel);
        spec.button->setClickingTogglesState(true);
        spec.button->setTooltip(juce::String {spec.spokenName} + ". " + spec.hint);
        spec.button->setName(spec.spokenName);
// aba sem cromo: so' a palavra e o filete do estado. O desenho e' do LookAndFeel,
// que trata "tab" sem fundo nem bisel.
        spec.button->getProperties().set("uiIcon", "tab");
    }

// sem ButtonGroup: cada botao e' independente e o LookAndFeel desenha o estado. O
// grupo daria exclusao mutua mas trazia um listener que so' reescrevia o que
// updateViewButtons ja' escreve a partir do modo.
    viewWaveButton_.onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::waveform);
        updateViewButtons();
    };
    viewHexButton_.onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::hex);
        updateViewButtons();
    };
    viewEntropyButton_.onClick = [this] {
        grid_.setMode(ByteDisplay::ViewMode::entropy);
        updateViewButtons();
    };

// zoom clicavel do viewport. Nomes falados completos: "-" sozinho nao diz nada num
// leitor de tela.
    const auto setupZoom = [this](juce::TextButton& button, const char* glyph,
                                 const char* spokenName, const char* hint) {
        button.setButtonText(glyph);
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName} + ". " + hint);
        button.getProperties().set("uiIcon", "tab");
    };
    setupZoom(zoomOutButton_, "-", "Zoom out of the waveform", "Tecla -");
    setupZoom(zoomInButton_, "+", "Zoom into the waveform", "Tecla +");
    setupZoom(zoomFitButton_, "FIT", "Fit the region to the view", "Tecla 0");
    zoomOutButton_.onClick = [this] { grid_.zoomOut(); };
    zoomInButton_.onClick = [this] { grid_.zoomIn(); };
    zoomFitButton_.onClick = [this] { grid_.fitView(); };

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
}

void PluginEditor::updateHeaderVisibility() {
    const auto& info = owner_.sourceInfo();

// duas condicoes, e ambas verdadeiras: espaco e conteudo. Nunca se desenha uma
// moldura vazia — o mesmo motivo pelo qual os chips vazios se escondem.
    fileSize_.setVisible(headerFitsSize_ && info.sizeBytes > 0);
    formatTag_.setVisible(headerFitsFormat_ && info.formatTag.isNotEmpty());
}

void PluginEditor::updateFooterVisibility() {
// mesma regra do header: a largura decide se a caixa existe, o conteudo decide se
// ha' o que escrever. Caixa pequena de mais e' pior que nenhuma.
    peakReadout_.setVisible(footerFitsPeak_);
    entropyReadout_.setVisible(footerFitsEntropy_ && entropyReadout_.getText().isNotEmpty());

// a leitura de transporte so' aparece a tocar: moldura vazia le-se como defeito.
// durante a reproducao e' a unica pista textual da cabeca, e o 1.4.1 nao aceita a
// barra animada como unica informacao.
    transportReadout_.setVisible(footerFitsTransport_ &&
                                 transportReadout_.getText().isNotEmpty());
}

void PluginEditor::updateStatusVisibility() {
// o botao de reproducao nunca sai por falta de espaco — e' a unica forma de ouvir
// sem teclado MIDI. O que se sacrifica e' o campo de endereco, que escreve um
// sitio que o POSITION e o clique tambem escrevem.
    playButton_.setVisible(owner_.hasSource());
    address_.setVisible(owner_.hasSource() && statusFitsAddress_);
}

void PluginEditor::refreshPlayButton() {
    const bool playing = owner_.isTransportPlaying();

// a forma e' a pista que sobrevive a quem nao ve cor: ▶ parado, ■ a tocar. O nome
// acessivel troca junto para o leitor dizer o mesmo.
    if ((playButton_.getName() == "Parar a reproducao") != playing) {
        playButton_.setName(playing ? "Parar a reproducao" : "Reproduzir a regiao");
    }

    if (playButton_.getToggleState() != playing) {
        playButton_.setToggleState(playing, juce::dontSendNotification);
    }

// desativado e nao invisivel, e sem fingir que esta' ativo. Sem material nao ha'
// regiao; com um ou dois bytes nao ha' posicao que soe (o motor precisa de
// `index + 1` e hasRegion() e' >= 3); com o host parado nao ha' callback. Um botao
// que aceita o toque e nao faz nada parece avaria.
    const auto canPlay = owner_.hasSource() && owner_.hasPlayableRegion() &&
                         !owner_.hostTransportIsKnownToBeStopped();
    playButton_.setEnabled(canPlay);
}

void PluginEditor::pushTransportAnchor() {
// a ancora so' e' escrita quando o utilizador mexeu em POSITION. Escrever a cada
// quadro subiria a geracao do nucleo a cada quadro e a cabeca nunca passaria do
// ponto de ancoragem. Transport.SeekIsAppliedOnceAndThenTheHeadAdvanced cobre
// isso do outro lado.
    if (!positionChangedSinceLastAnchor_) {
        return;
    }
    positionChangedSinceLastAnchor_ = false;
    lastAnchoredPosition_ = readPosition();
    owner_.seekTransportTo(lastAnchoredPosition_);
}

void PluginEditor::buildParameterPanel() {
// ordem de construcao = ordem dos modulos: GRAIN, ENVELOPE, FILTER, MOD, CLOUD,
// OUT, KEYS. As fatias ficam guardadas para a aba mostrar sem procurar por ID.
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

    buildModuleTabs();
    showModule(activeModule_);
}

void PluginEditor::buildModuleTabs() {
// mesma lingua das abas de vista: palavra sem cromo, ativa com peso forte e
// filete. Sete abas de 52 px mais 4 de vao cabem a 480.
    const auto setup = [this](juce::TextButton& button, const char* title,
                              const char* spokenName, ParamModule module) {
        button.setButtonText(title);
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName});
        button.setClickingTogglesState(true);
// aba sem cromo, o mesmo "tab" do LookAndFeel: palavra e filete, sem fundo nem
// bisel.
        button.getProperties().set("uiIcon", "tab");
        button.onClick = [this, module] { showModule(module); };
        addAndMakeVisible(button);
    };
    setup(moduleGrainButton_, "GRAIN", "Grain module", ParamModule::grain);
    setup(moduleEnvelopeButton_, "ENV", "Envelope module", ParamModule::envelope);
    setup(moduleFilterButton_, "FILTER", "Filter module", ParamModule::filter);
    setup(moduleModButton_, "MOD", "Modulation module", ParamModule::mod);
    setup(moduleCloudButton_, "CLOUD", "Grain cloud module", ParamModule::cloud);
    setup(moduleOutButton_, "OUT", "Output module", ParamModule::out);
    setup(moduleKeysButton_, "KEYS", "Keyboard and tuning module", ParamModule::keys);
}

void PluginEditor::updateModuleTabs() {
    const auto mark = [this](juce::TextButton& button, ParamModule candidate) {
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
    mark(moduleOutButton_, ParamModule::out);
    mark(moduleKeysButton_, ParamModule::keys);
}

void PluginEditor::showModule(ParamModule module) {
    activeModule_ = module;
    const auto index = static_cast<std::size_t>(module);
    const auto first = moduleFirstKnob_[index];
    for (std::size_t i = 0; i < knobs_.size(); ++i) {
        knobs_[i]->setVisible(i >= first && i < first + moduleKnobCount_[index]);
    }
    updateModuleTabs();
    resized();
}

void PluginEditor::buildKeyboard() {
// teclado horizontal com o mapa de teclas do JUCE por omissao (A W S E D F T G Y H
// U J K...), que e' o que a F018 pede. Rato, foco por Tab e desenho da tecla ja'
// sao do componente; falta so' escutar o estado e mandar a nota ao motor.
    keyboard_.setAvailableRange(36, 96);
    keyboard_.setLowestVisibleKey(48);
    keyboard_.setKeyWidth(24.0f);
    keyboard_.setName("Virtual keyboard: play with the mouse or the computer keys "
                      "A W S E D F T G Y H U J K");
    keyboard_.setWantsKeyboardFocus(true);

    keyboard_.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                        palette::textDark.withAlpha(0.92f));
    keyboard_.setColour(juce::MidiKeyboardComponent::blackNoteColourId, palette::chassisDark);
    keyboard_.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, palette::chassisBorder);
    keyboard_.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                        palette::accent.withAlpha(0.22f));
    keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        palette::accent.withAlpha(0.72f));
    keyboard_.setColour(juce::MidiKeyboardComponent::textLabelColourId, palette::chassisDark);
    keyboard_.setColour(juce::MidiKeyboardComponent::shadowColourId,
                        juce::Colours::black.withAlpha(0.35f));

    addAndMakeVisible(keyboard_);
    keyboardState_.addListener(this);
}

// duas linhas e nao uma tabela: o listener tem duas entradas, e um booleano por
// dentro obrigaria a uma terceira funcao.
void PluginEditor::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity) {
    owner_.pushKeyboardNote(midiNoteNumber, velocity, true);
}

void PluginEditor::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity) {
    owner_.pushKeyboardNote(midiNoteNumber, velocity, false);
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(palette::chassis);

// faixa do header: um tom acima do chassis, com a borda de 1 px que separa as tres
// faixas.
    g.setColour(palette::header);
    g.fillRect(getLocalBounds().withHeight(kHeaderHeight));
    g.setColour(palette::chassisBorder);
    g.drawHorizontalLine(kHeaderHeight, 0.0f, static_cast<float>(getWidth()));

// sombra de 1 px sob a costura: o header assenta sobre o chassis em vez de estar
// pintado nele. Preta a 0,15 — sombra e nao linha: o divisor ja' existe.
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.drawHorizontalLine(kHeaderHeight + 1, 0.0f, static_cast<float>(getWidth()));

// realce de 1 px no topo: da' sensacao de superficie em vez de retangulo liso.
// 0,08 porque o fundo e' escuro.
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
}

void PluginEditor::resized() {
    auto area = getLocalBounds();

// o teclado sai da base antes de tudo: daqui para cima nada cresce para cima
// dele. O que custa sai da metade do display, nao do painel de knobs.
    keyboard_.setBounds(area.removeFromBottom(kKeyboardHeight).reduced(10, 4));

// ---- header ----
// cursor explicito em vez de encadear removeFromLeft: aquele devolve um retangulo
// novo e NAO consome o original, por isso encadear sobrepoe.
    const auto header = area.removeFromTop(kHeaderHeight).reduced(10, 0);
    int x = header.getX();

// medidas do header em constantes com nome: o layout e' uma cadeia de larguras
// que tem de fechar, e numeros soltos nao se somam a olho.
    constexpr int kLedWidth {16};
    constexpr int kGap {6};
// o wordmark e' a unica peca com largura propria que nao e' cadeia: o nome nao
// cresce com a janela. 180 px era o que o subtitulo pedia; sem ele, metade serve.
    constexpr int kIdentityWidth {96};
    constexpr int kIdentityGap {12};
// 40 px porque e' um icone de pasta e nao a palavra LOAD: os 36 que sobram vao
// para o nome do ficheiro.
    constexpr int kLoadWidth {40};
    constexpr int kNameGap {6};
    constexpr int kMinNameWidth {40};
    constexpr int kChipGap {4};
    constexpr int kFormatWidth {78};
    constexpr int kSizeWidth {58};

    powerLed_.setBounds(juce::Rectangle<int> {x, header.getY(), kLedWidth, header.getHeight()}
                             .withSizeKeepingCentre(14, 14));
    x += kLedWidth + kGap;

// titulo centrado na altura do cabecalho. Estava a 10 px do topo por causa do
// subtitulo; sem ele, centrar evita o wordmark colado ao topo.
    title_.setBounds(juce::Rectangle<int> {x, header.getY(), kIdentityWidth, header.getHeight()}
                         .withSizeKeepingCentre(kIdentityWidth, 19));
    x += kIdentityWidth + kIdentityGap;

// seletor de preset entre o wordmark e o LOAD: identidade do som antes da origem.
// setas de 22 com nome de 84; a 480 o nome do ficheiro encolhe mas o preset nunca
// e' sacrificado — e' feature, nao chip.
    constexpr int kPresetArrow {22};
    constexpr int kPresetName {84};
    constexpr int kPresetGap {2};
    presetPrevButton_.setBounds(juce::Rectangle<int> {x, header.getY() + 11, kPresetArrow, 22});
    x += kPresetArrow + kPresetGap;
    presetNameButton_.setBounds(juce::Rectangle<int> {x, header.getY() + 11, kPresetName, 22});
    x += kPresetName + kPresetGap;
    presetNextButton_.setBounds(juce::Rectangle<int> {x, header.getY() + 11, kPresetArrow, 22});
    x += kPresetArrow + kIdentityGap;

// daqui para a direita tudo tem de caber a serio. leftLimit e' o fim do bloco
// fixo: LED, wordmark, preset, LOAD e o nome no minimo. Cada peca decide contra
// ele se cabe.
    const int leftLimit = x + kLoadWidth + kNameGap + kMinNameWidth;

// stripEdge e' o cursor que desce da direita. Nem cursor (membro de
// juce::Component, o /W4 trata a ocultacao como erro) nem edge (ja' e' o cursor do
// rodape nesta funcao).
//
// a borda direita e' a do cabecalho: o bloco VOICES de 78 px saiu por repetir o
// rodape.
    int stripEdge = header.getRight();

// a ordem de sacrificio e' a importancia ao contrario: formato primeiro, tamanho
// em ultimo. Ambos sao metadado, e o nome vale mais.
//
// sem esta regra o chip de formato comecava em 174 numa janela de 560 e o LOAD
// acabava em 300: 126 px de sobreposicao que so' aparecia depois de carregar.

// 1. tamanho: ponta direita da faixa.
    headerFitsSize_ = stripEdge - kSizeWidth >= leftLimit;
    if (headerFitsSize_) {
        fileSize_.setBounds(juce::Rectangle<int> {stripEdge - kSizeWidth, header.getY() + 17,
                                                  kSizeWidth, 16});
        stripEdge -= kSizeWidth + kChipGap;
    }

// 2. formato, logo a seguir.
    headerFitsFormat_ = stripEdge - kFormatWidth >= leftLimit;
    if (headerFitsFormat_) {
        formatTag_.setBounds(juce::Rectangle<int> {stripEdge - kFormatWidth, header.getY() + 17,
                                                   kFormatWidth, 16});
        stripEdge -= kFormatWidth + kChipGap;
    }

// 3. O nome cresce com o que sobrou e nunca desce do minimo.
    loadButton_.setBounds(juce::Rectangle<int> {x, header.getY() + 10, kLoadWidth, 28});
    fileName_.setBounds(juce::Rectangle<int> {x + kLoadWidth + kNameGap, header.getY() + 14,
                                              juce::jmax(kMinNameWidth,
                                                         stripEdge - (x + kLoadWidth + kNameGap)),
                                              24});

    updateHeaderVisibility();

// -- display --
    const auto displayHeight =
        juce::jlimit(kMinDisplayHeight, kMaxDisplayHeight, getHeight() / 2 - kKeyboardHeight);
    const auto displayBounds = area.removeFromTop(displayHeight);
    display_.setBounds(displayBounds);

// a linha de estado ocupa uma faixa propria no topo do display, e a grelha comeca
// abaixo. O erro tem de aparecer onde se ve: um E_BAD_PE no rodape, em mono de 10
// px ao lado do pico, passa sem ser lido. A faixa e' reservada e nao sobreposta:
// o fundo opaco da grelha tapava o "PRONTO".
    const auto statusY = displayBounds.getY();
    statusLed_.setBounds(juce::Rectangle<int> {displayBounds.getX() + 12, statusY, 16, 20}
                             .withSizeKeepingCentre(14, kStatusHeight));

    const auto footerY = displayBounds.getBottom() - kFooterHeight - 2;

// altura da grelha = do fim da faixa de estado ao inicio do rodape. Passar
// footerY como altura (coordenada absoluta) esticava a grelha 54 px e a ultima
// linha caia por cima das leituras.
    const auto gridTop = statusY + kStatusHeight + 1;
    grid_.setBounds(juce::Rectangle<int> {displayBounds.getX(), gridTop,
                                          displayBounds.getWidth(), footerY - gridTop});

// campo de endereco e botao de reproducao na linha de estado, a' direita: sao
// controles na zona que ja' tem moldura, e um campo dentro do rodape de leituras
// seria indistinguivel de uma leitura.
//
// a cadeia desce da direita em largura fixa e degrada em vez de espremer, com o
// botao de reproducao a ser o ultimo a cair. O campo cai primeiro.
    const int controlY = displayBounds.getY() + 2;
    const int controlHeight = kStatusHeight + 2;
    const int controlGap = 6;

    playButton_.setBounds(juce::Rectangle<int> {displayBounds.getRight() - 12 - kPlayButtonWidth,
                                                controlY, kPlayButtonWidth, controlHeight});
    const int afterPlay = playButton_.getX() - controlGap;

    statusFitsAddress_ = afterPlay - kAddressFieldWidth >= displayBounds.getX() + 150;
    address_.setBounds(juce::Rectangle<int> {afterPlay - kAddressFieldWidth, controlY,
                                             kAddressFieldWidth, controlHeight});

// o texto de estado e' medido depois dos controlos: e' a largura deles que diz
// onde o texto acaba. Com largura fixa o texto escrevia por baixo do campo, e o
// fundo opaco tapava o "PRONTO" que o 3.3.1 exige.
//
// a decisao vem das flags recem-calculadas e nao de isVisible(): nesta passagem a
// visibilidade ainda e' a da janela anterior, e o texto saltava ao redimensionar.
    const int statusLeft = displayBounds.getX() + 12 + 16 + 8;
    const auto hasSource = owner_.hasSource();
    const int statusRight =
        juce::jmax(statusLeft + 40,
                   (hasSource && statusFitsAddress_) ? address_.getX() - controlGap
                                                    : afterPlay);
    status_.setBounds(juce::Rectangle<int> {statusLeft, statusY, statusRight - statusLeft,
                                            kStatusHeight});

// rodape empilhado da direita para a esquerda, largura fixa por caixa. Medir cada
// caixa da margem esquerda a cada passo punha-as todas no mesmo intervalo: so' a
// ultima pintada aparecia.
//
// degrada em vez de espremer: ordem de sacrificio e' pico, depois entropia. POS,
// REG e VOICES nunca saem, que sao o que o painel existe para mostrar.
    constexpr int footerHeight {16};
    const int margin = displayBounds.getX() + 12;
    const int footerRight = displayBounds.getRight() - 12;

constexpr int kLedBoxWidth {12};
    constexpr int kBoxGap {8};
    constexpr int kVoicesBoxWidth {74};
    constexpr int kPeakWidth {86};
    constexpr int kTransportWidth {128};
    constexpr int kPositionWidth {116};
    constexpr int kRegionWidth {116};
    constexpr int kMinEntropyWidth {96};

// nunca saem: LED, numero de vozes, endereco da cabeca e intervalo da regiao — as
// leituras que respondem a "o que estou a ouvir".
    const int essential = kLedBoxWidth + kBoxGap + kVoicesBoxWidth + kPositionWidth + kRegionWidth;
    const auto room = footerRight - margin - essential;

// a prioridade esta' escrita na cadeia e nao numa frase ao lado: entropia,
// transporte, pico. A taxa saiu para o tooltip do titulo, e a cadeia encurtou.
//
// transporte antes do pico: e' a caixa que muda durante a sessao, e sem ela o
// display mostra uma barra a andar sem numero que a confirme.
    footerFitsEntropy_ = room >= kMinEntropyWidth;
    const auto afterEntropy = room - (footerFitsEntropy_ ? kMinEntropyWidth + kBoxGap : 0);
    footerFitsTransport_ = afterEntropy >= kTransportWidth;
    const auto afterTransport =
        afterEntropy - (footerFitsTransport_ ? kTransportWidth + kBoxGap : 0);
    footerFitsPeak_ = afterTransport >= kPeakWidth;

// da direita, so' o que cabe. Um salto de kBoxGap entre grupos: caixa omitida nao
// deixa intervalo vazio.
    int edge = footerRight;
    const auto put = [&edge, footerY](juce::Component& target, int width, bool visible) {
        if (!visible) {
            return;
        }
        target.setBounds(juce::Rectangle<int> {edge - width, footerY + 2, width, footerHeight});
        edge -= width + kBoxGap;
    };

    voicesLed_.setBounds(juce::Rectangle<int> {edge - kLedBoxWidth, footerY + 3, kLedBoxWidth, 12});
    put(voicesReadout_, kVoicesBoxWidth, true);
    put(positionReadout_, kPositionWidth, true);
    put(offsetReadout_, kRegionWidth, true);
    put(transportReadout_, kTransportWidth, footerFitsTransport_);
    put(peakReadout_, kPeakWidth, footerFitsPeak_);

// a entropia fica com o que sobra a' esquerda, colada ao indicador de vozes. E' a
// unica que cresce em vez de ter largura fixa: 0 a 8 bits por byte e' a leitura
// mais variavel.
    entropyReadout_.setBounds(juce::Rectangle<int> {margin, footerY + 2,
                                                     juce::jmax(0, edge - margin), footerHeight});

    updateFooterVisibility();

    area.removeFromTop(6);

// ---- selector de vista ----
//
// fica acima do painel e nao dentro do display: e' navegacao, e um instrumento
// dentro da coisa que instrumenta desaparece quando a coisa muda. Tres palavras
// sem cromo a' esquerda, zoom a' direita; a 480 as vistas colapsam para a activa
// e o zoom cai primeiro, porque a roda e as teclas cobrem.
//
// 22 px de altura: alvo perto dos 24 do 2.5.8, com 1/2/3 como caminho garantido.
// a aba activa leva peso forte e filete — o texto carrega o estado.
    {
        constexpr int kViewButtonWidth {46};
        constexpr int kViewButtonHeight {22};
        constexpr int kViewGap {4};
        const auto viewRow = area.removeFromTop(kViewButtonHeight + 2);

        struct View {
            juce::TextButton* button;
            ByteDisplay::ViewMode mode;
        };
        const View views[] = {
            {&viewWaveButton_, ByteDisplay::ViewMode::waveform},
            {&viewHexButton_, ByteDisplay::ViewMode::hex},
            {&viewEntropyButton_, ByteDisplay::ViewMode::entropy},
        };

        const int stripWidth = (kViewButtonWidth + kViewGap) * 3 - kViewGap;
        const auto active = grid_.mode();
        const bool fitsAll = viewRow.getWidth() >= stripWidth;

// quando nao cabem os tres, mostra so' o da vista activa. Um conjunto parcial e'
// pior que um botao unico: o que falta e' uma vista que o utilizador nao sabe que
// existe. Com um so', o selector continua verdadeiro, e as teclas 1/2/3 trocam
// sem depender de largura.
        int viewX = viewRow.getX();
        for (const auto& view : views) {
            const auto isActive = view.mode == active;
            view.button->setVisible(fitsAll || isActive);
            if (!fitsAll && !isActive) {
                continue;
            }
            view.button->setBounds(juce::Rectangle<int> {viewX, viewRow.getY() + 1,
                                                         kViewButtonWidth, kViewButtonHeight});
            viewX += kViewButtonWidth + kViewGap;
        }

// zoom a' direita: - e + de 30, FIT de 40. So' com folga para la' das vistas — a
// 480 nao ha', e a roda com as teclas cobrem o gesto.
        constexpr int kZoomSmall {30};
        constexpr int kZoomFit {40};
        const int zoomWidth = kZoomSmall + kViewGap + kZoomSmall + kViewGap + kZoomFit;
        const bool fitsZoom =
            fitsAll && viewRow.getWidth() >= stripWidth + kViewGap * 2 + zoomWidth;
        zoomOutButton_.setVisible(fitsZoom);
        zoomInButton_.setVisible(fitsZoom);
        zoomFitButton_.setVisible(fitsZoom);
        if (fitsZoom) {
            int zoomX = viewRow.getRight() - zoomWidth;
            zoomOutButton_.setBounds(juce::Rectangle<int> {zoomX, viewRow.getY() + 1,
                                                           kZoomSmall, kViewButtonHeight});
            zoomX += kZoomSmall + kViewGap;
            zoomInButton_.setBounds(juce::Rectangle<int> {zoomX, viewRow.getY() + 1,
                                                          kZoomSmall, kViewButtonHeight});
            zoomX += kZoomSmall + kViewGap;
            zoomFitButton_.setBounds(juce::Rectangle<int> {zoomX, viewRow.getY() + 1,
                                                           kZoomFit, kViewButtonHeight});
        }
    }

// ---- abas de modulo ----
//
// fileira propria entre a vista e os knobs, e nao titulo de modulo: o titulo antigo
// nomeava uma seccao unica e agora sao sete. Mesma altura e lingua das abas de
// vista; sete palavras de 52 px cabem a 480.
    {
        constexpr int kModuleButtonWidth {52};
        constexpr int kModuleButtonHeight {22};
        constexpr int kModuleGap {4};
        const auto moduleRow = area.removeFromTop(kModuleButtonHeight + 2);

        juce::TextButton* buttons[] = {
            &moduleGrainButton_, &moduleEnvelopeButton_, &moduleFilterButton_,
            &moduleModButton_, &moduleCloudButton_, &moduleOutButton_,
            &moduleKeysButton_,
        };
        int tabX = moduleRow.getX();
        for (auto* button : buttons) {
            button->setBounds(juce::Rectangle<int> {tabX, moduleRow.getY() + 1,
                                                    kModuleButtonWidth, kModuleButtonHeight});
            tabX += kModuleButtonWidth + kModuleGap;
        }
    }

// ---- painel de parametros ----
//
// so' os knobs do modulo ativo ocupam celulas; os escondidos nao entram na
// contagem. Uma linha continua — duas foi tentado e e' pior — e o diametro
// encolhe com a janela sem segundo layout.
    const auto knobArea = area.reduced(8, 2);
    const auto moduleIndex = static_cast<std::size_t>(activeModule_);
    const auto first = moduleFirstKnob_[moduleIndex];
    const auto count = moduleKnobCount_[moduleIndex];
    const int cellWidth = knobArea.getWidth() / static_cast<int>(count);

// o painel fecha o conteudo (titulo, knob, valor) e centra-se na vertical da
// area. Preencher a celula deixaria caixa cinza vazia debaixo do knob.
    constexpr int knobContentHeight {13 + 1 + 62 + 4 + 18};
    for (std::size_t k = 0; k < count; ++k) {
        const auto cell = juce::Rectangle<int> {knobArea.getX()
                                                     + (static_cast<int>(k) * cellWidth),
                                                 knobArea.getY(),
                                                 cellWidth,
                                                 knobArea.getHeight()};
        knobs_[first + k]->setBounds(cell.reduced(4).withHeight(
            juce::jmin(knobContentHeight, cell.getHeight() - 8)));
    }
}

void PluginEditor::timerCallback() {
// delta medido, nao assumido: assumir 1/60 faz a velocidade depender da taxa real,
// que no Windows vai de 60 a 144 Hz conforme o monitor.
//
// getMillisecondCounterHiRes e nao getMillisecondCounter: o inteiro tem 15 ms de
// resolucao e um quadro a 144 Hz dura 7 ms, por isso o delta sairia 0 ou 15 ms
// alternados, com um soluco a cada dois quadros.
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto delta = lastTick_ > 0.0 ? static_cast<float>((now - lastTick_) / 1000.0)
                                        : 1.0f / 60.0f;
    lastTick_ = now;

    grid_.tickAnimation(delta);

    refresh();
}

bool PluginEditor::keyPressed(const juce::KeyPress& key) {
// o display ja' consumiu o que lhe competia. Aqui so' chegam teclas de
// transporte, e so' quando o foco nao estava no display.
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        owner_.toggleTransport();
        refreshPlayButton();
        return true;
    }

// letras e digitos nao tem keycode no JUCE — ver isTypedCharacter em
// byte_display.h.
    if (isTypedCharacter(key, 'K')) {
// atalho de foco do teclado. Com o foco no teclado quem consome o 'K' e' o proprio
// componente, que o toca como nota — o atalho nunca rouba a nota.
        keyboard_.grabKeyboardFocus();
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

// zoom quando o foco nao esta' no display (num knob, por exemplo). No display e'
// ele que consome; aqui so' chegam as que sobraram.
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

void PluginEditor::refresh() {
// a thread de interface e' a dona da memoria de amostra: so' agora o ponteiro
// devolvido pela thread de audio pode ser liberado.
    owner_.releaseReturnedBuffers();

// os CCs sao aplicados aqui e nao na thread de audio. Ver
// PluginProcessor::applyPendingControllerChanges.
    owner_.applyPendingControllerChanges();

    const auto& info = owner_.sourceInfo();


    setIfChanged(fileName_, info.name.isNotEmpty() ? info.name : juce::String {"-"});
    setIfChanged(formatTag_, info.formatTag);
    setIfChanged(fileSize_,
                 info.sizeBytes > 0
                     ? juce::String {info.sizeBytes / (1024.0 * 1024.0), 2} + " MB"
                     : juce::String {});

// a visibilidade dos chips mora em updateHeaderVisibility(), que combina largura
// com conteudo. Escrever setVisible aqui desfazia a regra a cada tique.
    updateHeaderVisibility();

// sem ficheiro a grelha mostra "SEM BINARIO" e nao moldura vazia. A linha de
// estado nao muda de sitio com o material: alvo que salta e' pior que espaco
// constante.
    grid_.setVisible(owner_.hasSource());

    const auto hostStopped = owner_.hostTransportIsKnownToBeStopped();

    Led::State ledState = Led::State::off;
    juce::String message;

    if (owner_.lastError().isNotEmpty()) {
        ledState = Led::State::fault;
        message = "RECUSADO / " + owner_.lastError();
    } else if (!owner_.hasSource()) {
        message = "ARRASTE UM .EXE, .DLL OU .BIN PARA DENTRO DA JANELA";
    } else if (hostStopped) {
// o botao esta' desativado, e dizer porque evita que o utilizador ache que o
// plugin avariou. Sem host conhecido nao se diz nada — ver
// hostTransportIsKnownToBeStopped.
        ledState = Led::State::ready;
        message = "PRONTO / " + info.name + " / TRANSPORTE DO HOST PARADO";
    } else {
        ledState = Led::State::ready;
        message = "PRONTO / " + info.name;
    }

    setIfChanged(status_, message);
    status_.setColour(juce::Label::textColourId,
                      ledState == Led::State::fault ? palette::error : palette::textOnDark);

    statusLed_.setState(ledState);
    powerLed_.setState(ledState == Led::State::off ? Led::State::off : Led::State::ready);

// o botao e' atualizado depois da mensagem porque a mensagem depende do estado que
// ele produz.
    updateStatusVisibility();
    refreshPlayButton();
// o "*" de preset sujo chega por Listener; o desenho mora aqui, onde a comparacao
// evita o evento de acessibilidade a 60 Hz.
    refreshPresetName();

    refreshTelemetry(info);
}

// abas e grelha so' sao reenviadas quando o conteudo muda. Reconstruir o desenho a
// 60 Hz sem necessidade era trabalho inutil na thread de interface.
void PluginEditor::refreshTelemetry(const PluginProcessor::SourceInfo& info) {
    const auto signature = info.name + juce::String {info.sizeBytes};
    if (loadedSignature_ != signature) {
        loadedSignature_ = signature;

// o endereco do topo a zero e' decisao do componente: um ficheiro novo recomeca no
// inicio, senao o segundo abria a meio do primeiro.
        grid_.setSource(&owner_.sourceBytes(), static_cast<std::uint64_t>(info.sizeBytes));
        grid_.setSections(info.sections);
        address_.setHighestAddress(info.sizeBytes > 0
                                       ? static_cast<std::uint64_t>(info.sizeBytes) - 1
                                       : 0);
    }

    const auto range = owner_.byteRange();
    grid_.setRegion(range.start, range.end);

// o endereco e' o que o Processor tem, nao uma copia: showAddress nao emite o
// callback, por isso esta linha nao pode gerar ciclo com o campo.
    address_.showAddress(range.start);

// a cabeca vem do parametro. Se o host a moveu por automacao, o display segue o
// host e nao o contrario.
    const auto position = readPosition();
    const auto readHead = pe::byteForPosition(static_cast<double>(position), range);
    grid_.setReadHead(readHead);

// a ancora so' e' escrita quando o POSITION mudou. E' o que impede o editor de
// reancorar a cada quadro e prender a cabeca no ponto onde o knob estava.
    if (std::abs(position - lastAnchoredPosition_) > 1.0e-4f) {
        positionChangedSinceLastAnchor_ = true;
    }
    pushTransportAnchor();

// a telemetria vem do que a thread de audio ja' publicou, nao do transporte: o
// display mostra onde a cabeca de audio esta', e ler o nucleo de lado mostraria o
// ultimo bloco.
    const auto& telemetry = owner_.telemetry();
    const auto peak = telemetry.peakDb.load(std::memory_order_relaxed);
    const auto activeVoices = telemetry.activeVoices.load(std::memory_order_relaxed);
    const auto sounding = telemetry.sounding.load(std::memory_order_relaxed);

    setIfChanged(entropyReadout_,
                 owner_.hasSource() && !range.empty()
                     ? juce::String {entropyAtReadHead(readHead, range), 2} + " bits/byte"
                     : juce::String {});

// a cabeca em endereco. E' a leitura que responde a "o que estou a ouvir" e o que
// o clique na grelha acabou de escolher.
    if (owner_.hasSource() && !range.empty()) {
        setIfChanged(positionReadout_,
                     "POS 0x" + juce::String::formatted(
                                    "%08X", static_cast<unsigned long long>(readHead)));
    } else {
        setIfChanged(positionReadout_, juce::String {});
    }

// o rodape escreve o intervalo real da regiao, que e' o material que alimenta o
// motor. Hex porque o utilizador esta' dentro de um binario, e o resto das
// ferramentas tambem fala hex.
    if (range.empty()) {
        setIfChanged(offsetReadout_, juce::String {});
    } else {
        setIfChanged(offsetReadout_,
                     juce::String::formatted("REG 0x%08X-0x%08X",
                                             static_cast<unsigned long long>(range.start),
                                             static_cast<unsigned long long>(range.end)));
    }

    setIfChanged(peakReadout_,
                 peak < -0.5f ? juce::String {"PK -inf dB"}
                              : juce::String {"PK "} + juce::String {peak, 1} + " dB");

// leitura de transporte: cabeca de reproducao em endereco, instante da volta e
// duracao. E' a confirmacao textual da barra que a forma de onda vai mostrar.
    const auto transportPlaying = telemetry.playing.load(std::memory_order_relaxed);
    if (transportPlaying) {
        const auto transportFraction =
            telemetry.playheadFraction.load(std::memory_order_relaxed);
        const auto duration =
            telemetry.playDurationSeconds.load(std::memory_order_relaxed);
        const auto transportHead =
            pe::byteForPosition(static_cast<double>(transportFraction), range);
        const auto elapsed = duration * static_cast<double>(transportFraction);

// so' o que a thread de audio publicou, como o resto da telemetria.
    grid_.setPlayhead(telemetry.playheadFraction.load(std::memory_order_relaxed),
                      transportPlaying);
    grid_.setOutputLevel(peak);

    setIfChanged(transportReadout_,
                     juce::String::formatted("TP 0x%08X  %.1f/%.1fs",
                                             static_cast<unsigned long long>(transportHead),
                                             elapsed, duration));
    } else {
        setIfChanged(transportReadout_, juce::String {});
    }

// a taxa mora no tooltip do titulo: e' constante na sessao e nao precisa de faixa.
// escreve-se so' quando muda, pela mesma razao do setIfChanged.
    const auto rateText =
        juce::String {telemetry.sampleRate.load(std::memory_order_relaxed) / 1000.0f, 1}
        + " kHz";
    if (title_.getTooltip() != rateText) {
        title_.setTooltip(rateText);
    }
    setIfChanged(voicesReadout_,
                 juce::String {"VOICES "} + juce::String {activeVoices});

// sem material nao ha' entropia nem offset: caixa vazia le-se como defeito. A
// entropia tem regra propria em updateFooterVisibility, porque tambem depende da
// largura; as outras duas so' dependem do conteudo.
    updateFooterVisibility();
    positionReadout_.setVisible(positionReadout_.getText().isNotEmpty());
    offsetReadout_.setVisible(offsetReadout_.getText().isNotEmpty());

// o LED de vozes nunca e' a unica pista: o numero ao lado e' o mesmo dado em
// texto, que e' o que o 1.4.1 pede.
    voicesLed_.setState(sounding && activeVoices > 0 ? Led::State::ready : Led::State::off);

// graos na mesma leitura de 60 Hz. Quem publica e' a thread de audio, uma vez por
// bloco; o display mostra o ultimo bloco publicado.
    std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices> grains {};
    const auto grainCount = owner_.grainTelemetry().read(grains);
    grid_.setGrains(grains, grainCount);
}

float PluginEditor::readPosition() const {
// o parametro e' a fonte da verdade. Guardar a fracao no editor faz o display e o
// host falarem em zigue-zague: o host mexe, o display reescreve, o host volta.
//
// getValue() devolve a fracao normalizada. POSITION tem faixa 0 a 1, por isso a
// conversao nao muda nada aqui — mas fica escrita, que os outros parametros nao
// tem essa sorte.
    if (const auto* parameter = owner_.parameters().getParameter("position")) {
        return parameter->getNormalisableRange().convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
}

double PluginEditor::entropyAtReadHead(std::uint64_t address,
                                       const PluginProcessor::ByteRange& range) const {
// janela de 256 bytes a partir da cabeca, recortada pela regiao. Recortar e'
// obrigatorio: bytes fora do material nao sao material, e medi-los era o erro da
// curva do ficheiro inteiro ao lado de uma regiao estreita.
    constexpr std::size_t kWindow {256};

    const auto& bytes = owner_.sourceBytes();
    if (bytes.empty() || address < range.start || address >= range.end) {
        return 0.0;
    }

    const auto available = static_cast<std::uint64_t>(bytes.size()) - address;
    const auto length =
        juce::jmax<std::uint64_t>(1, juce::jmin<std::uint64_t>(kWindow, available));

    return entropy::shannonBitsPerByte(bytes.data(), bytes.size(),
                                       static_cast<std::size_t>(address),
                                       static_cast<std::size_t>(length));
}

void PluginEditor::activateByte(std::uint64_t address) {
// o clique pode cair fora da regiao; entao o material e' puxado para la' primeiro.
// o POSITION e' uma fracao da regiao, e escreve-lo antes de a regiao mudar
// apontaria para o sitio errado.
    const auto range = owner_.byteRange();
    if (address < range.start || address >= range.end) {
        moveRegionTo(address);
    }

    const auto updated = owner_.byteRange();

// setValueNotifyingHost e nao setValue: notifica o host, faz o knob seguir e entra
// no historico de automacao. E' o mesmo caminho dos CCs.
//
// getValue/setValue sao normalizados. POSITION tem faixa 0 a 1 e positionForByte
// devolve 0 a 1, por isso os espacos coincidem — nao coincidir seria erro
// silencioso em vez de visivel.
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
