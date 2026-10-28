#pragma once

#include "byte_display.h"
#include "choice_button.h"
#include "display_panel.h"
#include "envelope_view.h"
#include "filter_view.h"
#include "knob.h"
#include "look_and_feel.h"
#include "module_folder_tab.h"
#include "output_meter.h"
#include "palette.h"
#include "plugin_processor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace opcoda {

// editor de tamanho fixo: 1180x760, sem redimensionamento.
//
// **a janela fixa e' uma decisao de produto, nao uma limitacao.** com o tamanho
// amarrado, cada peca tem um lugar garantido: o ecra domina, os knobs tem 96 px
// de disco e o texto nao e' o que sobra de um layout elastico. a F023 pedia um
// minimo de 480x434 para o caso de o host encolher; esse caso deixou de existir
// quando o redimensionamento saiu.
//
// as faixas, de cima para baixo: header de chapa, ecra com tira de vistas a' direita,
// tira de navegacao, abas de modulo, painel de controlos. o master (volume, pan e
// medidor stereo) ocupa a coluna da direita ate' ao fundo, e o teclado fica em baixo,
// a' esquerda do medidor.
//
// **os controlos de cada aba vivem em campos semanticos.** grao, playback e mix sao
// grupos separados por legenda e filete; os parametros de escolha viram campos com
// menu, porque girar um knob de quatro posicoes nunca foi a melhor leitura. pan e
// volume formam o master, o unico sitio onde a saida se ve e se controla.
//
// **sem MAIN e sem OUT nas abas.** a MAIN repetia onze parametros que ja' vivem nos
// modulos, e o OUT tinha um volume que passou a ser o master sempre visivel. cada
// parametro aparece uma vez, no grupo a que pertence.
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::ValueTree::Listener,
                     private juce::MidiKeyboardState::Listener,
                     public juce::FileDragAndDropTarget {
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

    // devolve o foco ao teclado virtual. o standalone chama isto quando fecha o
    // ecra de configuracao: sem ele as teclas seguintes ficavam a falar com a
    // janela e o instrumento parecia surdo por causa de uma janela que ja' nao
    // esta' a' vista.
    void focusPlayKeyboard();

    // o clique nao rouba o foco ao teclado: sem isto, trocar de vista calava as
    // teclas do computador e o instrumento parecia avariado.
    void mouseDown(const juce::MouseEvent& event) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override {
        return accepts(files);
    }

    void fileDragEnter(const juce::StringArray&, int, int) override {
        dragHovered_ = true;
        repaint();
    }

    void fileDragExit(const juce::StringArray&) override {
        dragHovered_ = false;
        repaint();
    }

    void filesDropped(const juce::StringArray& files, int, int) override {
        dragHovered_ = false;
        repaint();
        ingestFirst(files);
    }

private:
    void buildHeader();
    void buildNavigation();
    void buildParameterPanel();
    void buildKeyboard();

    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber,
                      float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber,
                       float velocity) override;

    // acende no teclado do editor as teclas que o MIDI do host esta' a premir.
    // o estado do teclado continua a ser a unica fonte do desenho: o MIDI entra
    // por ele com a guarda ligada, para nao voltar a disparar a nota no motor.
    void mirrorIncomingNotes();
    bool mirroringNotes_ {false};
    std::uint64_t mirroredLow_ {0};
    std::uint64_t mirroredHigh_ {0};

    void timerCallback() override;
    void refresh();
    void refreshTelemetry(const PluginProcessor::SourceInfo& info);

    // a janela e' fixa: os chips de formato e tamanho existem sempre que haja
    // conteudo. nao ha' decisao de largura para combinar com a de conteudo.
    void updateHeaderVisibility();
    void updateFooterVisibility();
    void updateStatusVisibility();

    void pushTransportAnchor();
    void refreshPlayButton();
    void refreshHoldButton();

    void activateByte(std::uint64_t address);
    void moveRegionTo(std::uint64_t address);
    [[nodiscard]] float readPosition() const;

    // leitura crua de um parametro, para o preview do display. O valor nao passa
    // pelo slider: o que se mostra e' o que o host tem, incluindo automacao.
    [[nodiscard]] float rawParameter(const char* id, float fallback) const;

    static void makePlainLabel(juce::Label& label,
                               const juce::String& text,
                               juce::Colour colour,
                               juce::Font font);

    void chooseFile();

    static bool accepts(const juce::StringArray& files) {
        for (const auto& file : files) {
            if (juce::File(file).existsAsFile()) {
                return true;
            }
        }
        return false;
    }

    bool ingestFirst(const juce::StringArray& files) {
        for (const auto& file : files) {
            if (juce::File(file).existsAsFile()) {
                pushUndoSnapshot();
                if (owner_.ingest(file)) {
                    return true;
                }
            }
        }
        return false;
    }

    PluginProcessor& owner_;
    OpcodaLookAndFeel lookAndFeel_;

    juce::MidiKeyboardState keyboardState_;
    juce::MidiKeyboardComponent keyboard_;

    DisplayPanel display_;
    ByteDisplay grid_;

    // visualizadores dos modulos ENV e FILTER: o grafico ocupa a esquerda do
    // painel de parametros e os knobs recuam para a direita quando a aba deles
    // esta' aberta.
    EnvelopeView envelopeView_;
    FilterView filterView_;

    // tira vertical a' direita do ecra: vistas e zoom, icones empilhados.
    juce::TextButton viewWaveButton_;
    juce::TextButton viewHexButton_;
    juce::TextButton viewEntropyButton_;
    juce::TextButton viewMatrixButton_;
    juce::TextButton zoomOutButton_;
    juce::TextButton zoomInButton_;
    juce::TextButton zoomFitButton_;

    // tira horizontal por baixo do ecra: transporte a' esquerda, paginas do hex
    // ao centro. o hold vive colado ao play, como um par de transporte.
    juce::TextButton playButton_;
    juce::TextButton holdButton_;
    juce::TextButton pageUpButton_;
    juce::TextButton pageDownButton_;
    juce::TextButton homeButton_;
    juce::TextButton endButton_;

    void updateViewButtons();

    juce::Label status_;
    juce::Label positionReadout_;
    juce::Label offsetReadout_;

    juce::Label fileName_;
    juce::Label formatTag_;
    juce::Label fileSize_;

    juce::TextButton loadButton_;

    // medidor stereo no canto inferior direito, com o volume ao lado. e' o master
    // da mesa: o unico sitio onde a saida se ve e se controla.
    OutputMeter meter_;

    juce::TextButton presetPrevButton_;
    juce::TextButton presetNameButton_;
    juce::TextButton presetNextButton_;
    std::unique_ptr<juce::FileChooser> presetChooser_;
    juce::String currentPresetName_ {"Init"};
    int currentFactoryIndex_ {0};
    bool presetDirty_ {false};
    bool applyingPreset_ {false};

    void buildPresetSelector();
    void showPresetMenu();
    void applyFactoryPresetAndTrack(int index);
    void stepFactoryPreset(int direction);
    void refreshPresetName();
    void valueTreePropertyChanged(juce::ValueTree& changed,
                                    const juce::Identifier& property) override;
    void loadPresetFromFile();
    void savePresetToFile();

    // -- desfazer (Ctrl+Z) --
    // um retrato do estado por gesto, nao por evento: um arrasto de knob dispara
    // dezenas de mudancas e um retrato de cada uma enchia a pilha de passos que
    // ninguem distingue. o retrato que se guarda e' o do inicio do gesto, por isso
    // desfazer devolve o valor de antes e nao o penultimo tick.
    void pushUndoSnapshot();
    void settleUndo();
    void undoLast();
    void applyUndoSnapshot(const juce::XmlElement& xml);

    std::vector<std::unique_ptr<juce::XmlElement>> undo_;
    std::unique_ptr<juce::XmlElement> settledState_;
    std::uint32_t lastChangeMs_ {0};
    bool undoPending_ {false};
    bool restoringUndo_ {false};

    // o evento de parametro pode chegar de uma thread que nao a de interface: a
    // automacao do host escreve por setValueNotifyingHost e o listener do
    // ValueTree corre na thread de quem escreveu. tirar o retrato ali' alocava
    // XML fora da interface; fica a marcacao e o retrato sai no timer.
    std::atomic<bool> undoFromAudio_ {false};

    // sete modulos visiveis. OUT existe no enum porque o volume e o pan formam o
    // master; nao tem aba, e os dois knobs moram na coluna da direita.
    enum class ParamModule { grain, envelope, filter, mod, cloud, out, keys };
    ParamModule activeModule_ {ParamModule::grain};

    // um grupo semantico e' uma legenda e a fila de controlos que ela governa.
    // Idade, densidade e janela sao "grao"; posicao, spray e scan sao "playback";
    // nivel e largura sao "mix". o painel desenha a legenda e o filete, e o
    // layout nao deixa um grupo partir-se entre duas filas.
    struct ControlGroup {
        juce::String title;
        std::vector<juce::Component*> controls;
        juce::Rectangle<int> bounds;
    };

    std::array<std::vector<ControlGroup>, 7> moduleGroups_;
    std::vector<juce::Component*> masterControls_;
    std::vector<std::unique_ptr<juce::Component>> controls_;
    Knob* masterVolume_ {nullptr};
    Knob* masterPan_ {nullptr};
    juce::Rectangle<int> masterGroupBounds_;

    ModuleFolderTab moduleGrainButton_ {"grainMod", "Grain module"};
    ModuleFolderTab moduleEnvelopeButton_ {"envMod", "Envelope module"};
    ModuleFolderTab moduleFilterButton_ {"filterMod", "Filter module"};
    ModuleFolderTab moduleModButton_ {"modMod", "Modulation module"};
    ModuleFolderTab moduleCloudButton_ {"cloudMod", "Grain cloud module"};
    ModuleFolderTab moduleKeysButton_ {"keysMod", "Keyboard and tuning module"};

    void buildModuleTabs();
    void updateModuleTabs();
    void showModule(ParamModule module);
    std::unique_ptr<juce::FileChooser> chooser_;
    bool dragHovered_ {false};

    // areas que o paint precisa de conhecer sem perguntar aos filhos.
    juce::Rectangle<int> screenBezelBounds_;
    juce::Rectangle<int> railBounds_;
    juce::Rectangle<int> moduleRowBounds_;
    juce::Rectangle<int> parameterPanelBounds_;
    juce::Rectangle<int> masterBayBounds_;
    juce::Rectangle<int> keyboardBayBounds_;

    juce::String loadedSignature_;
    double lastTick_ {0.0};
    ByteDisplay::ViewMode lastViewMode_ {ByteDisplay::ViewMode::waveform};

    float lastAnchoredPosition_ {-1.0f};
    bool positionChangedSinceLastAnchor_ {false};
};

} // namespace opcoda
