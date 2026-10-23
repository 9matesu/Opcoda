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

// letras e a linha dos numeros nao tem codigo nomeado no JUCE: chegam com keyCode
// zero e o caracter em getTextCharacter(). KeyPress::aKey nao existe.
// `& 0xDF` e' o fold de maiusculas para ASCII, e so vale em letras: `'2' & 0xDF` da
// `0x12` e nenhum digito casa.
[[nodiscard]] inline bool isTypedCharacter(const juce::KeyPress& key, char character) noexcept {
    if (key.getKeyCode() != 0) {
        return false;
    }
    const auto fold = [](int c) { return (c >= 'a' && c <= 'z') ? (c & 0xDF) : c; };
    return fold(key.getTextCharacter()) == fold(static_cast<int>(character));
}

// digitos vem por duas frentes: keyDown com keyCode, ou char com keyCode zero.
// casar so uma deixa metade dos teclados sem troca de vista.
// chamar so onde a acao e' idempotente: se as duas chegarem, corre duas vezes. o
// toggle do snap (tecla A) nao pode, e fica no isTypedCharacter.
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

// o display do material, em tres leituras: forma de onda, hex e entropia.
// a regiao e' a unidade nas tres — a curva ja' mostrou o ficheiro inteiro ao lado de
// uma regiao estreita e mentia sobre o que esta' a soar.
// forma de onda e entropia desenham-se aqui; o hex e' o HexGrid, que vive como filho
// e tem geometria verificada por captura.
class ByteDisplay : public juce::Component {
public:
    enum class ViewMode {
        waveform,  // tecla 1
        hex,       // tecla 2
        entropy,   // tecla 3
    };

    ByteDisplay();

    // bytes crus. mesmo contrato do HexGrid: o ponteiro e' para o vector do Processor
    // e fica valido enquanto ele viver.
    void setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize);

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    void setRegion(std::uint64_t start, std::uint64_t end);

    void setReadHead(std::uint64_t address);

    // cabeca de reproducao, em fracao da regiao, como a publica a thread de audio.
    // separada da cabeca de leitura: esta anda com o transporte, a outra com o knob.
    // nota: a primeira chamada despista em vez de animar — o alvo vem de outra escala
    // e animar dava um varrimento falso.
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

    // graos publicados pela thread de audio, ja' em fracao da regiao. o editor le a
    // telemetria a 60 Hz e empurra para aqui; quem tem o timer decide o que se pinta.
    // o rastro e' por voz e nao por slot: os slots do publish sao compactados, e uma
    // cauda ligada ao slot saltava de grao a cada bloco. GrainView::voice e' fixo.
    void setGrains(const std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices>& views,
                   int count);

    void setMode(ViewMode mode);
    [[nodiscard]] ViewMode mode() const noexcept { return mode_; }

    // zoom do viewport, em fracao da regiao. viewStart_ + viewLength_ <= 1; 1 = regiao
    // inteira. muda o intervalo de bytes reduzido, nao a resolucao: as colunas
    // continuam uma por pixel e pe::reduceToColumns escolhe a reducao.
    void zoomIn();
    void zoomOut();
    void fitView();
    void zoomAround(float factor, float centreFraction);
    [[nodiscard]] float viewLength() const noexcept { return viewLength_; }

    // nivel de saida da ultima telemetria, em dB. e' o unico elemento que se mexe com
    // o transporte parado e ha som. sobe depressa e cai devagar: um medidor simetrico
    // le-se como ruido.
    void setOutputLevel(float decibels);

    // avanca a animacao, chamado pelo editor a 60 Hz. nunca do paint: o paint corre
    // varias vezes por quadro e o easing andava mais depressa que o tempo.
    void tickAnimation(float deltaSeconds);

    // endereco escolhido por clique nas vistas novas. quem escreve com ele e' o editor,
    // como no hex.
    std::function<void(std::uint64_t address)> onAddressActivated;
    std::function<void()> onSnapRequested;

    // o transporte, para a tecla de espaco. vem por callback para o display nao precisar
    // de saber que existe um Processor.
    std::function<void()> onToggleTransport;
    std::function<void()> onSnapRequestedFromKey;

    // a caret de teclado: o endereco que as setas movem. separada da cabeca de leitura de
    // proposito — navegar nao muda o som ate' ao Enter, e e' isso que torna a navegacao
    // inofensiva.
    // segue a cabeca de leitura enquanto o utilizador nao mexe; sem essa guarda, a volta
    // de 60 Hz do editor repunha-a e as setas nao faziam nada.
    void setCaret(std::uint64_t address);
    [[nodiscard]] std::uint64_t caret() const noexcept { return caret_; }

    // teclas partidas por consumidor: o display trata a navegacao, o editor o transporte.
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
    // cache de colunas. a chave e' (regiao, numero de colunas): uma coluna por pixel, por
    // isso redimensionar reconstroi. o ingest chama setSource, que a limpa.
    void rebuildColumnsIfNeeded();
    [[nodiscard]] std::uint32_t columnCountForWidth() const noexcept;

    // aplica o que decorre do modo. corre no construtor e em setMode: um estado derivado
    // que so se aplica numa das entradas fica errado quando a outra e' a primeira.
    void applyModeVisibility();

    void paintWaveform(juce::Graphics& g);
    void paintEntropy(juce::Graphics& g);

    // linha colorida por entropia: tres faixas (0-3, 3-6, 6-8 bits/byte), cada uma com
    // traco e glow. topOf/bottomOf mapeiam a coluna ao y; sem espelho dao o mesmo.
    void paintEntropyLine(juce::Graphics& g,
                          const std::vector<pe::Column>& columns,
                          float columnWidth,
                          float originX,
                          const std::function<float(std::size_t)>& topOf,
                          const std::function<float(std::size_t)>& bottomOf);

    // faixa 0, 1 ou 2: os mesmos tercos do eixo (0, 4, 8). estatico para a linha e as
    // barras usarem a mesma escala.
    [[nodiscard]] static int entropyBucket(float bits) noexcept;

    // barras acopladas ao hex: uma por grupo de colunas, com a altura do maximo do grupo
    // e a cor da faixa. maximo e nao media — o pico e' a leitura.
    void paintEntropyBars(juce::Graphics& g, const juce::Rectangle<float>& strip);

    // silhueta do rms: arestas de cima e de baixo do nucleo por faixa de entropia, com o
    // interior vazio. duas passagens sobre as colunas e tres Paths por passagem, sem
    // guardar pontos nem alocar no paint.
    void paintRmsSilhouette(juce::Graphics& g,
                            const juce::Rectangle<float>& plotArea,
                            float centreY,
                            float halfHeight,
                            float columnWidth);

    // faixa a direita: 30 % da largura e nada abaixo de 120 px. escondida, devolve um
    // retangulo vazio.
    [[nodiscard]] juce::Rectangle<int> entropyStripBounds() const noexcept;

    // o hex ocupa tudo menos a faixa, e so no modo hex. nos outros a faixa nao existe e o
    // hex invisivel mantem os limites cheios, para trocar de vista nao mexer na geometria.
    void layoutHex();
    void paintSectionTicks(juce::Graphics& g);
    // graos na forma de onda e na curva: a fracao ja' e' posicao na regiao, e a regiao e'
    // o ecra, por isso vai directa para x.
    void paintGrains(juce::Graphics& g, const juce::Rectangle<float>& area);

    // no hex e' outra coisa: a janela e' o ficheiro inteiro e a regiao pode ser 0,1% dele.
    // um grao so aparece se a janela estiver na linha certa, por isso aqui a fracao vira
    // endereco e o endereco vira celula.
    void paintGrainsInHex(juce::Graphics& g);

    // mantem a janela do hex nos graos. custa a posicao de quem esta' a ler bytes, por isso
    // grainFollow_ desliga-se no primeiro scrollByLines ou clique.
    void followGrainsInHex() noexcept;

    // cabeca de leitura nas vistas novas, em fracao do viewport.
    void paintReadHead(juce::Graphics& g, const juce::Rectangle<float>& area);
    void paintPlayhead(juce::Graphics& g, const juce::Rectangle<float>& area);
    // a animacao e' caminho de leitura, nao extra: a cabeca de reproducao anda sempre, por
    // coerencia com a do knob, que salta de 50 em 50 ms. deslizar em vez de saltar custa
    // uma divisao e uma exponencial por quadro.
    // meia-vidas diferentes: a de reproducao e' a mais rapida, porque e' a unica que se
    // mexe sem a mao do utilizador; a de leitura segue o knob e um salto ali seria mentira.
    static constexpr float kPlayheadHalfLife {0.030f};
    static constexpr float kReadHeadHalfLife {0.060f};
    static constexpr float kModeHalfLife {0.090f};

    // queda do medidor e as constantes da faixa.
    static constexpr float kLevelHalfLife {0.080f};
    static constexpr float kLevelFloorDb {-60.0f};
    static constexpr float kLevelHeight {3.0f};

    // comprimento do rastro: view::GrainTrailSet::kFrames. a razao do valor esta' no
    // ensaio GrainTrail.ASweepOfThisRegionIsSubPixel.
    static constexpr float kGrainCoreWidth {2.0f};
    static constexpr float kGrainGlowWidth {7.0f};

    void paintGrain(juce::Graphics& g,
                    const juce::Rectangle<float>& area,
                    const view::GrainTrailSet::Trail& trail,
                    float gain);

    void paintFocusRing(juce::Graphics& g);
    void paintOutputLevel(juce::Graphics& g);

    // dB para [0, 1] na faixa do medidor.
    [[nodiscard]] static float normalisedLevel(float decibels) noexcept;

    Eased readHeadFraction_;
    Eased playheadFraction_;
    Eased modeFade_;
    Eased outputLevel_;

    // o rastro vive no nucleo (opcoda::view::GrainTrailSet): deslocar, truncar, esvaziar
    // por voz e recusar posicoes invalidas e' aritmetica pura sobre arrays fixos, e dentro
    // de um Component nao ha como testar. pe::reduceToColumns, mesmo caso.
    view::GrainTrailSet grainTrails_ {};

    // a janela do hex segue os graos? verdadeiro ate' o utilizador rolar ou clicar. e' o
    // que impede o ecra de se mexer sozinho.
    bool grainFollow_ {true};

    bool readHeadAnimated_ {false};
    bool playheadAnimated_ {false};
    bool levelAnimated_ {false};

    // ultimo endereco que produz som na regiao: end - 2, nao end - 1. o motor desliga a voz
    // quando index + 1 >= sourceCount, e pe::positionForByte ja' e' meio aberto em cima.
    // end - 1 existe e nao se ouve.
    [[nodiscard]] std::uint64_t highestUsableAddress() const noexcept;

    // fracao da regiao onde cai um endereco, em coordenadas de ecra. fora do viewport
    // devolve <0 ou >1 e quem pinta recorta. o inverso (ecra -> endereco) e' addressAt.
    [[nodiscard]] float fractionOf(std::uint64_t address) const noexcept;

    // fracao da regiao sob um ponto do ecra, ou -1 se o ponto nao for material. e' o
    // caminho inverso em fracao, para o zoom centrado no cursor e para o pan.
    [[nodiscard]] float regionFractionAt(float x, float areaX, float areaWidth) const noexcept;

    // endereco sob um ponto, ou o maximo de uint64 se o ponto nao for material.
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;

    // fracao da regiao para um endereco. a conversao e' do nucleo.
    [[nodiscard]] double fractionFor(std::uint64_t address) const noexcept;

    // intervalo de bytes do viewport, dentro de [regionStart_, regionEnd_). a reducao
    // corre sobre ele, nao sobre a regiao inteira.
    [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> viewByteRange() const noexcept;

    static constexpr float kPadding {6.0f};
    static constexpr float kAxisHeight {12.0f};
    static constexpr float kAxisInset {22.0f};

    // regua no topo e overview no fundo. alturas proprias, descontadas do plot.
    static constexpr float kRulerHeight {16.0f};
    static constexpr float kOverviewHeight {30.0f};

    // zoom minimo: 64 bytes na regiao. abaixo disso so ha bytes soltos, que os pontos de
    // amostra ja' mostram.
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

    // endereco do arrasto ainda nao escrito. o rato entrega a mais de 60 Hz e o tick
    // descarrega um por quadro; maximo = nada pendente.
    std::uint64_t pendingScrub_ {std::numeric_limits<std::uint64_t>::max()};
    float playheadVisible_ {false};

    ViewMode mode_ {ViewMode::waveform};

    // viewport em fracao da regiao. o pan arrasta viewStart_, o zoom escala viewLength_
    // ancorado no cursor. setRegion volta a 0/1.
    float viewStart_ {0.0f};
    float viewLength_ {1.0f};

    // pan com botao direito/medio ou Ctrl+arrasto: x do ultimo evento.
    bool panning_ {false};
    float lastPanX_ {0.0f};

    // arrasto na faixa de navegacao: move o viewport, nao a leitura.
    bool overviewDrag_ {false};

    // geometria das vistas novas: overview no fundo, regua no topo, plot no meio. o hex nao
    // usa nenhuma.
    [[nodiscard]] juce::Rectangle<float> overviewBounds() const noexcept;
    [[nodiscard]] juce::Rectangle<float> plotBounds() const noexcept;
    // o plot da curva recua a esquerda para o eixo 0/4/8. graos e cabecas na entropia usam
    // este, senao marcavam outro x.
    [[nodiscard]] juce::Rectangle<float> entropyPlotBounds() const noexcept;

    // overview da regiao inteira: 192 colunas fixas, refeitas so quando a regiao ou a
    // fonte muda. o detalhe usa columns_ — dois caches, nenhum decode por quadro.
    std::vector<pe::Column> overviewColumns_ {};
    std::uint64_t overviewRegionStart_ {0};
    std::uint64_t overviewRegionEnd_ {0};
    bool overviewValid_ {false};
    void rebuildOverviewIfNeeded();

    void paintRuler(juce::Graphics& g, const juce::Rectangle<float>& plotArea);
    void paintOverview(juce::Graphics& g);
    void paintSourceTag(juce::Graphics& g, const juce::Rectangle<float>& plotArea);

    // pontos de amostra com zoom fundo: quando cada coluna cobre <=2 bytes, a miniatura
    // estica e mente, e os bytes reais contam a verdade. polilinha por (byte-127,5)/127,5,
    // a conta do nucleo, com ponto cheio em cada amostra.
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
