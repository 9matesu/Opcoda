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
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace opcoda {

// editor em seis faixas: header claro, display escuro, selector de vista, abas de
// modulo, faixa de parametros e teclado.
//
// os parametros vivem em sete modulos (GRAIN, ENVELOPE, FILTER, MOD, CLOUD, OUT,
// KEYS), com nomes em ingles e os IDs originais intactos. Uma aba mostra a sua
// faixa; empilhar as sete pedia 700 px e a promessa e' 480x434.
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

    // transporte e vista, quando nenhuma tecla mais focada as consumiu.
    //
    // a divisao com o display e' a do JUCE: a tecla chega ao componente com foco e
    // so' depois sobe. O display trata navegacao, o editor transporte, e cada um
    // devolve true quando consome. Uma funcao unica no editor mandaria as setas por
    // cima do display, que nao as consome para nao as roubar ao campo de endereco.
    bool keyPressed(const juce::KeyPress& key) override;

    // isInterestedInFileDrag decide se os outros callbacks chegam e corre a cada
    // movimento do rato. Tem de ser barato: so' testa existeAsFile, sem tocar no
    // disco.
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
    void buildKeyboard();

    // o teclado virtual toca pela mesma NoteTracker do MIDI do host. O listener so'
    // empurra para a fila do processor; nada aqui toca no motor.
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber,
                      float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber,
                       float velocity) override;

    void timerCallback() override;
    void refresh();
    void refreshTelemetry(const PluginProcessor::SourceInfo& info);

    // o header e' montado da direita para a esquerda a partir de VOICES, e cada peca
    // desaparece quando nao ha' espaco para ela a serio.
    //
    // as flags guardam a decisao de LARGURA, tomada em resized(); o conteudo vem do
    // SourceInfo e muda quando um ficheiro carrega. Sao dois motivos independentes, e
    // nenhum pode prevalecer: por isso a visibilidade se combina num sitio so'.
    bool headerFitsSize_ {false};
    bool headerFitsFormat_ {false};
    bool footerFitsPeak_ {false};
    bool footerFitsEntropy_ {false};
    bool footerFitsTransport_ {false};

    // mesma regra para a linha de estado, agora com dois controlos. O campo de
    // endereco cai primeiro porque o POSITION e o clique na grelha escrevem o mesmo
    // sitio. O alinhar saiu: tecla A e duplo-clique cobrem, e o terceiro controle
    // empurrava o texto para debaixo do campo.
    //
    // **O botao de reproducao e' o ultimo a sair** — e' a unica forma de ouvir sem
    // teclado MIDI, e feature que desaparece ao encolher ninguem encontra.
    bool statusFitsAddress_ {false};

    void updateHeaderVisibility();
    void updateFooterVisibility();
    void updateStatusVisibility();

    // -- transporte de audicao --
    // tres operacoes diferentes, que nao podem ser uma so': o botao e' um comando,
    // a ancora e' uma escrita do parametro, a seek so' acontece quando o utilizador
    // mexeu no knob.
    //
    // positionChangedSinceLastAnchor_ e' o que impede o editor de reancorar 60 vezes
    // por segundo: escrevendo a cada quadro, a geracao subia a cada quadro e a cabeca
    // nunca passava da ancoragem.
    void pushTransportAnchor();
    void refreshPlayButton();

    // clique numa celula: puxa a regiao para la' se o byte estiver fora, e escreve o
    // POSITION. Primeiro o material, depois a leitura — o POSITION e' relativo a
    // regiao e nao ao ficheiro.
    void activateByte(std::uint64_t address);

    // endereco escrito no campo: so' move a regiao. O POSITION fica onde esta', que
    // era o comportamento do seletor antigo e faz sentido quando se escolhe material
    // e nao um instante.
    void moveRegionTo(std::uint64_t address);

    // le a fracao de leitura do parametro, nao uma copia local. Ler o parametro e' o
    // que impede o editor de brigar com a automacao: se o host moveu a cabeca, o
    // editor mostra o que o host fez.
    [[nodiscard]] float readPosition() const;

    // entropia da janela de 256 bytes a partir da cabeca, recortada pela regiao. Um
    // histograma de 256 celulas por tique nao se ouve.
    [[nodiscard]] double entropyAtReadHead(std::uint64_t address,
                                           const PluginProcessor::ByteRange& range) const;

    // configura um rotulo sem fundo. Devolve void em vez de Label por valor porque
    // juce::Component tem copia deletada e nao declara move: devolver por valor nao
    // compila.
    static void makePlainLabel(juce::Label& label,
                               const juce::String& text,
                               juce::Colour colour,
                               juce::Font font);

    // launchAsync em vez de browseForFileToOpen: o dialogo modal nao existe por
    // omissao (JUCE_MODAL_LOOPS_PERMITTED = 0) porque travar a message thread
    // atrapalha o host. O retorno assincrono e' o caminho certo para plugin.
    void chooseFile();

    // arrastar uma pasta do Explorer pode trazer muitos ficheiros. Em vez de recusar
    // o arrasto, tenta por ordem e para no primeiro que o parser aceita.
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

    // teclado virtual. O estado vive no editor e o componente desenha-o; o som sai
    // pela fila do processor, porque o caminho de audio nao consulta objetos de
    // interface.
    //
    // a ordem dos membros e' a da construcao: o componente guarda referencia ao
    // estado e nao pode nascer primeiro.
    juce::MidiKeyboardState keyboardState_;
    juce::MidiKeyboardComponent keyboard_;

    Led powerLed_ {Led::State::off};
    Led statusLed_ {Led::State::off};
    Led voicesLed_ {Led::State::off};
    DisplayPanel display_;

    // o display ocupa a area toda, com tres vistas dentro: sao tres perguntas sobre
    // a mesma regiao, e o OE7 pede duas delas.
    ByteDisplay grid_;

    // o campo escreve o endereco da regiao. O alinhar nao tem botao: tecla A e
    // duplo-clique na grelha, e o help text do display diz isso.
    ByteAddressField address_;

    // transporte de audicao, na linha de estado ao lado do campo. A zona ja' tem
    // moldura; um botao dentro do rodape de leituras seria indistinguivel de uma
    // leitura.
    juce::TextButton playButton_;

    // tres botoes em vez de um seletor: sao tres estados, cabem tres palavras, e um
    // popup para mudar de vista seriam dois cliques numa coisa de cada minuto.
    //
    // nao e' ComboBox porque o popup esconde o display justamente quando se quer
    // comparar as tres vistas.
    juce::TextButton viewWaveButton_;
    juce::TextButton viewHexButton_;
    juce::TextButton viewEntropyButton_;

    // zoom do viewport: - / + / FIT a' direita das vistas. Texto curto sem cromo, a
    // mesma lingua das abas; o display tambem responde a roda, +/- e 0, e estes
    // botoes sao o caminho clicavel e o que o leitor de tela anuncia.
    juce::TextButton zoomOutButton_;
    juce::TextButton zoomInButton_;
    juce::TextButton zoomFitButton_;

    void buildViewButtons();
    void updateViewButtons();

    // texto puro, sem caixa nem moldura. As BoxedLabel morreram aqui: o contorno de
    // 1 px em cada leitura dava cara de painel de 2006. O contraste vem da cor,
    // textDark no claro e textOnDark no escuro.
    juce::Label title_;
    juce::Label status_;

    // rodape de telemetria, em texto e nunca so' por cor. A taxa saiu: e' constante
    // na sessao e morava na faixa mais disputada; foi para o tooltip do titulo, onde
    // quem procura um numero fixo le sem pressa.
    juce::Label entropyReadout_;
    juce::Label positionReadout_;
    juce::Label offsetReadout_;
    juce::Label transportReadout_;
    juce::Label peakReadout_;
    juce::Label voicesReadout_;

    juce::Label fileName_;
    juce::Label formatTag_;
    juce::Label fileSize_;

    juce::TextButton loadButton_;

    // preset no header: anterior, nome (abre o menu), seguinte. O nome mostra "*"
    // quando um parametro saiu do preset — sem isso o menu mente, porque diz "Cloud"
    // sobre um som que ja' nao e' Cloud. A sujeira vem do Listener da APVTS e nao de
    // comparar valores: comparar todos os floats a 60 Hz e' o contrario de barato.
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

    // sete modulos, uma faixa visivel de cada vez. Todos os 41 knobs existem desde a
    // construcao — os attachments vivem neles — e a aba so' decide quem aparece.
    // recriar knobs por aba recriaria attachments e largaria o undo.
    //
    // a ordem do enum e' a de construcao em buildParameterPanel: os arrays de fatias
    // sao indexados por static_cast<size_t>(module).
    enum class ParamModule { grain, envelope, filter, mod, cloud, out, keys };
    ParamModule activeModule_ {ParamModule::grain};
    std::vector<std::unique_ptr<Knob>> knobs_;
    std::vector<std::size_t> moduleFirstKnob_ {};
    std::vector<std::size_t> moduleKnobCount_ {};
    juce::TextButton moduleGrainButton_;
    juce::TextButton moduleEnvelopeButton_;
    juce::TextButton moduleFilterButton_;
    juce::TextButton moduleModButton_;
    juce::TextButton moduleCloudButton_;
    juce::TextButton moduleOutButton_;
    juce::TextButton moduleKeysButton_;

    void buildModuleTabs();
    void updateModuleTabs();
    void showModule(ParamModule module);
    std::unique_ptr<juce::FileChooser> chooser_;
    bool dragHovered_ {false};

    // identidade do material carregado. A grelha so' e' reenviada quando isto muda,
    // e nao a cada tique do timer.
    juce::String loadedSignature_;

    // instante do ultimo tique, em ms de alta resolucao. Medido e nao assumido:
    // animacao que pressupoe 1/60 muda de velocidade conforme o monitor.
    double lastTick_ {0.0};

    // ultima ancora escrita no transporte, e a posicao que a produziu.
    //
    // e' o par e nao o valor so': o que interessa e' "o utilizador mexeu", e isso so'
    // se sabe comparando com o anterior. Guardar so' a fracao permitiria detetar uma
    // mudanca de 0,001 do host e reancorar a cabeca por causa dela.
    float lastAnchoredPosition_ {-1.0f};
    bool positionChangedSinceLastAnchor_ {false};
};

} // namespace opcoda
