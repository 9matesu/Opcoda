#include "plugin_editor.h"

#include "factory_presets.h"
#include "param_defaults.h"

#include "opcoda_core/entropy/shannon_entropy.h"
#include "opcoda_core/pe/byte_to_position.h"

#include <array>
#include <iterator>

namespace opcoda {
namespace {

// Header do mock: 64 px para o wordmark de 20 px, o subtitulo de 8 px e o
// medidor de saida caberem sem se encostarem.
constexpr int kHeaderHeight {64};

// O display cresce com a janela ate' 420 px. Abaixo de 200 a grelha perde as
// seis linhas de bytes e o ASCII; acima de 420 o painel de parametros fica
// esmagado sem ganho de leitura.
constexpr int kMinDisplayHeight {200};
constexpr int kMaxDisplayHeight {420};

// 720x640 e' o tamanho em que seis tabs e dez knobs ainda cabem numa linha.
// A largura antiga de 480 obrigava a esconder metade das vistas e cortava a
// coluna ASCII; o mock foi desenhado para 1040x820.
constexpr int kMinWidth {720};
constexpr int kMinHeight {640};
constexpr int kAddressFieldWidth {132};
constexpr int kPlayButtonWidth {60};
constexpr int kStatusHeight {16};
constexpr int kFooterHeight {16};

// Faixas novas do mock: linha de controlo (vistas + tabs), rack de macros e
// rodape de estado.
constexpr int kControlStripHeight {28};
constexpr int kMacroHeight {120};
constexpr int kFooterBandHeight {30};

// Teclado curto: 56 px dao uma tecla branca legivel sem roubar altura ao rack.
constexpr int kKeyboardHeight {56};

// Os dez parametros do GRAIN, todos do nucleo. A ordem e' a do mock, mas cada
// knob e' um parametro real: nao ha fachada, e a coluna de macros reaproveita
// os mesmos IDs em vez de inventar valores paralelos.
// Inteiro para uma caixa de valor.
//
// **juce::String {valor, 0} nao arredonda.** Em juce_String.cpp, writeDouble so
// aplica a precisao ao stream quando numDecPlaces > 0; com zero o `o << n` sai
// com a precisao por omissao, que e' 6 digitos significativos. O resultado e'
// "28.756" onde se queria "29", e Nobody reparava porque 28.756 e' um valor
// plausivel de densidade.
//
// O JUCE nao tem construtor de String a partir de float arredondado, portanto o
// arredondamento tem de ser explicito. Helper em vez de repetir roundToInt nas
// seis lambdas, para a razao ficar escrita uma vez.
juce::String asInteger(float value) {
    return juce::String {juce::roundToInt(value)};
}

const auto percent = [](float v) { return asInteger(v * 100.0f) + " %"; };
const auto seconds = [](float v) { return asInteger(v * 1000.0f) + " ms"; };
const auto cutoffText = [](float v) {
    return v >= 1000.0f ? juce::String {v / 1000.0f, 1} + " kHz" : asInteger(v) + " Hz";
};

// Os 41 parametros ja no codigo em sete modulos: os 25 da Tabela 8, os tres da
// F018, os tres da F019, os tres da F020, os tres da F021 e os quatro de
// filtro/curvas. Os IDs dos seis originais nao mudam — sessoes salvas
// referenciam por ID — so o titulo e o nome falado passam a ingles. As abas
// trocam a faixa visivel em vez de empilhar: a 720 de largura dez knobs ainda
// cabem numa linha, e sete fileiras empilhadas pediam uma janela que o
// kMinHeight nao promete.
const std::array<Knob::Spec, 10> kGrainSpecs {
    Knob::Spec {params::grain.id, "SIZE", "Grain size, in milliseconds",
                params::grain.value, [](float v) { return asInteger(v) + " ms"; }},
    Knob::Spec {params::density.id, "DENSITY", "Density, in grains per second",
                params::density.value, [](float v) { return asInteger(v) + " /s"; }},
    Knob::Spec {params::position.id, "POSITION", "Read position in the material",
                params::position.value, percent},
    Knob::Spec {params::spray.id, "SPRAY", "Spray, start position spread",
                params::spray.value, percent},
    // Estas duas ja estavam certas: com uma casa decimal o writeDouble entra no
    // if e formata em fixo. Ficam como estava para nao mexer no que funciona.
    Knob::Spec {params::pitch.id, "PITCH", "Tuning, in semitones", params::pitch.value,
                [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {params::window.id, "WINDOW", "Grain window shape", params::window.value, {},
                {"Hann", "Gaussian", "Hamming", "Blackman"}},
    Knob::Spec {params::pan.id, "PAN", "Stereo pan, left to right", params::pan.value,
                [](float v) {
                    return v == 0.0f ? juce::String {"C"}
                         : v < 0.0f  ? asInteger(-v * 100.0f) + " L"
                                     : asInteger(v * 100.0f) + " R";
                }},
    Knob::Spec {params::grainlevel.id, "LEVEL", "Per-grain level",
                params::grainlevel.value, percent},
    Knob::Spec {params::pitchrand.id, "P.RAND", "Random detune per grain, in semitones",
                params::pitchrand.value, [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {params::scanspeed.id, "SCAN", "Playhead scan speed, times realtime",
                params::scanspeed.value, [](float v) { return juce::String {v, 2} + "x"; }},
};

// F020: as tres curvas ficam ao lado dos tempos que moldam. O valor diz a forma
// e nao um numero: -100 % e' exponencial, 0 linear, +100 % logaritmica.
const auto curveText = [](float v) {
    const int pct = static_cast<int>(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
    return (pct == 0) ? juce::String {"LIN"} : juce::String {pct} + " %";
};

const std::array<Knob::Spec, 7> kEnvelopeSpecs {
    Knob::Spec {params::attack.id, "ATK", "Attack time", params::attack.value, seconds},
    Knob::Spec {params::decay.id, "DEC", "Decay time", params::decay.value, seconds},
    Knob::Spec {params::sustain.id, "SUS", "Sustain level", params::sustain.value, percent},
    Knob::Spec {params::release.id, "REL", "Release time", params::release.value, seconds},
    Knob::Spec {params::atkcurve.id, "ATK CRV", "Attack curve, exponential to logarithmic",
                params::atkcurve.value, curveText},
    Knob::Spec {params::deccurve.id, "DEC CRV", "Decay curve, exponential to logarithmic",
                params::deccurve.value, curveText},
    Knob::Spec {params::relcurve.id, "REL CRV", "Release curve, exponential to logarithmic",
                params::relcurve.value, curveText},
};

const std::array<Knob::Spec, 10> kFilterSpecs {
    Knob::Spec {params::f1type.id, "F1 TYPE", "Filter 1 type", params::f1type.value, {},
                {"Low-pass", "High-pass", "Band-pass", "Notch"}},
    Knob::Spec {params::f1cutoff.id, "F1 CUT", "Filter 1 cutoff frequency",
                params::f1cutoff.value, cutoffText},
    Knob::Spec {params::f1q.id, "F1 Q", "Filter 1 resonance", params::f1q.value,
                [](float v) { return juce::String {v, 2}; }},
    // F020: caracter analogico. O slope e' a inclinacao; o drive satura antes
    // do filtro; o keytrack segue a nota e o env abre com o envelope.
    Knob::Spec {params::f1slope.id, "F1 SLOPE", "Filter 1 slope", params::f1slope.value, {},
                {"12 dB", "24 dB"}},
    Knob::Spec {params::f1drive.id, "F1 DRIVE", "Filter 1 drive, in decibels",
                params::f1drive.value, [](float v) { return juce::String {v, 1} + " dB"; }},
    Knob::Spec {params::f1keytrack.id, "F1 KEY", "Filter 1 key tracking",
                params::f1keytrack.value, percent},
    Knob::Spec {params::f1env.id, "F1 ENV", "Filter 1 envelope amount", params::f1env.value,
                [](float v) {
                    const int pct = static_cast<int>(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
                    return juce::String {pct} + " %";
                }},
    Knob::Spec {params::f2type.id, "F2 TYPE", "Filter 2 type", params::f2type.value, {},
                {"Low-pass", "High-pass", "Band-pass", "Notch"}},
    Knob::Spec {params::f2cutoff.id, "F2 CUT", "Filter 2 cutoff frequency",
                params::f2cutoff.value, cutoffText},
    Knob::Spec {params::f2q.id, "F2 Q", "Filter 2 resonance", params::f2q.value,
                [](float v) { return juce::String {v, 2}; }},
};

const std::array<Knob::Spec, 7> kModSpecs {
    Knob::Spec {params::lforate.id, "RATE", "LFO rate, in hertz", params::lforate.value,
                [](float v) { return juce::String {v, 2} + " Hz"; }},
    Knob::Spec {params::lfodepth.id, "DEPTH", "LFO depth", params::lfodepth.value, percent},
    Knob::Spec {params::lfotarget.id, "TARGET", "LFO target parameter",
                params::lfotarget.value, {}, {"Pitch", "Density", "Cutoff", "Position"}},
    Knob::Spec {params::lfowave.id, "WAVE", "LFO waveform", params::lfowave.value, {},
                {"Sine", "Tri", "Saw", "Square", "S&H"}},
    // F019: em Sync o RATE deixa de mandar e a divisao passa a mandar. Os dois
    // ficam visiveis no mesmo modulo porque sao a mesma pergunta ("a que
    // velocidade"), respondida em duas unidades.
    Knob::Spec {params::lfosync.id, "SYNC", "LFO follows the host tempo",
                params::lfosync.value, {}, {"Free", "Sync"}},
    // O default e' o indice de "1/4", que e' o que o layout cria. Antes da
    // tabela unica, o editor dizia 0.0 e o duplo-clique repunha "1/1".
    Knob::Spec {params::lfodiv.id, "DIV", "LFO division when synced",
                params::lfodiv.value, {},
                {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/4.", "1/8.",
                 "1/4T", "1/8T", "1/16T"}},
    Knob::Spec {params::lfosteps.id, "STEPS", "LFO steps per cycle",
                params::lfosteps.value,
                [](float v) { return juce::String {static_cast<int>(v + 0.5f)}; }},
};

// F021: a nuvem e' o mesmo motor com tres graus de liberdade no nascimento do
// grao. Ficam num modulo proprio para o GRAIN nao passar de dez celulas e o
// knob encolher abaixo do que o dedo acerta.
const std::array<Knob::Spec, 3> kCloudSpecs {
    Knob::Spec {params::cloudsize.id, "WIDTH", "Temporal width of the grain cloud",
                params::cloudsize.value,
                [](float v) { return asInteger(v * 500.0f) + " ms"; }},
    Knob::Spec {params::pitchspread.id, "SPREAD",
                "Pitch spread across the cloud, in semitones", params::pitchspread.value,
                [](float v) { return juce::String {v, 1} + " st"; }},
    Knob::Spec {params::grainjit.id, "JITTER", "Grain duration jitter",
                params::grainjit.value, percent},
};

const std::array<Knob::Spec, 1> kOutSpecs {
    Knob::Spec {params::volume.id, "VOLUME", "Output volume, in decibels",
                params::volume.value,
                [](float v) {
                    return v <= -60.0f ? juce::String {"-inf"}
                                       : juce::String {v, 1} + " dB";
                }},
};

// F018: teclado e afinacao. A nota MIDI vira transposicao ja quantizada pela
// escala; o glide interpola entre notas. Sao tres parametros novos e nenhum
// dos 25 antigos mudou de sitio.
const std::array<Knob::Spec, 3> kKeysSpecs {
    Knob::Spec {params::scale.id, "SCALE", "Scale applied to incoming notes",
                params::scale.value, {},
                {"Chromatic", "Major", "Natural Minor", "Harmonic Minor", "Dorian",
                 "Phrygian", "Lydian", "Mixolydian", "Pentatonic Major",
                 "Pentatonic Minor", "Blues", "Whole Tone"}},
    Knob::Spec {params::root.id, "ROOT", "Scale root note", params::root.value, {},
                {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}},
    Knob::Spec {params::glide.id, "GLIDE", "Glide time between notes",
                params::glide.value, seconds},
};

// Rack de macros do mock. Os seis nomes sao os do painel visual, mas cada um
// aponta para um parametro real: nao ha parametro de fachada.
const std::array<Knob::Spec, 6> kMacroSpecs {
    Knob::Spec {params::grain.id, "BYTES", "Grain size, in milliseconds",
                params::grain.value, [](float v) { return asInteger(v) + " ms"; }},
    Knob::Spec {params::density.id, "DENSITY", "Density, in grains per second",
                params::density.value, [](float v) { return asInteger(v) + " /s"; }},
    Knob::Spec {params::position.id, "POSITION", "Read position in the material",
                params::position.value, percent},
    Knob::Spec {params::f1drive.id, "CRUSH", "Filter 1 drive, in decibels",
                params::f1drive.value, [](float v) { return juce::String {v, 1} + " dB"; }},
    Knob::Spec {params::cloudsize.id, "SPACE", "Temporal width of the grain cloud",
                params::cloudsize.value,
                [](float v) { return asInteger(v * 500.0f) + " ms"; }},
    Knob::Spec {params::pitchrand.id, "CORRUPT", "Random detune per grain, in semitones",
                params::pitchrand.value, [](float v) { return juce::String {v, 1} + " st"; }},
};

} // namespace

// Trocar o texto de um Label dispara um evento de acessibilidade. A 20 Hz isso
// vira ruido para quem usa leitor de tela, entao so se escreve quando muda.
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

    makePlainLabel(title_, "OPCODA", palette::amberText, assets::fonts::tech(22.0f));
    title_.setJustificationType(juce::Justification::centredLeft);
    makePlainLabel(subtitle_, "GRANULAR PE SYNTHESIZER", palette::textSub,
                   assets::fonts::mono(8.0f));
    subtitle_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitle_);
    // **O subt├¡tulo, o contador de vozes do cabe├ºalho, a dica de arrasto e o t├¡tulo do
    // m├│dulo sa├¡ram todos, e cada um tinha um duplo algures no ecr├ú.**
    //
    //   - "Granular Synthesizer" dizia o que o t├¡tulo "Opcoda" ja diz ao lado;
    //   - "VOICES 8" no cabe├ºalho duplicava o rodap├®, que escreve VOICES a cada
    //     tique com o n├║mero real e n├úo o m├íximo;
    //   - "DRAG & DROP BINARY" duplicava a linha de estado, que escreve a mesma
    //     instru├º├úo por extenso enquanto n├úo h├í material;
    //   - "1 - GRANULAR ENGINE" nomeava um m├│dulo que ├® o ├║nico, e o "1" prometia
    //     um segundo que nunca existiu.
    //
    // Nenhum deles levava informacao que nao estivesse escrita noutro sitio. O que
    // fica no cabe├ºalho ├® o LED, o nome, o LOAD, o nome do ficheiro, o formato e o
    // tamanho.
    makePlainLabel(status_, "", palette::textOnDark, assets::fonts::mono(9.0f));

    buildHeader();
    buildViewButtons();
    buildPresetSelector();
    buildParameterPanel();
    buildMacroBand();
    buildKeyboard();

    // A sujeira do preset chega por Listener da arvore: qualquer PARAM que mexa
    // fora de uma aplicacao de preset marca. O desenho do "*" mora no refresh,
    // onde a comparacao evita o evento de acessibilidade a 60 Hz.
    owner_.parameters().state.addListener(this);

    // A thread de interface e' a dona da memoria de amostra, entao e' ela que
    // libera o que a thread de audio ja devolveu.
    //
    // **60 Hz e nao 20 Hz, e a razao e' a animacao.** A 20 Hz cada quadro dura
    // 50 ms, e uma meia-vida de 60 ms da menos de dois quadros por meia-vida: isso
    // e' degrau e nao suavizacao. O custo da subida e' um repaint do display a mais
    // por segundo, e o desenho custa O(colunas) e nao O(bytes), porque a reducao
    // esta' memorizada.
    //
    // O texto nao vira ruido a 60 Hz porque setIfChanged so escreve quando o texto
    // muda, e trocar o texto de um Label dispara um evento de acessibilidade.
    startTimerHz(60);

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, 4096, 4096);
    setSize(1040, 820);

    // OpenGL primeiro, software como rede. attachTo devolve false quando a
    // placa ou o contexto recusam; nesse caso o JUCE pinta pelo caminho normal
    // e nao ha nada a fazer senao seguir.
    static_cast<void>(openGLContext_.attachTo(*this));

    refresh();
}

PluginEditor::~PluginEditor() {
    // O contexto GL tem de largar o componente antes de o componente morrer.
    openGLContext_.detach();

    // Primeiro os note-off, com o listener ainda ligado, senao fechar a janela
    // com uma tecla premida deixava a nota presa no motor ate ao proximo p├ónico.
    // allNotesOff corre na thread de interface e o listener empurra para a fila.
    keyboardState_.allNotesOff(1);
    keyboardState_.removeListener(this);
    owner_.parameters().state.removeListener(this);
}

void PluginEditor::buildPresetSelector() {
    // Setas sem cromo e nome como aba: a mesma lingua das abas de modulo, num
    // tamanho que cabe entre o wordmark e o LOAD. O nome e' o botao do menu ÔÇö
    // um botao separado "MENU" seria o quarto controle para uma so funcao.
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

    // Assincrono como o FileChooser: menu modal trava a message thread e o
    // host nao perdoa. O tick marca o preset ativo so quando limpo ÔÇö com "*"
    // nenhum item e' o som atual.
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
    // A guarda impede os 25 parameterChanged de sujarem o preset que acabaram
    // de aplicar: sem ela, todo preset nascia com "*".
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
    // So PARAM suja: sourcePath e a regiao mudam no ingest e nao dizem nada
    // sobre o som em relacao ao preset.
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
            // Nome primeiro, som depois: se o ficheiro falhar, o nome antigo
            // continua verdadeiro. O som so troca dentro do if.
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
            // Extensao forcada: o dialogo de save nao a poe sozinho e um preset
            // sem .opcoda nao abre por duplo-clique nem pelo filtro de load.
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
                            static_cast<juce::Component*>(&subtitle_),
                            static_cast<juce::Component*>(&vuMeter_),
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

    // A grelha escreve o POSITION. O duplo clique e a tecla A alternam entre a
    // regiao exacta e a secao PE mais proxima; o metodo do Processor e' quem
    // decide o sentido da alternancia, porque e' ele quem guarda a regiao
    // exacta anterior.
    grid_.onAddressActivated = [this](std::uint64_t address) { activateByte(address); };
    grid_.onSnapRequested = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };
    address_.onAddressEntered = [this](std::uint64_t address) { moveRegionTo(address); };

    // O display e' o componente que fica com o foco quando nao ha campo de endereco
    // com foco, e por isso e' ele que responde ao espaco e ao A. O editor responde
    // tambem, para o caso de o foco estar noutro sitio ÔÇö um botao, por exemplo.
    grid_.onToggleTransport = [this] {
        owner_.toggleTransport();
        refreshPlayButton();
    };
    grid_.onSnapRequestedFromKey = [this] { static_cast<void>(owner_.snapByteRangeToSection()); };

    // Tecla A e duplo-clique na grelha alinham a regiao; nao ha botao porque o
    // terceiro controle na linha de estado empurrava o texto para debaixo do
    // campo. A dica vive no help text do display e em docs/10 ÔÇö setHelpText e
    // nao setTooltip, pelo mesmo motivo do HexGrid: o Component so expoe ajuda
    // pelo AccessibilityHandler.
    grid_.setHelpText("Duplo-clique ou tecla A: alinha a regiao a secao PE mais proxima.");

    // Botao de transporte. E' um toggle, e a forma troca com o estado (ÔûÂ/Ôûá): e'
    // a segunda pista de estado, e a que funciona para quem nao distingue cores.
    // Sem texto e sem cromo ÔÇö so o glifo, desenhado pelo LookAndFeel.
    playButton_.getProperties().set("uiIcon", "transport");
    // Na linha de estado, sobre o display escuro: tinta clara. O chassis tambem
    // e' escuro agora, por isso a distincao uiOnDark e' redundante ÔÇö fica porque
    // o LookAndFeel ainda a le para o wash do hover.
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
        makePlainLabel(*readout, "", palette::textOnDark, assets::fonts::mono(9.0f));
        // Centrado como antes, quando havia caixa: a posicao do texto na faixa
        // nao muda, so some a moldura. Mudar o alinhamento junto seria duas
        // mudancas visuais numa.
        readout->setJustificationType(juce::Justification::centred);
        readout->setInterceptsMouseClicks(false, false);
        // A cor de texto por omissao do Label e' quase preta e desaparece sobre
        // o display escuro. textOnDark mantem a leitura acima de 4,5:1, que e'
        // o que o criterio 1.4.3 exige. O rodape vive no display, nao no chassis
        // claro, e por isso nao pode usar os tokens de superficie clara.
    }

    makePlainLabel(fileName_, "-", palette::textDark, assets::fonts::mono(9.5f));
    // Formato e tamanho sao metadado tecnico: texto secundario, para o nome da
    // fonte (o que importa) carregar a hierarquia sozinho.
    makePlainLabel(formatTag_, "", palette::textSub, assets::fonts::mono(8.5f));
    makePlainLabel(fileSize_, "", palette::textSub, assets::fonts::mono(8.5f));
    fileName_.setJustificationType(juce::Justification::centred);
    formatTag_.setJustificationType(juce::Justification::centred);
    fileSize_.setJustificationType(juce::Justification::centred);

    loadButton_.getProperties().set("uiIcon", "folder");
    loadButton_.setButtonText("");
    loadButton_.setTooltip("Escolher um binario para sintetizar (tecla L)");
    loadButton_.setName("Carregar binario");
    loadButton_.onClick = [this] { chooseFile(); };
}

// Um ponto so para a visibilidade do header, porque ha duas razoes independentes
// para uma peca nao estar la e nenhum dos lados pode prevailecer sobre o outro.
//
// Chamar isto de resized() e de refresh() e' o que mantem a verdade num sitio so:
// resized() sabe a largura, refresh() sabe o conteudo, e cada um escreve a sua
// metade. Se cada um chamasse setVisible por si, o timer de 20 Hz do refresh()
// voltava a mostrar um chip que nao cabe, e a sobreposicao voltava a aparecer.
void PluginEditor::buildViewButtons() {
    // Os rotulos sao curtos de proposito. O nome accessivel de cada um e' a
    // descricao completa, e o texto de ajuda explica a tecla: um rotulo de 15
    // caracteres num botao de 60 px sairia cortado, e um rotulo cortado e' pior do
    // que um rotulo curto com um nome accessivel por tras.
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
        // Aba sem cromo: so a palavra e o filete do estado. O desenho e' do
        // LookAndFeel, que trata "tab" sem fundo nem bisel.
        spec.button->getProperties().set("uiIcon", "tab");
    }

    // Os tres botoes nao estao em grupo: cada um e' independente e e' o LookAndFeel
    // que desenha o estado ligado. Um juce::ButtonGroup daria exclusao mutua de
    // graca, mas traria um listener que so serve para reescrever o que
    // updateViewButtons ja escreve a partir do modo.
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

    // Zoom clicavel do viewport. Nomes falados completos: "-" sozinho num
    // leitor de tela nao diz nada.
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

    // Duas condicoes, e ambas tem de ser verdade. A peca e' escondida quando nao
    // ha espaco OU quando nao ha conteudo ÔÇö e nunca se desenha uma moldura vazia,
    // que e' o mesmo motivo pelo qual os chips se escondem por vazio.
    fileSize_.setVisible(headerFitsSize_ && info.sizeBytes > 0);
    formatTag_.setVisible(headerFitsFormat_ && info.formatTag.isNotEmpty());
}

void PluginEditor::updateFooterVisibility() {
    // Mesma regra do header: a largura decide se a caixa existe, e o conteudo
    // decide se ha algo para escrever. Uma caixa a mais pequena e' pior do que
    // nenhuma, porque o texto sai cortado e nao se percebe o que e'.
    peakReadout_.setVisible(footerFitsPeak_);
    entropyReadout_.setVisible(footerFitsEntropy_ && entropyReadout_.getText().isNotEmpty());

    // A leitura de transporte so aparece a tocar, pela mesma razao que os chips
    // vazios do header se escondem: uma moldura vazia le-se como defeito. Durante
    // a reproducao e' a unica pista textual da cabeca, e sem ela a barra animada
    // seria a unica forma de saber o que se ouve ÔÇö o que o criterio 1.4.1 nao
    // permite.
    transportReadout_.setVisible(footerFitsTransport_ &&
                                 transportReadout_.getText().isNotEmpty());
}

void PluginEditor::updateStatusVisibility() {
    // O botao de reproducao nunca desaparece por falta de espaco: e' a unica
    // forma de ouvir o material sem teclado MIDI. O que se sacrifica e' o campo
    // de endereco, que escreve um sitio que o POSITION e o clique na grelha
    // escrevem tambem.
    playButton_.setVisible(owner_.hasSource());
    address_.setVisible(owner_.hasSource() && statusFitsAddress_);
}

void PluginEditor::refreshPlayButton() {
    const bool playing = owner_.isTransportPlaying();

    // A forma e' a pista de estado que sobrevive a quem nao ve cor: ÔûÂ parado,
    // Ôûá a tocar. O nome accessivel troca junto para o leitor de tela dizer o
    // que o glifo mostra.
    if ((playButton_.getName() == "Parar a reproducao") != playing) {
        playButton_.setName(playing ? "Parar a reproducao" : "Reproduzir a regiao");
    }

    if (playButton_.getToggleState() != playing) {
        playButton_.setToggleState(playing, juce::dontSendNotification);
    }

    // **Desativado e nao invisivel, e nao activo-a-fingir.** Sem material nao ha
    // regiao para percorrer; com uma regiao de um ou dois bytes nao ha posicao que
    // produza som, porque o motor precisa de `index + 1` e `hasRegion()` e' `>= 3`;
    // e com o host parado nao ha callback que produza o som. Um botao que aceita o
    // toque e nao faz nada e' pior do que um botao que recusa, porque o primeiro faz
    // o utilizador achar que o plugin avariou.
    const auto canPlay = owner_.hasSource() && owner_.hasPlayableRegion() &&
                         !owner_.hostTransportIsKnownToBeStopped();
    playButton_.setEnabled(canPlay);
}

void PluginEditor::pushTransportAnchor() {
    // A ancora so e' escrita quando o utilizador mexeu em POSITION. Escrever a cada
    // quadro faria a geracao do nucleo subir a cada quadro, e a cabeca nunca
    // passaria do ponto de ancoragem ÔÇö que e' o modo de falha que o teste
    // Transport.SeekIsAppliedOnceAndThenTheHeadAdvanced cobre do outro lado.
    if (!positionChangedSinceLastAnchor_) {
        return;
    }
    positionChangedSinceLastAnchor_ = false;
    lastAnchoredPosition_ = readPosition();
    owner_.seekTransportTo(lastAnchoredPosition_);
}

void PluginEditor::buildParameterPanel() {
    // Ordem de construcao e' ordem de modulo: GRAIN, ENVELOPE, FILTER, MOD,
    // OUT, KEYS. moduleFirstKnob_/moduleKnobCount_ guardam as fatias para a aba
    // mostrar sem procurar por ID.
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

void PluginEditor::buildMacroBand() {
    const std::array<juce::Colour, 6> swatches {
        palette::accent, palette::ok, palette::focusRing,
        palette::accentWarn, palette::accent, palette::error};

    for (std::size_t i = 0; i < kMacroSpecs.size(); ++i) {
        macroKnobs_.push_back(std::make_unique<Knob>(owner_.parameters(), kMacroSpecs[i]));
        macroKnobs_.back()->setSwatch(swatches[i]);
        addAndMakeVisible(*macroKnobs_.back());
    }
}

void PluginEditor::buildModuleTabs() {
    // Mesma lingua das abas de vista: palavra sem cromo, ativa em peso forte
    // com filete. As seis larguras somam 362 px com os vaos e cabem a 720;
    // GRAIN e OUT/KEYS sao as mais largas.
    const auto setup = [this](juce::TextButton& button, const char* title,
                              const char* spokenName, ParamModule module) {
        button.setButtonText(title);
        button.setName(spokenName);
        button.setTooltip(juce::String {spokenName});
        button.setClickingTogglesState(true);
        // Aba sem cromo, como as de vista: o mesmo "tab" do LookAndFeel, que
        // desenha a palavra e o filete do estado sem fundo nem bisel.
        button.getProperties().set("uiIcon", "tab");
        button.onClick = [this, module] { showModule(module); };
        addAndMakeVisible(button);
    };
    setup(moduleGrainButton_, "GRAIN", "Grain module", ParamModule::grain);
    setup(moduleEnvelopeButton_, "ENV", "Envelope module", ParamModule::envelope);
    setup(moduleFilterButton_, "FILTER", "Filter module", ParamModule::filter);
    setup(moduleModButton_, "MOD", "Modulation module", ParamModule::mod);
    setup(moduleCloudButton_, "CLOUD", "Grain cloud module", ParamModule::cloud);
    setup(moduleOutButton_, "OUT/KEYS", "Output, keyboard and tuning module",
          ParamModule::out);
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
}

void PluginEditor::showModule(ParamModule module) {
    activeModule_ = module;
    const auto index = static_cast<std::size_t>(module);
    const auto first = moduleFirstKnob_[index];

    // OUT e KEYS partilham a ultima aba: a fatia e' a soma das duas, e a ordem
    // de construcao (OUT depois KEYS) garante que sao contiguas.
    auto count = moduleKnobCount_[index];
    if (module == ParamModule::out) {
        count += moduleKnobCount_[static_cast<std::size_t>(ParamModule::keys)];
    }

    for (std::size_t i = 0; i < knobs_.size(); ++i) {
        knobs_[i]->setVisible(i >= first && i < first + count);
    }
    updateModuleTabs();
    resized();
}

void PluginEditor::buildKeyboard() {
    // Teclado horizontal com o mapa de teclas do JUCE por omissao
    // (A W S E D F T G Y H U J K ...), que e' exatamente o que o spec da F018
    // pede. O componente ja trata do rato, do foco por Tab e do desenho da tecla
    // premida; o que falta e' alguem escutar o estado e mandar a nota ao motor.
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

// Duas linhas e nao uma tabela: o listener do JUCE tem duas entradas, e passar
// um booleano por dentro obrigaria a uma terceira funcao so para isso.
void PluginEditor::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity) {
    owner_.pushKeyboardNote(midiNoteNumber, velocity, true);
}

void PluginEditor::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity) {
    owner_.pushKeyboardNote(midiNoteNumber, velocity, false);
}

void PluginEditor::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Metal escovado: gradiente vertical curto e linhas de 1 px quase
    // invisiveis. E' o que separa a superficie de um retangulo cinzento.
    g.setGradientFill(juce::ColourGradient {
        palette::metalTop, bounds.getTopLeft(),
        palette::metalBottom, bounds.getBottomLeft(), false});
    g.fillRect(bounds);

    g.setColour(juce::Colours::black.withAlpha(0.025f));
    for (float y = 2.0f; y < bounds.getHeight(); y += 4.0f) {
        g.drawHorizontalLine(juce::roundToInt(y), 0.0f, bounds.getWidth());
    }

    // Header: metal mais claro que o corpo, com costura e sombra.
    const auto header = bounds.withHeight(static_cast<float>(kHeaderHeight));
    g.setGradientFill(juce::ColourGradient {
        palette::metalTop, header.getTopLeft(),
        palette::header, header.getBottomLeft(), false});
    g.fillRect(header);
    g.setColour(palette::metalBorder);
    g.drawHorizontalLine(kHeaderHeight, 0.0f, bounds.getWidth());
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.drawHorizontalLine(kHeaderHeight + 1, 0.0f, bounds.getWidth());
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawHorizontalLine(0, 0.0f, bounds.getWidth());

    // Moldura do display: o painel escuro assenta numa caixa de chassi, com
    // bisel de metal. Sem isto o ecra parece um buraco colado ao painel.
    if (display_.getBounds().getWidth() > 0) {
        const auto bezel = display_.getBounds().toFloat().expanded(3.0f);
        g.setColour(palette::chassis);
        g.fillRoundedRectangle(bezel, 4.0f);
        g.setColour(palette::metalBorder);
        g.drawRoundedRectangle(bezel, 4.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawHorizontalLine(juce::roundToInt(bezel.getY() + 1.0f),
                             bezel.getX() + 4.0f, bezel.getRight() - 4.0f);
    }

    // Faixa do rack de macros: metal ligeiramente mais escuro, para os seis
    // knobs se lerem como um grupo e nao como parte do painel de parametros.
    auto content = getLocalBounds();
    content.removeFromBottom(kFooterBandHeight);
    content.removeFromBottom(kKeyboardHeight);
    content.removeFromTop(kHeaderHeight);
    const auto displayHeight = display_.getHeight();
    if (displayHeight > 0) {
        content.removeFromTop(displayHeight);
    }
    content.removeFromTop(kControlStripHeight);
    if (content.getHeight() >= kMacroHeight) {
        const auto macro = content.removeFromTop(kMacroHeight).toFloat();
        g.setColour(palette::metalMiddle);
        g.fillRect(macro);
        g.setColour(palette::metalBorder);
        g.drawHorizontalLine(juce::roundToInt(macro.getY()), macro.getX(),
                             macro.getRight());
        g.drawHorizontalLine(juce::roundToInt(macro.getBottom()) - 1,
                             macro.getX(), macro.getRight());
    }

    // Painel de parametros: rebaixo no metal, com borda fina. Os knobs do
    // modulo ativo vivem aqui e nao no rack de macros.
    if (content.getHeight() > 0) {
        const auto panel = content.toFloat().reduced(10.0f, 6.0f);
        g.setColour(palette::metalBase.darker(0.025f));
        g.fillRoundedRectangle(panel, 4.0f);
        g.setColour(palette::metalBorder);
        g.drawRoundedRectangle(panel, 4.0f, 1.0f);
    }

    // Rodape: faixa escura de estado, a mesma lingua do display.
    const auto footer = bounds.removeFromBottom(static_cast<float>(kFooterBandHeight));
    g.setColour(palette::footerSurface);
    g.fillRect(footer);
    g.setColour(palette::metalBorder);
    g.drawHorizontalLine(juce::roundToInt(footer.getY()), 0.0f, bounds.getWidth());

    g.setColour(palette::textOnDarkSub);
    g.setFont(assets::fonts::mono(8.0f));
    g.drawText("PE / GRAIN ENGINE", footer.reduced(14.0f, 0.0f),
               juce::Justification::centredRight, false);

    g.setColour(palette::amberText);
    g.setFont(assets::fonts::tech(10.0f));
    g.drawText("OPCODA", footer.reduced(14.0f, 0.0f),
               juce::Justification::centredLeft, false);

    // "OUT" ao lado do medidor. E' rotulo de unidade, nao um segundo medidor:
    // o nucleo e' mono e as duas barras mostram o mesmo pico.
    if (vuMeter_.isVisible()) {
        g.setColour(palette::textSub);
        g.setFont(assets::fonts::mono(8.0f));
        g.drawText("OUT", vuMeter_.getBounds().withX(vuMeter_.getX() - 28).withWidth(24),
                   juce::Justification::centredRight, false);
    }
}

void PluginEditor::resized() {
    auto area = getLocalBounds();

    // Rodape e teclado sao retirados primeiro. O resto do layout nunca cresce
    // para cima deles, e a altura que sobra e' a que o display e o rack podem
    // dividir.
    const auto footer = area.removeFromBottom(kFooterBandHeight);
    static_cast<void>(footer);
    keyboard_.setBounds(area.removeFromBottom(kKeyboardHeight).reduced(14, 4));

    // ---- header ----
    const auto header = area.removeFromTop(kHeaderHeight).reduced(14, 0);
    int x = header.getX();

    constexpr int kLedWidth {16};
    constexpr int kIdentityWidth {156};
    constexpr int kIdentityGap {14};
    constexpr int kPresetArrow {22};
    constexpr int kPresetName {96};
    constexpr int kPresetGap {2};
    constexpr int kLoadWidth {40};
    constexpr int kNameGap {8};
    constexpr int kMinNameWidth {96};

    powerLed_.setBounds(juce::Rectangle<int> {x, header.getY(), kLedWidth,
                                               header.getHeight()}
                            .withSizeKeepingCentre(14, 14));
    x += kLedWidth + 8;

    title_.setBounds(juce::Rectangle<int> {x, header.getY() + 10, kIdentityWidth, 26});
    subtitle_.setBounds(juce::Rectangle<int> {x, header.getY() + 36, kIdentityWidth, 14});
    x += kIdentityWidth + kIdentityGap;

    presetPrevButton_.setBounds(
        juce::Rectangle<int> {x, header.getY() + 19, kPresetArrow, 26});
    x += kPresetArrow + kPresetGap;
    presetNameButton_.setBounds(
        juce::Rectangle<int> {x, header.getY() + 19, kPresetName, 26});
    x += kPresetName + kPresetGap;
    presetNextButton_.setBounds(
        juce::Rectangle<int> {x, header.getY() + 19, kPresetArrow, 26});
    x += kPresetArrow + kIdentityGap;

    loadButton_.setBounds(juce::Rectangle<int> {x, header.getY() + 18, kLoadWidth, 28});
    x += kLoadWidth + kNameGap;

    const int leftLimit = x + kMinNameWidth;
    int rightEdge = header.getRight();

    // Medidor de saida no canto direito. O "OUT" e' pintado no paint().
    constexpr int kMeterWidth {74};
    vuMeter_.setBounds(juce::Rectangle<int> {rightEdge - kMeterWidth,
                                              header.getY() + 18, kMeterWidth, 28});
    rightEdge -= kMeterWidth + 16;

    constexpr int kFormatWidth {74};
    constexpr int kSizeWidth {58};
    constexpr int kChipGap {6};

    headerFitsSize_ = rightEdge - kSizeWidth >= leftLimit;
    if (headerFitsSize_) {
        fileSize_.setBounds(juce::Rectangle<int> {rightEdge - kSizeWidth,
                                                   header.getY() + 22, kSizeWidth, 20});
        rightEdge -= kSizeWidth + kChipGap;
    }

    headerFitsFormat_ = rightEdge - kFormatWidth >= leftLimit;
    if (headerFitsFormat_) {
        formatTag_.setBounds(juce::Rectangle<int> {rightEdge - kFormatWidth,
                                                    header.getY() + 22, kFormatWidth, 20});
        rightEdge -= kFormatWidth + kChipGap;
    }

    fileName_.setBounds(juce::Rectangle<int> {x, header.getY() + 20,
                                              juce::jmax(kMinNameWidth, rightEdge - x), 24});
    updateHeaderVisibility();

    // ---- display ----
    const auto displayHeight = juce::jlimit(kMinDisplayHeight, kMaxDisplayHeight,
                                             area.getHeight() * 2 / 5);
    const auto displayBounds = area.removeFromTop(displayHeight);
    display_.setBounds(displayBounds);

    const auto inner = displayBounds.reduced(8, 5);
    const auto statusY = inner.getY();
    statusLed_.setBounds(juce::Rectangle<int> {inner.getX(), statusY, 16, kStatusHeight}
                             .withSizeKeepingCentre(14, kStatusHeight));

    const int controlY = statusY;
    const int controlHeight = kStatusHeight;
    const int controlGap = 6;

    playButton_.setBounds(juce::Rectangle<int> {inner.getRight() - kPlayButtonWidth,
                                                controlY, kPlayButtonWidth, controlHeight});
    const int afterPlay = playButton_.getX() - controlGap;
    const int statusLeft = inner.getX() + 22;

    statusFitsAddress_ = afterPlay - kAddressFieldWidth >= statusLeft + 140;
    if (statusFitsAddress_) {
        address_.setBounds(juce::Rectangle<int> {afterPlay - kAddressFieldWidth,
                                                 controlY, kAddressFieldWidth,
                                                 controlHeight});
    }

    const auto hasSource = owner_.hasSource();
    const int statusRight = (hasSource && statusFitsAddress_)
                                ? address_.getX() - controlGap
                                : afterPlay;
    status_.setBounds(juce::Rectangle<int> {statusLeft, statusY,
                                            juce::jmax(40, statusRight - statusLeft),
                                            kStatusHeight});

    const auto footerY = inner.getBottom() - kFooterHeight;
    const auto gridTop = statusY + kStatusHeight + 2;
    grid_.setBounds(juce::Rectangle<int> {inner.getX(), gridTop, inner.getWidth(),
                                          juce::jmax(0, footerY - gridTop)});

    // ---- leituras do display ----
    constexpr int kBoxGap {8};
    constexpr int kLedBoxWidth {12};
    constexpr int kVoicesBoxWidth {74};
    constexpr int kPeakWidth {86};
    constexpr int kTransportWidth {128};
    constexpr int kPositionWidth {116};
    constexpr int kRegionWidth {116};
    constexpr int kMinEntropyWidth {96};

    const int margin = inner.getX();
    const int footerRight = inner.getRight();
    const int essential =
        kLedBoxWidth + kBoxGap + kVoicesBoxWidth + kPositionWidth + kRegionWidth;
    const auto room = footerRight - margin - essential;

    footerFitsEntropy_ = room >= kMinEntropyWidth;
    const auto afterEntropy = room - (footerFitsEntropy_ ? kMinEntropyWidth + kBoxGap : 0);
    footerFitsTransport_ = afterEntropy >= kTransportWidth;
    const auto afterTransport =
        afterEntropy - (footerFitsTransport_ ? kTransportWidth + kBoxGap : 0);
    footerFitsPeak_ = afterTransport >= kPeakWidth;

    int edge = footerRight;
    const auto put = [&edge, footerY](juce::Component& target, int width, bool visible) {
        if (!visible) {
            return;
        }
        target.setBounds(juce::Rectangle<int> {edge - width, footerY, width, kFooterHeight});
        edge -= width + kBoxGap;
    };

    voicesLed_.setBounds(juce::Rectangle<int> {edge - kLedBoxWidth, footerY + 2,
                                               kLedBoxWidth, 12});
    put(voicesReadout_, kVoicesBoxWidth, true);
    put(positionReadout_, kPositionWidth, true);
    put(offsetReadout_, kRegionWidth, true);
    put(transportReadout_, kTransportWidth, footerFitsTransport_);
    put(peakReadout_, kPeakWidth, footerFitsPeak_);

    entropyReadout_.setBounds(juce::Rectangle<int> {margin, footerY,
                                                     juce::jmax(0, edge - margin),
                                                     kFooterHeight});
    updateFooterVisibility();

    // ---- linha de controlo: vistas, zoom e abas ----
    const auto controlRow = area.removeFromTop(kControlStripHeight).reduced(12, 3);

    constexpr int kViewButtonWidth {46};
    constexpr int kViewGap {4};
    int viewX = controlRow.getX();
    for (auto* button : {&viewWaveButton_, &viewHexButton_, &viewEntropyButton_}) {
        button->setBounds(juce::Rectangle<int> {viewX, controlRow.getY(),
                                                kViewButtonWidth, controlRow.getHeight()});
        viewX += kViewButtonWidth + kViewGap;
    }

    constexpr int kZoomSmall {30};
    constexpr int kZoomFit {40};
    const int zoomWidth = kZoomSmall + kViewGap + kZoomSmall + kViewGap + kZoomFit;
    const auto tabWidths = std::array<int, 6> {56, 44, 58, 44, 56, 84};
    const int tabsWidth =
        tabWidths[0] + tabWidths[1] + tabWidths[2] + tabWidths[3] +
        tabWidths[4] + tabWidths[5] + kViewGap * 5;
    const int tabsX = controlRow.getRight() - tabsWidth;
    const bool fitsZoom = viewX + kViewGap * 2 + zoomWidth <= tabsX - 12;

    zoomOutButton_.setVisible(fitsZoom);
    zoomInButton_.setVisible(fitsZoom);
    zoomFitButton_.setVisible(fitsZoom);
    if (fitsZoom) {
        int zoomX = viewX + kViewGap * 2;
        zoomOutButton_.setBounds(juce::Rectangle<int> {zoomX, controlRow.getY(),
                                                       kZoomSmall, controlRow.getHeight()});
        zoomX += kZoomSmall + kViewGap;
        zoomInButton_.setBounds(juce::Rectangle<int> {zoomX, controlRow.getY(),
                                                      kZoomSmall, controlRow.getHeight()});
        zoomX += kZoomSmall + kViewGap;
        zoomFitButton_.setBounds(juce::Rectangle<int> {zoomX, controlRow.getY(),
                                                       kZoomFit, controlRow.getHeight()});
    }

    juce::TextButton* tabs[] = {&moduleGrainButton_, &moduleEnvelopeButton_,
                                &moduleFilterButton_, &moduleModButton_,
                                &moduleCloudButton_, &moduleOutButton_};
    int tabX = tabsX;
    for (std::size_t i = 0; i < std::size(tabs); ++i) {
        tabs[i]->setBounds(juce::Rectangle<int> {tabX, controlRow.getY(),
                                                 tabWidths[i], controlRow.getHeight()});
        tabX += tabWidths[i] + kViewGap;
    }

    // ---- rack de macros ----
    // showModule() chama resized() durante a construcao, antes de o rack
    // existir; sem a guarda a divisao por zero matava o standalone no arranque.
    const auto macroArea = area.removeFromTop(kMacroHeight).reduced(14, 6);
    if (macroKnobs_.empty() || macroArea.getWidth() <= 0) {
        updateStatusVisibility();
        return;
    }
    const int macroWidth = macroArea.getWidth() / static_cast<int>(macroKnobs_.size());
    for (std::size_t i = 0; i < macroKnobs_.size(); ++i) {
        const auto cell = juce::Rectangle<int> {
            macroArea.getX() + static_cast<int>(i) * macroWidth, macroArea.getY(),
            macroWidth, macroArea.getHeight()};
        macroKnobs_[i]->setBounds(cell.reduced(4, 0).withHeight(
            juce::jmin(108, cell.getHeight())));
    }

    // ---- painel de parametros do modulo ativo ----
    const auto knobArea = area.reduced(18, 8);
    const auto moduleIndex = static_cast<std::size_t>(activeModule_);
    const auto first = moduleFirstKnob_[moduleIndex];
    auto count = moduleKnobCount_[moduleIndex];
    if (activeModule_ == ParamModule::out) {
        count += moduleKnobCount_[static_cast<std::size_t>(ParamModule::keys)];
    }

    if (count == 0 || knobArea.getWidth() <= 0) {
        updateStatusVisibility();
        return;
    }

    const int cellWidth = knobArea.getWidth() / static_cast<int>(count);
    constexpr int knobContentHeight {98};
    for (std::size_t k = 0; k < count; ++k) {
        const auto cell = juce::Rectangle<int> {
            knobArea.getX() + static_cast<int>(k) * cellWidth, knobArea.getY(),
            cellWidth, knobArea.getHeight()};
        auto content = cell.reduced(3, 0).withHeight(
            juce::jmin(knobContentHeight, juce::jmax(0, cell.getHeight())));
        content.setY(cell.getCentreY() - content.getHeight() / 2);
        knobs_[first + k]->setBounds(content);
    }

    updateStatusVisibility();
}

void PluginEditor::timerCallback() {
    // O delta e' medido e nao assumido. Assumir 1/60 numa animacao que depende do
    // tempo deixa a velocidade depender da taxa de quadros real, que no Windows
    // varia entre 60 e 144 Hz conforme a ligacao do monitor.
    //
    // `getMillisecondCounterHiRes` e nao `getMillisecondCounter`: o contador
    // inteiro tem 15 ms de resolucao a 144 Hz, e um quadro de 144 Hz dura 7 ms. Com
    // o contador inteiro o delta seria 0 ou 15 ms alternadamente, e a animacao
    // dava um solu├ºo a cada dois quadros.
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto delta = lastTick_ > 0.0 ? static_cast<float>((now - lastTick_) / 1000.0)
                                        : 1.0f / 60.0f;
    lastTick_ = now;

    grid_.tickAnimation(delta);

    refresh();
}

bool PluginEditor::keyPressed(const juce::KeyPress& key) {
    // O display ja consumiu o que lhe competia. Aqui so chegam as teclas de
    // transporte, e so quando o foco nao estava no display.
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        owner_.toggleTransport();
        refreshPlayButton();
        return true;
    }

    // Letras e digitos nao tem codigo no JUCE; ver isTypedCharacter em
    // byte_display.h.
    if (isTypedCharacter(key, 'K')) {
        // Atalho para o foco do teclado. Com o foco no teclado quem consome o 'K'
        // e' o proprio componente, que o toca como nota ÔÇö por isso o atalho nunca
        // rouba a nota a quem esta' a tocar.
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

    // Zoom quando o foco nao esta no display (num knob, por exemplo). No
    // display e' ele que consome; aqui so chegam as que sobraram.
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
    // A thread de interface e' a dona da memoria de amostra: so agora o ponteiro
    // devolvido pela thread de audio pode ser liberado com seguranca.
    owner_.releaseReturnedBuffers();

    // Os CCs recebidos sao aplicados aqui, e nao na thread de audio. Ver
    // PluginProcessor::applyPendingControllerChanges para o porque.
    owner_.applyPendingControllerChanges();

    const auto& info = owner_.sourceInfo();


    setIfChanged(fileName_, info.name.isNotEmpty() ? info.name : juce::String {"-"});
    setIfChanged(formatTag_, info.formatTag);
    setIfChanged(fileSize_,
                 info.sizeBytes > 0
                     ? juce::String {info.sizeBytes / (1024.0 * 1024.0), 2} + " MB"
                     : juce::String {});

    // A visibilidade dos chips do header mora em updateHeaderVisibility(), que
    // combina a largura com o conteudo. Escrever setVisible aqui desfazia a
    // regra de largura ao fim de um tique do timer.
    updateHeaderVisibility();

    // Sem ficheiro a grelha mostra "SEM BINARIO" e nao uma moldura vazia. A linha
    // de estado nao muda de sitio com o material a carregar: um alvo que salta
    // quando se carrega um binario e' pior do que um espaco constante.
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
        // O botao esta' desativado, e dizer porqu├¬ e' o que evita que o utilizador
        // ache que o plugin avariou. Sem host nenhum esta' desligado nao se sabe,
        // e nao se diz nada ÔÇö ver hostTransportIsKnownToBeStopped.
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

    // O botao e' atualizado depois da mensagem porque a mensagem e' que depende do
    // estado que ele produz.
    updateStatusVisibility();
    refreshPlayButton();
    // O "*" de preset sujo chega por Listener e o desenho mora aqui, onde a
    // comparacao evita o evento de acessibilidade a 60 Hz.
    refreshPresetName();

    refreshTelemetry(info);
}

// Abas e grelha so sao reenviadas quando o conteudo muda. Reconstruir o desenho
// da grelha a 20 Hz sem necessidade seria trabalho inutil na thread de interface.
void PluginEditor::refreshTelemetry(const PluginProcessor::SourceInfo& info) {
    const auto signature = info.name + juce::String {info.sizeBytes};
    if (loadedSignature_ != signature) {
        loadedSignature_ = signature;

        // O endereco do topo a zero e' decis├úo do proprio componente: um
        // ficheiro novo recomeca no inicio, senao o segundo abria a meio do
        // primeiro porque a viewport so e' nossa.
        grid_.setSource(&owner_.sourceBytes(), static_cast<std::uint64_t>(info.sizeBytes));
        grid_.setSections(info.sections);
        address_.setHighestAddress(info.sizeBytes > 0
                                       ? static_cast<std::uint64_t>(info.sizeBytes) - 1
                                       : 0);
    }

    const auto range = owner_.byteRange();
    grid_.setRegion(range.start, range.end);

    // O endereco e' o que o Processor tem, e nao uma copia: showAddress nao emite
    // o callback, entao esta linha nao pode gerar um ciclo com o campo.
    address_.showAddress(range.start);

    // A cabeca de leitura vem do parametro. Se o host a moveu por automacao, o
    // display segue o host e nao o contrario.
    const auto position = readPosition();
    const auto readHead = pe::byteForPosition(static_cast<double>(position), range);
    grid_.setReadHead(readHead);

    // A ancora de busca do transporte so e' escrita quando o POSITION mudou. E' o
    // que impede o editor de reancorar a cada quadro, o que prenderia a cabeca de
    // reproducao no ponto onde o knob estava.
    if (std::abs(position - lastAnchoredPosition_) > 1.0e-4f) {
        positionChangedSinceLastAnchor_ = true;
    }
    pushTransportAnchor();

    // A telemetria de reproducao e' lida do que a thread de audio ja publicou, e
    // nao do transporte: o display mostra onde a cabeca de audio esta', e ler o
    // nucleo de lado mostraria o ultimo bloco, nao este.
    const auto& telemetry = owner_.telemetry();
    const auto peak = telemetry.peakDb.load(std::memory_order_relaxed);
    vuMeter_.setLevelDb(peak);
    const auto activeVoices = telemetry.activeVoices.load(std::memory_order_relaxed);
    const auto sounding = telemetry.sounding.load(std::memory_order_relaxed);

    setIfChanged(entropyReadout_,
                 owner_.hasSource() && !range.empty()
                     ? juce::String {entropyAtReadHead(readHead, range), 2} + " bits/byte"
                     : juce::String {});

    // A cabeca de leitura em endereco. E' a leitura que responde a pergunta "o
    // que estou a ouvir", e o que o clique na grelha acabou de escolher.
    if (owner_.hasSource() && !range.empty()) {
        setIfChanged(positionReadout_,
                     "POS 0x" + juce::String::formatted(
                                    "%08X", static_cast<unsigned long long>(readHead)));
    } else {
        setIfChanged(positionReadout_, juce::String {});
    }

    // O rodape escreve o intervalo real da regiao, que e' o material que alimenta
    // o motor. E' hex porque o utilizador esta' dentro de um binario, e o offset
    // em hexadecimal e' o que aparece no resto das ferramentas.
    if (range.empty()) {
        setIfChanged(offsetReadout_, juce::String {});
    } else {
        setIfChanged(offsetReadout_,
                     juce::String::formatted("REG 0x%08X-0x%08X",
                                             static_cast<unsigned long long>(range.start),
                                             static_cast<unsigned long long>(range.end)));
    }

    setIfChanged(peakReadout_,
                 peak <= kVuFloorDb ? juce::String {"PK -inf dB"}
                              : juce::String {"PK "} + juce::String {peak, 1} + " dB");

    // A leitura de transporte: posicao da cabeca de reproducao em endereco, o
    // instante da volta e a duracao de uma volta. E' a confirmacao textual da barra
    // que a forma de onda vai mostrar.
    const auto transportPlaying = telemetry.playing.load(std::memory_order_relaxed);
    if (transportPlaying) {
        const auto transportFraction =
            telemetry.playheadFraction.load(std::memory_order_relaxed);
        const auto duration =
            telemetry.playDurationSeconds.load(std::memory_order_relaxed);
        const auto transportHead =
            pe::byteForPosition(static_cast<double>(transportFraction), range);
        const auto elapsed = duration * static_cast<double>(transportFraction);

// A leitura de transporte so e' lida do que a thread de audio ja publicou, e
    // nao do transporte: o display mostra onde a cabeca de audio esta', e ler o
    // nucleo de lado mostraria o ultimo bloco, nao este.
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

    // A taxa mora no tooltip do titulo: e' constante na sessao e nao precisa de
    // faixa propria. Quem procura um numero fixo le sem pressa, e a faixa do
    // rodape fica para o que muda. So escreve quando muda, pela mesma razao do
    // setIfChanged nos Labels.
    const auto rateText =
        juce::String {telemetry.sampleRate.load(std::memory_order_relaxed) / 1000.0f, 1}
        + " kHz";
    if (title_.getTooltip() != rateText) {
        title_.setTooltip(rateText);
    }
    setIfChanged(voicesReadout_,
                 juce::String {"VOICES "} + juce::String {activeVoices});

    // Sem material nao ha entropia nem offset para ler. Uma caixa vazia e' um
    // retangulo com moldura e nada dentro, que se le como defeito, e' o mesmo
    // motivo que esconde os chips vazios do cabecalho.
    //
    // A entropia tem a sua regra propria, em updateFooterVisibility, porque
    // depende tambem da largura. As outras duas so dependem do conteudo, e sao
    // sempre largas o suficiente para escrever o que tem.
    updateFooterVisibility();
    positionReadout_.setVisible(positionReadout_.getText().isNotEmpty());
    offsetReadout_.setVisible(offsetReadout_.getText().isNotEmpty());

    // O LED de vozes nunca e' a unica pista: o numero ao lado e' o mesmo dado
    // em texto, que e' o que o criterio 1.4.1 do WCAG pede.
    voicesLed_.setState(sounding && activeVoices > 0 ? Led::State::ready : Led::State::off);

    // Os graos, na mesma leitura de 60 Hz. E' leitura de lado, tal como o resto da
    // telemetria: quem publica e' a thread de audio, uma vez por bloco, e o
    // display mostra o ultimo bloco publicado e nao um estado inventado aqui.
    std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices> grains {};
    const auto grainCount = owner_.grainTelemetry().read(grains);
    grid_.setGrains(grains, grainCount);
}

float PluginEditor::readPosition() const {
    // O parametro e' a fonte da verdade. Guardar a fracao numa variavel do editor
    // e' o caminho que faz o display e o host falarem um com o outro em zigue
    // zague: o host mexe, o display reescreve, e o host volta a mexer.
    //
    // Neste JUCE, getValue() devolve a fracao normalizada. POSITION tem faixa
    // 0 a 1, em que normalizado e' o valor, por isso a distincao nao aparece
    // aqui ÔÇö mas nao se pode estender a leitura aos outros cinco parametros sem
    // converter, e por isso a conversao fica escrita.
    if (const auto* parameter = owner_.parameters().getParameter("position")) {
        return parameter->getNormalisableRange().convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
}

double PluginEditor::entropyAtReadHead(std::uint64_t address,
                                       const PluginProcessor::ByteRange& range) const {
    // Janela de 256 bytes a partir da cabeca de leitura, recortada pela regiao.
    // Recortar pela regiao e' obrigatorio: bytes de fora do material nao sao
    // material, e medi-los era o mesmo erro que a curva do ficheiro inteiro ao
    // lado de uma regiao estreita.
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
    // O clique pode cair fora da regiao, e entao o material e' puxado para la
    // primeiro. A ordem nao e' arbitraria: o POSITION e' uma fracao da regiao, e
    // escrevelo antes de a regiao mudar apontaria para o sitio errado.
    const auto range = owner_.byteRange();
    if (address < range.start || address >= range.end) {
        moveRegionTo(address);
    }

    const auto updated = owner_.byteRange();

    // setValueNotifyingHost, e nao setValue: e' o que notifica o host, faz o
    // knob seguir e fica no historico de automacao. E' o mesmo caminho que
    // applyPendingControllerChanges usa para os CCs.
    //
    // Neste JUCE o valor e' normalizado. POSITION tem faixa 0 a 1, e
    // positionForByte devolve uma fracao de 0 a 1, entao os dois espacos
    // coincidem ÔÇö e nao coincidir era um erro silencioso em vez de um visivel.
    if (auto* parameter = owner_.parameters().getParameter("position")) {
        parameter->setValueNotifyingHost(
            static_cast<float>(pe::positionForByte(address, updated)));
    }
}

void PluginEditor::moveRegionTo(std::uint64_t address) {
    // O comprimento da regiao preserva-se, que e' a regra que o seletor de 30 px
    // usava: mudar o inicio nao muda o tamanho da janela. O Processor valida e
    // recorta o fim se o inicio novo empurrar a janela para fora do ficheiro.
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
