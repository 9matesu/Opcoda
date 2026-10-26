#include "byte_display.h"

#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opcoda {
namespace {

// min/max quase nao diz nada sobre bytes: o texto de um .exe cobre 0x00 a 0xFF em
// quase todas as colunas. por isso o rms desenha o corpo -- aleatorio fica perto de
// 0,577, filler de zero ou de 0xFF estica a 1.

// a janela do grao em t = 0..1, com a mesma forma que o motor usa. quatro
// expressoes fechadas em vez de uma tabela: o preview desenha a silhueta e nao
// precisa de alocar nada.
float windowEnvelope(int type, float t) noexcept {
    constexpr float twoPi = 6.28318530718f;
    const auto x = std::clamp(t, 0.0f, 1.0f);

    switch (type) {
        case 1: {
            const auto d = (x - 0.5f) / 0.18f;
            return std::exp(-0.5f * d * d);
        }
        case 2:
            return 0.54f - 0.46f * std::cos(twoPi * x);
        case 3:
            return 0.42f - 0.5f * std::cos(twoPi * x) + 0.08f * std::cos(2.0f * twoPi * x);
        default:
            return 0.5f - 0.5f * std::cos(twoPi * x);
    }
}

} // namespace

ByteDisplay::ByteDisplay() {
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);

    font_ = fonts().sans(10.0f);

    setName("Display do material binario");
    setHelpText(
        "Quatro vistas: forma de onda, grelha de bytes, curva de entropia e "
        "matriz de modulacao. Clique para mover a cabeca de leitura; alt+click "
        "acrescenta ou tira um marcador, e click sobre a bandeira salta para "
        "ele. Com o teclado: setas movem o cursor, Enter ativa o byte, Esc "
        "volta a cabeca de leitura, espaco liga e desliga a reproducao, 1 a 4 "
        "mudam de vista, + e - zoom, 0 enquadra a regiao e A alinha a regiao a "
        "uma secao PE.");

    hex_.onCellActivated = [this](std::uint64_t address) {
        if (onAddressActivated != nullptr) {
            onAddressActivated(address);
        }
    };
    hex_.onSnapRequested = [this] {
        if (onSnapRequested != nullptr) {
            onSnapRequested();
        }
    };
    hex_.onUserScrolled = [this] { repaint(); };

    addAndMakeVisible(hex_);
    addAndMakeVisible(matrix_);
    applyModeVisibility();
}

void ByteDisplay::applyModeVisibility() {
    const auto hexMode = mode_ == ViewMode::hex;
    const auto matrixMode = mode_ == ViewMode::matrix;

    hex_.setVisible(hexMode);
    hex_.setInterceptsMouseClicks(hexMode, hexMode);
    matrix_.setVisible(matrixMode);
}

void ByteDisplay::setSource(const std::vector<std::uint8_t>* bytes,
                            std::uint64_t fileSize) {
    bytes_ = bytes;
    fileSize_ = fileSize;

    cacheValid_ = false;
    overviewValid_ = false;
    hexColumnsValid_ = false;
    hexHeadScrolled_ = false;

    hex_.setSource(bytes, fileSize);
    // os marcadores sao do material: ficheiro novo, raspas fora.
    bookmarks_.clear();
    repaint();
}

void ByteDisplay::toggleBookmarkAt(std::uint64_t address) {
    if (regionEnd_ <= regionStart_ || bytes_ == nullptr || bytes_->empty()) {
        return;
    }
    address = std::clamp(address, regionStart_, highestUsableAddress());

    // dentro de 0.4% da regiao conta como o mesmo ponto: alternar por cima de
    // uma bandeira existente tira-a, em vez de enfileirar duas.
    const auto tolerance =
        std::max<std::uint64_t>(1, (regionEnd_ - regionStart_) / 250);
    auto it = std::lower_bound(bookmarks_.begin(), bookmarks_.end(), address);
    if (it != bookmarks_.end() && *it >= address && *it - address <= tolerance) {
        bookmarks_.erase(it);
        repaint();
        return;
    }
    if (it != bookmarks_.begin() && address - *(it - 1) <= tolerance) {
        bookmarks_.erase(it - 1);
        repaint();
        return;
    }
    bookmarks_.insert(it, address);
    repaint();
}

std::uint64_t ByteDisplay::bookmarkAt(float x,
                                      const juce::Rectangle<float>& area) const noexcept {
    if (area.getWidth() <= 0.0f || regionEnd_ <= regionStart_ || bookmarks_.empty()) {
        return std::numeric_limits<std::uint64_t>::max();
    }

    // o salto e' por pixel: 6 px de raio, o mais perto ganha. fora de vista nao
    // salta, que o gesto e' de olho.
    std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
    float bestDistance = 6.0f;
    for (const auto bookmark : bookmarks_) {
        const auto fraction = fractionOf(bookmark);
        if (fraction < 0.0f || fraction > 1.0f) {
            continue;
        }
        const auto distance = std::abs(area.getX() + fraction * area.getWidth() - x);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = bookmark;
        }
    }
    return best;
}

void ByteDisplay::paintBookmarks(juce::Graphics& g, const juce::Rectangle<float>& area) {
    if (bookmarks_.empty() || area.getWidth() <= 0.0f || regionEnd_ <= regionStart_) {
        return;
    }

    for (const auto bookmark : bookmarks_) {
        const auto fraction = fractionOf(bookmark);
        if (fraction < 0.0f || fraction > 1.0f) {
            continue;
        }
        const auto x = area.getX() + fraction * area.getWidth();

        // fio por baixo da bandeira: diz onde o salto cai sem depender da cor.
        g.setColour(palette::alpha(palette::accent, 0.10f));
        g.drawLine(x, area.getY() + 8.0f, x, area.getBottom(), 1.0f);

        juce::Path flag;
        flag.addTriangle(x - 4.0f, area.getY() + 1.0f, x + 4.0f, area.getY() + 1.0f,
                         x, area.getY() + 9.0f);
        g.setColour(palette::alpha(palette::accent, 0.90f));
        g.fillPath(flag);
        g.setColour(palette::alpha(palette::ivory, 0.55f));
        g.drawLine(x, area.getY() + 1.0f, x, area.getY() + 9.0f, 1.0f);
    }
}

void ByteDisplay::setParamPreview(const ParamPreview& preview) {
    if (previewValid_ && preview_ == preview) {
        return;
    }
    preview_ = preview;
    previewValid_ = true;
    repaint();
}

void ByteDisplay::setRegion(std::uint64_t start, std::uint64_t end) {
    const auto clampedEnd = juce::jmax(end, start);
    if (clampedEnd == regionEnd_ && start == regionStart_) {
        return;
    }

    regionStart_ = start;
    regionEnd_ = clampedEnd;

    if (clampedEnd != cachedRegionEnd_ || start != cachedRegionStart_) {
        cacheValid_ = false;
    }
    overviewValid_ = false;
    hexColumnsValid_ = false;

    viewStart_ = 0.0f;
    viewLength_ = 1.0f;

    hex_.setRegion(regionStart_, regionEnd_);

    grainTrails_.clear();
    repaint();
}

void ByteDisplay::tickAnimation(float deltaSeconds) {
    if (pendingScrub_ != std::numeric_limits<std::uint64_t>::max() &&
        onAddressActivated != nullptr) {
        onAddressActivated(pendingScrub_);
        pendingScrub_ = std::numeric_limits<std::uint64_t>::max();
    }

    if (!playheadVisible_) {
        playheadFraction_.jumpTo(0.0f);
    }

    const auto before = readHeadFraction_.value() + playheadFraction_.value() +
                        modeFade_.value();

    readHeadFraction_.tick(deltaSeconds, kReadHeadHalfLife);
    playheadFraction_.tick(deltaSeconds, kPlayheadHalfLife);
    modeFade_.tick(deltaSeconds, kModeHalfLife);

    const auto after = readHeadFraction_.value() + playheadFraction_.value() +
                       modeFade_.value();
    if (std::abs(after - before) > 1.0e-4f) {
        repaint();
    }
}

void ByteDisplay::setGrains(
    const std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices>& views, int count) {
    const bool drawable = bytes_ != nullptr && !bytes_->empty() &&
                          regionEnd_ > regionStart_;
    const auto toRead = drawable ? std::clamp(count, 0, dsp::GrainTelemetry::kMaxVoices) : 0;

    for (int i = 0; i < toRead; ++i) {
        const auto& view = views[static_cast<std::size_t>(i)];
        grainTrails_.push(view.voice, view.position);
    }

    repaint();
}

void ByteDisplay::setMode(ViewMode mode) {
    if (mode == mode_) {
        return;
    }

    grainTrails_.clear();
    mode_ = mode;

    applyModeVisibility();
    layoutHex();

    // ao entrar no hex, uma unica centralizacao na cabeca de leitura. nao e' um
    // follow: acontece quando a pessoa escolhe a vista, e depois o rato manda.
    if (mode == ViewMode::hex) {
        hex_.scrollToAddress(readHead_);
        hexHeadScrolled_ = true;
    }

    repaint();
}

int ByteDisplay::entropyBucket(float bits) noexcept {
    if (bits < 3.0f) {
        return 0;
    }
    return bits < 6.0f ? 1 : 2;
}

juce::Rectangle<float> ByteDisplay::entropyBandBounds() const noexcept {
    if (mode_ != ViewMode::hex) {
        return {};
    }
    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    if (full.getHeight() < kEntropyBandHeight + 40.0f) {
        return {};
    }
    return {full.getX(), full.getBottom() - kEntropyBandHeight, full.getWidth(),
            kEntropyBandHeight};
}

void ByteDisplay::layoutHex() {
    const auto band = entropyBandBounds();
    hex_.setBounds(band.isEmpty() ? getLocalBounds()
                                  : getLocalBounds().withTrimmedBottom(
                                        juce::roundToInt(kEntropyBandHeight + kPadding + 6.0f)));
}

void ByteDisplay::resized() {
    layoutHex();
    matrix_.setBounds(getLocalBounds().reduced(juce::roundToInt(kPadding)));

    const auto wanted = columnCountForWidth();
    if (wanted != cachedColumnCount_) {
        cacheValid_ = false;
    }
}

std::uint32_t ByteDisplay::columnCountForWidth() const noexcept {
    const auto width = getLocalBounds().toFloat().reduced(kPadding).getWidth();
    const auto scaled = static_cast<float>(width);
    return static_cast<std::uint32_t>(std::clamp(scaled, 16.0f, 8192.0f));
}

void ByteDisplay::rebuildColumnsIfNeeded() {
    const auto wanted = columnCountForWidth();
    if (cacheValid_ && wanted == cachedColumnCount_ && regionStart_ == cachedRegionStart_ &&
        regionEnd_ == cachedRegionEnd_ && viewStart_ == cachedViewStart_ &&
        viewLength_ == cachedViewLength_) {
        return;
    }

    columns_.clear();

    if (bytes_ != nullptr && !bytes_->empty() && regionEnd_ > regionStart_) {
        const auto [viewStart, viewEnd] = viewByteRange();
        if (viewEnd > viewStart) {
            static_cast<void>(pe::reduceToColumns(bytes_->data(), bytes_->size(),
                                                  viewStart, viewEnd, wanted, columns_));
        }
    }

    cachedColumnCount_ = wanted;
    cachedRegionStart_ = regionStart_;
    cachedRegionEnd_ = regionEnd_;
    cachedViewStart_ = viewStart_;
    cachedViewLength_ = viewLength_;
    cacheValid_ = true;
}

void ByteDisplay::rebuildOverviewIfNeeded() {
    if (overviewValid_ && overviewRegionStart_ == regionStart_ &&
        overviewRegionEnd_ == regionEnd_) {
        return;
    }
    overviewColumns_.clear();
    if (bytes_ != nullptr && !bytes_->empty() && regionEnd_ > regionStart_) {
        static constexpr std::uint32_t kOverviewColumns {192};
        static_cast<void>(pe::reduceToColumns(bytes_->data(), bytes_->size(),
                                              regionStart_, regionEnd_,
                                              kOverviewColumns, overviewColumns_));
    }
    overviewRegionStart_ = regionStart_;
    overviewRegionEnd_ = regionEnd_;
    overviewValid_ = true;
}

void ByteDisplay::rebuildHexColumnsIfNeeded() {
    const auto band = entropyBandBounds();
    if (band.isEmpty() || bytes_ == nullptr || fileSize_ == 0) {
        hexColumnsValid_ = false;
        return;
    }

    const auto wanted = static_cast<std::uint32_t>(
        std::clamp(band.getWidth() / 10.0f, 24.0f, 512.0f));

    if (hexColumnsValid_ && hexColumnsFileSize_ == fileSize_ &&
        hexColumnsWidth_ == wanted) {
        return;
    }

    hexColumns_.clear();
    static_cast<void>(pe::reduceToColumns(bytes_->data(), bytes_->size(), 0, fileSize_,
                                          wanted, hexColumns_));

    hexColumnsFileSize_ = fileSize_;
    hexColumnsWidth_ = wanted;
    hexColumnsValid_ = true;
}

std::pair<std::uint64_t, std::uint64_t> ByteDisplay::viewByteRange() const noexcept {
    if (regionEnd_ <= regionStart_) {
        return {regionStart_, regionStart_};
    }
    const auto length = regionEnd_ - regionStart_;
    const auto start = regionStart_ + static_cast<std::uint64_t>(viewStart_ * length);
    const auto end = regionStart_ + static_cast<std::uint64_t>(
                                          std::min(1.0f, viewStart_ + viewLength_) * length);
    return {std::min(start, regionEnd_), std::max(end, std::min(start + 1, regionEnd_))};
}

float ByteDisplay::regionFractionAt(float x, float areaX, float areaWidth) const noexcept {
    if (areaWidth <= 0.0f) {
        return -1.0f;
    }
    const auto screen = (x - areaX) / areaWidth;
    if (screen < 0.0f || screen > 1.0f) {
        return -1.0f;
    }
    return viewStart_ + screen * viewLength_;
}

void ByteDisplay::zoomAround(float factor, float centreFraction) {
    if (regionEnd_ <= regionStart_ || !(factor > 0.0f) ||
        centreFraction < 0.0f || centreFraction > 1.0f) {
        return;
    }
    const auto regionLength = regionEnd_ - regionStart_;
    const auto minLength = std::min(1.0f, static_cast<float>(kMinViewBytes) /
                                             static_cast<float>(regionLength));
    const auto anchored = std::clamp(viewStart_ + centreFraction * viewLength_, 0.0f, 1.0f);
    const auto newLength = std::clamp(viewLength_ * factor, minLength, 1.0f);
    viewStart_ = std::clamp(anchored - centreFraction * newLength, 0.0f, 1.0f - newLength);
    viewLength_ = newLength;
    cacheValid_ = false;
    repaint();
}

void ByteDisplay::zoomIn() {
    zoomAround(0.5f, 0.5f);
}

void ByteDisplay::zoomOut() {
    zoomAround(2.0f, 0.5f);
}

void ByteDisplay::fitView() {
    if (viewStart_ == 0.0f && viewLength_ == 1.0f) {
        return;
    }
    viewStart_ = 0.0f;
    viewLength_ = 1.0f;
    cacheValid_ = false;
    repaint();
}

void ByteDisplay::scrollHexByLines(int lines) {
    hex_.scrollByLines(lines);
}

void ByteDisplay::scrollHexPage(int pages) {
    hex_.scrollByLines(pages * 16);
}

void ByteDisplay::scrollHexToStart() {
    hex_.scrollToAddress(0);
}

void ByteDisplay::scrollHexToEnd() {
    hex_.scrollToAddress(fileSize_ > 0 ? fileSize_ - 1 : 0);
}

double ByteDisplay::fractionFor(std::uint64_t address) const noexcept {
    return pe::positionForByte(address, pe::ByteRange {regionStart_, regionEnd_});
}

float ByteDisplay::fractionOf(std::uint64_t address) const noexcept {
    if (viewLength_ <= 0.0f) {
        return -1.0f;
    }
    const auto region = static_cast<float>(fractionFor(address));
    return (region - viewStart_) / viewLength_;
}

void ByteDisplay::setReadHead(std::uint64_t address) {
    readHead_ = address;
    hex_.setReadHead(address);

    if (mode_ == ViewMode::hex && !hexHeadScrolled_ && bytes_ != nullptr &&
        regionEnd_ > regionStart_) {
        // uma unica centralizacao quando o primeiro material chega. nao e' follow:
        // depois disto o rato e' que manda no topo do hex.
        hex_.scrollToAddress(address);
        hexHeadScrolled_ = true;
    }

    if (!caretMoved_) {
        caret_ = address;
    }

    const auto fraction = fractionOf(address);
    if (!readHeadAnimated_) {
        readHeadFraction_.jumpTo(fraction);
        readHeadAnimated_ = true;
    } else {
        readHeadFraction_.set(fraction);
    }

    repaint();
}

void ByteDisplay::setCaret(std::uint64_t address) {
    caret_ = std::clamp(address, regionStart_, highestUsableAddress());
    caretMoved_ = caret_ != readHead_;
    if (mode_ == ViewMode::hex && !hex_.isAddressVisible(caret_)) {
        hex_.scrollToAddress(caret_);
    }
    repaint();
}

std::uint64_t ByteDisplay::highestUsableAddress() const noexcept {
    return regionEnd_ >= regionStart_ + 2 ? regionEnd_ - 2 : regionStart_;
}

void ByteDisplay::focusGained(juce::Component::FocusChangeType) {
    repaint();
}

void ByteDisplay::focusLost(juce::Component::FocusChangeType) {
    repaint();
}

void ByteDisplay::handleNavigationKey(const juce::KeyPress& key) {
    if (regionEnd_ <= regionStart_) {
        return;
    }

    constexpr std::uint64_t kStep {1};
    constexpr std::uint64_t kRow {16};
    constexpr std::uint64_t kPage {1024};

    const auto upper = highestUsableAddress();
    const auto shift = key.getModifiers().isShiftDown();
    const auto code = key.getKeyCode();
    std::uint64_t target = caret_;
    bool handled = true;

    if (code == juce::KeyPress::leftKey) {
        target = caret_ - std::min(caret_, shift ? kRow : kStep);
    } else if (code == juce::KeyPress::rightKey) {
        target = caret_ + (shift ? kRow : kStep);
    } else if (code == juce::KeyPress::upKey) {
        target = caret_ - std::min(caret_, kRow);
    } else if (code == juce::KeyPress::downKey) {
        target = caret_ + kRow;
    } else if (code == juce::KeyPress::pageUpKey) {
        target = caret_ - std::min(caret_, kPage);
    } else if (code == juce::KeyPress::pageDownKey) {
        target = caret_ + kPage;
    } else if (code == juce::KeyPress::homeKey) {
        target = regionStart_;
    } else if (code == juce::KeyPress::endKey) {
        target = upper;
    } else if (code == juce::KeyPress::returnKey) {
        caretMoved_ = false;
        if (onAddressActivated != nullptr) {
            onAddressActivated(caret_);
        }
        return;
    } else {
        handled = false;
    }

    if (!handled) {
        return;
    }

    setCaret(std::clamp(target, regionStart_, upper));
}

bool ByteDisplay::handleTransportKey(const juce::KeyPress& key) {
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
        if (onToggleTransport != nullptr) {
            onToggleTransport();
            return true;
        }
        return false;
    }

    if (isTypedCharacter(key, 'A') && onSnapRequestedFromKey != nullptr) {
        onSnapRequestedFromKey();
        return true;
    }

    return false;
}

bool ByteDisplay::keyPressed(const juce::KeyPress& key) {
    if (key.getKeyCode() == juce::KeyPress::escapeKey) {
        setCaret(readHead_);
        return true;
    }

    if (handleTransportKey(key)) {
        return true;
    }

    if (isDigitKey(key, '1')) {
        setMode(ViewMode::waveform);
        return true;
    }
    if (isDigitKey(key, '2')) {
        setMode(ViewMode::hex);
        return true;
    }
    if (isDigitKey(key, '3')) {
        setMode(ViewMode::entropy);
        return true;
    }
    if (isDigitKey(key, '4')) {
        setMode(ViewMode::matrix);
        return true;
    }

    if (isTypedCharacter(key, '+') || isTypedCharacter(key, '=')) {
        zoomIn();
        return true;
    }
    if (isTypedCharacter(key, '-')) {
        zoomOut();
        return true;
    }
    if (isTypedCharacter(key, '0')) {
        fitView();
        return true;
    }

    handleNavigationKey(key);
    return true;
}

void ByteDisplay::paint(juce::Graphics& g) {
    g.fillAll(palette::displayPanel);

    if (bytes_ == nullptr || bytes_->empty() || regionEnd_ <= regionStart_) {
        const auto area = getLocalBounds().toFloat().reduced(22.0f);
        const auto centreY = area.getCentreY();

        g.setColour(palette::alpha(palette::textOnDarkSub, 0.22f));
        g.drawHorizontalLine(juce::roundToInt(centreY), area.getX() + 28.0f,
                             area.getRight() - 28.0f);

        constexpr float kCorner {18.0f};
        const auto frame = area.reduced(34.0f, 28.0f);
        g.setColour(palette::alpha(palette::accent, 0.35f));
        const juce::Point<float> corners[] {
            frame.getTopLeft(), frame.getTopRight(),
            frame.getBottomLeft(), frame.getBottomRight()};
        for (int i = 0; i < 4; ++i) {
            const auto corner = corners[i];
            const auto dx = (i % 2 == 0) ? kCorner : -kCorner;
            const auto dy = (i < 2) ? kCorner : -kCorner;
            g.drawLine(corner.x, corner.y, corner.x + dx, corner.y, 1.0f);
            g.drawLine(corner.x, corner.y, corner.x, corner.y + dy, 1.0f);
        }

        g.setFont(fonts().sans(16.0f, true));
        g.setColour(palette::textOnDark);
        g.drawText("NO SOURCE", area.withTrimmedTop(area.getHeight() * 0.36f),
                   juce::Justification::centredTop, false);

        g.setFont(fonts().sans(9.0f));
        g.setColour(palette::textOnDarkSub);
        g.drawText("DROP .EXE / .DLL / .BIN  OR  PRESS L",
                   area.withTrimmedTop(area.getHeight() * 0.36f + 22.0f),
                   juce::Justification::centredTop, false);
        return;
    }

    rebuildColumnsIfNeeded();
    rebuildOverviewIfNeeded();

    switch (mode_) {
        case ViewMode::waveform: paintWaveform(g); break;
        case ViewMode::entropy: paintEntropy(g); break;
        case ViewMode::hex: paintEntropyBand(g); break;
        case ViewMode::matrix: break;  // a vista MATRIX e' o filho, pintado por cima.
    }

    if (mode_ == ViewMode::waveform) {
        paintGrains(g, plotBounds());
    } else if (mode_ == ViewMode::entropy) {
        paintGrains(g, entropyPlotBounds());
    }

    if (mode_ != ViewMode::hex && mode_ != ViewMode::matrix) {
        paintSectionTicks(g);
    }
}

void ByteDisplay::paintEntropyBand(juce::Graphics& g) {
    const auto band = entropyBandBounds();
    if (band.isEmpty()) {
        return;
    }

    rebuildHexColumnsIfNeeded();
    if (hexColumns_.empty()) {
        return;
    }

    // calha do mapa, separada do hex por um filete.
    g.setColour(palette::display);
    g.fillRoundedRectangle(band, 3.0f);
    g.setColour(palette::alpha(palette::displayBorder, 0.95f));
    g.drawRoundedRectangle(band, 3.0f, 1.0f);
    g.setColour(palette::alpha(palette::brass, 0.35f));
    g.drawHorizontalLine(juce::roundToInt(band.getY()) - 4,
                         band.getX(), band.getRight());

    g.setFont(fonts().sans(8.5f, true));
    g.setColour(palette::textOnDarkSub);
    g.drawText("ENTROPY  0-8 bits/byte", band.reduced(8.0f, 3.0f).withHeight(11.0f),
               juce::Justification::centredLeft, false);

    const auto plot = band.reduced(8.0f, 0.0f).withTrimmedTop(16.0f).withTrimmedBottom(5.0f);
    if (plot.getHeight() <= 0.0f) {
        return;
    }

    const juce::Colour inks[3] {palette::accentSoft, palette::accentWarn, palette::accent};
    const auto count = hexColumns_.size();
    const auto barStep = plot.getWidth() / static_cast<float>(count);
    const auto barWidth = juce::jmax(2.0f, barStep - 2.0f);

    for (std::size_t i = 0; i < count; ++i) {
        const auto bits = std::clamp(hexColumns_[i].entropyBits, 0.0f, 8.0f);
        const auto height = bits / 8.0f * plot.getHeight();
        if (height <= 0.5f) {
            continue;
        }
        const auto x = plot.getX() + static_cast<float>(i) * barStep;
        g.setColour(palette::alpha(inks[entropyBucket(bits)], 0.9f));
        g.fillRect(juce::Rectangle<float> {x, plot.getBottom() - height, barWidth, height});
    }

    // regiao: um filete, nao uma faixa. o material que soa e' o que interessa.
    if (fileSize_ > 0 && regionEnd_ > regionStart_) {
        const auto left = plot.getX() + static_cast<float>(regionStart_) /
                                             static_cast<float>(fileSize_) * plot.getWidth();
        const auto right = plot.getX() + static_cast<float>(
                                              std::min(regionEnd_, fileSize_)) /
                                              static_cast<float>(fileSize_) * plot.getWidth();
        g.setColour(palette::accent);
        g.drawRect(juce::Rectangle<float> {left, plot.getY(),
                                           juce::jmax(2.0f, right - left),
                                           plot.getHeight()},
                   1.0f);
    }
}

void ByteDisplay::paintGrainsInHex(juce::Graphics& g) {
    if (regionEnd_ <= regionStart_) {
        return;
    }

    for (int voice = 0; voice < view::GrainTrailSet::kMaxVoices; ++voice) {
        if (!grainTrails_.active(voice)) {
            continue;
        }

        const auto cell = hex_.cellRectForAddress(grainTrails_.address(voice));
        if (cell.isEmpty()) {
            continue;
        }

        g.setColour(palette::alpha(juce::Colours::white, 0.26f));
        g.fillRect(cell.reduced(1.0f));
        g.setColour(palette::alpha(juce::Colours::white, 0.92f));
        g.drawRect(cell.reduced(1.0f), 1.5f);
    }
}

void ByteDisplay::paintGrains(juce::Graphics& g, const juce::Rectangle<float>& area) {
    for (int voice = 0; voice < view::GrainTrailSet::kMaxVoices; ++voice) {
        if (!grainTrails_.active(voice)) {
            continue;
        }

        paintGrain(g, area, grainTrails_.trail(voice), 1.0f);
    }
}

void ByteDisplay::paintGrain(juce::Graphics& g,
                             const juce::Rectangle<float>& area,
                             const view::GrainTrailSet::Trail& trail,
                             float gain) {
    const auto xOf = [this, &area](float regionFraction) {
        if (viewLength_ <= 0.0f) {
            return -1.0f;
        }
        const auto screen = (regionFraction - viewStart_) / viewLength_;
        if (screen <= 0.0f || screen >= 1.0f) {
            return -1.0f;
        }
        return area.getX() + screen * area.getWidth();
    };

    const auto newestX = xOf(trail.positions[0]);
    if (newestX < 0.0f) {
        return;
    }

    const auto centreY = area.getCentreY();
    const auto height = juce::jmax(6.0f, area.getHeight() * 0.30f * gain);
    const auto top = centreY - height * 0.5f;

    float oldestX = newestX;
    for (int f = 1; f < trail.count; ++f) {
        const auto x = xOf(trail.positions[static_cast<std::size_t>(f)]);
        if (x >= 0.0f) {
            oldestX = std::min(oldestX, x);
        }
    }

    const auto span = newestX - oldestX;
    if (trail.count > 1 && span > 0.5f) {
        constexpr int kSteps {6};
        const auto step = juce::jmax(1.0f, std::ceil(span / static_cast<float>(kSteps)));
        for (int s = 0; s < kSteps; ++s) {
            const auto t = static_cast<float>(s + 1) / static_cast<float>(kSteps);
            const auto x0 = oldestX + step * static_cast<float>(s);
            if (x0 >= newestX) {
                break;
            }
            const auto width = juce::jmin(step, newestX - x0);
            g.setColour(palette::alpha(juce::Colours::white, 0.42f * gain * t * t));
            g.fillRect(juce::Rectangle<float> {x0, top, width, height});
        }
    }

    g.setColour(palette::alpha(juce::Colours::white, 0.14f * gain));
    g.fillRoundedRectangle(juce::Rectangle<float> {newestX - kGrainGlowWidth * 0.5f,
                                                   top - 1.5f, kGrainGlowWidth,
                                                   height + 3.0f},
                           kGrainGlowWidth * 0.5f);
    g.setColour(palette::alpha(juce::Colours::white, 0.9f * gain));
    g.fillRoundedRectangle(juce::Rectangle<float> {newestX - kGrainCoreWidth * 0.5f, top,
                                                   kGrainCoreWidth, height},
                           kGrainCoreWidth * 0.5f);
}

void ByteDisplay::paintSectionTicks(juce::Graphics& g) {
    if (sections_.empty() || regionEnd_ <= regionStart_) {
        return;
    }

    const auto area = plotBounds();
    if (area.getWidth() <= 0.0f) {
        return;
    }

    for (const auto& section : sections_) {
        if (section.rawSize == 0) {
            continue;
        }
        const auto start = static_cast<std::uint64_t>(section.rawOffset);
        if (start < regionStart_ || start >= regionEnd_) {
            continue;
        }

        const auto screen = fractionOf(start);
        if (screen < 0.0f || screen > 1.0f) {
            continue;
        }
        const auto x = area.getX() + screen * area.getWidth();

        g.setColour(palette::alpha(palette::brass, 0.55f));
        g.fillRect(juce::Rectangle<float> {x, area.getY(), 1.0f, area.getHeight()});
    }
}

void ByteDisplay::paintOverChildren(juce::Graphics& g) {
    if (bytes_ == nullptr || bytes_->empty()) {
        return;
    }

    if (mode_ == ViewMode::matrix) {
        // na matriz nada por cima: os combos e a barra sao o conteudo.
        return;
    }

    juce::Rectangle<float> area = getLocalBounds().toFloat().reduced(kPadding);
    if (mode_ == ViewMode::waveform) {
        area = plotBounds();
    } else if (mode_ == ViewMode::entropy) {
        area = entropyPlotBounds();
    }

    if (mode_ == ViewMode::hex) {
        paintGrainsInHex(g);
        // no hex a cabeca de reproducao vive na faixa de entropia: sobre as
        // linhas de bytes seria mais uma barra clara no meio do texto.
        paintPlayhead(g, entropyBandBounds());
    } else {
        paintReadHead(g, area);
        paintPlayhead(g, area);
    }

    if (hasKeyboardFocus(true) && mode_ != ViewMode::hex) {
        const auto screen = fractionOf(caret_);
        if (screen >= 0.0f && screen <= 1.0f) {
            const auto caretX = area.getX() + screen * area.getWidth();
            g.setColour(palette::ivory);
            g.drawRect(juce::Rectangle<float> {caretX - 3.0f, area.getY() + 2.0f, 6.0f,
                                               area.getHeight() - 4.0f},
                       1.0f);
        }
    }
}

void ByteDisplay::paintParamPreview(juce::Graphics& g,
                                    const juce::Rectangle<float>& area) {
    if (!previewValid_ || area.getWidth() <= 0.0f) {
        return;
    }

    const auto [viewStartByte, viewEndByte] = viewByteRange();
    const auto viewBytes = viewEndByte > viewStartByte ? viewEndByte - viewStartByte : 0;
    if (viewBytes == 0) {
        return;
    }

    const auto position = std::clamp(preview_.position, 0.0f, 1.0f);
    const auto screen = viewLength_ > 0.0f ? (position - viewStart_) / viewLength_ : -1.0f;
    if (screen < 0.0f || screen > 1.0f) {
        return;
    }

    const auto centreX = area.getX() + screen * area.getWidth();

    // a nuvem nunca e' mais estreita que 56 px: com spray a 0 a banda real seria
    // um risco de 1 px e as barrinhas desapareciam. o preview e' um esquema, nao
    // uma regua: mostra a relacao entre spray, densidade e tamanho do grao.
    const auto bandWidth = juce::jlimit(56.0f, area.getWidth(),
                                        juce::jmax(56.0f, preview_.spray * area.getWidth()));
    const auto band = juce::Rectangle<float> {
        juce::jlimit(area.getX(), area.getRight() - bandWidth,
                     centreX - bandWidth * 0.5f),
        area.getY(), bandWidth, area.getHeight()};

    // **branco, recto, opaco.** as agulhas competiam com o laranja da onda e
    // liam-se como parte do material. o marfim separa o que e' leitura do que e'
    // som, e as linhas rectas dizem "isto e' um cursor", nao "isto e' sinal".
    const auto ink = palette::ivory;
    const auto top = area.getY() + 3.0f;
    const auto bottom = area.getBottom() - 3.0f;

    // spray: a nuvem onde os graos podem nascer. banho fraco e dois filetes
    // verticais, que delimitam a zona sem a riscar de alto a baixo.
    g.setColour(palette::alpha(ink, 0.045f));
    g.fillRect(band);
    g.setColour(palette::alpha(ink, 0.38f));
    g.fillRect(juce::Rectangle<float> {band.getX(), top, 1.2f, bottom - top});
    g.fillRect(juce::Rectangle<float> {band.getRight() - 1.2f, top, 1.2f, bottom - top});

    // densidade: uma regua de tracinhos no fundo. as barras ao centro cruzavam a
    // onda e liam-se como sinal; no fundo sao escala, que e' o que sao.
    const auto centreY = area.getCentreY();
    const auto needleCount = juce::jlimit(
        3, 16, 3 + static_cast<int>(std::round(preview_.density * 0.07f)));
    const auto spacingPx = band.getWidth() / static_cast<float>(needleCount);
    for (int i = 0; i < needleCount; ++i) {
        const auto x = band.getX() + spacingPx * (static_cast<float>(i) + 0.5f);
        g.setColour(palette::alpha(ink, 0.9f));
        g.fillRect(juce::Rectangle<float> {x - 0.9f, area.getBottom() - 12.0f, 1.8f,
                                           9.0f});
    }

    // o grao: a janela desenhada com a mesma forma que o motor usa. a largura vem
    // do tamanho, o nivel da opacidade e o pitch inclina a silhueta para o lado
    // do desvio de afinacao.
    const auto level = juce::jlimit(0.0f, 1.0f, preview_.level);
    const auto grainWidth =
        juce::jlimit(16.0f, juce::jmax(16.0f, area.getWidth() * 0.35f),
                     16.0f + preview_.grainMs * 1.8f);
    const auto grainHeight = area.getHeight() * 0.44f;
    const auto pitchTilt = juce::jlimit(-1.0f, 1.0f, preview_.pitchSemitones / 48.0f);
    const auto spreadTilt = juce::jlimit(0.0f, 1.0f, preview_.pitchSpread / 24.0f);

    const auto drawWindow = [&](float cx, float tilt, float fillAlpha,
                                float lineAlpha) {
        // a silhueta nunca e' cortada pela borda do ecra: um grao na posicao 0
        // aparecia meio desenhado e lia-se como um borrao.
        const auto halfGrain = grainWidth * 0.5f;
        const auto anchorX = area.getWidth() > grainWidth
                                 ? juce::jlimit(area.getX() + halfGrain,
                                                area.getRight() - halfGrain, cx)
                                 : cx;

        juce::Path shape;
        constexpr int kSteps {48};
        for (int i = 0; i <= kSteps; ++i) {
            const auto t = static_cast<float>(i) / static_cast<float>(kSteps);
            const auto env = windowEnvelope(preview_.window, t);
            const auto x = anchorX - halfGrain + t * grainWidth +
                           tilt * env * grainWidth * 0.55f;
            const auto y = centreY - env * grainHeight * 0.5f;
            if (i == 0) {
                shape.startNewSubPath(x, y);
            } else {
                shape.lineTo(x, y);
            }
        }
        for (int i = kSteps; i >= 0; --i) {
            const auto t = static_cast<float>(i) / static_cast<float>(kSteps);
            const auto env = windowEnvelope(preview_.window, t);
            const auto x = anchorX - halfGrain + t * grainWidth +
                           tilt * env * grainWidth * 0.55f;
            shape.lineTo(x, centreY + env * grainHeight * 0.5f);
        }
        shape.closeSubPath();

        g.setColour(palette::alpha(ink, fillAlpha));
        g.fillPath(shape);
        if (lineAlpha > 0.0f) {
            g.setColour(palette::alpha(ink, lineAlpha));
            g.strokePath(shape, juce::PathStrokeType {1.2f});
        }
    };

    // pitch spread: uma sombra desviada, sem contorno. Duas copias contornadas
    // cruzavam-se e o desenho virava um no'. Com o spread a zero, nem aparece.
    if (spreadTilt > 0.2f) {
        drawWindow(centreX, pitchTilt + spreadTilt * 0.8f, 0.035f, 0.0f);
        drawWindow(centreX, pitchTilt - spreadTilt * 0.8f, 0.035f, 0.0f);
    }
    drawWindow(centreX, pitchTilt, 0.05f + 0.16f * level, 0.30f + 0.50f * level);

    // posicao: agulha recta, solida, por cima da nuvem. e' o unico traco que nao
    // se confunde com nada.
    g.setColour(ink);
    g.fillRect(juce::Rectangle<float> {centreX - 1.3f, area.getY(), 2.6f,
                                       area.getHeight()});

    // pan: um ponto no topo deslocado para o lado do canal. e' a unica leitura
    // stereo do preview, e nao ocupa uma caixa.
    const auto panX = centreX + std::clamp(preview_.pan, -1.0f, 1.0f) * area.getWidth() * 0.04f;
    g.setColour(ink);
    g.fillEllipse(panX - 2.5f, area.getY() + 2.0f, 5.0f, 5.0f);
}

void ByteDisplay::paintWaveform(juce::Graphics& g) {
    if (columns_.empty()) {
        return;
    }

    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto plot = plotBounds();
    if (plot.getWidth() <= 0.0f || plot.getHeight() <= 0.0f) {
        return;
    }

    paintRuler(g, {full.getX(), plot.getY() - kRulerHeight - 2.0f, full.getWidth(),
                   kRulerHeight});

    // **sem caixa de fundo.** o vidro do DisplayPanel ja' tem gradiente e sombra
    // interna; pintar outro rectangulo aqui desenhava uma moldura por dentro da
    // moldura, e a onda deixava de ser seamless. o que fica e' so' o material.
    const auto centreY = plot.getCentreY();
    const auto halfHeight = plot.getHeight() * 0.5f;
    const auto [viewStartByte, viewEndByte] = viewByteRange();

    if (sampleDotsVisible()) {
        paintSampleDots(g, plot, viewStartByte, viewEndByte);
    } else {
        // envelope reamostrado por pixel. varias colunas caem no mesmo pixel em
        // zoom-out; iterar as colunas cruas fazia o path ziguezaguear meio pixel
        // e o preenchimento abria riscos verticais.
        resampleEnvelope(std::max(2, static_cast<int>(std::ceil(plot.getWidth()))));
        const auto pixels = static_cast<int>(pixelMax_.size());
        const auto xAt = [&](int i) {
            return plot.getX() + static_cast<float>(i) + 0.5f;
        };

        juce::Path swing;
        swing.startNewSubPath(xAt(0), centreY - pixelMax_[0] * halfHeight);
        for (int i = 1; i < pixels; ++i) {
            swing.lineTo(xAt(i), centreY - pixelMax_[static_cast<std::size_t>(i)] * halfHeight);
        }
        for (int i = pixels; i-- > 0;) {
            swing.lineTo(xAt(i), centreY - pixelMin_[static_cast<std::size_t>(i)] * halfHeight);
        }
        swing.closeSubPath();

        // sombra projectada: 2 px abaixo, quase preta. da' corpo ao envelope.
        {
            auto shadow = swing;
            shadow.applyTransform(juce::AffineTransform::translation(0.0f, 2.0f));
            g.setColour(palette::alpha(juce::Colours::black, 0.42f));
            g.fillPath(shadow);
        }

        // **silhueta solida.** a versao de linha fina lia-se como osciloscopio
        // de poucos dados: dentro do envelope escuro, o sample nao tinha massa.
        // aqui o envelope enche com um gradiente vertical quente — brasa nas
        // bordas, nucleo claro no centro — e o contorno de marfim fecha a forma.
        juce::ColourGradient body {
            palette::alpha(palette::accent, 0.62f), plot.getCentreX(), plot.getY(),
            palette::alpha(palette::accent, 0.62f), plot.getCentreX(), plot.getBottom(),
            false};
        body.addColour(0.22f, palette::alpha(palette::accentWarn, 0.88f));
        body.addColour(0.50f, palette::alpha(palette::accentSoft, 0.98f));
        body.addColour(0.78f, palette::alpha(palette::accentWarn, 0.88f));
        g.setGradientFill(body);
        g.fillPath(swing);

        // contorno: marfim fino, sem halo largo. a forma e' o que le, nao o glow.
        g.setColour(palette::alpha(palette::ivory, 0.50f));
        g.strokePath(swing, juce::PathStrokeType {1.1f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded});

        paintRmsSilhouette(g, plot, centreY, halfHeight, 1.0f);
    }

    g.setColour(palette::alpha(juce::Colours::white, 0.12f));
    g.drawHorizontalLine(juce::roundToInt(centreY), plot.getX(), plot.getRight());

    paintBookmarks(g, plot);
    paintParamPreview(g, plot);

    paintSourceTag(g, plot);
    paintOverview(g);
}

bool ByteDisplay::sampleDotsVisible() const noexcept {
    if (regionEnd_ <= regionStart_ || bytes_ == nullptr) {
        return false;
    }
    const auto plotWidth = plotBounds().getWidth();
    if (plotWidth <= 0.0f) {
        return false;
    }
    const auto [viewStartByte, viewEndByte] = viewByteRange();
    const auto viewBytes =
        viewEndByte > viewStartByte ? (viewEndByte - viewStartByte) : 0;
    return viewBytes > 0 &&
           static_cast<float>(viewBytes) / plotWidth <= 2.0f;
}

void ByteDisplay::paintSampleDots(juce::Graphics& g,
                                  const juce::Rectangle<float>& plotArea,
                                  std::uint64_t viewStartByte,
                                  std::uint64_t viewEndByte) {
    if (bytes_ == nullptr || viewEndByte <= viewStartByte ||
        viewStartByte >= bytes_->size()) {
        return;
    }
    const auto last = std::min(viewEndByte, static_cast<std::uint64_t>(bytes_->size()));
    const auto count = last - viewStartByte;
    if (count == 0 || count > 8192) {
        return;
    }

    const auto centreY = plotArea.getCentreY();
    const auto halfHeight = plotArea.getHeight() * 0.5f;
    const auto xOf = [&](std::uint64_t byte) {
        return plotArea.getX() +
               (static_cast<float>(byte - viewStartByte) + 0.5f) /
                   static_cast<float>(count) * plotArea.getWidth();
    };
    const auto yOf = [&](std::uint8_t byte) {
        return centreY -
               ((static_cast<float>(byte) - 127.5f) / 127.5f) * halfHeight;
    };

    juce::Path line;
    bool started = false;
    for (auto byte = viewStartByte; byte < last; ++byte) {
        const auto x = xOf(byte);
        const auto y = yOf((*bytes_)[static_cast<std::size_t>(byte)]);
        if (!started) {
            line.startNewSubPath(x, y);
            started = true;
        } else {
            line.lineTo(x, y);
        }
    }
    g.setColour(palette::accentSoft);
    g.strokePath(line, juce::PathStrokeType {1.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded});

    g.setColour(palette::alpha(palette::accentSoft, 0.95f));
    for (auto byte = viewStartByte; byte < last; ++byte) {
        const auto x = xOf(byte);
        const auto y = yOf((*bytes_)[static_cast<std::size_t>(byte)]);
        g.fillEllipse(x - 1.5f, y - 1.5f, 3.0f, 3.0f);
    }
}

void ByteDisplay::paintRuler(juce::Graphics& g, const juce::Rectangle<float>& strip) {
    if (regionEnd_ <= regionStart_ || strip.getWidth() <= 0.0f) {
        return;
    }
    const auto [viewStartByte, viewEndByte] = viewByteRange();
    const auto viewBytes = viewEndByte > viewStartByte ? viewEndByte - viewStartByte : 0;
    if (viewBytes == 0) {
        return;
    }

    const auto pxPerByte = strip.getWidth() / static_cast<float>(viewBytes);
    const auto target = 90.0 / static_cast<double>(pxPerByte);
    const auto magnitude = std::pow(10.0, std::floor(std::log10(std::max(target, 1.0))));
    std::uint64_t stepBytes {1};
    for (const auto mult : {1.0, 2.0, 5.0, 10.0}) {
        if (mult * magnitude >= target) {
            stepBytes = static_cast<std::uint64_t>(mult * magnitude);
            break;
        }
        stepBytes = static_cast<std::uint64_t>(10.0 * magnitude);
    }
    if (stepBytes == 0) {
        return;
    }

    const auto hexOf = [](std::uint64_t value) {
        constexpr char digits[] {"0123456789ABCDEF"};
        juce::String text {"0x"};
        bool started = false;
        for (int shift = 60; shift >= 0; shift -= 4) {
            const auto nibble = (value >> shift) & 0xFu;
            if (nibble != 0 || started || shift == 0) {
                text += digits[nibble];
                started = true;
            }
        }
        return text;
    };

    g.setFont(fonts().sans(9.0f));
    const auto firstTick = (viewStartByte + stepBytes - 1) / stepBytes * stepBytes;
    for (auto tick = firstTick; tick < viewEndByte;) {
        const auto x = strip.getX() +
                       static_cast<float>(tick - viewStartByte) * pxPerByte;
        g.setColour(palette::alpha(palette::chassisBorder, 0.9f));
        g.fillRect(juce::Rectangle<float> {x, strip.getBottom() - 5.0f, 1.0f, 5.0f});
        g.setColour(palette::textOnDarkSub);
        g.drawText(hexOf(tick),
                   juce::Rectangle<float> {x + 3.0f, strip.getY(), 76.0f,
                                            strip.getHeight()},
                   juce::Justification::centredLeft, false);
        if (tick > viewEndByte - stepBytes) {
            break;
        }
        tick += stepBytes;
    }

    g.setColour(palette::alpha(palette::displayBorder, 0.9f));
    g.drawHorizontalLine(juce::roundToInt(strip.getBottom()), strip.getX(),
                         strip.getRight());
}

void ByteDisplay::paintSourceTag(juce::Graphics& g,
                                 const juce::Rectangle<float>& plotArea) {
    const juce::String tag {mode_ == ViewMode::waveform
                                ? "BYTES > SAMPLES"
                                : "BYTES > ENTROPY"};
    g.setFont(fonts().sans(8.5f));
    const auto width = 104.0f;
    const auto tagBounds = juce::Rectangle<float> {plotArea.getX() + 4.0f,
                                                 plotArea.getBottom() - 18.0f, width,
                                                  14.0f};
    g.setColour(palette::alpha(palette::display, 0.72f));
    g.fillRoundedRectangle(tagBounds, 2.0f);
    g.setColour(palette::textOnDarkSub);
    g.drawText(tag, tagBounds.reduced(6.0f, 0.0f), juce::Justification::centredLeft,
               false);
}

void ByteDisplay::paintOverview(juce::Graphics& g) {
    if (regionEnd_ <= regionStart_ || overviewColumns_.empty()) {
        return;
    }
    const auto strip = overviewBounds();
    if (strip.getWidth() <= 0.0f || strip.getHeight() <= 0.0f) {
        return;
    }

    g.setColour(palette::display);
    g.fillRoundedRectangle(strip, 2.0f);
    g.setColour(palette::alpha(palette::displayBorder, 0.9f));
    g.drawRoundedRectangle(strip, 2.0f, 1.0f);

    const auto plot = strip.reduced(3.0f, 3.0f);
    const auto centreY = plot.getCentreY();
    const auto halfHeight = plot.getHeight() * 0.5f;
    const auto columnWidth = plot.getWidth() / static_cast<float>(overviewColumns_.size());

    if (mode_ == ViewMode::entropy) {
        // na vista de entropia a miniatura tem de ser entropia; um envelope de
        // amostras aqui e' outro grafico, nao o mesmo material.
        juce::Path mini;
        mini.startNewSubPath(plot.getX(), plot.getBottom());
        for (std::size_t i = 0; i < overviewColumns_.size(); ++i) {
            const auto bits = std::clamp(overviewColumns_[i].entropyBits, 0.0f, 8.0f);
            mini.lineTo(plot.getX() + static_cast<float>(i) * columnWidth,
                        plot.getBottom() - bits / 8.0f * plot.getHeight());
        }
        mini.lineTo(plot.getRight(), plot.getBottom());
        mini.closeSubPath();

        g.setColour(palette::alpha(palette::accent, 0.20f));
        g.fillPath(mini);
        g.setColour(palette::alpha(palette::accent, 0.85f));
        g.strokePath(mini, juce::PathStrokeType {1.2f});
    } else {
        juce::Path mini;
        mini.startNewSubPath(plot.getX(), centreY);
        for (std::size_t i = 0; i < overviewColumns_.size(); ++i) {
            mini.lineTo(plot.getX() + static_cast<float>(i) * columnWidth,
                        centreY - std::clamp(overviewColumns_[i].maximum, -1.0f, 1.0f) *
                                      halfHeight);
        }
        for (std::size_t i = overviewColumns_.size(); i-- > 0;) {
            mini.lineTo(plot.getX() + static_cast<float>(i) * columnWidth,
                        centreY - std::clamp(overviewColumns_[i].minimum, -1.0f, 1.0f) *
                                      halfHeight);
        }
        mini.closeSubPath();
        g.setColour(palette::alpha(palette::accent, 0.55f));
        g.fillPath(mini);
    }

    const auto viewX = plot.getX() + viewStart_ * plot.getWidth();
    const auto viewW = std::max(3.0f, viewLength_ * plot.getWidth());
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    if (viewX > plot.getX()) {
        g.fillRect(
            juce::Rectangle<float> {plot.getX(), plot.getY(), viewX - plot.getX(),
                                    plot.getHeight()});
    }
    if (viewX + viewW < plot.getRight()) {
        g.fillRect(juce::Rectangle<float> {viewX + viewW, plot.getY(),
                                            plot.getRight() - (viewX + viewW),
                                            plot.getHeight()});
    }
    g.setColour(palette::accent);
    g.drawRect(juce::Rectangle<float> {viewX, plot.getY(), viewW, plot.getHeight()},
               1.0f);

    const auto readRegion = static_cast<float>(fractionFor(readHead_));
    if (readRegion >= 0.0f && readRegion <= 1.0f) {
        const auto x = plot.getX() + readRegion * plot.getWidth();
        g.setColour(juce::Colours::white);
        g.fillRect(juce::Rectangle<float> {x - 0.5f, plot.getY(), 1.0f,
                                            plot.getHeight()});
    }
    // marcadores no mapa: risco de 2 px em cheio, saltaveis por clique.
    for (const auto bookmark : bookmarks_) {
        const auto fraction = static_cast<float>(fractionFor(bookmark));
        if (fraction < 0.0f || fraction > 1.0f) {
            continue;
        }
        const auto x = plot.getX() + fraction * plot.getWidth();
        g.setColour(palette::alpha(palette::accent, 0.85f));
        g.fillRect(juce::Rectangle<float> {x - 1.0f, plot.getY(), 2.0f,
                                            plot.getHeight()});
    }
    if (playheadVisible_) {
        const auto region = std::clamp(playheadFraction_.value(), 0.0f, 1.0f);
        const auto x = plot.getX() + region * plot.getWidth();
        g.setColour(palette::accent);
        g.fillRect(juce::Rectangle<float> {x - 0.5f, plot.getY(), 1.0f,
                                            plot.getHeight()});
    }
}

void ByteDisplay::paintEntropyLine(
    juce::Graphics& g,
    const std::vector<pe::Column>& columns,
    float columnWidth,
    float originX,
    const std::function<float(std::size_t)>& topOf,
    const std::function<float(std::size_t)>& bottomOf) {
    juce::Path low;
    juce::Path mid;
    juce::Path high;

    std::size_t lastInBucket[3] {columns.size(), columns.size(), columns.size()};

    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto x = originX + static_cast<float>(i) * columnWidth;
        const auto b = entropyBucket(columns[i].entropyBits);

        auto& path = b == 0 ? low : b == 1 ? mid : high;
        if (path.isEmpty() || lastInBucket[b] + 1 != i) {
            path.startNewSubPath(x, topOf(i));
        } else {
            path.lineTo(x, topOf(i));
        }
        path.lineTo(x, bottomOf(i));
        lastInBucket[b] = i;
    }

    const juce::Colour inks[3] {palette::accentSoft, palette::accentWarn, palette::accent};
    const juce::Path* paths[3] {&low, &mid, &high};
    for (int b = 0; b < 3; ++b) {
        if (paths[b]->isEmpty()) {
            continue;
        }
        g.setColour(palette::alpha(inks[b], 0.20f));
        g.strokePath(*paths[b], juce::PathStrokeType {3.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded});
        g.setColour(palette::alpha(inks[b], 0.95f));
        g.strokePath(*paths[b], juce::PathStrokeType {1.0f});
    }
}

void ByteDisplay::resampleEnvelope(int pixels) {
    if (pixels < 2) {
        pixels = 2;
    }
    const auto count = static_cast<std::size_t>(pixels);
    if (pixelMax_.size() != count) {
        pixelMax_.assign(count, 0.0f);
        pixelMin_.assign(count, 0.0f);
        pixelRms_.assign(count, 0.0f);
    }

    const auto columns = columns_.size();
    if (columns == 0) {
        return;
    }

    for (std::size_t px = 0; px < count; ++px) {
        const auto begin = static_cast<std::size_t>(
            static_cast<double>(px) * static_cast<double>(columns) / static_cast<double>(count));
        auto end = static_cast<std::size_t>(
            static_cast<double>(px + 1) * static_cast<double>(columns) /
            static_cast<double>(count));
        if (end <= begin) {
            end = begin + 1;
        }
        if (end > columns) {
            end = columns;
        }

        float peakMax = 0.0f;
        float peakMin = 0.0f;
        float sum = 0.0f;
        for (auto i = begin; i < end; ++i) {
            peakMax = std::max(peakMax, std::clamp(columns_[i].maximum, -1.0f, 1.0f));
            peakMin = std::min(peakMin, std::clamp(columns_[i].minimum, -1.0f, 1.0f));
            sum += std::clamp(columns_[i].rms, 0.0f, 1.0f);
        }

        pixelMax_[px] = peakMax;
        pixelMin_[px] = peakMin;
        pixelRms_[px] = sum / static_cast<float>(end - begin);
    }

    // hull: um maximo corrido de +-2 px fecha as valas de um pixel. sem isto, o
    // envelope de material ruidoso lia-se como um pente de riscos, e nao como a
    // silhueta do sample.
    constexpr int kHullRadius {2};
    hullMax_ = pixelMax_;
    hullMin_ = pixelMin_;
    for (int i = 0; i < pixels; ++i) {
        auto high = hullMax_[static_cast<std::size_t>(i)];
        auto low = hullMin_[static_cast<std::size_t>(i)];
        const auto from = std::max(0, i - kHullRadius);
        const auto to = std::min(pixels - 1, i + kHullRadius);
        for (int k = from; k <= to; ++k) {
            high = std::max(high, hullMax_[static_cast<std::size_t>(k)]);
            low = std::min(low, hullMin_[static_cast<std::size_t>(k)]);
        }
        pixelMax_[static_cast<std::size_t>(i)] = high;
        pixelMin_[static_cast<std::size_t>(i)] = low;
    }
}

void ByteDisplay::paintRmsSilhouette(juce::Graphics& g,
                                      const juce::Rectangle<float>& plotArea,
                                      float centreY,
                                      float halfHeight,
                                      float columnWidth) {
    // nucleo de rms: uma faixa cheia, nao um traco por coluna. a linha fina a
    // zigzaguear lia-se como pente — com material aleatorio virava uma textura
    // de riscos, e a silhueta desaparecia por dentro dela. a faixa e' o corpo
    // do sample por dentro da forma.
    if (pixelRms_.empty() || columnWidth <= 0.0f) {
        return;
    }
    const auto pixels = static_cast<int>(pixelRms_.size());
    const auto xAt = [&](int i) {
        return plotArea.getX() + (static_cast<float>(i) + 0.5f) * columnWidth;
    };

    juce::Path core;
    core.startNewSubPath(xAt(0), centreY - pixelRms_[0] * halfHeight);
    for (int i = 1; i < pixels; ++i) {
        core.lineTo(xAt(i), centreY - pixelRms_[static_cast<std::size_t>(i)] * halfHeight);
    }
    for (int i = pixels; i-- > 0;) {
        core.lineTo(xAt(i), centreY + pixelRms_[static_cast<std::size_t>(i)] * halfHeight);
    }
    core.closeSubPath();

    juce::ColourGradient relief {
        palette::alpha(palette::accentSoft, 0.30f), plotArea.getCentreX(), plotArea.getY(),
        palette::alpha(palette::accentSoft, 0.30f), plotArea.getCentreX(),
        plotArea.getBottom(), false};
    relief.addColour(0.5f, palette::alpha(palette::accentSoft, 0.62f));
    g.setGradientFill(relief);
    g.fillPath(core);
}

void ByteDisplay::paintEntropy(juce::Graphics& g) {
    if (columns_.empty()) {
        return;
    }

    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto area = plotBounds();
    if (area.getWidth() <= 0.0f || area.getHeight() <= 0.0f) {
        return;
    }
    const auto plot = area.withTrimmedLeft(kAxisInset).withTrimmedBottom(kAxisHeight);

    paintRuler(g, {full.getX(), area.getY() - kRulerHeight - 2.0f, full.getWidth(),
                   kRulerHeight});

    const auto yFor = [&plot](float bits) {
        return plot.getBottom() - std::clamp(bits, 0.0f, 8.0f) / 8.0f * plot.getHeight();
    };

    g.setColour(palette::alpha(palette::displayGrid, 1.4f));
    for (const auto bits : {0.0f, 4.0f, 8.0f}) {
        g.drawHorizontalLine(juce::roundToInt(yFor(bits)), plot.getX(), plot.getRight());
    }

    g.setFont(font_);
    g.setColour(palette::textOnDarkSub);
    for (const auto bits : {0.0f, 4.0f, 8.0f}) {
        g.drawText(juce::String::formatted("%.0f", static_cast<double>(bits)),
                   juce::Rectangle<float> {area.getX(), yFor(bits) - 7.0f, kAxisInset - 6.0f,
                                           14.0f},
                   juce::Justification::centredRight, false);
    }
    g.drawText("bits/byte", juce::Rectangle<float> {plot.getX(), plot.getBottom() + 1.0f,
                                                    plot.getWidth(), kAxisHeight - 2.0f},
               juce::Justification::centredRight, false);

    const auto columnWidth = plot.getWidth() / static_cast<float>(columns_.size());

    juce::Path curve;
    curve.startNewSubPath(plot.getX(), plot.getBottom());
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const auto x = plot.getX() + static_cast<float>(i) * columnWidth;
        curve.lineTo(x, yFor(columns_[i].entropyBits));
    }
    curve.lineTo(plot.getRight(), plot.getBottom());
    curve.closeSubPath();

    // o preenchimento e' um veu, nao uma parede. a 0,42 a curva desaparecia dentro
    // do proprio banho e a vista lia-se como um bloco laranja.
    juce::ColourGradient veil {palette::alpha(palette::accent, 0.20f),
                               plot.getCentreX(), plot.getY(),
                               palette::alpha(palette::accent, 0.02f),
                               plot.getCentreX(), plot.getBottom(), false};
    g.setGradientFill(veil);
    g.fillPath(curve);

    const auto yAt = [&](std::size_t i) { return yFor(columns_[i].entropyBits); };
    paintEntropyLine(g, columns_, columnWidth, plot.getX(), yAt, yAt);

    paintBookmarks(g, plot);
    paintOverview(g);
}

void ByteDisplay::paintReadHead(juce::Graphics& g, const juce::Rectangle<float>& area) {
    const auto fraction = readHeadFraction_.value();
    if (fraction < 0.0f || fraction > 1.0f) {
        return;
    }

    const auto x = area.getX() + fraction * area.getWidth();

    g.setColour(palette::alpha(juce::Colours::white, 0.16f));
    g.fillRect(juce::Rectangle<float> {x - 2.0f, area.getY(), 4.0f, area.getHeight()});
    g.setColour(juce::Colours::white);
    g.fillRect(juce::Rectangle<float> {x - 1.0f, area.getY(), 2.0f, area.getHeight()});
}

void ByteDisplay::paintPlayhead(juce::Graphics& g, const juce::Rectangle<float>& area) {
    if (!playheadVisible_ || columns_.empty() || area.isEmpty()) {
        return;
    }

    const auto region = std::clamp(playheadFraction_.value(), 0.0f, 1.0f);

    // no hex a area e' a faixa de entropia, que mostra o ficheiro inteiro: a
    // fraccao da regiao tem de passar por endereco antes de virar x.
    float screen = -1.0f;
    if (mode_ == ViewMode::hex && fileSize_ > 0 && regionEnd_ > regionStart_) {
        const auto address = pe::byteForPosition(
            static_cast<double>(region), pe::ByteRange {regionStart_, regionEnd_});
        screen = static_cast<float>(address) / static_cast<float>(fileSize_);
    } else if (viewLength_ > 0.0f) {
        screen = (region - viewStart_) / viewLength_;
    }

    if (screen < 0.0f || screen > 1.0f) {
        return;
    }
    const auto x = area.getX() + screen * area.getWidth();

    juce::Path marker;
    marker.addTriangle(x - 4.0f, area.getY(), x + 4.0f, area.getY(), x, area.getY() + 5.0f);
    g.setColour(palette::accentSoft);
    g.fillPath(marker);

    g.setColour(palette::alpha(palette::accentSoft, 0.55f));
    g.fillRect(juce::Rectangle<float> {x - 0.5f, area.getY() + 5.0f, 1.0f,
                                        area.getHeight() - 5.0f});
}

std::uint64_t ByteDisplay::bandAddressAt(float x) const {
    constexpr auto none = std::numeric_limits<std::uint64_t>::max();
    const auto band = entropyBandBounds();
    if (band.isEmpty() || fileSize_ == 0) {
        return none;
    }
    const auto plot = band.reduced(8.0f, 0.0f);
    if (x < plot.getX() || x > plot.getRight()) {
        return none;
    }
    const auto fraction = (x - plot.getX()) / juce::jmax(1.0f, plot.getWidth());
    const auto address = static_cast<std::uint64_t>(
        static_cast<double>(fraction) * static_cast<double>(fileSize_ - 1));
    return std::min(address, fileSize_ - 1);
}

std::uint64_t ByteDisplay::addressAt(juce::Point<float> position) const {
    constexpr auto none = std::numeric_limits<std::uint64_t>::max();

    const auto area = mode_ == ViewMode::entropy ? entropyPlotBounds() : plotBounds();
    if (bytes_ == nullptr || area.getWidth() <= 0.0f || regionEnd_ <= regionStart_) {
        return none;
    }

    const auto region = regionFractionAt(position.x, area.getX(), area.getWidth());
    if (region < 0.0f || region >= 1.0f) {
        return none;
    }

    const auto address = pe::byteForPosition(static_cast<double>(region),
                                             pe::ByteRange {regionStart_, regionEnd_});
    return address < regionEnd_ ? address : none;
}

juce::Rectangle<float> ByteDisplay::plotBounds() const noexcept {
    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto overviewH = overviewBounds().getHeight();
    return full.withTrimmedTop(kRulerHeight + 4.0f)
        .withTrimmedBottom(overviewH + 4.0f);
}

juce::Rectangle<float> ByteDisplay::entropyPlotBounds() const noexcept {
    return plotBounds().withTrimmedLeft(kAxisInset);
}

juce::Rectangle<float> ByteDisplay::overviewBounds() const noexcept {
    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto height = std::min(kOverviewHeight, full.getHeight() * 0.25f);
    return {full.getX(), full.getBottom() - height, full.getWidth(), height};
}

void ByteDisplay::mouseDown(const juce::MouseEvent& event) {
    grabKeyboardFocus();

    if (mode_ == ViewMode::hex) {
        // a faixa de entropia e' do pai; o hex e' filho e trata do resto.
        if (entropyBandBounds().contains(event.position)) {
            const auto address = bandAddressAt(event.position.x);
            if (address != std::numeric_limits<std::uint64_t>::max() &&
                onAddressActivated != nullptr) {
                caretMoved_ = false;
                hexBandDrag_ = true;
                // a banda e' o mapa: saltar nela leva o hex para o sitio escolhido.
                hex_.scrollToAddress(address);
                onAddressActivated(address);
            }
        }
        return;
    }

    if (overviewBounds().contains(event.position)) {
        const auto area = overviewBounds();
        // bandeira no overview salta como na vista: o mapa tem de obedecer ao
        // mesmo gesto. fora de bandeira, pan.
        if (const auto jump = bookmarkAt(event.position.x, area);
            jump != std::numeric_limits<std::uint64_t>::max() &&
            onAddressActivated != nullptr) {
            caretMoved_ = false;
            onAddressActivated(jump);
            return;
        }
        const auto region = regionFractionAt(event.position.x, area.getX(), area.getWidth());
        if (region >= 0.0f) {
            viewStart_ = std::clamp(region - viewLength_ * 0.5f, 0.0f, 1.0f - viewLength_);
            cacheValid_ = false;
            repaint();
        }
        overviewDrag_ = true;
        lastPanX_ = event.position.x;
        return;
    }

    // marcador: alt+click alterna no endereco clicado; click perto da bandeira
    // salta para ela em vez de escrever a posicao.
    const auto plotArea = mode_ == ViewMode::entropy ? entropyPlotBounds() : plotBounds();
    if (plotArea.contains(event.position)) {
        if (event.mods.isAltDown()) {
            if (const auto address = addressAt(event.position);
                address != std::numeric_limits<std::uint64_t>::max()) {
                toggleBookmarkAt(address);
            }
            return;
        }
        if (const auto jump = bookmarkAt(event.position.x, plotArea);
            jump != std::numeric_limits<std::uint64_t>::max() &&
            onAddressActivated != nullptr) {
            caretMoved_ = false;
            onAddressActivated(jump);
            return;
        }
    }

    if (event.mods.isRightButtonDown() || event.mods.isMiddleButtonDown() ||
        event.mods.isCtrlDown()) {
        panning_ = true;
        lastPanX_ = event.position.x;
        return;
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max() && onAddressActivated != nullptr) {
        caretMoved_ = false;
        onAddressActivated(address);
    }
}

void ByteDisplay::mouseDrag(const juce::MouseEvent& event) {
    if (mode_ == ViewMode::hex) {
        if (hexBandDrag_) {
            const auto address = bandAddressAt(event.position.x);
            if (address != std::numeric_limits<std::uint64_t>::max()) {
                pendingScrub_ = address;
                hex_.scrollToAddress(address);
            }
        }
        return;
    }

    if (overviewDrag_ || panning_) {
        const auto area = overviewDrag_ ? overviewBounds()
                          : mode_ == ViewMode::entropy ? entropyPlotBounds()
                                                      : plotBounds();
        if (area.getWidth() > 0.0f && viewLength_ < 1.0f) {
            const auto dx = (event.position.x - lastPanX_) / area.getWidth();
            lastPanX_ = event.position.x;
            viewStart_ = std::clamp(viewStart_ - dx * viewLength_, 0.0f, 1.0f - viewLength_);
            cacheValid_ = false;
            repaint();
        }
        return;
    }

    const auto address = addressAt(event.position);
    if (address != std::numeric_limits<std::uint64_t>::max()) {
        pendingScrub_ = address;
    }
}

void ByteDisplay::mouseUp(const juce::MouseEvent&) {
    panning_ = false;
    overviewDrag_ = false;
    hexBandDrag_ = false;
}

void ByteDisplay::mouseDoubleClick(const juce::MouseEvent& event) {
    if (mode_ == ViewMode::hex) {
        return;
    }

    if (addressAt(event.position) == std::numeric_limits<std::uint64_t>::max()) {
        return;
    }
    if (onSnapRequested != nullptr) {
        onSnapRequested();
    }
}

void ByteDisplay::mouseWheelMove(const juce::MouseEvent& event,
                                 const juce::MouseWheelDetails& wheel) {
    if (mode_ == ViewMode::hex) {
        if (entropyBandBounds().contains(event.position)) {
            // a faixa acelera como o hex: o boost devolve o passo deste gesto.
            const auto lines = 3.0f * nextWheelBoost();
            hex_.scrollByLines(wheel.deltaY > 0.0f
                                   ? -static_cast<int>(lines)
                                   : static_cast<int>(lines));
        }
        return;
    }
    if (mode_ == ViewMode::matrix) {
        return;  // os combos da matriz tem a sua propria roda.
    }

    const auto boost = nextWheelBoost();
    const auto area = mode_ == ViewMode::entropy ? entropyPlotBounds() : plotBounds();
    if (event.mods.isShiftDown()) {
        if (viewLength_ < 1.0f && area.getWidth() > 0.0f) {
            viewStart_ = std::clamp(viewStart_ - wheel.deltaY * viewLength_ * 2.0f * boost,
                                    0.0f, 1.0f - viewLength_);
            cacheValid_ = false;
            repaint();
        }
    } else {
        const auto centre = regionFractionAt(event.position.x, area.getX(),
                                             area.getWidth());
        const auto inPlot = centre >= 0.0f;
        const auto delta = static_cast<float>(wheel.deltaY);
        // o expoente e' que acelera: cada entalhe seguido amplia o passo, e o
        // clamp do zoomAround continua a manda no teto.
        zoomAround(std::pow(1.15f, -delta * boost),
                   inPlot ? (centre - viewStart_) / viewLength_ : 0.5f);
    }
}

} // namespace opcoda
