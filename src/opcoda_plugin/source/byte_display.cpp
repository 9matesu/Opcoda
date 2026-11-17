#include "byte_display.h"

#include "look_and_feel.h"

#include "opcoda_core/pe/byte_to_position.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opcoda {
namespace {


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
        "matriz de modulacao. Clique para mover a cabeca de leitura; o botao "
        "direito acrescenta ou tira um marcador, e click sobre a bandeira salta para "
        "ele. Com o teclado: setas movem o cursor, Enter ativa o byte, Esc "
        "volta a cabeca de leitura, espaco liga e desliga a reproducao, 1 a 4 "
        "mudam de vista, + e - zoom, 0 enquadra a regiao e A alinha a regiao a "
        "uma secao PE.");

    hex_.onCellActivated = [this](std::uint64_t address) {
        if (onAddressActivated != nullptr) {
            onAddressActivated(address);
        }
    };

    hex_.onRegionSelected = [this](std::uint64_t start, std::uint64_t end) {
        if (onRegionSelected != nullptr) {
            onRegionSelected(start, end);
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
    addChildComponent(sectionCombo_);
    sectionCombo_.setName("Secoes do PE");
    sectionCombo_.setTooltip("Secoes do formato PE");
    sectionCombo_.onChange = [this] {
        const auto index = sectionCombo_.getSelectedId() - 1;
        if (index >= 0 && index < static_cast<int>(sections_.size())) {
            hex_.scrollToAddress(sections_[static_cast<std::size_t>(index)].rawOffset);
        }
    };
    applyModeVisibility();
}

void ByteDisplay::applyModeVisibility() {
    const auto hexMode = mode_ == ViewMode::hex;
    const auto matrixMode = mode_ == ViewMode::matrix;

    hex_.setVisible(hexMode);
    hex_.setInterceptsMouseClicks(hexMode, hexMode);
    matrix_.setVisible(matrixMode);
    sectionCombo_.setVisible(hexMode);
}

void ByteDisplay::setSource(const std::vector<std::uint8_t>* bytes,
                            std::uint64_t fileSize) {
    bytes_ = bytes;
    fileSize_ = fileSize;

    cacheValid_ = false;
    overviewValid_ = false;
    entropyCacheValid_ = false;
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
    entropyCacheValid_ = false;

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
    sprayE_.tick(deltaSeconds, kPreviewHalfLife);
    densityE_.tick(deltaSeconds, kPreviewHalfLife);
    sizeE_.tick(deltaSeconds, kPreviewHalfLife);
    pitchSpreadE_.tick(deltaSeconds, kPreviewHalfLife);
    levelE_.tick(deltaSeconds, kPreviewHalfLife);
    pitchE_.tick(deltaSeconds, kPreviewHalfLife);
    panE_.tick(deltaSeconds, kPreviewHalfLife);

    const auto after = readHeadFraction_.value() + playheadFraction_.value() +
                       modeFade_.value() + sprayE_.value() + densityE_.value() +
                       sizeE_.value() + pitchSpreadE_.value() + levelE_.value() +
                       pitchE_.value() + panE_.value();
    if (std::abs(after - before) > 1.0e-4f) {
        repaint();
    }
}

void ByteDisplay::setGrains(
    const std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices>& views, int count) {
    const bool drawable = bytes_ != nullptr && !bytes_->empty() &&
                          regionEnd_ > regionStart_;
    const auto toRead = drawable ? std::clamp(count, 0, dsp::GrainTelemetry::kMaxVoices) : 0;

    if (toRead == 0) {
        if (grainTrails_.anyActive()) {
            grainTrails_.clear();
            repaint();
        }
        return;
    }

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

void ByteDisplay::layoutHex() {
    const auto full = getLocalBounds();
    hex_.setBounds(mode_ == ViewMode::hex ? full.withTrimmedTop(22) : full);

    sectionCombo_.setBounds(full.getWidth() - 232, 3, 224, 18);
}

void ByteDisplay::resized() {
    layoutHex();
    matrix_.setBounds(getLocalBounds().reduced(juce::roundToInt(kPadding)));

    const auto wanted = columnCountForWidth();
    if (wanted != cachedColumnCount_) {
        cacheValid_ = false;
    }
}

std::uint32_t ByteDisplay::entropyColumnCount() const noexcept {
    const auto width = getLocalBounds().toFloat().reduced(kPadding).getWidth();
    const auto zoom = juce::jlimit(0.0f, 1.0f, viewLength_);
    const auto density = 0.30f + 0.70f * (1.0f - zoom);
    return static_cast<std::uint32_t>(
        std::clamp(width * density, 16.0f, 8192.0f));
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

void ByteDisplay::rebuildEntropyColumnsIfNeeded() {
    const auto wanted = entropyColumnCount();
    if (entropyCacheValid_ && entropyColumnCount_ == wanted &&
        entropyRegionStart_ == regionStart_ && entropyRegionEnd_ == regionEnd_ &&
        entropyViewStart_ == viewStart_ && entropyViewLength_ == viewLength_) {
        return;
    }

    entropyColumns_.clear();

    if (bytes_ != nullptr && !bytes_->empty() && regionEnd_ > regionStart_) {
        const auto [viewStart, viewEnd] = viewByteRange();
        const auto viewBytes = viewEnd - viewStart;
        if (viewBytes > 0) {
            const auto maxColumns = std::max<std::uint64_t>(1, viewBytes / kEntropyColumnBytes);
            const auto count = std::min<std::uint64_t>(wanted, maxColumns);
            static_cast<void>(pe::reduceToColumns(bytes_->data(), bytes_->size(), viewStart,
                                                  viewEnd, static_cast<std::uint32_t>(count),
                                                  entropyColumns_));
        }
    }

    entropyColumnCount_ = wanted;
    entropyRegionStart_ = regionStart_;
    entropyRegionEnd_ = regionEnd_;
    entropyViewStart_ = viewStart_;
    entropyViewLength_ = viewLength_;
    entropyCacheValid_ = true;
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
    const auto fraction = fractionOf(address);
    const bool moving = !readHeadAnimated_ || address != readHead_ ||
                        std::abs(fraction - readHeadFraction_.value()) > 1.0e-6f;
    if (!moving) {
        return;
    }

    readHead_ = address;
    hex_.setReadHead(address);

    if (mode_ == ViewMode::hex && !hexHeadScrolled_ && bytes_ != nullptr &&
        regionEnd_ > regionStart_) {
        hex_.scrollToAddress(address);
        hexHeadScrolled_ = true;
    }

    if (!caretMoved_) {
        caret_ = address;
    }

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

        OpcodaLookAndFeel::paintMark(
            g, juce::Rectangle<float> {area.getCentreX() - 11.0f, centreY - 44.0f,
                                       22.0f, 22.0f});

        g.setFont(type::displayFont());
        g.setColour(palette::textOnDark);
        g.drawText("No source", area.withTrimmedTop(area.getHeight() * 0.36f),
                   juce::Justification::centredTop, false);

        g.setFont(fonts().sans(9.5f));
        g.setColour(palette::textOnDarkSub);
        g.drawText("DROP .EXE / .DLL / .BIN  OR  PRESS L",
                   area.withTrimmedTop(area.getHeight() * 0.36f + 20.0f),
                   juce::Justification::centredTop, false);
        return;
    }

    rebuildColumnsIfNeeded();
    rebuildOverviewIfNeeded();

    switch (mode_) {
        case ViewMode::waveform: paintWaveform(g); break;
        case ViewMode::entropy: paintEntropy(g); break;
        case ViewMode::hex: break;     // a vista HEX e' o filho, pintado por cima.
        case ViewMode::matrix: break;  // a vista MATRIX e' o filho, pintado por cima.
    }

    if (mode_ == ViewMode::waveform) {
        paintGrains(g, plotBounds());
    } else if (mode_ == ViewMode::entropy) {
        paintGrains(g, entropyPlotBounds());
    }

    if (mode_ == ViewMode::entropy) {
        paintSectionTicks(g);
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

        g.setColour(palette::alpha(palette::accentSoft, 0.26f));
        g.fillRect(cell.reduced(1.0f));
        g.setColour(palette::alpha(palette::accentSoft, 0.92f));
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
            g.setColour(palette::alpha(palette::accentSoft, 0.42f * gain * t * t));
            g.fillRect(juce::Rectangle<float> {x0, top, width, height});
        }
    }

    g.setColour(palette::alpha(palette::accentSoft, 0.14f * gain));
    g.fillRoundedRectangle(juce::Rectangle<float> {newestX - kGrainGlowWidth * 0.5f,
                                                   top - 1.5f, kGrainGlowWidth,
                                                   height + 3.0f},
                           kGrainGlowWidth * 0.5f);
    g.setColour(palette::alpha(palette::accentSoft, 0.9f * gain));
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
        return;
    }

    paintParamPreview(g, area);

    if (hasKeyboardFocus(true)) {
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

void ByteDisplay::paintWaveform(juce::Graphics& g) {
    if (columns_.empty()) {
        return;
    }

    const auto plot = plotBounds();
    if (plot.getWidth() <= 0.0f || plot.getHeight() <= 0.0f) {
        return;
    }

    // regua de enderecos no topo, como no arrange: ticks e numeros.
    {
        const auto full = getLocalBounds().toFloat().reduced(kPadding);
        paintRuler(g, {full.getX(), full.getY(), full.getWidth(), kRulerHeight});
    }

    const auto centreY = plot.getCentreY();
    const auto halfHeight = plot.getHeight() * 0.47f;
    const auto [viewStartByte, viewEndByte] = viewByteRange();

    const auto plotWidth = juce::jmax(1.0f, plot.getWidth());
    const auto density = static_cast<float>(viewEndByte - viewStartByte) / plotWidth;
    const auto envelopeValid = density >= 1.0f;
    const auto dotsAlpha = juce::jlimit(0.0f, 1.0f, 3.0f - density);

    if (envelopeValid) {
        resampleEnvelope(std::max(2, static_cast<int>(std::ceil(plot.getWidth())) * 2));
        const auto pixels = static_cast<int>(pixelMax_.size());
        const auto xAt = [&](int i) {
            return plot.getX() +
                   (static_cast<float>(i) + 0.5f) * plot.getWidth() /
                       static_cast<float>(pixels);
        };

        juce::Path body;
        body.startNewSubPath(xAt(0), centreY - pixelMax_[0] * halfHeight);
        for (int i = 1; i < pixels; ++i) {
            body.lineTo(xAt(i), centreY - pixelMax_[static_cast<std::size_t>(i)] * halfHeight);
        }
        for (int i = pixels; i-- > 0;) {
            body.lineTo(xAt(i), centreY - pixelMin_[static_cast<std::size_t>(i)] * halfHeight);
        }
        body.closeSubPath();

        const auto breathe = 0.10f + 0.05f * liveLevel_;
        g.setColour(palette::alpha(palette::waveBody, breathe));
        g.fillPath(body);

        if (hullRms_.size() == pixelMax_.size()) {
            juce::Path core;
            core.startNewSubPath(xAt(0), centreY - hullRms_[0] * halfHeight);
            for (int i = 1; i < pixels; ++i) {
                core.lineTo(xAt(i),
                            centreY - hullRms_[static_cast<std::size_t>(i)] * halfHeight);
            }
            for (int i = pixels; i-- > 0;) {
                core.lineTo(xAt(i),
                            centreY + hullRms_[static_cast<std::size_t>(i)] * halfHeight);
            }
            core.closeSubPath();
            g.setColour(palette::alpha(palette::waveHigh, 0.30f));
            g.fillPath(core);
        }

        juce::Path edgeTop;
        edgeTop.startNewSubPath(xAt(0), centreY - pixelMax_[0] * halfHeight);
        for (int i = 1; i < pixels; ++i) {
            edgeTop.lineTo(xAt(i),
                           centreY - pixelMax_[static_cast<std::size_t>(i)] * halfHeight);
        }
        juce::Path edgeBottom;
        edgeBottom.startNewSubPath(xAt(0), centreY - pixelMin_[0] * halfHeight);
        for (int i = 1; i < pixels; ++i) {
            edgeBottom.lineTo(xAt(i),
                              centreY - pixelMin_[static_cast<std::size_t>(i)] * halfHeight);
        }

        g.setColour(palette::alpha(palette::waveHigh, 0.65f));
        g.strokePath(edgeTop, juce::PathStrokeType {1.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded});
        g.strokePath(edgeBottom, juce::PathStrokeType {1.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});
    }

    if (density > 1.0f) {
        paintNoiseTexture(g, plot, viewStartByte, viewEndByte);
    }
    if (dotsAlpha > 0.0f) {
        paintSampleDots(g, plot, viewStartByte, viewEndByte, dotsAlpha);
    }

    paintBookmarks(g, plot);

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
           static_cast<float>(viewBytes) / plotWidth <= 3.0f;
}

void ByteDisplay::paintNoiseTexture(juce::Graphics& g,
                                    const juce::Rectangle<float>& plotArea,
                                    std::uint64_t viewStartByte,
                                    std::uint64_t viewEndByte) {
    if (bytes_ == nullptr || viewEndByte <= viewStartByte || plotArea.getWidth() <= 0.0f) {
        return;
    }
    const auto available = std::min<std::uint64_t>(bytes_->size(), viewEndByte) - viewStartByte;
    if (available < 2) {
        return;
    }

    constexpr std::size_t kPoints {2400};
    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(kPoints, available));
    const auto centreY = plotArea.getCentreY();
    const auto halfHeight = plotArea.getHeight() * 0.47f;

    juce::Path trace;
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = viewStartByte + static_cast<std::uint64_t>(i) * available /
                                               static_cast<std::uint64_t>(count);
        if (offset >= bytes_->size()) {
            break;
        }
        const float sample =
            (static_cast<float>((*bytes_)[static_cast<std::size_t>(offset)]) - 128.0f) / 128.0f;
        const float x = plotArea.getX() + static_cast<float>(i) /
                                              static_cast<float>(count - 1) *
                                              plotArea.getWidth();
        const float y = centreY - sample * halfHeight;
        if (i == 0) {
            trace.startNewSubPath(x, y);
        } else {
            trace.lineTo(x, y);
        }
    }

    g.setColour(palette::alpha(palette::waveHigh, 0.34f));
    g.strokePath(trace, juce::PathStrokeType {0.9f});
}

void ByteDisplay::paintSampleDots(juce::Graphics& g,
                                  const juce::Rectangle<float>& plotArea,
                                  std::uint64_t viewStartByte,
                                  std::uint64_t viewEndByte,
                                  float alpha) {
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
    g.setColour(palette::alpha(palette::accentSoft, 0.85f * alpha));
    g.strokePath(line, juce::PathStrokeType {1.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded});

    g.setColour(palette::alpha(palette::ivory, 0.90f * alpha));
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

    g.setFont(fonts().mono(9.0f));
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

void ByteDisplay::paintOverview(juce::Graphics& g) {
    if (!overviewShown()) {
        return;
    }
    const auto strip = overviewBounds();
    if (strip.getWidth() <= 0.0f || strip.getHeight() <= 0.0f) {
        return;
    }

    g.setColour(palette::display);
    g.fillRect(strip);
    g.setColour(palette::alpha(palette::ivory, 0.07f));
    g.fillRect(juce::Rectangle<float> {strip.getX(), strip.getY(), strip.getWidth(),
                                       1.0f});

    const auto plot = strip.reduced(3.0f, 4.0f);
    const auto centreY = plot.getCentreY();
    const auto halfHeight = plot.getHeight() * 0.5f;
    const auto columnWidth = plot.getWidth() / static_cast<float>(overviewColumns_.size());

    if (mode_ == ViewMode::entropy) {
        juce::Path mini;
        mini.startNewSubPath(plot.getX(), plot.getBottom());
        for (std::size_t i = 0; i < overviewColumns_.size(); ++i) {
            const auto bits = std::clamp(overviewColumns_[i].entropyBits, 0.0f, 8.0f);
            mini.lineTo(plot.getX() + static_cast<float>(i) * columnWidth,
                        plot.getBottom() - bits / 8.0f * plot.getHeight());
        }
        mini.lineTo(plot.getRight(), plot.getBottom());
        mini.closeSubPath();

        g.setColour(palette::alpha(palette::accent, 0.28f));
        g.fillPath(mini);
        g.setColour(palette::alpha(palette::accent, 0.85f));
        g.strokePath(mini, juce::PathStrokeType {1.0f});
    } else {
        constexpr float kMiniAmplitude {0.78f};
        juce::Path mini;
        mini.startNewSubPath(plot.getX(), centreY);
        for (std::size_t i = 0; i < overviewColumns_.size(); ++i) {
            mini.lineTo(plot.getX() + static_cast<float>(i) * columnWidth,
                        centreY - std::clamp(overviewColumns_[i].maximum, -1.0f, 1.0f) *
                                      halfHeight * kMiniAmplitude);
        }
        for (std::size_t i = overviewColumns_.size(); i-- > 0;) {
            mini.lineTo(plot.getX() + static_cast<float>(i) * columnWidth,
                        centreY - std::clamp(overviewColumns_[i].minimum, -1.0f, 1.0f) *
                                      halfHeight * kMiniAmplitude);
        }
        mini.closeSubPath();
        g.setColour(palette::alpha(palette::accent, 0.42f));
        g.fillPath(mini);
        g.setColour(palette::alpha(palette::accent, 0.95f));
        g.strokePath(mini, juce::PathStrokeType {1.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded});
        g.setColour(palette::alpha(palette::ivory, 0.10f));
        g.drawHorizontalLine(juce::roundToInt(centreY), plot.getX(), plot.getRight());
    }

    const auto viewX = plot.getX() + viewStart_ * plot.getWidth();
    const auto viewW = std::max(3.0f, viewLength_ * plot.getWidth());

    if (viewLength_ < 0.995f) {
        // fora da janela o mapa escurece; dentro dela um veu quente diz "e' aqui".
        g.setColour(palette::alpha(palette::display, 0.55f));
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
        g.setColour(palette::alpha(palette::accent, 0.14f));
        g.fillRect(juce::Rectangle<float> {viewX, plot.getY(), viewW, plot.getHeight()});
        g.setColour(palette::accent);
        g.fillRect(juce::Rectangle<float> {viewX, plot.getY(), 2.0f, plot.getHeight()});
        g.fillRect(juce::Rectangle<float> {viewX + viewW - 2.0f, plot.getY(), 2.0f,
                                           plot.getHeight()});
        g.fillRect(juce::Rectangle<float> {viewX, plot.getBottom() - 1.0f, viewW, 1.0f});
    }

    const auto readRegion = static_cast<float>(fractionFor(readHead_));
    if (readRegion >= 0.0f && readRegion <= 1.0f) {
        const auto x = plot.getX() + readRegion * plot.getWidth();
        g.setColour(palette::accentSoft);
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
        float rmsSum = 0.0f;
        std::size_t rmsCount = 0;
        for (auto i = begin; i < end; ++i) {
            peakMax = std::max(peakMax, std::clamp(columns_[i].maximum, -1.0f, 1.0f));
            peakMin = std::min(peakMin, std::clamp(columns_[i].minimum, -1.0f, 1.0f));
            rmsSum += std::abs(columns_[i].rms);
            ++rmsCount;
        }

        pixelMax_[px] = peakMax;
        pixelMin_[px] = peakMin;
        pixelRms_[px] = (rmsCount > 0) ? rmsSum / static_cast<float>(rmsCount) : 0.0f;
    }

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

    hullRms_ = pixelRms_;
    for (int i = 0; i < pixels; ++i) {
        const auto from = std::max(0, i - 1);
        const auto to = std::min(pixels - 1, i + 1);
        float sum = 0.0f;
        for (int k = from; k <= to; ++k) {
            sum += pixelRms_[static_cast<std::size_t>(k)];
        }
        hullRms_[static_cast<std::size_t>(i)] =
            sum / static_cast<float>(to - from + 1);
    }
}

void ByteDisplay::rebuildSectionCombo() {
    sectionCombo_.clear(juce::dontSendNotification);
    int id = 1;
    for (const auto& section : sections_) {
        auto label = section.name.isEmpty() ? juce::String {"Seccao"} : section.name;
        label += "  0x" + juce::String::toHexString(static_cast<int>(section.rawOffset)).toUpperCase();
        sectionCombo_.addItem(label, id++);
    }
}

void ByteDisplay::paintEntropy(juce::Graphics& g) {
    rebuildEntropyColumnsIfNeeded();
    if (entropyColumns_.empty()) {
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

    g.setColour(palette::displayGrid);
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

    const auto count = entropyColumns_.size();
    const auto columnWidth = plot.getWidth() / static_cast<float>(count);

    const auto yAt = [&](std::size_t i) { return yFor(entropyColumns_[i].entropyBits); };

    const juce::Colour inks[3] {palette::alpha(palette::accent, 0.45f), palette::accent,
                                palette::accentSoft};
    const auto barWidth = juce::jmax(1.5f, columnWidth * 0.74f);
    const auto gap = (columnWidth - barWidth) * 0.5f;

    for (std::size_t i = 0; i < count; ++i) {
        const auto ink = inks[entropyBucket(entropyColumns_[i].entropyBits)];
        const auto x = plot.getX() + static_cast<float>(i) * columnWidth + gap;
        const auto top = juce::jmin(plot.getBottom() - 2.0f, yAt(i));
        const auto height = juce::jmax(2.0f, plot.getBottom() - top);

        g.setColour(palette::alpha(ink, 0.30f));
        g.fillRect(juce::Rectangle<float> {x, plot.getY(), barWidth,
                                           plot.getHeight()});
        g.setColour(ink);
        g.fillRect(juce::Rectangle<float> {x, top, barWidth, height});
        g.setColour(palette::alpha(palette::ivory, 0.85f));
        g.fillRect(juce::Rectangle<float> {x, top, barWidth, 1.0f});
    }

    paintBookmarks(g, plot);
    paintOverview(g);
}

void ByteDisplay::paintParamPreview(juce::Graphics& g,
                                     const juce::Rectangle<float>& area) {
    if (!previewValid_ || area.isEmpty() || regionEnd_ <= regionStart_) {
        return;
    }

    const auto fraction = readHeadFraction_.value();
    if (fraction < 0.0f || fraction > 1.0f) {
        return;
    }

    const auto x = area.getX() + fraction * area.getWidth();
    const auto ink = palette::ivory;
    const auto top = area.getY();
    const auto bottom = area.getBottom();

    const auto spray = juce::jlimit(0.0f, 1.0f, sprayE_.value());
    const auto bandWidth = juce::jlimit(48.0f, area.getWidth(), spray * area.getWidth());
    const auto bandLeft =
        juce::jlimit(area.getX(), area.getRight() - bandWidth, x - bandWidth * 0.5f);
    if (spray > 0.005f) {
        const auto band = juce::Rectangle<float> {bandLeft, top, bandWidth, area.getHeight()};
        g.setColour(palette::alpha(ink, 0.05f));
        g.fillRect(band);
        g.setColour(palette::alpha(ink, 0.30f));
        g.fillRect(juce::Rectangle<float> {band.getX(), top, 1.0f, band.getHeight()});
        g.fillRect(juce::Rectangle<float> {band.getRight() - 1.0f, top, 1.0f, band.getHeight()});
    }

    const auto density = juce::jlimit(0.0f, 1.0f, densityE_.value() / 100.0f);
    const auto teeth = 3 + static_cast<int>(std::round(density * 13.0f));
    const auto combWidth = juce::jmax(48.0f, bandWidth, area.getWidth() * (0.12f + 0.55f * density));
    const auto combLeft =
        juce::jlimit(area.getX(), area.getRight() - combWidth, x - combWidth * 0.5f);
    const auto combStep = combWidth / static_cast<float>(teeth);
    g.setColour(palette::alpha(ink, 0.35f + 0.45f * density));
    for (int i = 0; i < teeth; ++i) {
        const auto tx = combLeft + combStep * (static_cast<float>(i) + 0.5f);
        g.fillRect(juce::Rectangle<float> {tx - 0.7f, bottom - 11.0f, 1.4f, 8.0f});
    }

    const auto [viewStartByte, viewEndByte] = viewByteRange();
    const auto sampleRate = preview_.sampleRate > 1.0f ? preview_.sampleRate : 48000.0f;
    const auto visibleSeconds =
        viewEndByte > viewStartByte
            ? static_cast<float>(viewEndByte - viewStartByte) / sampleRate
            : 0.0f;
    const auto grainSeconds = juce::jmax(0.001f, sizeE_.value() / 1000.0f);
    const auto grainWidth =
        visibleSeconds > 0.0f
            ? juce::jlimit(16.0f, area.getWidth() * 0.55f,
                           grainSeconds / visibleSeconds * area.getWidth())
            : 16.0f;
    const auto grainHeight = area.getHeight() * 0.42f;
    const auto centreY = area.getCentreY();
    const auto level = juce::jlimit(0.0f, 1.0f, levelE_.value());
    const auto tilt = juce::jlimit(-1.0f, 1.0f, pitchE_.value() / 48.0f);
    const auto spread = juce::jlimit(0.0f, 1.0f, pitchSpreadE_.value() / 12.0f);

    const auto drawWindow = [&](float cx, float lean, float fillAlpha, float lineAlpha) {
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
            const auto px = anchorX - halfGrain + t * grainWidth +
                            lean * env * grainWidth * 0.55f;
            const auto py = centreY - env * grainHeight * 0.5f;
            if (i == 0) {
                shape.startNewSubPath(px, py);
            } else {
                shape.lineTo(px, py);
            }
        }
        for (int i = kSteps; i >= 0; --i) {
            const auto t = static_cast<float>(i) / static_cast<float>(kSteps);
            const auto env = windowEnvelope(preview_.window, t);
            const auto px = anchorX - halfGrain + t * grainWidth +
                            lean * env * grainWidth * 0.55f;
            shape.lineTo(px, centreY + env * grainHeight * 0.5f);
        }
        shape.closeSubPath();

        g.setColour(palette::alpha(ink, fillAlpha));
        g.fillPath(shape);
        if (lineAlpha > 0.0f) {
            g.setColour(palette::alpha(ink, lineAlpha));
            g.strokePath(shape, juce::PathStrokeType {1.2f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded});
        }
    };

    if (spread > 0.05f) {
        drawWindow(x, tilt + spread * 0.8f, 0.04f, 0.0f);
        drawWindow(x, tilt - spread * 0.8f, 0.04f, 0.0f);
    }
    drawWindow(x, tilt, 0.05f + 0.16f * level, 0.28f + 0.45f * level);

    {
        const auto kRadius = 28.0f + 52.0f * density;
        juce::ColourGradient glow(juce::Colours::transparentBlack, x - kRadius, 0.0f,
                                  juce::Colours::transparentBlack, x + kRadius, 0.0f, false);
        glow.addColour(0.5f, palette::alpha(palette::accent,
                                             0.04f + 0.10f * density + 0.06f * liveLevel_));
        g.setGradientFill(glow);
        g.fillRect(juce::Rectangle<float> {x - kRadius, top, kRadius * 2.0f,
                                           area.getHeight()});
    }

    const auto haloWide = 4.0f + 7.0f * density;
    g.setColour(palette::alpha(ink, 0.07f + 0.07f * density));
    g.fillRect(juce::Rectangle<float> {x - haloWide, top, haloWide * 2.0f,
                                       area.getHeight()});
    g.setColour(palette::alpha(ink, 0.14f + 0.12f * density));
    g.fillRect(juce::Rectangle<float> {x - 2.75f, top, 5.5f, area.getHeight()});
    g.setColour(playheadVisible_ ? palette::accentSoft : ink);
    g.fillRect(juce::Rectangle<float> {x - 1.5f, top, 3.0f, area.getHeight()});

    juce::Path head;
    head.addTriangle(x, top - 1.5f, x + 5.5f, top + 5.5f, x, top + 12.5f);
    head.addTriangle(x, top - 1.5f, x - 5.5f, top + 5.5f, x, top + 12.5f);
    g.setColour(palette::accentSoft);
    g.fillPath(head);

    // o pe fecha a leitura na base do ecra.
    g.setColour(palette::alpha(ink, 0.85f));
    g.fillRect(juce::Rectangle<float> {x - 4.0f, bottom - 2.5f, 8.0f, 2.5f});

    const auto panX = x + juce::jlimit(-1.0f, 1.0f, panE_.value()) * area.getWidth() * 0.04f;
    g.setColour(ink);
    g.fillEllipse(panX - 2.5f, top + 12.0f, 5.0f, 5.0f);
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
    const auto overviewH = overviewShown() ? overviewBounds().getHeight() + 4.0f : 0.0f;
    const auto top = kRulerHeight + 4.0f;
    return full.withTrimmedTop(top).withTrimmedBottom(overviewH);
}

juce::Rectangle<float> ByteDisplay::entropyPlotBounds() const noexcept {
    return plotBounds().withTrimmedLeft(kAxisInset);
}

bool ByteDisplay::overviewShown() const noexcept {
    return regionEnd_ > regionStart_ && !overviewColumns_.empty() &&
           viewLength_ < 0.995f;
}

juce::Rectangle<float> ByteDisplay::overviewBounds() const noexcept {
    const auto full = getLocalBounds().toFloat().reduced(kPadding);
    const auto height = std::min(kOverviewHeight, full.getHeight() * 0.25f);
    return {full.getX(), full.getBottom() - height, full.getWidth(), height};
}

void ByteDisplay::mouseDown(const juce::MouseEvent& event) {
    grabKeyboardFocus();

    if (mode_ == ViewMode::hex) {
        return;
    }

    if (overviewShown() && overviewBounds().contains(event.position)) {
        const auto area = overviewBounds();
        if (event.mods.isAltDown() || event.mods.isRightButtonDown()) {
            const auto fraction = regionFractionAt(event.position.x, area.getX(),
                                                   area.getWidth());
            if (fraction >= 0.0f && regionEnd_ > regionStart_) {
                const auto span = regionEnd_ - regionStart_;
                toggleBookmarkAt(regionStart_ +
                                 static_cast<std::uint64_t>(fraction * static_cast<float>(span - 1)));
            }
            return;
        }
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

    const auto plotArea = mode_ == ViewMode::entropy ? entropyPlotBounds() : plotBounds();
    if (plotArea.contains(event.position)) {
        if (event.mods.isAltDown() || event.mods.isRightButtonDown()) {
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

    // pan: botao do meio ou ctrl+arrasto. o direito passou a marcar.
    if (event.mods.isMiddleButtonDown() || event.mods.isCtrlDown()) {
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
        return;  // a grelha tem a sua propria roda, com o passo em linhas.
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
        zoomAround(std::pow(1.15f, -delta * boost),
                   inPlot ? (centre - viewStart_) / viewLength_ : 0.5f);
    }
}

} // namespace opcoda
