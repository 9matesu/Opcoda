#include "hex_grid.h"

#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace opcoda {
namespace {

// Os bytes nulos com um tom a parte. E' o que um hex dump mostra e serve para
// ver o preenchimento de uma secao sem ler as duas colunas.
constexpr std::uint8_t kZeroByte {0};

juce::String toHex2(std::uint8_t byte) {
    return juce::String::formatted("%02X", static_cast<unsigned>(byte));
}

// Caractere visivel do byte, ou ponto. O Ghidra imprime o que da para imprimir
// e ponto no resto: um byte 0x00 dentro de texto legivel e' ruido, e um byte
// 0xE4 e' um acento que nao se ve.
//
// Devolve char e nao juce_wchar porque a linha e' montada num std::string e
// entregue de uma vez, caractere a caractere.
char toAscii(std::uint8_t byte) {
    return (byte >= 0x20 && byte < 0x7F) ? static_cast<char>(byte) : '.';
}

} // namespace

HexGrid::HexGrid() {
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    font_ = fonts().sans(11.0f);

    // 4.1.2 Nome, funcao e valor. A grelha e' operada so com o rato, mas nao
    // pode ser um buraco sem nome para quem navega por leitor de tela: o nome
    // diz o que e' e o texto de ajuda diz que os dois caminhos de teclado estao
    // noutro sitio.
    //
    // setHelpText e nao setTooltip: o Component expoe a ajuda pelo
    // AccessibilityHandler, que e' o que um leitor de tela le. A dica de rato
    // vinha de um TooltipClient que so chegava aqui porHEADER transitivo do
    // Slider, e numa grelha que so se opera com o rato a dica nao servia para
    // nada.
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

    // Um ficheiro novo recomeca no inicio. Sem isto, o ficheiro seguinte abria
    // a meio do anterior, porque o offset da viewport e' apenas nosso.
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

    // O cursor tem de estar visivel. Um cursor fora do ecra e' pior do que
    // nenhum: o knob mexe-se, o som mexe-se, e o display nao mostra nada.
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

    // A linha que contem o endereco tem de ficar visivel, e a viewport ancorada
    // nela e nao nela menos meia ecra. Ancorar na linha punha o byte na ultima
    // linha do ecra, a um pixel de cair fora.
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
    // A ultima linha que existe comeca em floor((size - 1) / 16) * 16, e e' essa
    // que pode ficar no topo. Sem este piso, rolar para o fim abria uma linha
    // inteira vazia.
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

// As medidas sao derivadas da fonte: uma celula mais estreita do que dois
// digitos corta o segundo, e o display perde-se sem dar erro nenhum.
void HexGrid::rebuildMetrics() {
    const auto area = getLocalBounds().toFloat().reduced(kPadding);
    const auto bodyHeight = juce::jmax(0.0f, area.getHeight() - kHeaderHeight);

    metrics_.rows = juce::jmax(1, static_cast<int>(std::floor(bodyHeight / kRowHeight)));
    metrics_.gutterDigits = 8;
    if (fileSize_ > 0) {
        // Um PE acima de 4 GB tem enderecos com mais de oito digitos. A coluna do
        // endereco e' medida pelo ficheiro carregado e nao fixa, para o numero
        // caber em vez de ser cortado a meio.
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

    // A coluna ASCII e' a primeira a cair. Perder os caracteres e' perder
    // contexto; perder colunas de hexadecimal parte os enderecos ao meio, e um
    // endereco ao meio e' pior do que nenhum. E' o que faz o criterio 1.4.10
    // de refluxo: a janela estreita perde informacao em vez de a truncar.
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

    // As dezasseis celulas sao testadas uma a uma em vez de fazer a divisao
    // pela largura da coluna. A divisao tem de desfazer o vao do separador de
    // grupo e erra o indice ao longo de toda a segunda metade; testar dezasseis
    // rectangulos e' uma fracao de microsegundo e nao tem casosyen.
    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        const auto cell = cellRect(column, row);
        if (position.x < cell.getX() || position.x >= cell.getRight()) {
            continue;
        }

        const auto address = rowAddress(row) + column;
        return address < fileSize_ ? address : none;
    }

    // Um clique no vao entre duas colunas nao escolhe nenhuma. Deixar cair na
    // coluna vizinha seria um caracter trocado sem o rato se ter mexido.
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
    // Mesmo corte que na roda: um clique e' o utilizador a dizer que a janela e'
    // dele.
    if (onUserScrolled != nullptr) {
        onUserScrolled();
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max() && onCellActivated != nullptr) {
        onCellActivated(address);
    }
}

void HexGrid::mouseDoubleClick(const juce::MouseEvent& event) {
    // O duplo clique passa pelo mesmo addressAt. Um segundo clique no vao entre
    // colunas nao alinha nada, em vez de alinhar a secao a partir de um byte
    // vizinho.
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

    // **Rolar e' o utilizador a dizer que manda ele.** A janela do hex segue os
    // graos enquanto ninguem mexe, e o primeiro entalhe corta isso: quem esta' a
    // ler bytes nao aceita que o ecra se mexa. Sem este corte, ler uma linha e a
    // tela saltar para o material seria um ciclo sem fim.
    if (onUserScrolled != nullptr) {
        onUserScrolled();
    }

    // Uma linha por entalhe, com acumulador. Um entalhe chega como 1.0, mas
    // uma roda de alta resolucao e um trackpad mandam fraccoes, e sem
    // acumulador a leitura saltava a meio do gesto.
    wheelAccumulator_ += wheel.deltaY;
    if (std::abs(wheelAccumulator_) < 1.0) {
        return;
    }

    const auto lines = static_cast<int>(wheelAccumulator_);
    wheelAccumulator_ -= static_cast<double>(lines);

    // O sinal e' invertido de proposito. Medido no Standalone em 03/10/2026:
    // vinte entalhes para baixo do rato levaram o topo de 0x1C0 para 0x180, ou
    // seja, para conteudo mais antigo. Descer a roda tem de mostrar conteudo
    // mais adiante no ficheiro, entao e' scrollByLines que recebe o sinal
    // trocado. A primeira versao fazia ao contrario e foi a captura de ecra que
    //apanhou.
    scrollByLines(-lines);
}

void HexGrid::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(palette::displayPanel);
    g.fillRect(bounds);
    g.setFont(font_);

    if (fileSize_ == 0 || bytes_ == nullptr || bytes_->empty()) {
        // Sem ficheiro, um rectangulo vazio com moldura le-se como defeito. E' o
        // mesmo motivo que escondeu as leituras vazias do rodape.
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

    // 00 01 02 ... 0F em cima das colunas. Sem este cabecalho a coluna
    // hexadecimal e' uma parede de digitos sem referencia, e a coluna 0xe nao se
    // distingue da 0x0.
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

    // Filete de 1 px entre linhas. Sem ele o endereco de uma linha pode ser
    // lido como pertencer a outra, e o endereco e' a unica coisa que se le sem
    // contar.
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

    // Uma secao que comeca dentro desta linha merece mais do que um endereco: e'
    // onde o nome tem de estar. E so a linha onde comeca escreve o nome, senao
    // ".text" aparecia repetido em milhares de linhas.
    const auto section = sectionAt(address);
    const bool startsHere =
        section != nullptr && static_cast<std::uint64_t>(section->rawOffset) >= address &&
        static_cast<std::uint64_t>(section->rawOffset) < address + kBytesPerRow;

    if (startsHere) {
        // Barra de 2 px a toda a altura da linha: um sinal que nao depende de
        // cor. E' o que substitui as abas de secao, que ocupavam 18 px para dizer
        // a mesma coisa em menos sitio.
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

    // As colunas da regiao nesta linha sao decididas no nucleo, em
    // pe::regionColumnsInRow, e nao aqui dentro do paint. A aritmetica tem casos
    // de bordo — a regiao a comecar em 0x308 corta a linha ao meio — e dentro de
    // um paint() nao ha onde a testar.
    const auto span = pe::regionColumnsInRow(address, kBytesPerRow,
                                             {regionStart_, regionEnd_});

    // A regiao ativa tem fundo e, em cima e em baixo, um filete de 1 px. Sao
    // dois sinais em vez de um: o fundo diz a quem ve cor, o filete diz a quem
    // nao ve, que e' o que o criterio 1.4.1 do WCAG 2.2 pede.
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

        // A cabeca de leitura e' uma barra branca de 2 px na margem esquerda da
        // celula, mais um fundo claro por baixo do texto. O fundo sem a barra
        // seria uma celula a tremer; a barra sem o fundo sumia sobre os bytes
        // claros.
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