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
[[nodiscard]] inline bool isTypedCharacter(const juce::KeyPress& key, char character) noexcept {
    return key.getKeyCode() == 0 &&
           static_cast<char>(key.getTextCharacter() & 0xDF) == character;
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

    // Fracao da regiao em que um endereco cai, em [0, 1]. Devolve 1,0 para um
    // endereco fora da regiao, e quem chama recorta.
    [[nodiscard]] float fractionOf(std::uint64_t address) const noexcept;

    // Endereco sob um ponto, ou o maximo de uint64 se o ponto nao for material.
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;

    // Linha da regiao acima da banda. Existe porque a regiao vem por endereco e o
    // desenho e' por fracao, e a conversao e' do nucleo.
    [[nodiscard]] double fractionFor(std::uint64_t address) const noexcept;

    static constexpr float kPadding {6.0f};
    static constexpr float kAxisHeight {12.0f};
    static constexpr float kAxisInset {22.0f};

    HexGrid hex_;

    const std::vector<std::uint8_t>* bytes_ {nullptr};
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t regionStart_ {0};
    std::uint64_t regionEnd_ {0};
    std::uint64_t readHead_ {0};
    std::uint64_t caret_ {0};
    bool caretMoved_ {false};
    float playheadVisible_ {false};

    ViewMode mode_ {ViewMode::waveform};

    // Resolucao das vistas novas: quantas colunas por pixel. A roda muda isto e nao
    // o tamanho do componente, porque o resized do editor e' o dono da geometria e
    // um setSize de dentro de um paint fightaria com ele.
    float columnScale_ {1.0f};

    std::vector<pe::Column> columns_;
    std::uint64_t cachedRegionStart_ {0};
    std::uint64_t cachedRegionEnd_ {0};
    std::uint32_t cachedColumnCount_ {0};
    bool cacheValid_ {false};

    juce::Font font_ {fonts().sans(10.0f)};
};

} // namespace opcoda
