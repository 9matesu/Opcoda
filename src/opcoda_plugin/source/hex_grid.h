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
// dezasseis bytes por linha e a coluna ascii. substitui o seletor de 30 px -- um
// ficheiro de 12 MB nao cabe num mapa, e cada linha ja traz o endereco.
//
// o clique escreve POSITION, nao um ByteRange: se o byte estiver fora da regiao,
// quem a puxa e' o editor, antes de escrever a posicao.
//
// **A grelha so se opera com o rato, e isso ja deixou de ser um problema de
// acessibilidade.** as celulas sao pintadas e nao sao componentes, e nao ha como
// dar nome a 200 celulas.
//
// o caminho de teclado vive no ByteDisplay, que contem esta grelha e tem uma
// caret de navegacao com nome e valor. As setas, `Inicio`, `Fim`, `Enter` e `Esc`
// operam o display inteiro, e a excecao ao 2.5.8 foi retirada de
// docs/10-acessibilidade-w3c.md.
//
// o ByteAddressField nao serve de alternativa: consome as quatro teclas de
// navegacao de proposito, para o caret nao responder a setas conforme o cursor
// esteja ou nao no fim do texto.
class HexGrid : public juce::Component {
public:
    HexGrid();

    // endereco da celula clicada. o editor decide o que fazer com ele: e' ele
    // que sabe se o byte esta dentro da regiao.
    std::function<void(std::uint64_t address)> onCellActivated;

    // duplo clique: alterna entre a regiao exacta e a secao PE mais proxima.
    std::function<void()> onSnapRequested;

    // o utilizador rolou ou clicou numa celula: corta o seguimento dos graos,
    // porque a partir daqui quem manda e' a pessoa.
    std::function<void()> onUserScrolled;

    // bytes crus. o ponteiro e' para o vector do Processor e vive enquanto ele;
    // o conteudo muda e o editor avisa com setSource.
    void setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize);

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    void setRegion(std::uint64_t start, std::uint64_t end);

    // cabeca de leitura. se sair da vista, a vista move-se para la: um cursor
    // que o utilizador nao ve nao diz nada.
    void setReadHead(std::uint64_t address);

    // publica porque quem tem o foco do teclado e' o campo de endereco, e as
    // teclas de pagina precisam de mexer aqui.
    void scrollToAddress(std::uint64_t address);
    void scrollByLines(int lines);

    // retangulo da celula em coordenadas locais, ou vazio se a linha nao estiver
    // na janela.
    //
    // existe para o ByteDisplay desenhar os graos na celula, e nao numa fracao da
    // largura: a regiao pode ser 0,1% de um ficheiro de 51 MB, e a fracao 0,5 cai
    // numa linha que ninguem esta' a ver.
    [[nodiscard]] juce::Rectangle<float> cellRectForAddress(std::uint64_t address) const;

    // verdadeiro quando a linha do endereco esta' dentro da janela visivel.
    [[nodiscard]] bool isAddressVisible(std::uint64_t address) const noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
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
        int gutterDigits {8};
        int rows {1};
        bool showAscii {true};
        juce::Rectangle<float> bytes;
        juce::Rectangle<float> gutter;
        juce::Rectangle<float> ascii;
    };

    void rebuildMetrics();
    void clampViewport();
    [[nodiscard]] std::uint64_t rowAddress(int row) const;
    [[nodiscard]] std::uint64_t highestTop() const noexcept;

    // desenho e clique leem a mesma geometria: o separador de grupo acerta nos
    // dois, senao o byte marcado deixaria de ser o byte clicado.
    [[nodiscard]] juce::Rectangle<float> cellRect(std::uint64_t column, int row) const;

    // endereco com as casas que o tamanho pede. oito e' o minimo, para um PE com
    // mais de 4 GB nao ser cortado.
    [[nodiscard]] juce::String formatAddress(std::uint64_t address) const;

    // endereco sob um ponto, ou uint64 max quando o ponto nao e' celula. o max e'
    // um endereco que nenhum ficheiro tem.
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;

    void paintColumnHeader(juce::Graphics& g);
    void paintRow(juce::Graphics& g, int row);
    void paintGutter(juce::Graphics& g, int row);
    void paintBytes(juce::Graphics& g, int row);
    void paintAscii(juce::Graphics& g, int row);

    // secao que contem o endereco, ou nullptr.
    [[nodiscard]] const PluginProcessor::SourceInfo::SectionInfo* sectionAt(
        std::uint64_t address) const;

    static constexpr std::uint64_t kBytesPerRow {16};
    static constexpr std::uint64_t kGroupGapColumns {8};
    static constexpr float kRowHeight {24.0f};
    static constexpr float kHeaderHeight {14.0f};
    static constexpr float kPadding {6.0f};
    static constexpr float kGroupGap {8.0f};
    static constexpr float kInnerGap {10.0f};

    const std::vector<std::uint8_t>* bytes_ {nullptr};
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t regionStart_ {0};
    std::uint64_t regionEnd_ {0};
    std::uint64_t readHead_ {0};

    // sempre multiplo de kBytesPerRow: mantem o endereco de cada linha redondo,
    // que e' como se le um hex dump.
    std::uint64_t topByte_ {0};

    juce::Font font_ {fonts().sans(11.0f)};
    Metrics metrics_;
    double wheelAccumulator_ {0.0};
};

} // namespace opcoda
