#include "hex_grid.h"

#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace opcoda {
namespace {

// bytes nulos com um tom a parte: mostra o preenchimento da secao sem ler as
// duas colunas.
constexpr std::uint8_t kZeroByte {0};

juce::String toHex2(std::uint8_t byte) {
    return juce::String::formatted("%02X", static_cast<unsigned>(byte));
}

// caractere visivel do byte, ou ponto -- como o Ghidra. char e nao juce_wchar
// porque a linha e' montada num std::string.
char toAscii(std::uint8_t byte) {
    return (byte >= 0x20 && byte < 0x7F) ? static_cast<char>(byte) : '.';
}

} // namespace

HexGrid::HexGrid() {
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    font_ = fonts().sans(11.0f);

    // 4.1.2 nome, funcao e valor. a grelha so se opera com o rato, mas nao pode
    // ser um buraco sem nome para quem usa leitor de tela.
    //
    // setHelpText e nao setTooltip: o AccessibilityHandler le a ajuda. a dica de
    // rato vinha de um TooltipClient por header transitivo e aqui nao servia.
    setName("Grelha de bytes do binario");
    setHelpText(
        "Cada celula e' um byte do ficheiro, com o endereco a esquerda e o "
        "caractere a direita. Clique num byte para mover a cabeca de leitura "
        "para ele. Com a roda do rato desloca-se o ficheiro. A leitura e o "
        "endereco escrevem-se tambem pelo knob POSITION e pelo campo de "
        "endereco, que tem foco de teclado.");
}

void HexGrid::setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize) {
    bytes_ = bytes;
    fileSize_ = fileSize;

    // ficheiro novo recomeca no inicio. o offset da viewport e' apenas nosso:
    // sem isto, o ficheiro seguinte abria a meio do anterior.
    topByte_ = 0;
    rebuildMetrics();
    clampViewport();
    repaint();
}

void HexGrid::setRegion(std::uint64_t start, std::uint64_t end) {
    regionStart_ = start;
    regionEnd_ = juce::jmax(end, start);
    repaint();
}

void HexGrid::setReadHead(std::uint64_t address) {
    readHead_ = address;

    // cursor fora do ecra e' pior que nenhum: o knob mexe-se, o som mexe-se, e o
    // display nao mostra nada.
    const auto bottom = topByte_ +
                        static_cast<std::uint64_t>(metrics_.rows) * kBytesPerRow;
    if (address < topByte_ || address >= bottom) {
        scrollToAddress(address);
    }
    repaint();
}

void HexGrid::scrollToAddress(std::uint64_t address) {
    const auto rows = static_cast<int>(metrics_.rows);
    const auto lastTop = static_cast<int64_t>(highestTop());

    // a linha do endereco fica visivel e centrada na viewport. ancorar na linha
    // punha o byte na ultima linha do ecra, a um pixel de cair fora.
    const auto wantedRow = static_cast<int64_t>(address / kBytesPerRow);
    const auto target = juce::jlimit<int64_t>(
        0, lastTop, (wantedRow - rows / 2) * static_cast<int64_t>(kBytesPerRow));

    const auto next = static_cast<std::uint64_t>(target);
    if (next != topByte_) {
        topByte_ = next;
        repaint();
    }
}

void HexGrid::scrollByLines(int lines) {
    if (lines == 0) {
        return;
    }

    const auto offset = static_cast<int64_t>(topByte_) +
                        static_cast<int64_t>(lines) * static_cast<int64_t>(kBytesPerRow);
    const auto next = static_cast<std::uint64_t>(juce::jlimit<int64_t>(
        0, static_cast<int64_t>(highestTop()), offset));

    if (next != topByte_) {
        topByte_ = next;
        repaint();
    }
}

std::uint64_t HexGrid::highestTop() const noexcept {
    if (fileSize_ == 0) {
        return 0;
    }
    // a ultima linha que existe pode ficar no topo. sem este piso, rolar ate ao
    // fim abria uma linha inteira vazia.
    return ((fileSize_ - 1) / kBytesPerRow) * kBytesPerRow;
}

void HexGrid::clampViewport() {
    const auto next = juce::jlimit<std::uint64_t>(0, highestTop(), topByte_);
    topByte_ = (next / kBytesPerRow) * kBytesPerRow;
}

std::uint64_t HexGrid::rowAddress(int row) const {
    return topByte_ + static_cast<std::uint64_t>(row) * kBytesPerRow;
}

bool HexGrid::isAddressVisible(std::uint64_t address) const noexcept {
    if (metrics_.rows < 1) {
        return false;
    }

    const auto wantedRow = static_cast<int64_t>(address / kBytesPerRow);
    const auto firstRow = static_cast<int64_t>(topByte_ / kBytesPerRow);
    return wantedRow >= firstRow && wantedRow < firstRow + metrics_.rows;
}

juce::Rectangle<float> HexGrid::cellRectForAddress(std::uint64_t address) const {
    if (!isAddressVisible(address)) {
        return {};
    }

    const auto row = static_cast<int>(static_cast<int64_t>(address / kBytesPerRow) -
                                       static_cast<int64_t>(topByte_ / kBytesPerRow));
    return cellRect(address % kBytesPerRow, row);
}

// medidas derivadas da fonte: celula mais estreita que dois digitos corta o
// segundo e o display perde-se sem dar erro.
void HexGrid::rebuildMetrics() {
    const auto area = getLocalBounds().toFloat().reduced(kPadding);
    const auto bodyHeight = juce::jmax(0.0f, area.getHeight() - kHeaderHeight);

    metrics_.rows = juce::jmax(1, static_cast<int>(std::floor(bodyHeight / kRowHeight)));
    metrics_.gutterDigits = 8;
    if (fileSize_ > 0) {
        // a coluna do endereco cresce com o ficheiro: um PE acima de 4 GB tem
        // mais de oito digitos e nao pode ser cortado a meio.
        auto remaining = fileSize_ - 1;
        while (remaining > 0) {
            ++metrics_.gutterDigits;
            remaining >>= 4;
        }
    }

    const auto widthOf = [this](const juce::String& text) {
        return juce::GlyphArrangement::getStringWidth(font_, text);
    };

    metrics_.gutterWidth = widthOf("0x" + juce::String::repeatedString(
                                              juce::StringRef("0"), metrics_.gutterDigits)) +
                           kInnerGap;
    metrics_.cellWidth = widthOf("FF") + 4.0f;
    metrics_.cellAdvance = metrics_.cellWidth + 2.0f;
    metrics_.asciiWidth = widthOf("0000000000000000");

    const auto bytesWidth = static_cast<float>(kBytesPerRow) * metrics_.cellAdvance + kGroupGap;

    // a coluna ASCII e' a primeira a cair: perder os caracteres e' perder
    // contexto; partir os enderecos ao meio e' pior. e' o refluxo do 1.4.10.
    const auto neededWithAscii =
        metrics_.gutterWidth + bytesWidth + kInnerGap + metrics_.asciiWidth;
    metrics_.showAscii = area.getWidth() >= neededWithAscii;

    const auto bodyTop = area.getY() + kHeaderHeight;

    metrics_.gutter = {area.getX(), bodyTop, metrics_.gutterWidth,
                       static_cast<float>(metrics_.rows) * kRowHeight};
    metrics_.bytes = {metrics_.gutter.getRight() + kInnerGap, bodyTop, bytesWidth,
                      static_cast<float>(metrics_.rows) * kRowHeight};
    metrics_.ascii = metrics_.showAscii
                         ? juce::Rectangle<float> {metrics_.bytes.getRight() + kInnerGap, bodyTop,
                                                   metrics_.asciiWidth,
                                                   static_cast<float>(metrics_.rows) * kRowHeight}
                         : juce::Rectangle<float> {};
}

std::uint64_t HexGrid::addressAt(juce::Point<float> position) const {
    constexpr auto none = std::numeric_limits<std::uint64_t>::max();

    if (fileSize_ == 0 || bytes_ == nullptr || metrics_.rows < 1) {
        return none;
    }

    const auto row = static_cast<int>(
        std::floor((position.y - metrics_.bytes.getY()) / kRowHeight));
    if (row < 0 || row >= metrics_.rows) {
        return none;
    }

    // as dezasseis celulas sao testadas uma a uma: a divisao pela largura da
    // coluna tem de desfazer o vao do grupo e erra o indice na segunda metade.
    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        const auto cell = cellRect(column, row);
        if (position.x < cell.getX() || position.x >= cell.getRight()) {
            continue;
        }

        const auto address = rowAddress(row) + column;
        return address < fileSize_ ? address : none;
    }

    // clique no vao entre duas colunas nao escolhe nenhuma: cair na vizinha
    // seria um byte trocado sem o rato se ter mexido.
    return none;
}

const PluginProcessor::SourceInfo::SectionInfo* HexGrid::sectionAt(std::uint64_t address) const {
    for (const auto& section : sections_) {
        if (section.rawSize == 0) {
            continue;
        }
        const auto start = static_cast<std::uint64_t>(section.rawOffset);
        if (address >= start && address < start + section.rawSize) {
            return &section;
        }
    }
    return nullptr;
}

void HexGrid::resized() {
    rebuildMetrics();
    clampViewport();
}

void HexGrid::mouseDown(const juce::MouseEvent& event) {
    // mesmo corte que a roda: o clique diz que a janela e' do utilizador.
    if (onUserScrolled != nullptr) {
        onUserScrolled();
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max() && onCellActivated != nullptr) {
        onCellActivated(address);
    }
}

void HexGrid::mouseDoubleClick(const juce::MouseEvent& event) {
    // o duplo clique passa pelo mesmo addressAt: no vao entre colunas nao alinha
    // nada, em vez de alinhar a partir de um byte vizinho.
    if (addressAt(event.position) == std::numeric_limits<std::uint64_t>::max()) {
        return;
    }
    if (onSnapRequested != nullptr) {
        onSnapRequested();
    }
}

void HexGrid::mouseWheelMove(const juce::MouseEvent& event,
                             const juce::MouseWheelDetails& wheel) {
    static_cast<void>(event);

    // rolar e' o utilizador a dizer que manda ele. a janela segue os graos
    // enquanto ninguem mexe, e o primeiro entalhe corta isso -- quem esta' a ler
    // bytes nao aceita que o ecra se mexa sozinho.
    if (onUserScrolled != nullptr) {
        onUserScrolled();
    }

    // uma linha por entalhe, com acumulador: trackpad e roda de alta resolucao
    // mandam fraccoes, e sem ele a leitura saltava a meio do gesto.
    wheelAccumulator_ += wheel.deltaY;
    if (std::abs(wheelAccumulator_) < 1.0) {
        return;
    }

    const auto lines = static_cast<int>(wheelAccumulator_);
    wheelAccumulator_ -= static_cast<double>(lines);

    // sinal invertido de proposito: descer a roda tem de mostrar conteudo mais
    // adiante no ficheiro. nota: medido no standalone em 03/10/2026, o deltaY
    // vinha ao contrario e a captura de ecra apanhou.
    scrollByLines(-lines);
}

void HexGrid::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(palette::displayPanel);
    g.fillRect(bounds);
    g.setFont(font_);

    if (fileSize_ == 0 || bytes_ == nullptr || bytes_->empty()) {
        // sem ficheiro, um rectangulo com moldura le-se como defeito.
        g.setColour(palette::textOnDarkSub);
        g.drawText("SEM BINARIO", bounds.reduced(kPadding, 0.0f), juce::Justification::centred,
                   false);
        return;
    }

    paintColumnHeader(g);

    for (int row = 0; row < metrics_.rows; ++row) {
        if (rowAddress(row) >= fileSize_) {
            break;
        }
        paintRow(g, row);
    }
}

void HexGrid::paintColumnHeader(juce::Graphics& g) {
    const auto top = getLocalBounds().toFloat().reduced(kPadding).getY();

    g.setColour(palette::textOnDarkSub);
    g.setFont(font_);

    // cabecalho 00..0F em cima das colunas: sem ele a coluna hex e' uma parede
    // de digitos e a coluna 0xe nao se distingue da 0x0.
    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        g.drawText(toHex2(static_cast<std::uint8_t>(column)),
                   cellRect(column, 0).withY(top).withHeight(kHeaderHeight),
                   juce::Justification::centred, false);
    }

    if (metrics_.showAscii) {
        g.drawText("ASCII", metrics_.ascii.withY(top).withHeight(kHeaderHeight),
                   juce::Justification::centredLeft, false);
    }
}

void HexGrid::paintRow(juce::Graphics& g, int row) {
    const auto top = metrics_.bytes.getY() + static_cast<float>(row) * kRowHeight;
    const auto area = getLocalBounds().toFloat().reduced(kPadding);

    // filete de 1 px entre linhas, para o endereco de uma nao ser lido como de
    // outra.
    g.setColour(palette::alpha(palette::displayBorder, 0.7f));
    g.drawHorizontalLine(juce::roundToInt(top), area.getX(), area.getRight());

    paintGutter(g, row);
    paintBytes(g, row);
    paintAscii(g, row);
}

void HexGrid::paintGutter(juce::Graphics& g, int row) {
    const auto address = rowAddress(row);
    const auto band = metrics_.gutter.withY(metrics_.gutter.getY() +
                                            static_cast<float>(row) * kRowHeight)
                          .withHeight(kRowHeight);

    // o nome da secao so se escreve na linha onde ela comeca, senao ".text"
    // aparecia repetido em milhares de linhas.
    const auto section = sectionAt(address);
    const bool startsHere =
        section != nullptr && static_cast<std::uint64_t>(section->rawOffset) >= address &&
        static_cast<std::uint64_t>(section->rawOffset) < address + kBytesPerRow;

    if (startsHere) {
        // barra de 2 px a toda a altura: um sinal que nao depende de cor, no
        // lugar das abas de secao, que ocupavam 18 px para dizer o mesmo.
        g.setColour(palette::accent);
        g.fillRect(juce::Rectangle<float> {band.getX(), band.getY(), 2.0f, kRowHeight});
        g.setFont(font_);
        g.drawText(section->name, band.reduced(5.0f, 0.0f), juce::Justification::centredRight,
                   false);
        return;
    }

    g.setColour(palette::textOnDarkSub);
    g.setFont(font_);
    g.drawText(formatAddress(address), band.reduced(4.0f, 0.0f), juce::Justification::centredRight,
               false);
}

void HexGrid::paintBytes(juce::Graphics& g, int row) {
    const auto address = rowAddress(row);

    // as colunas da regiao vem de pe::regionColumnsInRow, nao do paint. a
    // aritmetica tem casos de bordo -- a regiao a comecar a meio da linha -- e
    // dentro de um paint() nao ha onde os testar.
    const auto span = pe::regionColumnsInRow(address, kBytesPerRow,
                                             {regionStart_, regionEnd_});

    // regiao ativa: fundo e filete de 1 px em cima e em baixo. dois sinais -- o
    // fundo para quem ve cor, o filete para quem nao ve (1.4.1).
    if (!span.empty()) {
        const auto left = cellRect(span.firstColumn, row).getX();
        const auto right = cellRect(span.lastColumn - 1, row).getRight();
        const auto band = juce::Rectangle<float> {left, metrics_.bytes.getY() +
                                                               static_cast<float>(row) * kRowHeight,
                                                  juce::jmax(metrics_.cellWidth, right - left),
                                                  kRowHeight};

        g.setColour(palette::alpha(palette::accent, 0.22f));
        g.fillRect(band);
        g.setColour(palette::alpha(palette::accent, 0.85f));
        g.fillRect(band.withHeight(1.0f));
        g.fillRect(juce::Rectangle<float> {band.getX(), band.getBottom() - 1.0f, band.getWidth(),
                                            1.0f});
    }

    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        const auto byteAddress = address + column;
        if (byteAddress >= fileSize_ || bytes_ == nullptr) {
            break;
        }

        const auto cell = cellRect(column, row);
        const auto byte = (*bytes_)[static_cast<std::size_t>(byteAddress)];

        // cabeca de leitura: barra branca de 2 px na margem esquerda da celula e
        // fundo claro por baixo do texto. a barra sozinha some nos bytes claros;
        // o fundo sozinho parece uma celula a tremer.
        if (byteAddress == readHead_) {
            g.setColour(palette::alpha(juce::Colours::white, 0.16f));
            g.fillRect(cell);
            g.setColour(juce::Colours::white);
            g.fillRect(cell.withWidth(2.0f));
        }

        const auto inRegion = byteAddress >= regionStart_ && byteAddress < regionEnd_;
        g.setColour(byte == kZeroByte
                        ? palette::textOnDarkSub
                        : (inRegion ? palette::textOnDark
                                    : palette::alpha(palette::textOnDark, 0.42f)));
        g.drawText(toHex2(byte), cell, juce::Justification::centredLeft, false);
    }
}

void HexGrid::paintAscii(juce::Graphics& g, int row) {
    if (!metrics_.showAscii) {
        return;
    }

    const auto address = rowAddress(row);
    std::string line;

    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        const auto byteAddress = address + column;
        if (byteAddress >= fileSize_ || bytes_ == nullptr) {
            line += ' ';
            continue;
        }
        line += toAscii((*bytes_)[static_cast<std::size_t>(byteAddress)]);
    }

    const auto inRegionRow = !pe::regionColumnsInRow(address, kBytesPerRow,
                                                   {regionStart_, regionEnd_})
                             .empty();
    g.setColour(inRegionRow ? palette::textOnDark
                            : palette::alpha(palette::textOnDark, 0.42f));
    g.setFont(font_);
    g.drawText(juce::String {line},
               metrics_.ascii.withY(metrics_.ascii.getY() + static_cast<float>(row) * kRowHeight)
                   .withHeight(kRowHeight),
               juce::Justification::centredLeft, false);
}

juce::Rectangle<float> HexGrid::cellRect(std::uint64_t column, int row) const {
    const auto x = metrics_.bytes.getX() + static_cast<float>(column) * metrics_.cellAdvance +
                   (column >= kGroupGapColumns ? kGroupGap : 0.0f);
    return {x, metrics_.bytes.getY() + static_cast<float>(row) * kRowHeight, metrics_.cellWidth,
            kRowHeight};
}

juce::String HexGrid::formatAddress(std::uint64_t address) const {
    return "0x" + juce::String::formatted("%0*llX", metrics_.gutterDigits,
                                          static_cast<unsigned long long>(address));
}

} // namespace opcoda