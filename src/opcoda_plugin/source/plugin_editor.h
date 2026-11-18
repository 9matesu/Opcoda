#pragma once

#include "byte_display.h"
#include "choice_control.h"
#include "display_panel.h"
#include "envelope_view.h"
#include "entropy_legend.h"
#include "file_field.h"
#include "filter_view.h"
#include "keyboard.h"
#include "knob.h"
#include "lfo_view.h"
#include "look_and_feel.h"
#include "mini_slider.h"
#include "module_folder_tab.h"
#include "output_meter.h"
#include "palette.h"
#include "plugin_processor.h"
#include "ui_animation.h"
#include "view_tab.h"
#include "midi_led.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace opcoda {

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

    void setStandaloneChrome(bool on) {
        chromeInset_ = 0;
        chromeRightInset_ = on ? 72 : 0;
        windowDraggable_ = on;
        resized();
        repaint();
    }

    void setOnThemeRequested(std::function<void()> callback) {
        onThemeRequested_ = std::move(callback);
        themeButton_.setVisible(onThemeRequested_ != nullptr);
    }

    void focusPlayKeyboard();

    void refreshTheme();

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

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

    void animateButtons();

    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber,
                      float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber,
                       float velocity) override;
    void setPendingNote(int note, bool on);

    void mirrorIncomingNotes();
    bool mirroringNotes_ {false};
    std::uint64_t mirroredLow_ {0};
    std::uint64_t mirroredHigh_ {0};

    void timerCallback() override;
    void refresh();
    void refreshTelemetry(const PluginProcessor::SourceInfo& info);

    [[nodiscard]] int designHeight() const;
    void applyWindowSize();

    void updateStatusVisibility();

    void pushTransportAnchor();
    void refreshPlayButton();
    void refreshHoldButton();

    void activateByte(std::uint64_t address);
    void moveRegionTo(std::uint64_t address);
    [[nodiscard]] float readPosition() const;

    [[nodiscard]] float rawParameter(const char* id, float fallback) const;

    static void makePlainLabel(juce::Label& label,
                               const juce::String& text,
                               juce::Colour colour,
                               juce::Font font);

    void chooseFile();

    class DesignCanvas : public juce::Component {
    public:
        std::function<void(juce::Graphics&)> onPaint;
        void paint(juce::Graphics& g) override {
            if (onPaint) {
                onPaint(g);
            }
        }
    };

    void paintDesign(juce::Graphics& g);
    DesignCanvas canvas_;

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
    OpcodaKeyboard keyboard_;
    juce::TextButton keyboardToggle_;
    bool keyboardOpen_ {true};

    DisplayPanel display_;
    ByteDisplay grid_;

    EnvelopeView envelopeView_;
    FilterView filterView_;
    LfoView lfoView_;

    // tira vertical a' direita do ecra: vistas e zoom, icones empilhados.
    juce::TextButton viewWaveButton_;
    juce::TextButton viewHexButton_;
    juce::TextButton viewMatrixButton_;

    // abas tacteis das vistas: expandem com o hover mostrando o nome.
    std::unique_ptr<ViewTab> viewWaveTab_;
    std::unique_ptr<ViewTab> viewHexTab_;
    std::unique_ptr<ViewTab> viewMatrixTab_;

    juce::TextButton playButton_;
    juce::TextButton holdButton_;
    MiniSlider pitchRandSlider_;
    MiniSlider scanSpeedSlider_;
    juce::TextButton pageUpButton_;
    juce::TextButton pageDownButton_;
    juce::TextButton homeButton_;
    juce::TextButton endButton_;

    void updateViewButtons();

    juce::Label status_;

    // campo do ficheiro: pasta, nome, tamanho e formato no mesmo sulco.
    FileField fileField_;

    OutputMeter meter_;

    juce::TextButton presetPrevButton_;
    juce::TextButton presetNameButton_;
    juce::TextButton presetNextButton_;
    juce::TextButton themeButton_;
    MidiLed midiLed_;
    EntropyLegend entropyLegend_;
    std::uint32_t lastMidiActivity_ {0};
    float midiLedLevel_ {0.0f};
    juce::TextButton brandButton_;
    std::function<void()> onThemeRequested_;
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

    void pushUndoSnapshot();
    void settleUndo();
    void undoLast();
    void applyUndoSnapshot(const juce::XmlElement& xml);

    std::vector<std::unique_ptr<juce::XmlElement>> undo_;
    std::unique_ptr<juce::XmlElement> settledState_;
    std::uint32_t lastChangeMs_ {0};
    bool undoPending_ {false};
    bool restoringUndo_ {false};

    std::atomic<bool> undoFromAudio_ {false};

    // notas locais ainda nao confirmadas pelo audio: o espelho nao as toca.
    std::uint64_t pendingOnLow_ {0};
    std::uint64_t pendingOnHigh_ {0};
    std::uint64_t pendingOffLow_ {0};
    std::uint64_t pendingOffHigh_ {0};

    // hover da marca no header, para a animacao de chegada.
    float lastBrandHover_ {0.0f};

    enum class ParamModule { grain, envelope, filter, mod, out };
    ParamModule activeModule_ {ParamModule::grain};
    std::vector<std::unique_ptr<Knob>> knobs_;
    std::vector<std::unique_ptr<ChoiceControl>> controls_;
    // um animador por botao de TextButton. os do ChoiceControl ficam dentro dele.
    std::vector<std::unique_ptr<PressHover>> buttonHovers_;

    struct Cell {
        Knob* knob {nullptr};
        ChoiceControl* control {nullptr};
        int span {1};
    };

    struct ModuleGrid {
        std::vector<std::vector<Cell>> rows {};
        int columns {1};
        int maxCellWidth {0};
    };

    std::vector<ModuleGrid> moduleGrids_;
    Knob* masterKnob_ {nullptr};
    Knob* masterPanKnob_ {nullptr};
    Knob* masterStereoKnob_ {nullptr};
    Knob* masterLevelKnob_ {nullptr};
    Knob* modRateKnob_ {nullptr};
    ChoiceControl* modDivControl_ {nullptr};
    bool modSyncOn_ {false};

    void layoutModuleGrid(const ModuleGrid& grid, juce::Rectangle<int> area);
    void updateModSyncLayout();

    // geometria da ultima grelha desenhada, para o paint por os filetes.
    juce::Rectangle<int> gridArea_;
    int gridCellWidth_ {0};
    int gridRowHeight_ {0};
    int gridColumns_ {0};
    int gridRows_ {0};

    ModuleFolderTab moduleGrainButton_ {"grainMod", "Grain module"};
    ModuleFolderTab moduleEnvelopeButton_ {"envMod", "Envelope module"};
    ModuleFolderTab moduleFilterButton_ {"filterMod", "Filter module"};
    ModuleFolderTab moduleModButton_ {"modMod", "Modulation module"};

    void buildModuleTabs();
    void updateModuleTabs();
    void showModule(ParamModule module);
    std::unique_ptr<juce::FileChooser> chooser_;
    bool dragHovered_ {false};

    // areas que o paint precisa de conhecer sem perguntar aos filhos.
    juce::Rectangle<int> screenBezelBounds_;
    juce::Rectangle<int> parameterPanelBounds_;
    juce::Rectangle<int> masterBayBounds_;
    juce::Rectangle<int> keyboardBayBounds_;

    // filete vertical entre o grupo do preset e o grupo do ficheiro no header.
    int headerDividerX_ {-1};

    int chromeInset_ {0};
    int chromeRightInset_ {0};

    bool windowDraggable_ {false};
    bool draggingWindow_ {false};
    juce::Point<int> windowDragOffset_ {};

    // escala do desenho: calculada no construtor a partir da area util do monitor.
    float uiScale_ {0.9f};

    juce::String loadedSignature_;
    double lastTick_ {0.0};
    ByteDisplay::ViewMode lastViewMode_ {ByteDisplay::ViewMode::waveform};

    float lastAnchoredPosition_ {-1.0f};
    bool positionChangedSinceLastAnchor_ {false};
};

} // namespace opcoda
