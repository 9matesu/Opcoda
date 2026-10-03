#pragma once

#include "boxed_label.h"
#include "display_panel.h"
#include "entropy_curve.h"
#include "knob.h"
#include "led.h"
#include "look_and_feel.h"
#include "palette.h"
#include "plugin_processor.h"
#include "section_tabs.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace opcoda {

// Editor em tres faixas: header claro, display escuro, painel de parametros.
//
// Os seis parametros sao os documentados na Tabela 8 do artigo. Os modulos
// STATE FILTER e MOD & OUTPUT que aparecem no mock do Stitch ficaram de fora de
// proposito: o biquad, o envelope e o dry/wet nao existem no nucleo, e desenhar
// controle que nao controla nada seria pior que a ausencia dele.
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     public juce::FileDragAndDropTarget {
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // isInterestedInFileDrag decide se os demais callbacks chegam, e e' chamado
    // varias vezes enquanto o mouse se move. Precisa ser barato: so testa
    // existeAsFile e nao toca em disco.
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
    void buildParameterPanel();
    void timerCallback() override;
    void refresh();
    void refreshTelemetry(const PluginProcessor::SourceInfo& info);

    // Configura um rotulo sem fundo. Devolve void em vez de Label por valor
    // porque juce::Component tem construtor de copia deletado e nao declara
    // move, entao devolver por valor nao compila.
    static void makePlainLabel(juce::Label& label,
                               const juce::String& text,
                               juce::Colour colour,
                               juce::Font font);

    // launchAsync em vez de browseForFileToOpen: o dialogo modal nao existe por
    // padrao no JUCE (JUCE_MODAL_LOOPS_PERMITTED = 0) porque travar a message
    // thread atrapalha o host. O retorno assincrono e' o caminho correto para
    // plugin e nao custa mais codigo.
    void chooseFile();

    // Arrastar uma pasta do Explorer pode trazer muitos arquivos. Em vez de
    // recusar o arrasto inteiro, tenta em ordem e para no primeiro que o
    // parser aceita.
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

    Led powerLed_ {Led::State::off};
    Led statusLed_ {Led::State::off};
    Led voicesLed_ {Led::State::off};
    DisplayPanel display_;
    SectionTabs tabs_;
    EntropyCurve curve_;

    BoxedLabel title_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel subtitle_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel dropHint_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel voices_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel status_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel engineTitle_ {juce::Colours::transparentBlack, palette::chassisBorder};

    // Rodape de telemetria: leituras em mono, alinhadas a direita, como no
    // mock. Todas em texto, nunca so por cor.
    BoxedLabel entropyReadout_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel offsetReadout_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel peakReadout_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel rateReadout_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel voicesReadout_ {juce::Colours::transparentBlack, palette::chassisBorder};

    BoxedLabel fileName_ {palette::subPanel, palette::chassisBorder};
    BoxedLabel formatTag_ {palette::subPanel, palette::chassisBorder};
    BoxedLabel fileSize_ {palette::subPanel, palette::chassisBorder};

    juce::TextButton loadButton_;
    std::vector<std::unique_ptr<Knob>> knobs_;
    std::unique_ptr<juce::FileChooser> chooser_;
    bool dragHovered_ {false};

    // Identidade do material carregado. A curva e as abas so sao reconstruidas
    // quando isto muda, e nao a cada tique do timer.
    juce::String loadedSignature_;
};

} // namespace opcoda