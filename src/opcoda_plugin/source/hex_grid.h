#pragma once

#include "boxed_label.h"
#include "palette.h"

#include "plugin_processor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace opcoda {

// Grelha de bytes do binario carregado, no formato de um hex dump: endereco a
// esquerda, dezasseis bytes por linha em hexadecimal, e a coluna ASCII a dizer o
// que cada byte diz.
//
// **Substitui o seletor de 30 px, e nao o acompanha.** A faixa antiga era um
// mapa do ficheiro inteiro com um cursor; um ficheiro de 12 MB nao cabe em
// grelha, e o que o utilizador quer e' ver os bytes, nao a sua posicao relativa.
// A posicao no ficheiro continua legivel porque cada linha tem o seu endereco.
//
// O clique escreve o parametro POSITION, um dos seis da Tabela 8. Nao escreve
// um ByteRange novo: se o byte estiver fora da regiao, quem puxa a regiao e' o
// editor, antes de escrever a posicao.
//
// **A grelha e' operada so com o rato, e isso ja deixou de ser um problema de
// acessibilidade.** As celulas sao pintadas e nao sao componentes, e um Component so
// nao tem filhos acessiveis: nao ha como dar nome a 200 celulas. Isso continua
    // verdadeiro e por isso a grelha nao ganha um nome por celula.
//
// O caminho de teclado passou a existir no ByteDisplay, que e' o componente que
// contem esta grelha e que tem uma caret de navegacao com o nome e o valor
// tertentu. As setas, `Inicio`, `Fim`, `Enter` e `Esc` operam o display inteiro sem
// passar por nenhum outro controle, e a excecao ao criterio 2.5.8 que esta classe
// justificava foi retirada de docs/10-acessibilidade-w3c.md.
//
// O ByteAddressField tambem nao e' a alternativa: ele consome as quatro teclas de
// navegacao de proposito, para o caret do TextEditor nao responder a setas de forma
// diferente conforme o cursor esteja ou nao no fim do texto.
class HexGrid : public juce::Component {
public:
    HexGrid();

    // Endereco da celula clicada. Quem decide o que se escreve com ele e' o
    // editor: e' ele que sabe se o byte esta dentro da regiao.
    std::function<void(std::uint64_t address)> onCellActivated;

    // Duplo clique: alterna entre a regiao exacta e a secao PE mais proxima, a
    // mesma operacao que o duplo clique no seletor antigo fazia.
    std::function<void()> onSnapRequested;

    // Os bytes crus. O ponteiro e' para o vector do Processor e fica valido
    // enquanto o Processor viver, porque o vector nao muda de endereco quando o
    // ingest atribui outro. O conteudo muda, e o editor avisa com setSource.
    void setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize);

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    void setRegion(std::uint64_t start, std::uint64_t end);

    // Endereco da cabeca de leitura. Move-se para a vista se sair do ecrã: um
    // cursor que o utilizador nao ve nao diz nada.
    void setReadHead(std::uint64_t address);

    // Navegacao. Publica porque quem tem o foco do teclado e' o campo de
    // endereco, e as teclas de pagina precisam de mexer aqui.
    void scrollToAddress(std::uint64_t address);
    void scrollByLines(int lines);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

private:
    // As medidas sao derivadas da fonte e nao sao constantes cruas: uma celula
    // estreita demais corta o segundo digito do byte, e o display inteiro
    // perde-se sem dar erro nenhum.
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

    // Rectangulo de uma celula. Existe para o desenho e para o clique leerem a
    // mesma geometria: se cada um fizesse a conta, o separador de grupo
    // acertaria no desenho e erraria no clique, e o byte marcado deixaria de
    // ser o byte clicado.
    [[nodiscard]] juce::Rectangle<float> cellRect(std::uint64_t column, int row) const;

    // Endereco do ficheiro com as casas que o digest do tamanho pede. Oito e' o
    // minimo, para um PE com mais de 4 GB de endereco nao ser cortado.
    [[nodiscard]] juce::String formatAddress(std::uint64_t address) const;

    // Endereco sob um ponto, ou std::numeric_limits<std::uint64_t>::max() se o
    // ponto nao for uma celula. O max e' um endereco que nenhum ficheiro tem.
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;

    void paintColumnHeader(juce::Graphics& g);
    void paintRow(juce::Graphics& g, int row);
    void paintGutter(juce::Graphics& g, int row);
    void paintBytes(juce::Graphics& g, int row);
    void paintAscii(juce::Graphics& g, int row);

    // Secao que contem um endereco, ou nullptr se nenhuma contiver.
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

    // Sempre multiplo de kBytesPerRow: e' o que mantem o endereco de cada
    // linha em round numbers, que e' como se le um hex dump.
    std::uint64_t topByte_ {0};

    juce::Font monoFont_ {BoxedLabel::monoFont(11.0f)};
    Metrics metrics_;
    double wheelAccumulator_ {0.0};
};

} // namespace opcoda
