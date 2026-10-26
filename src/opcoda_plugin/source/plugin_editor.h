#pragma once

#include "byte_display.h"
#include "display_panel.h"
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
// tira de navegacao, abas de modulo, painel de knobs. o master (volume, medidor
// stereo) ocupa a coluna da direita ate' ao fundo, e o teclado fica em baixo, a'
// esquerda do medidor.
//
// **sem MAIN e sem OUT nas abas.** a MAIN repetia onze parametros que ja' vivem nos
// modulos, e o OUT tinha um volume que passou a ser o master sempre visivel. cada
// parametro aparece uma vez, no modulo a que pertence.
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
            if (juce::File(file).existsAsFile() && owner_.ingest(file)) {
                return true;
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

    juce::Label title_;
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

    // sete modulos visiveis. OUT existe no enum porque o volume precisa de um
    // Knob; nao tem aba, e o knob mora no master.
    enum class ParamModule { grain, envelope, filter, mod, cloud, out, keys };
    ParamModule activeModule_ {ParamModule::grain};
    std::vector<std::unique_ptr<Knob>> knobs_;
    std::vector<std::size_t> moduleFirstKnob_ {};
    std::vector<std::size_t> moduleKnobCount_ {};
    std::size_t masterKnobIndex_ {0};

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
