#pragma once

#include "boxed_label.h"
#include "byte_address_field.h"
#include "display_panel.h"
#include "hex_grid.h"
#include "knob.h"
#include "led.h"
#include "look_and_feel.h"
#include "palette.h"
#include "plugin_processor.h"

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

    // O header e' montado da direita para a esquerda a partir de VOICES, e cada
    // peca e' escondida quando ja nao ha espaco para ela a serio.
    //
    // As flags guardam a decisao de LARGURA, tomada em resized(); o conteudo vem
    // do SourceInfo e muda quando um ficheiro carrega. Sao dois motivos
    // independentes para uma peca nao estar la, e nenhum dos dois pode prevailecer
    // sobre o outro: e' por isso que a visibilidade se combine num sitio so, em vez
    // de cada um dos dois lados escrever setVisible por sua conta.
    bool headerFitsHint_ {false};
    bool headerFitsSize_ {false};
    bool headerFitsFormat_ {false};
    bool footerFitsRate_ {false};
    bool footerFitsPeak_ {false};
    bool footerFitsEntropy_ {false};

    void updateHeaderVisibility();
    void updateFooterVisibility();

    // Clique numa celula: puxa a regiao para la se o byte estiver fora, e escreve
    // o POSITION. A ordem e' a do else: primeiro move-se o material, depois a
    // leitura, porque o POSITION e' relativo a regiao e nao ao ficheiro.
    void activateByte(std::uint64_t address);

    // Endereco escrito no campo: so move a regiao. O POSITION fica onde esta,
    // que e' o comportamento do seletor antigo e o que faz sentido quando se esta
    // a escolher material e nao a escolher um instante.
    void moveRegionTo(std::uint64_t address);

    // Le a fracao de leitura do parametro POSITION, e nao uma copia local. Ler o
    // parametro e' o que impede o editor de brigar com a automacao do host: se o
    // host moveu a cabeca, o editor mostra o que o host fez.
    [[nodiscard]] float readPosition() const;

    // Entropia da janela de 256 bytes a partir do endereco da cabeca de leitura,
    // recortada pela regiao. Um histograma de 256 celulas por tique de 20 Hz nao
    // se ouve.
    [[nodiscard]] double entropyAtReadHead(std::uint64_t address,
                                           const PluginProcessor::ByteRange& range) const;

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

    // A grelha de bytes ocupa o display todo. Nao ha barra de posicao nem curva
    // ao lado: a grelha mostra os bytes e cada linha traz o seu endereco, que e'
    // mais informacao do que a barra dava.
    HexGrid grid_;

    // Os dois caminhos de teclado para o que o rato faz na grelha. O campo
    // escreve o endereco da regiao; o botao substitui o Enter que o seletor
    // antigo usava para alinhar a secao.
    ByteAddressField address_;
    juce::TextButton snapButton_;

    BoxedLabel title_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel subtitle_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel dropHint_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel voices_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel status_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel engineTitle_ {juce::Colours::transparentBlack, palette::chassisBorder};

    // Rodape de telemetria: leituras em mono, alinhadas a direita, como no
    // mock. Todas em texto, nunca so por cor.
    BoxedLabel entropyReadout_ {juce::Colours::transparentBlack, palette::chassisBorder};
    BoxedLabel positionReadout_ {juce::Colours::transparentBlack, palette::chassisBorder};
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

    // Identidade do material carregado. A grelha so e' reenviada quando isto
    // muda, e nao a cada tique do timer.
    juce::String loadedSignature_;
};

} // namespace opcoda