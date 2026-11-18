#include "hex_grid.h"

#include "opcoda_core/disasm/x86_disasm.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace opcoda {
namespace {

juce::String toHex2(std::uint8_t byte) {
    return juce::String::formatted("%02X", static_cast<unsigned>(byte));
}

// caractere visivel do byte, ou ponto -- como o Ghidra.
char toAscii(std::uint8_t byte) {
    return (byte >= 0x20 && byte < 0x7F) ? static_cast<char>(byte) : '.';
}


bool isNumberToken(const juce::String& token) {
    if (token.isEmpty()) {
        return false;
    }
    if (juce::CharacterFunctions::isDigit(token[0])) {
        return true;
    }
    if ((token[0] == '-' || token[0] == '+') && token.length() > 1 &&
        juce::CharacterFunctions::isDigit(token[1])) {
        return true;
    }
    return token.contains("0x");
}

bool isRegisterToken(const juce::String& token) {
    static const juce::StringArray registers {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp", "rip",
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp", "eip",
        "ax",  "bx",  "cx",  "dx",  "si",  "di",  "bp",  "sp",  "ip",
        "al",  "ah",  "bl",  "bh",  "cl",  "ch",  "dl",  "dh",
        "sil", "dil", "bpl", "spl",
        "cs",  "ds",  "es",  "fs",  "gs",  "ss",
        "st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7"};
    if (registers.contains(token)) {
        return true;
    }
    if (token.startsWith("xmm") || token.startsWith("ymm") || token.startsWith("zmm")) {
        return true;
    }
    // r8..r15 nas quatro larguras: r8, r8d, r8w, r8b.
    return token.startsWithChar('r') && token.length() >= 2 && token.length() <= 4 &&
           juce::CharacterFunctions::isDigit(token[1]);
}

bool isPunctuation(juce::juce_wchar c) {
    static const juce::String marks {",[]+*:()"};
    return marks.containsChar(c);
}

juce::Colour asmInkFor(const juce::String& token, bool first) {
    if (first) {
        return (token == "db" || token == "bad") ? palette::textMuted : palette::accentWarn;
    }
    if (token.length() == 1 && isPunctuation(token[0])) {
        return palette::alpha(palette::textOnDarkSub, 0.65f);
    }
    if (isNumberToken(token)) {
        return palette::okBright;
    }
    if (isRegisterToken(token)) {
        return palette::textOnDark;
    }
    return palette::textOnDarkSub;
}

} // namespace

HexGrid::HexGrid() {
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    font_ = fonts().sans(13.5f);

    setName("Grelha de bytes do binario");
    setHelpText(
        "Cada celula e' um byte do ficheiro, com o endereco a esquerda, o "
        "caractere ASCII e a instrucao ASM a direita. Clique num byte, na "
        "coluna ASCII ou na barra de rolagem para saltar. Com a roda do rato "
        "desloca-se o ficheiro; Shift rola mais depressa.");
}

void HexGrid::setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize) {
    bytes_ = bytes;
    fileSize_ = fileSize;

    topByte_ = 0;
    rebuildMetrics();
    clampViewport();
    repaint();
}

void HexGrid::setDisassemblyEnabled(bool enabled) {
    if (asmEnabled_ == enabled) {
        return;
    }
    asmEnabled_ = enabled;
    asmTopByte_ = 0;
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

    const auto visibleRows = static_cast<std::uint64_t>(metrics_.rows) * kBytesPerRow;
    metrics_.showRail = fileSize_ > visibleRows;
    const auto railSpace = metrics_.showRail ? kRailWidth + kRailGap : 0.0f;
    const auto contentWidth = area.getWidth() - railSpace;

    metrics_.showAscii = contentWidth >= 700.0f;
    metrics_.showAsm = asmEnabled_ && contentWidth >= 880.0f;

    const auto gutterWidth = juce::jlimit(120.0f, 172.0f, contentWidth * 0.16f);
    const auto asciiWidth = metrics_.showAscii
                                ? juce::jlimit(150.0f, 216.0f, contentWidth * 0.15f)
                                : 0.0f;
    const auto asmWidth = metrics_.showAsm
                              ? juce::jlimit(200.0f, 430.0f, contentWidth * 0.26f)
                              : 0.0f;
    const auto gaps = (metrics_.showAscii ? 1 : 0) + (metrics_.showAsm ? 1 : 0);
    const auto bytesWidth = contentWidth - gutterWidth - asciiWidth - asmWidth -
                            static_cast<float>(gaps) * kInnerGap;

    metrics_.gutterWidth = gutterWidth;
    metrics_.cellAdvance = juce::jmax(22.0f, bytesWidth / static_cast<float>(kBytesPerRow));
    metrics_.cellWidth = juce::jmax(17.0f, metrics_.cellAdvance - 5.0f);
    metrics_.asciiCharWidth = metrics_.showAscii ? asciiWidth / 16.0f : 0.0f;
    metrics_.asciiWidth = asciiWidth;
    metrics_.asmWidth = asmWidth;

    const auto bodyTop = area.getY() + kHeaderHeight;
    const auto gridHeight = static_cast<float>(metrics_.rows) * kRowHeight;

    metrics_.gutter = {area.getX(), bodyTop, metrics_.gutterWidth, gridHeight};
    metrics_.bytes = {metrics_.gutter.getRight() + kInnerGap, bodyTop,
                      static_cast<float>(kBytesPerRow) * metrics_.cellAdvance, gridHeight};

    auto nextX = metrics_.bytes.getRight();
    if (metrics_.showAscii) {
        metrics_.ascii = {nextX + kInnerGap, bodyTop, asciiWidth, gridHeight};
        nextX = metrics_.ascii.getRight();
    } else {
        metrics_.ascii = {};
    }
    if (metrics_.showAsm) {
        metrics_.asmColumn = {nextX + kInnerGap, bodyTop, asmWidth, gridHeight};
    } else {
        metrics_.asmColumn = {};
    }

    metrics_.rail = metrics_.showRail
                        ? juce::Rectangle<float> {area.getRight() - kRailWidth + 1.0f, bodyTop,
                                                  kRailWidth - 1.0f, bodyHeight}
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
    g.fillRoundedRectangle(metrics_.rail, 3.0f);

    const auto thumb = thumbRect();
    if (!thumb.isEmpty()) {
        g.setColour(palette::alpha(palette::accent, 0.85f));
        g.fillRoundedRectangle(thumb, 3.0f);
    }
}

void HexGrid::mouseDown(const juce::MouseEvent& event) {
    if (onUserScrolled != nullptr) {
        onUserScrolled();
    }

    if (metrics_.showRail && metrics_.rail.expanded(4.0f).contains(event.position)) {
        draggingRail_ = true;
        scrollToFraction((event.position.y - metrics_.rail.getY()) /
                         juce::jmax(1.0f, metrics_.rail.getHeight()));
        return;
    }

    anchor_ = addressAt(event.position);
    selecting_ = false;
}

void HexGrid::mouseDrag(const juce::MouseEvent& event) {
    if (draggingRail_) {
        scrollToFraction((event.position.y - metrics_.rail.getY()) /
                         juce::jmax(1.0f, metrics_.rail.getHeight()));
        return;
    }

    if (anchor_ == std::numeric_limits<std::uint64_t>::max()) {
        return;
    }

    const auto address = addressAt(event.position);
    if (address == std::numeric_limits<std::uint64_t>::max() || address == anchor_) {
        return;
    }

    selecting_ = true;
    selStart_ = std::min(anchor_, address);
    selEnd_ = std::max(anchor_, address);
    repaint();
}

void HexGrid::mouseUp(const juce::MouseEvent&) {
    draggingRail_ = false;

    if (selecting_ && onRegionSelected != nullptr) {
        onRegionSelected(selStart_, selEnd_ + 1);
    } else if (anchor_ != std::numeric_limits<std::uint64_t>::max() &&
               onCellActivated != nullptr) {
        onCellActivated(anchor_);
    }

    anchor_ = std::numeric_limits<std::uint64_t>::max();
    if (selecting_) {
        selecting_ = false;
        repaint();
    }
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

    if (event.mods.isCtrlDown()) {
        const auto rows = static_cast<int>(metrics_.rows);
        scrollByLines(wheel.deltaY > 0.0f ? -rows * 16 : rows * 16);
        return;
    }

    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastWheelMs_ > 320) {
        wheelBoost_ = 1.0;
    }
    lastWheelMs_ = now;

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

    // o mesmo vidro preto da vista de onda: mudar de vista nao muda o material.
    g.setColour(palette::display);
    g.fillRect(bounds);
    g.setFont(font_);

    if (fileSize_ == 0 || bytes_ == nullptr || bytes_->empty()) {
        g.setColour(palette::textOnDarkSub);
        g.drawText("Sem binário", bounds.reduced(kPadding, 0.0f), juce::Justification::centred,
                   false);
        return;
    }

    paintColumnHeader(g);
    paintReadingColumns(g);

    rebuildAsmIfNeeded();

    for (int row = 0; row < metrics_.rows; ++row) {
        if (rowAddress(row) >= fileSize_) {
            break;
        }
        paintRow(g, row);
    }

    paintCaret(g);
    paintScrollRail(g);
}

// caixa opaca da cabeca de leitura, deslizando entre celulas.
void HexGrid::paintCaret(juce::Graphics& g) {
    if (bytes_ == nullptr || fileSize_ == 0 || readHead_ >= fileSize_) {
        return;
    }

    const auto row = static_cast<int>(
        (readHead_ - firstVisibleRowAddress()) / kBytesPerRow);
    const auto column = static_cast<int>(readHead_ % kBytesPerRow);
    const auto target = cellRect(static_cast<std::uint64_t>(column),
                                 static_cast<std::uint64_t>(row));

    if (!caretValid_) {
        caretX_ = target.getX();
        caretY_ = target.getY();
        caretValid_ = true;
    } else {
        caretX_ += (target.getX() - caretX_) * 0.35f;
        caretY_ += (target.getY() - caretY_) * 0.35f;
    }

    const auto caretRect = juce::Rectangle<float> {
        caretX_ - 2.0f, caretY_ + (target.getHeight() - 15.0f) * 0.5f,
        target.getWidth() + 4.0f, 15.0f};

    g.setColour(palette::ivory);
    g.fillRect(caretRect);
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
    if (metrics_.showAsm) {
        g.drawText("ASM", metrics_.asmColumn.withY(top).withHeight(kHeaderHeight),
                   juce::Justification::centredLeft, false);
    }
}

void HexGrid::paintReadingColumns(juce::Graphics& g) {
    const auto draw = [&g](juce::Rectangle<float> column) {
        const auto box = column.expanded(5.0f, 0.0f);
        g.setColour(palette::display);
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(palette::alpha(palette::displayBorder, 0.55f));
        g.drawRoundedRectangle(box, 3.0f, 1.0f);
    };

    if (metrics_.showAscii) {
        draw(metrics_.ascii);
    }
    if (metrics_.showAsm) {
        draw(metrics_.asmColumn);
    }
}

void HexGrid::paintRow(juce::Graphics& g, int row) {
    const auto top = metrics_.bytes.getY() + static_cast<float>(row) * kRowHeight;
    const auto area = getLocalBounds().toFloat().reduced(kPadding);

    g.setColour(palette::alpha(palette::displayBorder, 0.45f));
    g.drawHorizontalLine(juce::roundToInt(top), area.getX(), metrics_.bytes.getRight());

    paintGutter(g, row);
    paintBytes(g, row);
    paintAscii(g, row);
    paintAsm(g, row);
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

    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        const auto byteAddress = address + column;
        if (byteAddress >= fileSize_ || bytes_ == nullptr) {
            break;
        }

        const auto cell = cellRect(column, row);
        const auto byte = (*bytes_)[static_cast<std::size_t>(byteAddress)];
        const auto touched = byteAddress == readHead_;

        // a caixa opaca e' desenhada uma vez em paintCaret, deslizando.
        // aqui so' o texto fica invertido quando a celula e' a da cabeca.
        if (touched) {
            g.setFont(fonts().sans(13.5f, true));
            g.setColour(palette::charcoalDeep);
        } else {
            g.setFont(font_);
            g.setColour(palette::textOnDark);
        }
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

    // o fundo da coluna vive em paintReadingColumns; aqui so' o texto.
    g.setFont(font_);

    for (std::uint64_t column = 0; column < kBytesPerRow; ++column) {
        const auto byteAddress = address + column;
        if (byteAddress >= fileSize_ || bytes_ == nullptr) {
            break;
        }

        const auto character = toAscii((*bytes_)[static_cast<std::size_t>(byteAddress)]);
        const auto touched = byteAddress == readHead_;

        const auto cell = juce::Rectangle<float> {
            band.getX() + static_cast<float>(column) * metrics_.asciiCharWidth, band.getY(),
            metrics_.asciiCharWidth, kRowHeight};

        g.setColour(touched ? palette::accentSoft
                            : palette::alpha(palette::textOnDark, 0.72f));
        g.drawText(juce::String::charToString(static_cast<juce::juce_wchar>(character)),
                   cell, juce::Justification::centred, false);
    }
}

void HexGrid::rebuildAsmIfNeeded() {
    if (!metrics_.showAsm || bytes_ == nullptr || fileSize_ == 0) {
        asmItems_.clear();
        asmTopByte_ = 0;
        asmRows_ = 0;
        return;
    }

    if (asmTopByte_ == topByte_ && asmRows_ == metrics_.rows) {
        return;
    }

    const auto span = static_cast<std::uint64_t>(metrics_.rows + 2) * kBytesPerRow;
    const auto fromTop = juce::jmin(topByte_, fileSize_ - 1);
    const auto window = juce::jmin(span, fileSize_ - fromTop);

    asmItems_.clear();
    asmItems_.reserve(static_cast<std::size_t>(window / 2 + 1));

    std::uint64_t address = fromTop;
    std::uint64_t consumed = 0;
    while (consumed < window) {
        const auto decoded =
            disasm::decode(bytes_->data() + static_cast<std::size_t>(fromTop + consumed),
                           static_cast<std::size_t>(window - consumed));
        if (decoded.length == 0) {
            break;
        }

        AsmItem item;
        item.address = address;
        item.length = decoded.length;
        item.text = juce::String::fromUTF8(decoded.text.data(),
                                           static_cast<int>(decoded.text.size()));
        if (decoded.hasTarget) {
            const auto destination = static_cast<std::int64_t>(address) +
                                     static_cast<std::int64_t>(decoded.length) + decoded.target;
            if (destination >= 0 &&
                destination < static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()) *
                                      16) {
                item.hasTarget = true;
                item.target = static_cast<std::uint64_t>(destination);
            }
        }
        asmItems_.push_back(std::move(item));

        address += decoded.length;
        consumed += decoded.length;
    }

    asmTopByte_ = topByte_;
    asmRows_ = metrics_.rows;
}

void HexGrid::paintAsm(juce::Graphics& g, int row) {
    if (!metrics_.showAsm) {
        return;
    }

    const auto address = rowAddress(row);
    const auto band = metrics_.asmColumn.withY(metrics_.asmColumn.getY() +
                                               static_cast<float>(row) * kRowHeight)
                          .withHeight(kRowHeight);

    // o fundo da coluna vive em paintReadingColumns; aqui so' a instrucao.
    const AsmItem* started {nullptr};
    const AsmItem* previous {nullptr};
    for (const auto& item : asmItems_) {
        if (item.address < address) {
            previous = &item;
            continue;
        }
        if (item.address >= address + kBytesPerRow) {
            break;
        }
        if (started == nullptr) {
            started = &item;
        }
    }

    if (started == nullptr) {
        if (previous != nullptr && previous->address + previous->length > address) {
            g.setFont(fonts().mono(11.0f));
            g.setColour(palette::alpha(palette::textOnDarkSub, 0.55f));
            g.drawText("...", band.reduced(4.0f, 0.0f), juce::Justification::centredLeft,
                       false);
        }
        return;
    }

    auto text = started->text;
    if (started->hasTarget) {
        text += " 0x" + juce::String::toHexString(
                           static_cast<std::int64_t>(started->target));
    }

    paintAsmText(g, text, band.reduced(4.0f, 0.0f));
}

void HexGrid::paintAsmText(juce::Graphics& g,
                           const juce::String& text,
                           juce::Rectangle<float> area) const {
    if (text.isEmpty()) {
        return;
    }

    const auto font = fonts().mono(11.5f);

    juce::AttributedString run;
    run.setJustification(juce::Justification::centredLeft);
    run.setWordWrap(juce::AttributedString::none);

    const auto length = static_cast<int>(text.length());
    bool first = true;
    for (int i = 0; i < length;) {
        const auto c = text[i];
        if (juce::CharacterFunctions::isWhitespace(c)) {
            run.append(juce::String::charToString(c), font, palette::textOnDarkSub);
            ++i;
            continue;
        }

        if (isPunctuation(c)) {
            run.append(juce::String::charToString(c), font,
                       palette::alpha(palette::textOnDarkSub, 0.65f));
            ++i;
            continue;
        }

        int end = i;
        while (end < length && !juce::CharacterFunctions::isWhitespace(text[end]) &&
               !isPunctuation(text[end])) {
            ++end;
        }

        const auto token = text.substring(i, end);
        run.append(token, font, asmInkFor(token, first));
        first = false;
        i = end;
    }

    juce::TextLayout layout;
    layout.createLayout(run, area.getWidth());

    // a linha de 26 px centra o texto: o layout nasce no topo.
    const auto textHeight = 14.0f;
    layout.draw(g, area.withHeight(textHeight).withY(area.getY() +
                                                     (area.getHeight() - textHeight) * 0.5f));
}

juce::Rectangle<float> HexGrid::cellRect(std::uint64_t column, int row) const {
    const auto x = metrics_.bytes.getX() + static_cast<float>(column) * metrics_.cellAdvance;
    return {x, metrics_.bytes.getY() + static_cast<float>(row) * kRowHeight, metrics_.cellWidth,
            kRowHeight};
}

juce::String HexGrid::formatAddress(std::uint64_t address) const {
    return "0x" + juce::String::formatted("%0*llX", metrics_.gutterDigits,
                                          static_cast<unsigned long long>(address));
}

} // namespace opcoda
