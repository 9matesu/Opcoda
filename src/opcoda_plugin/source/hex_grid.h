#pragma once

#include "fonts.h"
#include "palette.h"

#include "plugin_processor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace opcoda {

// grelha de bytes do binario em formato de hex dump: endereco a esquerda,
// dezasseis bytes por linha e a coluna ascii.
//
// **a janela e' estavel.** o desenho so' se move quando a pessoa rola, clica na
// barra ou usa as teclas de pagina. a versao anterior seguia os graos e a cabeca
// de leitura ao mesmo tempo, e as duas regras empurravam a janela para sitios
// diferentes a cada bloco: o hex subia e descia sozinho. agora ha' uma unica
// fonte do topo visivel, topByte_, escrita por gestos.
//
// o clique escreve POSITION, nao um ByteRange: se o byte estiver fora da regiao,
// quem a puxa e' o editor, antes de escrever a posicao.
//
// **a grelha so se opera com o rato, e isso ja deixou de ser um problema de
// acessibilidade.** as celulas sao pintadas e nao sao componentes, e nao ha como
// dar nome a 200 celulas. o caminho de teclado vive no ByteDisplay, que contem
// esta grelha e tem uma caret de navegacao com nome e valor.
class HexGrid : public juce::Component {
public:
    HexGrid();

    // endereco da celula clicada. o editor decide o que fazer com ele: e' ele
    // que sabe se o byte esta dentro da regiao.
    std::function<void(std::uint64_t address)> onCellActivated;

    // duplo clique: alterna entre a regiao exacta e a secao PE mais proxima.
    std::function<void()> onSnapRequested;

    // o utilizador rolou ou clicou numa celula. hoje so' serve para o editor
    // saber que a janela e' dele; a grelha nao se move sozinha.
    std::function<void()> onUserScrolled;

    // bytes crus. o ponteiro e' para o vector do Processor e vive enquanto ele;
    // o conteudo muda e o editor avisa com setSource.
    void setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize);

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    void setRegion(std::uint64_t start, std::uint64_t end);

    // cabeca de leitura. nao move a janela: quem move a janela e' a pessoa.
    void setReadHead(std::uint64_t address);

    // rolagem publica: teclado, botoes de navegacao e roda.
    void scrollToAddress(std::uint64_t address);
    void scrollByLines(int lines);
    void scrollToFraction(float fraction);

    // retangulo da celula em coordenadas locais, ou vazio se a linha nao estiver
    // na janela. existe para o ByteDisplay desenhar os graos na celula.
    [[nodiscard]] juce::Rectangle<float> cellRectForAddress(std::uint64_t address) const;

    [[nodiscard]] bool isAddressVisible(std::uint64_t address) const noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

private:
    // medidas derivadas da fonte, nao constantes cruas: uma celula estreita
    // demais corta o segundo digito e o display perde-se sem dar erro.
    struct Metrics {
        float cellWidth {0.0f};
        float cellAdvance {0.0f};
        float gutterWidth {0.0f};
        float asciiWidth {0.0f};
        float asciiCharWidth {0.0f};
        int gutterDigits {8};
        int rows {1};
        bool showAscii {true};
        bool showRail {false};
        juce::Rectangle<float> bytes;
        juce::Rectangle<float> gutter;
        juce::Rectangle<float> ascii;
        juce::Rectangle<float> rail;
    };

    void rebuildMetrics();
    void clampViewport();
    [[nodiscard]] std::uint64_t rowAddress(int row) const;
    [[nodiscard]] std::uint64_t highestTop() const noexcept;
    [[nodiscard]] juce::Rectangle<float> thumbRect() const noexcept;
    void paintScrollRail(juce::Graphics& g);

    [[nodiscard]] juce::Rectangle<float> cellRect(std::uint64_t column, int row) const;

    [[nodiscard]] juce::String formatAddress(std::uint64_t address) const;

    // endereco sob um ponto, ou uint64 max quando o ponto nao e' celula.
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;

    void paintColumnHeader(juce::Graphics& g);
    void paintRow(juce::Graphics& g, int row);
    void paintGutter(juce::Graphics& g, int row);
    void paintBytes(juce::Graphics& g, int row);
    void paintAscii(juce::Graphics& g, int row);

    [[nodiscard]] const PluginProcessor::SourceInfo::SectionInfo* sectionAt(
        std::uint64_t address) const;

    static constexpr std::uint64_t kBytesPerRow {16};
    static constexpr std::uint64_t kGroupGapColumns {8};
    static constexpr float kRowHeight {26.0f};
    static constexpr float kHeaderHeight {16.0f};
    static constexpr float kPadding {8.0f};
    static constexpr float kGroupGap {12.0f};
    static constexpr float kInnerGap {14.0f};
    static constexpr float kRailWidth {10.0f};
    static constexpr float kRailGap {10.0f};

    const std::vector<std::uint8_t>* bytes_ {nullptr};
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t regionStart_ {0};
    std::uint64_t regionEnd_ {0};
    std::uint64_t readHead_ {0};

    // sempre multiplo de kBytesPerRow: mantem o endereco de cada linha redondo,
    // que e' como se le um hex dump.
    std::uint64_t topByte_ {0};

    bool draggingRail_ {false};

    juce::Font font_ {fonts().sans(12.0f)};
    Metrics metrics_;
    double wheelAccumulator_ {0.0};
    // velocidade da roda: cada entalhe seguido multiplica o passo; pausa volta
    // ao de base. ver mouseWheelMove.
    double wheelBoost_ {1.0};
    std::uint32_t lastWheelMs_ {0};
};

} // namespace opcoda
