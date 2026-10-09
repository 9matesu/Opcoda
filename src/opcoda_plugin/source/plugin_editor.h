#pragma once

#include "byte_display.h"
#include "byte_address_field.h"
#include "display_panel.h"
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

    // Transporte e vista, quando nenhuma tecla mais focada as consumiu.
    //
    // **A divisao com o display e' a que o JUCE ja impõe.** Uma tecla chega primeiro
    // ao componente com o foco e so depois sobe para os ancestrais. O display trata
    // a navegacao e o editor trata o transporte, e cada um devolve `true` quando
    // consome. Uma unica funcao no editor seria mais curta e errada: as setas
    // chegariam aqui por cima do display sempre que o display nao as consumisse, e
    // o display nao as consome para nao as roubar ao campo de endereco.
    bool keyPressed(const juce::KeyPress& key) override;

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
    bool headerFitsSize_ {false};
    bool headerFitsFormat_ {false};
    bool footerFitsRate_ {false};
    bool footerFitsPeak_ {false};
    bool footerFitsEntropy_ {false};
    bool footerFitsTransport_ {false};

    // Mesma regra para a linha de estado, que agora tem dois controlos e nao um:
    // o campo de endereco cai primeiro porque o POSITION e o clique na grelha
    // escrevem o mesmo sitio. O alinhar saiu da linha — tecla A e duplo-clique
    // cobrem, e o terceiro controle empurrava o texto para debaixo do campo.
    // **O botao de reproducao e' o ultimo a sair**, porque e' a unica forma de
    // ouvir o material sem teclado MIDI, e uma feature que desaparece quando a
    // janela encolhe e' uma feature que ninguem encontra.
    bool statusFitsAddress_ {false};

    void updateHeaderVisibility();
    void updateFooterVisibility();
    void updateStatusVisibility();

// O transporte de audicao no editor. Sao tres operacoes diferentes e nao
    // podem ser um so: o botao e' um comando, a ancora e' uma escrita do parametro
    // e a seek so' acontece quando o utilizador mexeu no knob.
    //
    // A guarda `positionChangedSinceLastAnchor_` e' o que impede o editor de
    // reancorar sessenta vezes por segundo. O nucleo conta geracoes para nao
    // reaplicar a mesma ancora, mas se o editor a escrevesse a cada quadro a
    // geracao subiria a cada quadro e a cabeca nunca passaria do ponto de
    // ancoragem.
    void pushTransportAnchor();
    void refreshPlayButton();

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

    // O display ocupa a area toda. Sao tres vistas dentro dele, porque sao tres
    // perguntas sobre a mesma regiao e porque o OE7 pede duas delas.
    ByteDisplay grid_;

    // O campo escreve o endereco da regiao. O alinhar nao tem botao: tecla A e
    // duplo-clique na grelha sao os dois caminhos, e o help text do display diz.
    ByteAddressField address_;

    // Transporte de audicao. Fica na linha de estado ao lado do campo e do
    // alinhamento porque e' a zona do display que ja tem moldura: um botao dentro
    // do rodape de leituras seria indistinguivel de uma leitura.
    juce::TextButton playButton_;

    // Selector de vista. Tres botoes em vez de um seletor: sao tres estados e
    // cabem tres palavras, e um popup para mudar de vista seria dois cliques para
    // uma coisa que se faz a cada minuto.
    //
    // Nao e' juce::ComboBox porque um popup esconde o display no momento em que se
    // quer comparar as tres vistas.
    juce::TextButton viewWaveButton_;
    juce::TextButton viewHexButton_;
    juce::TextButton viewEntropyButton_;

    void buildViewButtons();
    void updateViewButtons();

    // Texto puro, sem caixa nem moldura. As BoxedLabel morreram aqui: o contorno
    // de 1 px em cada leitura era o que dava a cara de painel de 2006, e a
    // informacao nao precisa dele. O contraste vem da cor do texto, que continua
    // textDark no claro e textOnDark no escuro.
    juce::Label title_;
    juce::Label status_;

    // Rodape de telemetria: leituras em Inter, como no resto. Todas em texto,
    // nunca so por cor.
    juce::Label entropyReadout_;
    juce::Label positionReadout_;
    juce::Label offsetReadout_;
    juce::Label transportReadout_;
    juce::Label peakReadout_;
    juce::Label rateReadout_;
    juce::Label voicesReadout_;

    juce::Label fileName_;
    juce::Label formatTag_;
    juce::Label fileSize_;

    juce::TextButton loadButton_;
    std::vector<std::unique_ptr<Knob>> knobs_;
    std::unique_ptr<juce::FileChooser> chooser_;
    bool dragHovered_ {false};

    // Identidade do material carregado. A grelha so e' reenviada quando isto
    // muda, e nao a cada tique do timer.
    juce::String loadedSignature_;

    // Instante do ultimo tique, em milissegundos de alta resolucao. Medido e nao
    // assumido: uma animacao que pressupoe 1/60 muda de velocidade conforme o monitor.
    double lastTick_ {0.0};

    // Ultima ancora de busca escrita no transporte, e a posicao que a produziu.
    //
    // E' o par e nao o valor so, porque o que interessa e' "o utilizador mexeu",
    // e isso so se sabe comparando com o valor anterior. Guardar so a fracao
    // permitiria detetar uma mudanca de 0,001 que o host fez e reancorar a cabeca
    // por causa dela.
    float lastAnchoredPosition_ {-1.0f};
    bool positionChangedSinceLastAnchor_ {false};
};

} // namespace opcoda
