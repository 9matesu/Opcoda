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
                        modeFade_.value() + crosshairYE_.value();

    readHeadFraction_.tick(deltaSeconds, kReadHeadHalfLife);
    playheadFraction_.tick(deltaSeconds, kPlayheadHalfLife);
    modeFade_.tick(deltaSeconds, kModeHalfLife);
    crosshairYE_.tick(deltaSeconds, kModeHalfLife);
    sprayE_.tick(deltaSeconds, kPreviewHalfLife);
    densityE_.tick(deltaSeconds, kPreviewHalfLife);
    sizeE_.tick(deltaSeconds, kPreviewHalfLife);
    pitchSpreadE_.tick(deltaSeconds, kPreviewHalfLife);
    levelE_.tick(deltaSeconds, kPreviewHalfLife);
    pitchE_.tick(deltaSeconds, kPreviewHalfLife);
    panE_.tick(deltaSeconds, kPreviewHalfLife);

    const auto after = readHeadFraction_.value() + playheadFraction_.value() +
                       modeFade_.value() + crosshairYE_.value() + sprayE_.value() +
                       densityE_.value() + sizeE_.value() + pitchSpreadE_.value() +
                       levelE_.value() + pitchE_.value() + panE_.value();
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

    const auto regionBytes = regionEnd_ - regionStart_;
    const auto minBytes = std::min<std::uint64_t>(kMinViewBytes, regionBytes);
    const auto anchored = std::clamp(viewStart_ + centreFraction * viewLength_, 0.0f, 1.0f);

    // zoom inteiro: a vista mede-se em bytes, nunca em fraccoes de byte. Cada
    // entalhe salta de byte em byte -- e' a unidade do material.
    auto wantedBytes = static_cast<std::uint64_t>(std::llround(
        static_cast<double>(viewLength_) * static_cast<double>(regionBytes) *
        static_cast<double>(factor)));
    wantedBytes = std::clamp<std::uint64_t>(wantedBytes, minBytes, regionBytes);

    // ancorar no byte que esta' debaixo do cursor, sem sair do ficheiro.
    const auto anchorByte = static_cast<std::uint64_t>(
        std::llround(static_cast<double>(anchored) * static_cast<double>(regionBytes)));
    auto startByte = static_cast<std::uint64_t>(std::llround(
        static_cast<double>(anchorByte) -
        static_cast<double>(centreFraction) * static_cast<double>(wantedBytes)));
    startByte = std::min(startByte, regionBytes - wantedBytes);

    viewStart_ = static_cast<float>(startByte) / static_cast<float>(regionBytes);
    viewLength_ = static_cast<float>(wantedBytes) / static_cast<float>(regionBytes);
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
    // o vidro e' o mesmo preto do DisplayPanel: um ecra so', sem dois fundos.
    g.fillAll(palette::display);

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
        case ViewMode::hex: break;     // a vista HEX e' o filho, pintado por cima.
        case ViewMode::matrix: break;  // a vista MATRIX e' o filho, pintado por cima.
    }

    if (mode_ == ViewMode::waveform) {
        paintGrains(g, plotBounds());
    }
}

void ByteDisplay::paintGrainsInHex(juce::Graphics& g) {
    if (regionEnd_ <= regionStart_) {
        return;
    }

    // Os parametros reflectem-se aqui de forma logica, nao num painel:
    //   densidade -> numero de celulas marcadas a volta da cabeca
    //   spray     -> o raio (em bytes) desse leque
    //   grain size-> o comprimento da cauda, em bytes
    const auto density = previewValid_ ? juce::jlimit(0.0f, 1.0f, densityE_.value() / 200.0f)
                                       : 0.0f;
    const auto spray = previewValid_ ? juce::jlimit(0.0f, 1.0f, sprayE_.value()) : 0.0f;
    const auto sizeMs = previewValid_ ? sizeE_.value() : 40.0f;

    const auto span = regionEnd_ - regionStart_;
    const auto sampleRate = preview_.sampleRate > 1.0f ? preview_.sampleRate : 48000.0f;
    const auto bytesPerSecond = static_cast<float>(sampleRate);

    // cauda do grão em bytes: o que o grão leu, convertido em enderecos.
    const auto tailBytes = static_cast<std::uint64_t>(
        juce::jlimit(1.0f, 4096.0f, sizeMs / 1000.0f * bytesPerSecond));

    // raio do spray em bytes, limitado ao que a vista mostra.
    const auto radiusBytes = static_cast<std::uint64_t>(
        juce::jlimit(1.0f, 64.0f, spray * 64.0f));

    const auto marks = 1 + static_cast<int>(std::round(density * 7.0f));

    for (int voice = 0; voice < view::GrainTrailSet::kMaxVoices; ++voice) {
        if (!grainTrails_.active(voice)) {
            continue;
        }

        const auto head = grainTrails_.address(voice);
        if (head < regionStart_ || head >= regionEnd_) {
            continue;
        }

        // cauda: as celulas que o grão atravessou, opacas.
        const auto tailStart = head > tailBytes ? head - tailBytes : regionStart_;
        for (auto address = tailStart; address < head; address += tailBytes / 8 + 1) {
            const auto cell = hex_.cellRectForAddress(address);
            if (cell.isEmpty()) {
                continue;
            }
            g.setColour(palette::alpha(palette::ivory, 0.16f));
            g.fillRect(cell.reduced(1.0f));
        }

        // leque do spray: celulas a volta da cabeca, em leque simetrico.
        for (int m = 0; m < marks; ++m) {
            const auto t = marks > 1 ? (static_cast<float>(m) / static_cast<float>(marks - 1))
                                     : 0.5f;
            const auto offset = static_cast<std::int64_t>(
                (t * 2.0f - 1.0f) * static_cast<float>(radiusBytes));
            const auto address = static_cast<std::uint64_t>(
                juce::jlimit<juce::int64>(static_cast<juce::int64>(regionStart_),
                                          static_cast<juce::int64>(regionEnd_) - 1,
                                          static_cast<juce::int64>(head) + offset));
            const auto cell = hex_.cellRectForAddress(address);
            if (cell.isEmpty()) {
                continue;
            }
            g.setColour(palette::alpha(palette::ivory, 0.30f + 0.4f * (1.0f - std::abs(t * 2.0f - 1.0f))));
            g.fillRect(cell.reduced(2.0f));
        }

        // cabeca: caixa cheia.
        const auto headCell = hex_.cellRectForAddress(head);
        if (!headCell.isEmpty()) {
            g.setColour(palette::ivory);
            g.fillRect(headCell.reduced(1.0f));
        }
        static_cast<void>(span);
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
    const auto halfHeight = plot.getHeight() * 0.46f;
    const auto [viewStartByte, viewEndByte] = viewByteRange();
    const auto viewBytes = viewEndByte > viewStartByte ? (viewEndByte - viewStartByte) : 0;
    if (viewBytes == 0 || columns_.empty()) {
        return;
    }

    // barras: um numero INTEIRO de bytes por barra.
    // O zoom salta de byte em byte -- nao existe meio byte nem um terco de byte.
    // No enquadramento total ficam poucas barras grandes; ao aproximar entram
    // mais, ate' 1 barra por byte. E' isto que impede as barras inchadas no
    // zoom medio: cada barra cobre bytes inteiros, nunca uma fraccao.
    //
    // A area das barras e' a area de dados: a mesma caixa que a regua usa para
    // converter byte em x. Se as duas divergirem, o endereco flutua sobre a barra
    // errada -- e' aqui que o alinhamento se ganha ou se perde.
    const auto barPlot = plot;
    const auto barAreaWidth = juce::jmax(1.0f, barPlot.getWidth());
    const auto pxPerByte = barAreaWidth / static_cast<float>(viewBytes);
    // passo minimo entre barras: 8 px. Abaixo disto a barra deixa de se ler.
    constexpr float kBarPitch {8.0f};
    // barras no enquadramento total. 96 e' o ponto em que a onda ainda e'
    // barras e ja' nao e' uma parede de blocos: cada barra cobre ~2 KB do
    // notepad.exe e a coluna de baixo mostra a entropia desse intervalo.
    constexpr float kFullViewBars {96.0f};
    const auto widthBars = static_cast<std::uint64_t>(
        juce::jmax(1.0f, std::floor(barAreaWidth / kBarPitch)));
    const auto zoomBars = static_cast<std::uint64_t>(std::llround(
        static_cast<double>(kFullViewBars) /
        std::max(1.0e-6, static_cast<double>(viewLength_))));
    const auto barCount = std::min(std::min(widthBars, zoomBars), viewBytes);
    const auto bytesPerBar = std::max<std::uint64_t>(1, (viewBytes + barCount - 1) / barCount);
    const auto blocks = static_cast<int>((viewBytes + bytesPerBar - 1) / bytesPerBar);
    // 78% do passo: as barras quase se tocam e a onda le-se como um continuo,
    // nao como uma fila de retangulos separados por buracos.
    constexpr float kBarFill {0.78f};

    // entropia media das colunas: recurso quando uma fatia devolve H = 0
    // (todos os bytes iguais), para a barra nunca ficar invisivel.
    float meanEntropy = 0.0f;
    {
        double sum = 0.0;
        for (const auto& column : columns_) {
            sum += column.entropyBits;
        }
        meanEntropy = static_cast<float>(sum / static_cast<double>(columns_.size()) / 8.0);
    }

    // uma leitura por barra, directa das colunas: nada de reamostrar o que ja'
    // foi reamostrado, nada de quatro passadas de media movel a achatar os picos.
    struct Bar {
        float envelope {0.0f};
        float entropy {0.0f};
    };
    std::vector<Bar> bars(static_cast<std::size_t>(blocks));
    float minRms = 1.0f;
    float maxRms = 0.0f;

    const auto columnCount = static_cast<double>(columns_.size());
    const auto barBytes = static_cast<double>(viewBytes);

    for (int i = 0; i < blocks; ++i) {
        const auto firstByte = static_cast<double>(i) * static_cast<double>(bytesPerBar);
        const auto lastByte =
            std::min(firstByte + static_cast<double>(bytesPerBar), barBytes);
        auto begin = static_cast<std::size_t>(firstByte * columnCount / barBytes);
        auto end = static_cast<std::size_t>(std::ceil(lastByte * columnCount / barBytes));
        begin = std::min(begin, columns_.size() - 1);
        end = std::clamp<std::size_t>(end, begin + 1, columns_.size());

        float sumSquares = 0.0f;
        float entropySum = 0.0f;
        for (auto c = begin; c < end; ++c) {
            const auto& column = columns_[c];
            sumSquares += column.rms * column.rms;
            entropySum += column.entropyBits;
        }

        auto& bar = bars[static_cast<std::size_t>(i)];
        bar.envelope = std::sqrt(sumSquares / static_cast<float>(end - begin));
        bar.entropy = entropySum / static_cast<float>(end - begin) / 8.0f;
        if (bar.entropy <= 1.0e-4f) {
            bar.entropy = meanEntropy;
        }
        minRms = std::min(minRms, bar.envelope);
        maxRms = std::max(maxRms, bar.envelope);
    }

    // contraste esticado entre a barra mais baixa e a mais alta: o binario tem
    // amplitudes muito proximas e sem isto a onda saia uma pente de altura fixa.
    const auto rmsSpan = maxRms - minRms;
    const auto invSpan = rmsSpan > 1.0e-6f ? 1.0f / rmsSpan : 0.0f;

    // alturas em fraccao de meia-area, para centrar o bloco inteiro (envelope
    // em cima, entropia em baixo) no eixo vertical do plot.
    const auto heightOf = [&](const Bar& bar) {
        const auto norm = juce::jlimit(0.0f, 1.0f, (bar.envelope - minRms) * invSpan);
        const auto env = juce::jmax(4.0f / halfHeight,
                                    0.12f + 0.88f * std::pow(norm, 1.2f));
        const auto contrast = juce::jlimit(0.0f, 1.0f, (bar.entropy - 0.45f) / 0.45f);
        const auto ent = juce::jmax(4.0f / halfHeight, contrast * 0.95f);
        return std::pair<float, float> {env, ent};
    };

    float maxEnv = 0.0f;
    float maxEnt = 0.0f;
    for (const auto& bar : bars) {
        const auto [env, ent] = heightOf(bar);
        maxEnv = std::max(maxEnv, env);
        maxEnt = std::max(maxEnt, ent);
    }
    // baseline deslocada para o bloco ficar centrado, mesmo quando a entropia
    // e' quase nula (a 1 byte por barra nao ha entropia para mostrar).
    const auto baseline = juce::jlimit(plot.getY() + maxEnv * halfHeight,
                                       plot.getBottom() - maxEnt * halfHeight,
                                       centreY + (maxEnv - maxEnt) * 0.5f * halfHeight);

    // audio a tocar: barras tremem levemente com o nivel do envelope.
    const auto playing = playheadVisible_;
    const auto level = liveLevel_;
    const auto time = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // eixo: um filete de 1 px na base das barras, continuo por baixo delas.
    // E' o chão da leitura -- sem ele, cada barra parece um rectangulo solto.
    g.setColour(palette::alpha(palette::ivory, 0.16f));
    g.drawHorizontalLine(juce::roundToInt(baseline), barPlot.getX(), barPlot.getRight());

    for (int i = 0; i < blocks; ++i) {
        const auto& bar = bars[static_cast<std::size_t>(i)];

        // envelope: metade de cima, branco cheio. O contraste esticado garante
        // que a forma aparece sempre, mesmo quando todas as barras tem RMS igual.
        const auto [envFraction, entFraction] = heightOf(bar);
        const auto envHeight = envFraction * halfHeight;

        // entropia: contraste expandido na janela 0.45..0.90 (3.6 a 7.2 bits).
        auto entropyHeight = entFraction * halfHeight;
        if (playing) {
            const auto wobble = std::sin(time * 8.0f + static_cast<float>(i) * 1.3f) *
                                    0.5f +
                                std::sin(time * 13.0f + static_cast<float>(i) * 2.7f) *
                                    0.3f;
            entropyHeight *= 1.0f + wobble * level * 0.15f;
        }

        // x da barra: o inicio do seu intervalo de bytes convertido pela MESMA
        // regra da regua -- (byte - inicio da vista) * px por byte. Assim o
        // endereco e a barra assentam no mesmo pixel.
        const auto firstByte =
            static_cast<float>(static_cast<double>(i) * static_cast<double>(bytesPerBar));
        const auto lastByte = juce::jmin(
            firstByte + static_cast<float>(bytesPerBar), static_cast<float>(viewBytes));
        const auto spanX = (lastByte - firstByte) * pxPerByte;
        const auto barWidth = juce::jmax(1.0f, spanX * kBarFill);
        const auto x = barPlot.getX() + firstByte * pxPerByte + (spanX - barWidth) * 0.5f;

        // metade de cima: envelope branco (sobe a partir da baseline)
        g.setColour(juce::Colour {0xfff2f2f2});
        g.fillRect(juce::Rectangle<float> {x, baseline - envHeight, barWidth, envHeight});

        // metade de baixo: entropia em greyscale (escuro = alta, claro = baixa).
        // A gama e' comprimida para [30, 245]: com o mapeamento antigo (255*(1-ent))
        // uma entropia de 6 bits dava cinzento 64 sobre fundo quase preto e a barra
        // desaparecia. Assim nenhuma barra fica invisivel.
        const auto grey = static_cast<std::uint8_t>(
            juce::jlimit(30.0f, 245.0f, 245.0f - (bar.entropy - 0.45f) / 0.45f * 215.0f));
        const juce::Colour entropyGrey {0xff000000 | (grey << 16) | (grey << 8) | grey};
        g.setColour(entropyGrey);
        g.fillRect(juce::Rectangle<float> {x, baseline, barWidth, entropyHeight});
    }

    paintBookmarks(g, plot);
    paintOverview(g);
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

    g.setFont(fonts().mono(10.0f, true));
    const auto firstTick = (viewStartByte + stepBytes - 1) / stepBytes * stepBytes;
    for (auto tick = firstTick; tick < viewEndByte;) {
        const auto x = strip.getX() +
                       static_cast<float>(tick - viewStartByte) * pxPerByte;
        // tick mais presente: haste de 8 px e fio cheio.
        g.setColour(palette::alpha(palette::ivory, 0.55f));
        g.fillRect(juce::Rectangle<float> {x, strip.getBottom() - 8.0f, 1.0f, 8.0f});
        g.setColour(palette::textOnDark);
        // o ultimo endereco nao cabe na margem: sai o texto, fica o tick.
        if (x + 96.0f <= strip.getRight()) {
            g.drawText(hexOf(tick),
                       juce::Rectangle<float> {x + 4.0f, strip.getY() - 1.0f, 92.0f,
                                                strip.getHeight() + 2.0f},
                       juce::Justification::centredLeft, false);
        }
        if (tick > viewEndByte - stepBytes) {
            break;
        }
        tick += stepBytes;
    }

    g.setColour(palette::alpha(palette::ivory, 0.30f));
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

    {
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

void ByteDisplay::rebuildSectionCombo() {
    sectionCombo_.clear(juce::dontSendNotification);
    int id = 1;
    for (const auto& section : sections_) {
        auto label = section.name.isEmpty() ? juce::String {"Seccao"} : section.name;
        label += "  0x" + juce::String::toHexString(static_cast<int>(section.rawOffset)).toUpperCase();
        sectionCombo_.addItem(label, id++);
    }
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

    // barrinhas brancas e finas: spray (banda) e densidade (comb), alinhadas.
    const auto density = juce::jlimit(0.0f, 1.0f, densityE_.value() / 100.0f);
    const auto baseline = bottom - 12.0f;

    // spray: a banda e' marcada por duas hastes finas.
    if (spray > 0.005f) {
        const juce::Colour thin {0xfff2f2f2};
        g.setColour(thin);
        g.fillRect(juce::Rectangle<float> {bandLeft, baseline - 8.0f, 1.0f, 8.0f});
        g.fillRect(juce::Rectangle<float> {bandLeft + bandWidth - 1.0f, baseline - 8.0f,
                                           1.0f, 8.0f});
    }

    // densidade: haste por grama, finas e brancas, na mesma linha do spray.
    {
        const auto bars = 3 + static_cast<int>(std::round(density * 21.0f));
        const auto span = juce::jmin(area.getWidth() * 0.6f, 48.0f + density * 260.0f);
        const auto step = span / static_cast<float>(bars);
        const auto barW = 1.0f;
        const auto left = x - span * 0.5f;
        g.setColour(juce::Colour {0xfff2f2f2});
        for (int i = 0; i < bars; ++i) {
            g.fillRect(juce::Rectangle<float> {left + step * static_cast<float>(i),
                                               baseline - 8.0f, barW, 8.0f});
        }
    }

    // mira: agulha vertical (posicao) + uma barra horizontal (pitch).
    const juce::Colour needleRed {0xffff2020};
    // grossura da agulha vem do grain size: mais size, linhas mais grossas.
    const auto sizeNorm = previewValid_ ? juce::jlimit(0.0f, 1.0f, sizeE_.value() / 2000.0f)
                                        : 0.0f;
    const auto needleWidth = 2.0f + sizeNorm * 4.0f;

    const auto yFrac = juce::jlimit(0.0f, 1.0f, crosshairYE_.value());
    const auto crossY = area.getBottom() - yFrac * area.getHeight();

    // agulha vertical
    g.setColour(needleRed);
    g.fillRect(juce::Rectangle<float> {x - needleWidth * 0.5f, top, needleWidth,
                                       area.getHeight()});

    // barra do meio: uma so, ligada ao pitch (posicao Y da mira).
    const auto midThickness = 2.0f + sizeNorm * 3.0f;
    g.fillRect(juce::Rectangle<float> {x - 24.0f, crossY - midThickness * 0.5f,
                                       48.0f, midThickness});
}

std::uint64_t ByteDisplay::addressAt(juce::Point<float> position) const {
    constexpr auto none = std::numeric_limits<std::uint64_t>::max();

    const auto area = plotBounds();
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

    const auto plotArea = plotBounds();
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

        // clicar e arrastar: rola livremente pela posicao da agulha.
        crosshairDrag_ = true;
        dragTo(event.position);
        return;
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

// arrasto livre: mapeia o cursor no eixo X e no eixo Y e publica.
void ByteDisplay::dragTo(juce::Point<float> position) {
    const auto area = plotBounds();
    if (area.getWidth() <= 0.0f || area.getHeight() <= 0.0f) {
        return;
    }

    const auto xFrac = juce::jlimit(0.0f, 1.0f,
                                    (position.x - area.getX()) / area.getWidth());
    const auto yFrac = juce::jlimit(0.0f, 1.0f,
                                    (area.getBottom() - position.y) / area.getHeight());

    // x: rola a cabeca de leitura para a posicao pedida.
    if (onAddressActivated != nullptr && regionEnd_ > regionStart_) {
        const auto address = pe::byteForPosition(static_cast<double>(xFrac),
                                                 pe::ByteRange {regionStart_, regionEnd_});
        if (address < regionEnd_) {
            caretMoved_ = false;
            onAddressActivated(address);
        }
    }

    // y: move o parametro vertical (pitch).
    crosshairY_ = yFrac;
    crosshairYE_.jumpTo(yFrac);
    if (onCrosshairMoved != nullptr) {
        onCrosshairMoved(xFrac, yFrac);
    }

    repaint();
}

void ByteDisplay::mouseDrag(const juce::MouseEvent& event) {
    if (mode_ == ViewMode::hex) {
        return;
    }

    if (overviewDrag_ || panning_) {
        const auto area = overviewDrag_ ? overviewBounds() : plotBounds();
        if (area.getWidth() > 0.0f && viewLength_ < 1.0f) {
            const auto dx = (event.position.x - lastPanX_) / area.getWidth();
            lastPanX_ = event.position.x;
            viewStart_ = std::clamp(viewStart_ - dx * viewLength_, 0.0f, 1.0f - viewLength_);
            cacheValid_ = false;
            repaint();
        }
        return;
    }

    // arrasto livre: a agulha segue o cursor, x = posicao, y = pitch.
    if (crosshairDrag_) {
        dragTo(event.position);
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
    crosshairDrag_ = false;
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
    const auto area = plotBounds();
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
