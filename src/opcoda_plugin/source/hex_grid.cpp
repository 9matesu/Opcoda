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

// caractere visivel do byte, ou ponto -- como o Ghidra.
char toAscii(std::uint8_t byte) {
    return (byte >= 0x20 && byte < 0x7F) ? static_cast<char>(byte) : '.';
}

} // namespace

HexGrid::HexGrid() {
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    font_ = fonts().sans(13.5f);

    setName("Grelha de bytes do binario");
    setHelpText(
        "Cada celula e' um byte do ficheiro, com o endereco a esquerda e o "
        "caractere a direita. Clique num byte, na coluna ASCII ou na barra de "
        "rolagem para saltar. Com a roda do rato desloca-se o ficheiro; Shift "
        "rola mais depressa.");
}

void HexGrid::setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize) {
    bytes_ = bytes;
    fileSize_ = fileSize;

    // ficheiro novo recomeca no inicio. o offset da viewport e' apenas nosso: sem
    // isto, o ficheiro seguinte abria a meio do anterior.
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

    // **nada de scroll aqui.** a cabeca anda com o knob e com o transporte, e uma
    // janela que a segue sozinha briga com o rato. a barra de rolagem e o clique
    // dizem onde a pessoa quer estar.
    repaint();
}

void HexGrid::scrollToAddress(std::uint64_t address) {
    const auto rows = static_cast<int>(metrics_.rows);
    const auto lastTop = static_cast<int64_t>(highestTop());

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

void HexGrid::scrollToFraction(float fraction) {
    if (fileSize_ == 0) {
        return;
    }
    const auto clamped = std::clamp(fraction, 0.0f, 1.0f);
    const auto wanted = static_cast<std::uint64_t>(
        static_cast<double>(clamped) * static_cast<double>(fileSize_ - 1));
    const auto aligned = (wanted / kBytesPerRow) * kBytesPerRow;
    const auto next = juce::jmin(aligned, highestTop());
    if (next != topByte_) {
        topByte_ = next;
        repaint();
    }
}

std::uint64_t HexGrid::highestTop() const noexcept {
    if (fileSize_ == 0) {
        return 0;
    }
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

void HexGrid::rebuildMetrics() {
    auto area = getLocalBounds().toFloat().reduced(kPadding);
    if (area.getWidth() <= 0.0f) {
        metrics_.rows = 1;
        return;
    }

    const auto bodyHeight = juce::jmax(0.0f, area.getHeight() - kHeaderHeight);
    metrics_.rows = juce::jmax(1, static_cast<int>(std::floor(bodyHeight / kRowHeight)));

    // oito digitos chegam ate' 4 GB. o loop conta os digitos que o ficheiro pede
    // e so' sobe acima de oito quando o endereco nao cabe -- antes comecava em 8 e
    // somava, o que dava 14 digitos num ficheiro de 3 MB.
    metrics_.gutterDigits = 8;
    if (fileSize_ > 0) {
        auto remaining = fileSize_ - 1;
        int needed = 1;
        while (remaining > 0) {
            ++needed;
            remaining >>= 4;
        }
        metrics_.gutterDigits = juce::jmax(8, needed);
    }

    // a grelha ocupa a largura do ecra em vez de se encostar a' esquerda. o
    // endereco leva ~19%, os bytes ~55% e o ASCII ~26%; as celulas esticam para
    // preencher o que sobra. e' o que torna um dump de 16 bytes legivel numa
    // janela de 1180 em vez de deixar metade do ecra vazio.
    const auto visibleRows = static_cast<std::uint64_t>(metrics_.rows) * kBytesPerRow;
    metrics_.showRail = fileSize_ > visibleRows;
    const auto railSpace = metrics_.showRail ? kRailWidth + kRailGap : 0.0f;
    const auto contentWidth = area.getWidth() - railSpace;

    metrics_.showAscii = contentWidth >= 760.0f;

    const auto gutterWidth = juce::jlimit(148.0f, 220.0f, contentWidth * 0.19f);
    const auto asciiWidth = metrics_.showAscii
                                ? juce::jlimit(230.0f, 340.0f, contentWidth * 0.26f)
                                : 0.0f;
    const auto gaps = metrics_.showAscii ? 2.0f * kInnerGap : kInnerGap;
    const auto bytesWidth = contentWidth - gutterWidth - asciiWidth - gaps;

    metrics_.gutterWidth = gutterWidth;
    metrics_.cellAdvance = juce::jmax(22.0f, (bytesWidth - kGroupGap) /
                                                   static_cast<float>(kBytesPerRow));
    metrics_.cellWidth = juce::jmax(17.0f, metrics_.cellAdvance - 5.0f);
    metrics_.asciiCharWidth = metrics_.showAscii ? asciiWidth / 16.0f : 0.0f;
    metrics_.asciiWidth = asciiWidth;

    const auto bodyTop = area.getY() + kHeaderHeight;

    metrics_.gutter = {area.getX(), bodyTop, metrics_.gutterWidth,
                       static_cast<float>(metrics_.rows) * kRowHeight};
    metrics_.bytes = {metrics_.gutter.getRight() + kInnerGap, bodyTop,
                      static_cast<float>(kBytesPerRow) * metrics_.cellAdvance,
                      static_cast<float>(metrics_.rows) * kRowHeight};
    metrics_.ascii =
        metrics_.showAscii
            ? juce::Rectangle<float> {metrics_.bytes.getRight() + kInnerGap, bodyTop,
                                      metrics_.asciiWidth,
                                      static_cast<float>(metrics_.rows) * kRowHeight}
            : juce::Rectangle<float> {};
    metrics_.rail =
        metrics_.showRail
            ? juce::Rectangle<float> {area.getRight() - kRailWidth + 1.0f, bodyTop,
                                      kRailWidth - 1.0f,
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

    // a coluna ASCII tambem e' clicavel: a letra e' o mesmo byte.
    if (metrics_.showAscii && position.x >= metrics_.ascii.getX() &&
        position.x < metrics_.ascii.getRight()) {
        const auto offset = static_cast<std::uint64_t>(
            std::floor((position.x - metrics_.ascii.getX()) / metrics_.asciiCharWidth));
        if (offset < kBytesPerRow) {
            const auto address = rowAddress(row) + offset;
            return address < fileSize_ ? address : none;
        }
    }

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

juce::Rectangle<float> HexGrid::thumbRect() const noexcept {
    if (!metrics_.showRail || metrics_.rail.getHeight() <= 0.0f || fileSize_ == 0) {
        return {};
    }

    const auto totalRows = static_cast<double>((fileSize_ + kBytesPerRow - 1) / kBytesPerRow);
    const auto visible = static_cast<double>(metrics_.rows);
    const auto ratio = std::clamp(visible / totalRows, 0.06, 1.0);
    const auto thumbHeight = juce::jmax(18.0f, static_cast<float>(metrics_.rail.getHeight() * ratio));
    const auto travel = metrics_.rail.getHeight() - thumbHeight;
    const auto position = juce::jlimit(
        0.0f, 1.0f,
        static_cast<float>(topByte_) / static_cast<float>(juce::jmax<std::uint64_t>(1, highestTop())));
    return {metrics_.rail.getX(), metrics_.rail.getY() + travel * position,
            metrics_.rail.getWidth(), thumbHeight};
}

void HexGrid::paintScrollRail(juce::Graphics& g) {
    if (!metrics_.showRail) {
        return;
    }
    g.setColour(palette::alpha(palette::displayBorder, 0.65f));
    g.fillRoundedRectangle(metrics_.rail, 2.0f);

    const auto thumb = thumbRect();
    if (!thumb.isEmpty()) {
        g.setColour(palette::alpha(palette::accent, 0.85f));
        g.fillRoundedRectangle(thumb, 2.0f);
    }
}

void HexGrid::mouseDown(const juce::MouseEvent& event) {
    if (onUserScrolled != nullptr) {
        onUserScrolled();
    }

    // barra de rolagem: salto directo. clicar no polegar tambem salta porque a
    // barra e' curta e o polegar e' o indicador, nao um alvo de precisao.
    if (metrics_.showRail && metrics_.rail.expanded(4.0f).contains(event.position)) {
        draggingRail_ = true;
        scrollToFraction((event.position.y - metrics_.rail.getY()) /
                         juce::jmax(1.0f, metrics_.rail.getHeight()));
        return;
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max() && onCellActivated != nullptr) {
        onCellActivated(address);
    }
}

void HexGrid::mouseDrag(const juce::MouseEvent& event) {
    if (draggingRail_) {
        scrollToFraction((event.position.y - metrics_.rail.getY()) /
                         juce::jmax(1.0f, metrics_.rail.getHeight()));
    }
}

void HexGrid::mouseUp(const juce::MouseEvent&) {
    draggingRail_ = false;
}

void HexGrid::mouseDoubleClick(const juce::MouseEvent& event) {
    if (addressAt(event.position) == std::numeric_limits<std::uint64_t>::max()) {
        return;
    }
    if (onSnapRequested != nullptr) {
        onSnapRequested();
    }
}

void HexGrid::mouseWheelMove(const juce::MouseEvent& event,
                             const juce::MouseWheelDetails& wheel) {
    if (onUserScrolled != nullptr) {
        onUserScrolled();
    }

    // ctrl: salto de um quarto do ficheiro. e' o "rolar rapidamente" para um
    // ficheiro de 12 MB, onde uma linha por entalhe seriam milhares de voltas.
    if (event.mods.isCtrlDown()) {
        const auto rows = static_cast<int>(metrics_.rows);
        scrollByLines(wheel.deltaY > 0.0f ? -rows * 16 : rows * 16);
        return;
    }

    // velocidade exponencial: cada entalhe seguido dentro de 320 ms multiplica o
    // passo por 1.7, ate' 32x; uma pausa maior volta ao passo de base. e' o que
    // torna a roda agil num ficheiro grande sem perder o andamento linha a linha.
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastWheelMs_ > 320) {
        wheelBoost_ = 1.0;
    }
    lastWheelMs_ = now;

    // shift multiplica por quatro; a roda de precisao manda fracoes e o
    // acumulador evita saltos a meio do gesto.
    wheelAccumulator_ += static_cast<double>(wheel.deltaY) *
                         (event.mods.isShiftDown() ? 12.0 : 3.0) * wheelBoost_;
    wheelBoost_ = juce::jmin(wheelBoost_ * 1.7, 32.0);
    if (std::abs(wheelAccumulator_) < 1.0) {
        return;
    }

    const auto lines = static_cast<int>(wheelAccumulator_);
    wheelAccumulator_ -= static_cast<double>(lines);
    scrollByLines(-lines);
}

void HexGrid::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(palette::displayPanel);
    g.fillRect(bounds);
    g.setFont(font_);

    if (fileSize_ == 0 || bytes_ == nullptr || bytes_->empty()) {
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

    paintScrollRail(g);
}

void HexGrid::paintColumnHeader(juce::Graphics& g) {
    const auto top = getLocalBounds().toFloat().reduced(kPadding).getY();

    g.setColour(palette::alpha(palette::textOnDark, 0.80f));
    g.setFont(fonts().sans(10.5f, true));

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

    // listras alternadas muito fracas: numa parede de digitos, seguir uma linha
    // ate' ao ASCII e' o que custa. o contraste e' baixo de proposito para nao
    // competir com o laranja do endereco tocado.
    if (row % 2 == 1) {
        g.setColour(palette::alpha(palette::ivory, 0.022f));
        g.fillRect(juce::Rectangle<float> {area.getX(), top, area.getWidth(), kRowHeight});
    }

    g.setColour(palette::alpha(palette::displayBorder, 0.55f));
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

    const auto section = sectionAt(address);
    const bool startsHere =
        section != nullptr && static_cast<std::uint64_t>(section->rawOffset) >= address &&
        static_cast<std::uint64_t>(section->rawOffset) < address + kBytesPerRow;

    if (startsHere) {
        g.setColour(palette::brass);
        g.fillRect(juce::Rectangle<float> {band.getX(), band.getY(), 2.0f, kRowHeight});
        g.setFont(fonts().sans(11.0f, true));
        g.setColour(palette::accentSoft);
        g.drawText(section->name, band.reduced(5.0f, 0.0f), juce::Justification::centredRight,
                   false);
        return;
    }

    g.setColour(palette::alpha(palette::textOnDark, 0.74f));
    g.setFont(font_);
    g.drawText(formatAddress(address), band.reduced(5.0f, 0.0f), juce::Justification::centredRight,
               false);
}

void HexGrid::paintBytes(juce::Graphics& g, int row) {
    const auto address = rowAddress(row);

    // as colunas da regiao vem de pe::regionColumnsInRow, nao do paint. a
    // aritmetica tem casos de bordo -- a regiao a comecar a meio da linha -- e
    // dentro de um paint() nao ha onde os testar.
    const auto span = pe::regionColumnsInRow(address, kBytesPerRow,
                                             {regionStart_, regionEnd_});

    // a regiao e' um filete, nao uma faixa: o laranja fica reservado ao endereco
    // tocado, para a marcacao nao competir com ele.
    if (!span.empty()) {
        const auto left = cellRect(span.firstColumn, row).getX();
        const auto right = cellRect(span.lastColumn - 1, row).getRight();
        const auto bandTop = metrics_.bytes.getY() + static_cast<float>(row) * kRowHeight;
        g.setColour(palette::alpha(palette::accent, 0.55f));
        g.fillRect(juce::Rectangle<float> {left, bandTop, right - left, 1.0f});
        g.fillRect(juce::Rectangle<float> {left, bandTop + kRowHeight - 1.0f,
                                           right - left, 1.0f});
    }

    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        const auto byteAddress = address + column;
        if (byteAddress >= fileSize_ || bytes_ == nullptr) {
            break;
        }

        const auto cell = cellRect(column, row);
        const auto byte = (*bytes_)[static_cast<std::size_t>(byteAddress)];
        const auto touched = byteAddress == readHead_;

        // so' o endereco selecionado leva laranja: uma barra de 3 px na margem
        // esquerda e um banho fraco na celula. o byte anterior levava uma barra
        // branca a toda a altura, e tres marcas claras na mesma linha nao se
        // distinguiam.
        if (touched) {
            g.setColour(palette::alpha(palette::accent, 0.16f));
            g.fillRect(cell);
            g.setColour(palette::accent);
            g.fillRect(juce::Rectangle<float> {cell.getX() - 1.0f, cell.getY(), 3.0f,
                                                kRowHeight});
        }

        juce::Colour ink = palette::textOnDark;
        if (byte == kZeroByte) {
            ink = palette::alpha(palette::textOnDark, 0.55f);
        } else if (!(byteAddress >= regionStart_ && byteAddress < regionEnd_)) {
            ink = palette::alpha(palette::textOnDark, 0.32f);
        }
        if (touched) {
            ink = palette::accentSoft;
        }

        g.setFont(touched ? fonts().sans(13.5f, true) : font_);
        g.setColour(ink);
        g.drawText(toHex2(byte), cell, juce::Justification::centredLeft, false);
    }
}

void HexGrid::paintAscii(juce::Graphics& g, int row) {
    if (!metrics_.showAscii) {
        return;
    }

    const auto address = rowAddress(row);
    const auto band = metrics_.ascii.withY(metrics_.ascii.getY() +
                                           static_cast<float>(row) * kRowHeight)
                          .withHeight(kRowHeight);

    // a coluna ASCII leva fundo proprio: 16 caracteres seguidos sem entrelinha
    // eram uma parede. o fundo separa-a da parede de digitos hex.
    g.setColour(palette::alpha(palette::display, 0.85f));
    g.fillRect(band);
    g.setColour(palette::alpha(palette::displayBorder, 0.75f));
    g.fillRect(juce::Rectangle<float> {band.getX(), band.getY(), 1.0f, kRowHeight});

    const auto inRegionRow =
        !pe::regionColumnsInRow(address, kBytesPerRow, {regionStart_, regionEnd_}).empty();
    g.setFont(font_);

    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        const auto byteAddress = address + column;
        if (byteAddress >= fileSize_ || bytes_ == nullptr) {
            break;
        }

        const auto character = toAscii((*bytes_)[static_cast<std::size_t>(byteAddress)]);
        const auto touched = byteAddress == readHead_;
        const auto inRegion = byteAddress >= regionStart_ && byteAddress < regionEnd_;

        auto ink = (inRegionRow && inRegion) ? palette::textOnDark
                                             : palette::alpha(palette::textOnDark, 0.40f);
        if (touched) {
            ink = palette::accentSoft;
        }

        const auto cell = juce::Rectangle<float> {
            band.getX() + static_cast<float>(column) * metrics_.asciiCharWidth, band.getY(),
            metrics_.asciiCharWidth, kRowHeight};

        g.setColour(ink);
        g.drawText(juce::String::charToString(static_cast<juce::juce_wchar>(character)),
                   cell, juce::Justification::centred, false);
    }
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
