#pragma once

#include "animator.h"
#include "fonts.h"
#include "hex_grid.h"
#include "palette.h"
#include "plugin_processor.h"

#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/column_reduction.h"
#include "opcoda_core/view/grain_trail.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

namespace opcoda {

// **As teclas de letra e as da linha dos numeros nao tem codigo nomeado no JUCE.**
// KeyPress so enumera navegacao, edicao e teclas de multimedia; uma letra ou um
// digito chega com keyCode a zero e o caracter em getTextCharacter(). Pedir
// KeyPress::aKey ou KeyPress::oneKey da erro de compilacao, e foi o que aconteceu
// nas duas primeiras versoes deste codigo.
//
// `& 0xDF` e' o truque de maiusculas para ASCII: 'a' e 'A' dao o mesmo valor, e o
// mapa e' o mesmo com ou sem Shift.
//
// **So em letras.** A primeira versao aplicava o fold em tudo, e `'2' & 0xDF`
// da `0x12`: nenhum digito casava nunca, e as teclas 1, 2 e 3 nao trocavam de
// vista nem no display nem no editor. So se percebeu porque uma automacao de
// captura mandou '2' e a vista nao mudou — a mao no teclado ninguem tinha
// testado as tres teclas juntas, porque cada vista parecia funcionar sozinha.
[[nodiscard]] inline bool isTypedCharacter(const juce::KeyPress& key, char character) noexcept {
    if (key.getKeyCode() != 0) {
        return false;
    }
    const auto fold = [](int c) { return (c >= 'a' && c <= 'z') ? (c & 0xDF) : c; };
    return fold(key.getTextCharacter()) == fold(static_cast<int>(character));
}

// Digitos: aceitam as duas formas que o JUCE entrega. Uma tecla imprimivel
// chega como keyDown (com keyCode) e como char (com keyCode zero e o texto),
// e cada frente conta a sua historia: o keyDown traz o codigo, o char traz o
// texto. Casar so um dos dois deixaria metade dos teclados sem troca de vista.
//
// **Chamar so onde a acao e' idempotente.** Se as duas frentes chegarem, a acao
// corre duas vezes — `setMode` para o mesmo modo e' um nao-op, e por isso 1, 2
// e 3 podem usar isto. Um toggle como o snap da tecla A nao pode: duas
// chamadas anulavam-se, e por isso ele continua no isTypedCharacter.
[[nodiscard]] inline bool isDigitKey(const juce::KeyPress& key, char digit) noexcept {
    if (digit < '0' || digit > '9') {
        return false;
    }
    const auto want = static_cast<juce::juce_wchar>(digit);
    if (key.getKeyCode() == 0) {
        return key.getTextCharacter() == want;
    }
    return static_cast<juce::juce_wchar>(key.getKeyCode()) == want;
}

// O display do material, em tres leituras.
//
// Sao tres vistas porque sao tres perguntas diferentes sobre a mesma regiao, e
// porque o OE7 pede duas delas: "forma de onda navegavel" e "curva de entropia em
// versao enxuta". Nenhuma existed antes desta feature — o commit e99d089 trocou o
// seletor de 30 px por uma grelha de bytes e levou a curva com ele, e o display
// passou a mostrar bytes e nada mais.
//
//   - **forma de onda**: que som e' este. E' a unica vista que responde sem somar;
//   - **hex**: que bytes sao estes. Nao e' alterada: a geometria foi verificada por
//     captura em tres tamanhos de janela e nao ha razao para a mexer;
//   - **entropia**: o quanto o material e' aleatorio. E' o que a dispersao do grao
//     consome, e ate agora era invisivel.
//
// **A regiao e' a unidade nas tres.** A curva removida chegou a mostrar o ficheiro
// inteiro ao lado de uma regiao estreita, o que mente sobre o que esta a soar.
//
// A forma de onda e a entropia sao desenhadas aqui; o hex e' o HexGrid, que
// continua a ser um Component independente e vive como filho. Reutiliza-lo em vez de
// o reescrever e' deliberado: a sua geometria tem tres capturas atras e um
// redesenho seria o caminho de perder as tres.
class ByteDisplay : public juce::Component {
public:
    enum class ViewMode {
        waveform,  // 1
        hex,       // 2
        entropy,   // 3
    };

    ByteDisplay();

    // Os bytes crus. Mesmo contrato do HexGrid: o ponteiro e' para o vector do
    // Processor e fica valido enquanto o Processor viver.
    void setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize);

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    void setRegion(std::uint64_t start, std::uint64_t end);

    void setReadHead(std::uint64_t address);

    // Cabeca de reproducao, em fracao da regiao, tal como a publicada pela thread
    // de audio. Fica separada da cabeca de leitura porque sao coisas diferentes: a
    // primeira anda sozinha com o transporte, a segunda responde ao knob.
    //
    // A primeira chamada despista e nao anima: o alvo vem de uma fracao e o valor de
    // outra escala, e animar entre as duas daria um varrimento falso de ecra a
    // ecra.
    void setPlayhead(float fraction, bool visible) {
        playheadVisible_ = visible;

        if (!playheadAnimated_ || !visible) {
            playheadFraction_.jumpTo(fraction);
        } else {
            playheadFraction_.set(fraction);
        }
        playheadAnimated_ = visible;

        repaint();
    }

    // Os graos que a thread de audio publicou, ja' lidos e ja' em fracao da regiao.
    //
    // O editor le a telemetria a 60 Hz e empurra aqui, em vez de o display ir
    // buscar. E' o mesmo caminho que o playhead, e pela mesma razao: quem decide
    // o que se pinta a 60 Hz e' o editor, porque e' ele que tem o timer.
    //
    // **O rastro e' por voz, e nao por slot.** Os slots do publish sao compactados,
    // portanto o slot 0 e' o primeiro grao activo *deste* bloco e nada mais; ligar
    // uma cauda a um slot faria a cauda saltar de um grao para outro cada vez que
    // um deles morre, e o ecra ganhava um rasgo falso atravessado a cada bloco.
    // `GrainView::voice` e' o indice fixo no motor, e e' o que fecha o rastro.
    void setGrains(const std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices>& views,
                   int count);

    void setMode(ViewMode mode);
    [[nodiscard]] ViewMode mode() const noexcept { return mode_; }

    // Zoom do viewport da forma de onda e da curva: fracao da regiao visivel.
    // viewStart_ + viewLength_ <= 1 sempre; comprimento 1 = regiao inteira.
    //
    // O zoom muda o *intervalo de bytes reduzido*, nao a resolucao: as colunas
    // continuam uma por pixel e a reducao (pe::reduceToColumns, com teto de
    // amostras) escolhe a resolucao sozinha. E' a piramide de picos sem
    // estrutura extra — a reducao memorizada e' o nivel certo para o zoom.
    void zoomIn();
    void zoomOut();
    void fitView();
    void zoomAround(float factor, float centreFraction);
    [[nodiscard]] float viewLength() const noexcept { return viewLength_; }

    // Nivel de saida da ultima telemetria, em dB.
    //
    // **O medidor e' a peca animada que responde ao que se ouve**, e e' o unico
    // elemento do display que se mexe quando o transporte esta' parado e ha som. A
    // subida e' rapida e a queda e' lenta, que e' o comportamento de um medidor de
    // verdade: um medidor que cai tao depressa quanto sobe le-se como ruido.
    void setOutputLevel(float decibels);

    // Avanca a animacao. Chamado pelo editor a 60 Hz; nunca pelo paint, porque o
    // paint e' chamado varias vezes por quadro e um easing dentro dele correria
    // mais rapido que o tempo.
    void tickAnimation(float deltaSeconds);

    // Endereco escolhido por clique nas vistas novas. Quem decide o que se escreve
    // com ele e' o editor, como no hex.
    std::function<void(std::uint64_t address)> onAddressActivated;
    std::function<void()> onSnapRequested;

    // O transporte, para a tecla de espaco. Vem por callback e nao por referencia
    // ao Processor porque o display nao deve saber que existe um Processor.
    std::function<void()> onToggleTransport;
    std::function<void()> onSnapRequestedFromKey;

    // A caret de teclado: o endereco que as setas movem.
    //
    // **E' separada da cabeca de leitura de proposito.** A cabeca de leitura e' o
    // que se esta a ouvir e responde ao knob e a automacao do host; a caret e' o
    // cursor de navegacao. Sao a mesma posicao na maioria do tempo e nao precisam de
    // estar ligadas: enquanto o utilizador navega com as setas o som nao muda ate
    // carregar em Enter, que e' o que torna a navegacao inofensiva.
    //
    // A caret segue a cabeca de leitura enquanto o utilizador nao a mexer, e deixa de
    // seguir assim que ele mexe. Sem isso, uma volta de 60 Hz do editor repunha a
    // caret no meio e as setas nao fariam nada.
    void setCaret(std::uint64_t address);
    [[nodiscard]] std::uint64_t caret() const noexcept { return caret_; }

    // Traseiras de teclas. Divididas porque sao consumidores diferentes: o display
    // trata a navegacao e o editor trata o transporte.
    void handleNavigationKey(const juce::KeyPress& key);
    bool handleTransportKey(const juce::KeyPress& key);

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(juce::Component::FocusChangeType cause) override;
    void focusLost(juce::Component::FocusChangeType cause) override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

private:
    // A cache de colunas, com a chave que a invalida.
    //
    // **A chave e' (regiao, numero de colunas) e nada mais.** Os bytes mudam com o
    // ingest, e o ingest chama setSource, que limpa a cache. Uma coluna por pixel e'
    // o que torna a chave depender da largura, e por isso um redimensionar reconstrói
    // — o que e' o comportamento certo, porque mudar a largura muda o desenho.
    void rebuildColumnsIfNeeded();
    [[nodiscard]] std::uint32_t columnCountForWidth() const noexcept;

    // Aplica o que decorre do modo. Chamado pelo construtor E por setMode, e nao
    // so por setMode: um estado derivado que so se aplica numa das duas entradas
    // fica errado sempre que a outra e' a primeira a correr.
    void applyModeVisibility();

    void paintWaveform(juce::Graphics& g);
    void paintEntropy(juce::Graphics& g);

    // Linha colorida por entropia: tres faixas (0-3, 3-6, 6-8 bits/byte), cada
    // uma com o seu traco e o seu glow. O `topOf`/`bottomOf` mapeiam a coluna ao
    // y; sem espelho, os dois devolvem o mesmo.
    void paintEntropyLine(juce::Graphics& g,
                          const std::vector<pe::Column>& columns,
                          float columnWidth,
                          float originX,
                          const std::function<float(std::size_t)>& topOf,
                          const std::function<float(std::size_t)>& bottomOf);

    // Faixa de 0, 1 ou 2: os mesmos tercos do eixo da curva (0, 4, 8). E' estatico
    // para a linha e as barras usarem a mesma escala sem uma segunda fonte.
    [[nodiscard]] static int entropyBucket(float bits) noexcept;

    // Barras de entropia acopladas ao hex: uma barra por grupo de colunas, com a
    // altura do maximo do grupo e a cor da faixa. O valor e' o maximo e nao a
    // media, pela mesma razao do contorno do nucleo — o pico e' a leitura.
    void paintEntropyBars(juce::Graphics& g, const juce::Rectangle<float>& strip);

    // Silhueta do RMS: as arestas de cima e de baixo do nucleo, tracadas por
    // faixa de entropia, com o interior vazio. Duas passagens sobre as colunas
    // (uma por aresta) e tres Paths por passagem — sem guardar pontos, sem
    // alocar no paint, sem verticais de altura cheia que fundiam a massa.
    void paintRmsSilhouette(juce::Graphics& g,
                            const juce::Rectangle<float>& plotArea,
                            float centreY,
                            float halfHeight,
                            float columnWidth);

    // Geometria da faixa: 30 % da largura a direita, e nada abaixo de 120 px —
    // a faixa estreita rouba colunas do hex sem dar leitura, e uma faixa que
    // some volta o hex ao tamanho cheio. Retangulo vazio quando escondida.
    [[nodiscard]] juce::Rectangle<int> entropyStripBounds() const noexcept;

    // O hex ocupa tudo menos a faixa, e so no modo hex. Nos outros modos a faixa
    // nao existe e o hex invisivel mantem os limites cheios, para a troca de
    // vista nao mexer na geometria do ecra — que e' a regra do resized().
    void layoutHex();
    void paintSectionTicks(juce::Graphics& g);
    // Graos na forma de onda e na curva: a fracao e' a posicao na regiao e a
    // regiao e' o ecra, entao a fracca vai directamente para x.
    void paintGrains(juce::Graphics& g, const juce::Rectangle<float>& area);

    // **No hex e' outra coisa.** O hex mostra o ficheiro inteiro e a regiao e' uma
    // fracao minuscula dele — 678 KB num ficheiro de 51 MB sao 0,1%, e cabem em
    // 42 000 linhas quando a janela mostra oito. Um grao so aparece se a janela
    // estiver na linha certa, por isso aqui a fracca vira endereco e o endereco
    // vira celula.
    void paintGrainsInHex(juce::Graphics& g);

    // Mantem a janela do hex nos graos. Custa a posicao de quem esta' a ler bytes,
    // por isso `grainFollow_` desliga-se no primeiro `scrollByLines` ou clique
    // Recentrar os graos
    void followGrainsInHex() noexcept;

    // Recentrar os graos
    void paintReadHead(juce::Graphics& g, const juce::Rectangle<float>& area);
    void paintPlayhead(juce::Graphics& g, const juce::Rectangle<float>& area);
    // **A animacao nao e' um extra e' um caminho de leitura.** A cabeca de leitura e a
    // de reproducao andam sempre por coerencia com a do knob, que salta de 50 em 50
    // milissegundos. Deslizar em vez de saltar e' o que faz o display parecer vivo,
    // e custa uma divisao e uma exponencial por quadro.
    //
    // **Meia-vidas diferentes para coisas diferentes.** A cabeca de reproducao e' a
    // mais rapida, porque e' a unica que se mexe sem a mao do utilizador e um atraso
    // nela parece o display em falta. A da cabeca de leitura e' mais lenta, porque
    // segue o knob e um salto ali seria uma mentira sobre o movimento real.
    static constexpr float kPlayheadHalfLife {0.030f};
    static constexpr float kReadHeadHalfLife {0.060f};
    static constexpr float kModeHalfLife {0.090f};

    // Metade-vida de queda do medidor, e as duas constantes da faixa.
    static constexpr float kLevelHalfLife {0.080f};
    static constexpr float kLevelFloorDb {-60.0f};
    static constexpr float kLevelHeight {3.0f};

    // O comprimento do rastro e' `view::GrainTrailSet::kFrames`, no nucleo, e a
    // razao do valor esta' num ensaio la: `GrainTrail.ASweepOfThisRegionIsSubPixel`.
    static constexpr float kGrainCoreWidth {2.0f};
    static constexpr float kGrainGlowWidth {7.0f};

    void paintGrain(juce::Graphics& g,
                    const juce::Rectangle<float>& area,
                    const view::GrainTrailSet::Trail& trail,
                    float gain);

    void paintFocusRing(juce::Graphics& g);
    void paintOutputLevel(juce::Graphics& g);

    // De dB para [0, 1] na faixa do medidor.
    [[nodiscard]] static float normalisedLevel(float decibels) noexcept;

    Eased readHeadFraction_;
    Eased playheadFraction_;
    Eased modeFade_;
    Eased outputLevel_;

    // **O rastro vive no nucleo** (`opcoda::view::GrainTrailSet`) e nao aqui. A
    // logica — deslocar, truncar, esvaziar por voz, recusar posicoes invalidas — e'
    // aritmetica pura sobre arrays fixos, e dentro de um `juce::Component` nao ha
    // como a testar. `pe::reduceToColumns` e' o mesmo caso e o mesmo motivo.
    view::GrainTrailSet grainTrails_ {};

    // A janela do hex segue os graos? Verdadeiro ate' o utilizador rolar ou
    // clicar, e e' o que impede que a tela se mexa sozinha enquanto ninguem
    // pediu.
    bool grainFollow_ {true};

    bool readHeadAnimated_ {false};
    bool playheadAnimated_ {false};
    bool levelAnimated_ {false};

    // Ultimo endereco que produz som dentro da regiao: `end - 2`.
    //
    // **Nao e' `end - 1`.** granular_engine.cpp desliga a voz quando
    // `index + 1 >= sourceCount`, e pe::positionForByte ja' e' meio aberto em cima
    // pelo mesmo motivo. Uma caret em `end - 1` seria uma posicao que existe e
    // nao se ouve.
    [[nodiscard]] std::uint64_t highestUsableAddress() const noexcept;

    // Fracao da regiao em que um endereco cai, em coordenadas de ecra.
    // Fora do viewport devolve <0 ou >1, e quem pinta recorta em vez de
    // adivinhar. O inverso (ecra -> endereco) e' addressAt.
    [[nodiscard]] float fractionOf(std::uint64_t address) const noexcept;

    // Fracao da regiao (0..1) sob um ponto do ecra, ou o maximo de uint64 se o
    // ponto nao for material. Nao e' addressAt: este e' o caminho inverso em
    // fracao, para o zoom centrado no cursor e para o pan.
    [[nodiscard]] float regionFractionAt(float x, float areaX, float areaWidth) const noexcept;

    // Endereco sob um ponto, ou o maximo de uint64 se o ponto nao for material.
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;

    // Linha da regiao acima da banda. Existe porque a regiao vem por endereco e o
    // desenho e' por fracao, e a conversao e' do nucleo.
    [[nodiscard]] double fractionFor(std::uint64_t address) const noexcept;

    // Intervalo de bytes do viewport, em [regionStart_, regionEnd_).
    // E' sobre ele — e nao sobre a regiao inteira — que a reducao corre.
    [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> viewByteRange() const noexcept;

    static constexpr float kPadding {6.0f};
    static constexpr float kAxisHeight {12.0f};
    static constexpr float kAxisInset {22.0f};

    // Regua de tempo/endereco no topo da forma de onda e faixa de navegacao
    // (overview) no fundo. Alturas proprias para o plot as descontar.
    static constexpr float kRulerHeight {16.0f};
    static constexpr float kOverviewHeight {30.0f};

    // Zoom minimo: 64 bytes na regiao. Abaixo disso nao ha transiente para ver
    // — so bytes individuais, que os pontos de amostra ja mostram.
    static constexpr std::uint64_t kMinViewBytes {64};

    HexGrid hex_;

    const std::vector<std::uint8_t>* bytes_ {nullptr};
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t regionStart_ {0};
    std::uint64_t regionEnd_ {0};
    std::uint64_t readHead_ {0};
    std::uint64_t caret_ {0};
    bool caretMoved_ {false};

    // Endereco do arrasto ainda nao escrito. O rato entrega a mais de 60 Hz e o
    // tick descarrega um por quadro; maximo significa "nada pendente".
    std::uint64_t pendingScrub_ {std::numeric_limits<std::uint64_t>::max()};
    float playheadVisible_ {false};

    ViewMode mode_ {ViewMode::waveform};

    // Viewport em fracao da regiao. O pan arrasta viewStart_; o zoom escala
    // viewLength_ ancorado no cursor. setRegion volta a 0/1.
    float viewStart_ {0.0f};
    float viewLength_ {1.0f};

    // Pan com botao direito/medio ou Ctrl+arrasto: x do ultimo evento.
    bool panning_ {false};
    float lastPanX_ {0.0f};

    // Arrasto na faixa de navegacao: move o viewport em vez de varrer a leitura.
    bool overviewDrag_ {false};

    // Geometria das vistas novas: faixa de navegacao no fundo, regua no topo,
    // plot entre as duas. O hex nao usa nenhuma das tres.
    [[nodiscard]] juce::Rectangle<float> overviewBounds() const noexcept;
    [[nodiscard]] juce::Rectangle<float> plotBounds() const noexcept;
    // O plot da curva recua a esquerda para o eixo 0/4/8. Graos e cabecas na
    // vista de entropia usam este, nao o plot cheio — ou marcavam outro x.
    [[nodiscard]] juce::Rectangle<float> entropyPlotBounds() const noexcept;

    // Overview da regiao inteira para a faixa de navegacao: 192 colunas fixas,
    // recalculadas so quando a regiao ou a fonte muda. O detalhe usa columns_
    // (viewport); o overview usa estas — duas resolucoes, dois caches, nenhum
    // decode por quadro.
    std::vector<pe::Column> overviewColumns_ {};
    std::uint64_t overviewRegionStart_ {0};
    std::uint64_t overviewRegionEnd_ {0};
    bool overviewValid_ {false};
    void rebuildOverviewIfNeeded();

    void paintRuler(juce::Graphics& g, const juce::Rectangle<float>& plotArea);
    void paintOverview(juce::Graphics& g);
    void paintSourceTag(juce::Graphics& g, const juce::Rectangle<float>& plotArea);

    // Pontos de amostra no zoom fundo: quando cada coluna cobre <=2 bytes, a
    // miniatura esticada mente e os bytes reais contam a verdade. Desenha a
    // polilinha pelos valores mapeados ((byte-127,5)/127,5, a mesma conta do
    // nucleo) com ponto cheio em cada amostra.
    void paintSampleDots(juce::Graphics& g,
                         const juce::Rectangle<float>& plotArea,
                         std::uint64_t viewStartByte,
                         std::uint64_t viewEndByte);
    [[nodiscard]] bool sampleDotsVisible() const noexcept;

    std::vector<pe::Column> columns_;
    std::uint64_t cachedRegionStart_ {0};
    std::uint64_t cachedRegionEnd_ {0};
    std::uint32_t cachedColumnCount_ {0};
    float cachedViewStart_ {-1.0f};
    float cachedViewLength_ {-1.0f};
    bool cacheValid_ {false};

    juce::Font font_ {fonts().sans(10.0f)};
};

} // namespace opcoda
