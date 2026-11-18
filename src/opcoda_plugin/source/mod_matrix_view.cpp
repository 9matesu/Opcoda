#include "mod_matrix_view.h"

#include <cmath>

namespace opcoda {

namespace {

constexpr int kHeaderHeight {18};
constexpr int kLabelWidth {92};
constexpr int kMinRowHeight {15};
constexpr int kMaxRowHeight {34};

const char* labelFor(dsp::ModDest dest) {
    switch (dest) {
        case dsp::ModDest::kPitch: return "Pitch";
        case dsp::ModDest::kDensity: return "Density";
        case dsp::ModDest::kPosition: return "Position";
        case dsp::ModDest::kSpray: return "Spray";
        case dsp::ModDest::kGrainSize: return "Grain";
        case dsp::ModDest::kCutoff1: return "Cut 1";
        case dsp::ModDest::kQ1: return "Q 1";
        case dsp::ModDest::kCutoff2: return "Cut 2";
        case dsp::ModDest::kQ2: return "Q 2";
        case dsp::ModDest::kPan: return "Pan";
        case dsp::ModDest::kGrainLevel: return "Level";
        case dsp::ModDest::kDrive: return "Drive";
        case dsp::ModDest::kWidth: return "Width";
        case dsp::ModDest::kNone:
        case dsp::ModDest::kCount:
            break;
    }
    return "";
}

int indexOf(dsp::ModDest dest) {
    return static_cast<int>(dest);
}

} // namespace

ModMatrixView::ModMatrixView() {
    setName("Modulation matrix");
    setHelpText("Matriz de modulacao: cinco fontes em colunas, o destino em linhas. "
                "Cada celula e' o amount da rota, editavel por arrasto.");
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

ModMatrixView::~ModMatrixView() {
    if (state_ != nullptr) {
        for (const auto& id : listenerIds_) {
            state_->removeParameterListener(id, this);
        }
    }
}

void ModMatrixView::attach(juce::AudioProcessorValueTreeState& state) {
    if (attached_) {
        return;
    }
    attached_ = true;
    state_ = &state;

    auto bind = [&state, this](const juce::String& id, std::array<juce::RangedAudioParameter*,
                                                                      dsp::kMaxModRoutes>& out,
                               std::size_t index) {
        auto* parameter = state.getParameter(id);
        if (parameter != nullptr) {
            out[index] = parameter;
            state.addParameterListener(id, this);
            listenerIds_.add(id);
        }
    };

    for (int i = 0; i < dsp::kMaxModRoutes; ++i) {
        const auto n = juce::String {i + 1};
        bind("m" + n + "src", srcParams_, static_cast<std::size_t>(i));
        bind("m" + n + "dst", dstParams_, static_cast<std::size_t>(i));
        bind("m" + n + "amt", amtParams_, static_cast<std::size_t>(i));
    }

    refreshRoutes();
    rebuildRows();
}

void ModMatrixView::setSelectedDest(dsp::ModDest dest) {
    if (selected_ == dest) {
        return;
    }
    selected_ = dest;
    rebuildRows();
    repaint();
}

void ModMatrixView::parameterChanged(const juce::String& id, float) {
    if (id.startsWith("m") && id.endsWith("amt")) {
        const auto index = id.substring(1, id.length() - 3).getIntValue() - 1;
        if (index >= 0 && index < dsp::kMaxModRoutes) {
            flashRoute_ = index;
            flash_ = 1.0f;
            if (!isTimerRunning()) {
                startTimerHz(60);
            }
        }
    }
    refreshRoutes();
    rebuildRows();
    repaint();
}

void ModMatrixView::timerCallback() {
    flash_ = juce::jmax(0.0f, flash_ - 0.11f);
    if (flash_ <= 0.0f) {
        stopTimer();
    }
    repaint();
}

void ModMatrixView::refreshRoutes() {
    for (int i = 0; i < dsp::kMaxModRoutes; ++i) {
        const auto index = static_cast<std::size_t>(i);
        auto& route = routes_[index];
        route = {};

        if (srcParams_[index] == nullptr || dstParams_[index] == nullptr ||
            amtParams_[index] == nullptr) {
            continue;
        }

        // getValue() e' normalizado; convertFrom0to1 devolve o valor real.
        const auto source = static_cast<dsp::ModSource>(
            juce::jlimit(0, static_cast<int>(dsp::ModSource::kCount) - 1,
                         juce::roundToInt(srcParams_[index]->convertFrom0to1(
                             srcParams_[index]->getValue()))));
        const auto dest = static_cast<dsp::ModDest>(
            juce::jlimit(0, static_cast<int>(dsp::ModDest::kCount) - 1,
                         juce::roundToInt(dstParams_[index]->convertFrom0to1(
                             dstParams_[index]->getValue()))));

        if (source == dsp::ModSource::kNone || dest == dsp::ModDest::kNone) {
            continue;
        }

        route.source = source;
        route.destination = dest;
        route.amount = juce::jlimit(
            -1.0f, 1.0f,
            amtParams_[index]->convertFrom0to1(amtParams_[index]->getValue()));
    }
}

void ModMatrixView::rebuildRows() {
    std::vector<dsp::ModDest> rows;
    for (int i = static_cast<int>(dsp::ModDest::kPitch);
         i < static_cast<int>(dsp::ModDest::kCount); ++i) {
        rows.push_back(static_cast<dsp::ModDest>(i));
    }
    rows_ = std::move(rows);
}

int ModMatrixView::findRoute(dsp::ModSource source, dsp::ModDest dest) const noexcept {
    for (int i = 0; i < dsp::kMaxModRoutes; ++i) {
        const auto& route = routes_[static_cast<std::size_t>(i)];
        if (route.source == source && route.destination == dest) {
            return i;
        }
    }
    return -1;
}

int ModMatrixView::findEmptyRoute() const noexcept {
    for (int i = 0; i < dsp::kMaxModRoutes; ++i) {
        const auto& route = routes_[static_cast<std::size_t>(i)];
        if (route.source == dsp::ModSource::kNone || route.destination == dsp::ModDest::kNone) {
            return i;
        }
    }
    return -1;
}

void ModMatrixView::writeRoute(int route, dsp::ModSource source, dsp::ModDest dest) const {
    const auto index = static_cast<std::size_t>(route);
    if (srcParams_[index] != nullptr) {
        srcParams_[index]->setValueNotifyingHost(
            srcParams_[index]->convertTo0to1(static_cast<float>(static_cast<int>(source))));
    }
    if (dstParams_[index] != nullptr) {
        dstParams_[index]->setValueNotifyingHost(
            dstParams_[index]->convertTo0to1(static_cast<float>(static_cast<int>(dest))));
    }
}

void ModMatrixView::writeAmount(int route, float amount) const {
    const auto index = static_cast<std::size_t>(route);
    if (amtParams_[index] != nullptr) {
        const auto clamped = juce::jlimit(-1.0f, 1.0f, amount);
        amtParams_[index]->setValueNotifyingHost(
            amtParams_[index]->convertTo0to1(clamped));
    }
}

void ModMatrixView::clearRoute(int route) const {
    const auto index = static_cast<std::size_t>(route);
    if (srcParams_[index] != nullptr) {
        srcParams_[index]->setValueNotifyingHost(srcParams_[index]->convertTo0to1(0.0f));
    }
    if (dstParams_[index] != nullptr) {
        dstParams_[index]->setValueNotifyingHost(dstParams_[index]->convertTo0to1(0.0f));
    }
    if (amtParams_[index] != nullptr) {
        amtParams_[index]->setValueNotifyingHost(amtParams_[index]->convertTo0to1(0.0f));
    }
}

juce::Rectangle<int> ModMatrixView::gridBounds() const {
    return getLocalBounds().reduced(10, 4);
}

juce::Rectangle<int> ModMatrixView::rowBoundsFor(int row) const {
    const auto area = gridBounds();
    if (rows_.empty() || row < 0 || row >= static_cast<int>(rows_.size())) {
        return {};
    }

    const auto body = area.withTrimmedTop(kHeaderHeight);
    const int rowHeight = juce::jlimit(
        kMinRowHeight, kMaxRowHeight,
        body.getHeight() / juce::jmax(1, static_cast<int>(rows_.size())));

    return {area.getX(), body.getY() + row * rowHeight, area.getWidth(), rowHeight};
}

juce::Rectangle<int> ModMatrixView::columnBoundsFor(int column) const {
    const auto area = gridBounds();
    const int columns = static_cast<int>(kColumns.size());
    if (column < 0 || column >= columns) {
        return {};
    }

    const int labelWidth = area.getWidth() < 420 ? 74 : kLabelWidth;
    const int remaining = juce::jmax(1, area.getWidth() - labelWidth);
    const int columnWidth = remaining / columns;

    return {area.getX() + labelWidth + column * columnWidth, area.getY(), columnWidth,
            area.getHeight()};
}

bool ModMatrixView::cellAt(const juce::Point<int>& position, int& row, int& column) const {
    for (int r = 0; r < static_cast<int>(rows_.size()); ++r) {
        if (!rowBoundsFor(r).contains(position)) {
            continue;
        }
        row = r;
        for (int c = 0; c < static_cast<int>(kColumns.size()); ++c) {
            if (columnBoundsFor(c).contains(position)) {
                column = c;
                return true;
            }
        }
    }
    return false;
}

void ModMatrixView::paint(juce::Graphics& g) {
    const auto area = gridBounds();
    g.setColour(palette::display);
    g.fillRect(getLocalBounds().toFloat());

    if (rows_.empty()) {
        g.setFont(type::bodyFont());
        g.setColour(palette::textOnDarkSub);
        g.drawText("Selecione um parâmetro para abrir a matriz",
                   area.withTrimmedTop(kHeaderHeight), juce::Justification::centred, false);
        return;
    }

    const int labelWidth = area.getWidth() < 420 ? 74 : kLabelWidth;
    const int columns = static_cast<int>(kColumns.size());
    const int remaining = juce::jmax(1, area.getWidth() - labelWidth);
    const int columnWidth = remaining / columns;
    const auto body = area.withTrimmedTop(kHeaderHeight);

    // cabecalho das fontes
    g.setFont(type::captionFont());
    g.setColour(palette::textOnDarkSub);
    for (int c = 0; c < columns; ++c) {
        const auto column = columnBoundsFor(c);
        g.drawText(kColumns[static_cast<std::size_t>(c)].label,
                   column.withHeight(kHeaderHeight), juce::Justification::centred, false);
    }
    g.setColour(palette::hair);
    g.fillRect(juce::Rectangle<float> {static_cast<float>(area.getX()),
                                       static_cast<float>(body.getY()) - 1.0f,
                                       static_cast<float>(area.getWidth()), 1.0f});

    // filetes verticais entre as colunas
    g.setColour(palette::hair);
    for (int c = 1; c < columns; ++c) {
        const auto x = area.getX() + labelWidth + c * columnWidth;
        g.fillRect(juce::Rectangle<float> {static_cast<float>(x),
                                           static_cast<float>(body.getY()),
                                           1.0f, static_cast<float>(body.getHeight())});
    }

    // linhas: nome do destino, celulas, divisoria
    for (int r = 0; r < static_cast<int>(rows_.size()); ++r) {
        const auto row = rowBoundsFor(r);
        const auto dest = rows_[static_cast<std::size_t>(r)];
        const bool selected = dest == selected_;
        bool hasRoute = false;
        for (const auto& route : routes_) {
            hasRoute = hasRoute || (route.destination == dest &&
                                    route.source != dsp::ModSource::kNone);
        }

        g.setFont(type::captionFont(hasRoute || selected));
        g.setColour(selected || hasRoute ? palette::textOnDark
                                         : palette::alpha(palette::textOnDark, 0.70f));
        g.drawText(labelFor(dest),
                   juce::Rectangle<int> {area.getX() + (selected ? 6 : 0), row.getY(),
                                         labelWidth - 6, row.getHeight()},
                   juce::Justification::centredLeft, false);

        if (selected) {
            g.setColour(palette::accent);
            g.fillRect(juce::Rectangle<int> {area.getX(), row.getY(), 2, row.getHeight()});
        }

        for (int c = 0; c < columns; ++c) {
            // a coluna e' so' x; a celula cruza-a com a linha.
            const auto column = columnBoundsFor(c);
            const auto cell =
                row.withX(column.getX()).withWidth(column.getWidth());
            const auto source = kColumns[static_cast<std::size_t>(c)].source;
            const int routeIndex = findRoute(source, dest);

            // celula com modulacao: fundo e barra de direcao
            if (routeIndex >= 0) {
                const auto amount = routes_[static_cast<std::size_t>(routeIndex)].amount;
                const auto magnitude = std::abs(amount);
                const auto positive = amount >= 0.0f;

                g.setColour(positive ? palette::alpha(palette::accent, 0.05f + 0.16f * magnitude)
                                     : palette::alpha(palette::textSub, 0.06f + 0.12f * magnitude));
                g.fillRect(cell);

                const auto centre = static_cast<float>(cell.getCentreX());
                const auto barWidth =
                    juce::jmax(2.0f, (static_cast<float>(cell.getWidth()) - 10.0f) * magnitude);
                const auto barY = static_cast<float>(cell.getBottom()) - 4.0f;
                const auto barInk = positive ? palette::accent
                                             : palette::alpha(palette::textSub, 0.90f);
                g.setColour(barInk);
                g.fillRect(juce::Rectangle<float> {centre - barWidth * 0.5f, barY, barWidth, 2.0f});

                g.setFont(type::captionFont());
                g.setColour(barInk);
                const auto percent = amount * 100.0f;
                const auto text = std::abs(percent) >= 9.95f
                                      ? juce::String {juce::roundToInt(percent)}
                                      : juce::String {percent, 1};
                g.drawText(text, cell.withTrimmedBottom(4), juce::Justification::centred, false);
            }

            // hover
            if (r == hoverRow_ && c == hoverColumn_ && !dragging_) {
                g.setColour(palette::alpha(palette::ivory, 0.05f));
                g.fillRect(cell);
            }

            if (routeIndex == flashRoute_ && flash_ > 0.0f) {
                g.setColour(palette::alpha(palette::accent, 0.22f * flash_));
                g.fillRect(cell);
            }

            if (r == hoverRow_ && c == hoverColumn_ && hasKeyboardFocus(true)) {
                g.setColour(palette::focusRing);
                g.drawRect(cell.reduced(1), 1);
            }
        }

        // divisoria horizontal
        if (r + 1 < static_cast<int>(rows_.size())) {
            g.setColour(palette::hair);
            g.fillRect(juce::Rectangle<float> {static_cast<float>(area.getX()),
                                               static_cast<float>(row.getBottom()) - 1.0f,
                                               static_cast<float>(area.getWidth()), 1.0f});
        }
    }
}

void ModMatrixView::resized() { repaint(); }

void ModMatrixView::mouseDown(const juce::MouseEvent& event) {
    int row = -1;
    int column = -1;
    if (!cellAt(event.position.toInt(), row, column)) {
        return;
    }

    const auto dest = rows_[static_cast<std::size_t>(row)];
    const auto source = kColumns[static_cast<std::size_t>(column)].source;

    int route = findRoute(source, dest);
    if (route < 0 && event.mods.isRightButtonDown()) {
        return;
    }
    if (route >= 0 && event.mods.isRightButtonDown()) {
        clearRoute(route);
        refreshRoutes();
        rebuildRows();
        repaint();
        return;
    }

    if (route < 0) {
        route = findEmptyRoute();
        if (route < 0) {
            return;
        }
        writeRoute(route, source, dest);
        writeAmount(route, 0.0f);
        refreshRoutes();
        rebuildRows();
    }

    dragRow_ = row;
    dragColumn_ = column;
    dragRoute_ = route;
    dragStartAmount_ = routes_[static_cast<std::size_t>(route)].amount;
    dragStart_ = event.position.toInt();
    dragging_ = true;
    repaint();
}

void ModMatrixView::mouseDrag(const juce::MouseEvent& event) {
    if (!dragging_ || dragRoute_ < 0) {
        return;
    }

    // vertical ou horizontal: a soma dos dois gestos e' o delta.
    const auto delta = static_cast<float>(dragStart_.y - event.position.y) / 70.0f +
                       static_cast<float>(event.position.x - dragStart_.x) / 140.0f;
    writeAmount(dragRoute_, juce::jlimit(-1.0f, 1.0f, dragStartAmount_ + delta));
    refreshRoutes();
    repaint();
}

void ModMatrixView::mouseUp(const juce::MouseEvent&) {
    if (dragging_ && dragRoute_ >= 0 &&
        std::abs(routes_[static_cast<std::size_t>(dragRoute_)].amount) < 0.005f) {
        clearRoute(dragRoute_);
        refreshRoutes();
        rebuildRows();
    }
    dragging_ = false;
    dragRoute_ = -1;
    dragRow_ = -1;
    dragColumn_ = -1;
    repaint();
}

void ModMatrixView::mouseMove(const juce::MouseEvent& event) {
    int row = -1;
    int column = -1;
    const bool inside = cellAt(event.position.toInt(), row, column);
    const int newRow = inside ? row : -1;
    const int newColumn = inside ? column : -1;
    if (newRow != hoverRow_ || newColumn != hoverColumn_) {
        hoverRow_ = newRow;
        hoverColumn_ = newColumn;
        setMouseCursor(inside ? juce::MouseCursor::PointingHandCursor
                              : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ModMatrixView::mouseExit(const juce::MouseEvent&) {
    if (hoverRow_ >= 0 || hoverColumn_ >= 0) {
        hoverRow_ = -1;
        hoverColumn_ = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ModMatrixView::mouseDoubleClick(const juce::MouseEvent& event) {
    int row = -1;
    int column = -1;
    if (!cellAt(event.position.toInt(), row, column)) {
        return;
    }
    const auto dest = rows_[static_cast<std::size_t>(row)];
    const auto source = kColumns[static_cast<std::size_t>(column)].source;
    if (const int route = findRoute(source, dest); route >= 0) {
        clearRoute(route);
        refreshRoutes();
        rebuildRows();
        repaint();
    }
}

bool ModMatrixView::keyPressed(const juce::KeyPress& key) {
    if (rows_.empty()) {
        return false;
    }

    const auto keyCode = key.getKeyCode();
    const auto move = [this](int dr, int dc) {
        hoverRow_ = hoverRow_ < 0 ? 0
                                  : juce::jlimit(0, static_cast<int>(rows_.size()) - 1,
                                                 hoverRow_ + dr);
        hoverColumn_ = hoverColumn_ < 0
                           ? 0
                           : juce::jlimit(0, static_cast<int>(kColumns.size()) - 1,
                                          hoverColumn_ + dc);
        repaint();
    };

    if (keyCode == juce::KeyPress::upKey) { move(-1, 0); return true; }
    if (keyCode == juce::KeyPress::downKey) { move(1, 0); return true; }
    if (keyCode == juce::KeyPress::leftKey) { move(0, -1); return true; }
    if (keyCode == juce::KeyPress::rightKey) { move(0, 1); return true; }
    if (hoverRow_ < 0 || hoverColumn_ < 0) {
        return false;
    }

    const auto dest = rows_[static_cast<std::size_t>(hoverRow_)];
    const auto source = kColumns[static_cast<std::size_t>(hoverColumn_)].source;
    const int route = findRoute(source, dest);

    if (keyCode == juce::KeyPress::deleteKey || keyCode == juce::KeyPress::backspaceKey) {
        if (route >= 0) {
            clearRoute(route);
            refreshRoutes();
            repaint();
        }
        return true;
    }
    if (keyCode == juce::KeyPress::returnKey) {
        if (route < 0) {
            if (const int free = findEmptyRoute(); free >= 0) {
                writeRoute(free, source, dest);
                writeAmount(free, 0.25f);
                refreshRoutes();
                repaint();
            }
        } else {
            writeAmount(route, 0.0f);
            refreshRoutes();
            repaint();
        }
        return true;
    }
    const auto isMinus = keyCode == static_cast<int>('-');
    const auto isPlus = keyCode == static_cast<int>('+') ||
                        keyCode == static_cast<int>('=');
    if (isPlus || isMinus) {
        const int target = route >= 0 ? route : findEmptyRoute();
        if (target >= 0) {
            if (route < 0) {
                writeRoute(target, source, dest);
            }
            const auto base = route >= 0 ? routes_[static_cast<std::size_t>(route)].amount : 0.0f;
            writeAmount(target, base + (isMinus ? -0.05f : 0.05f));
            refreshRoutes();
            repaint();
        }
        return true;
    }
    return false;
}

const char* ModMatrixView::destName(dsp::ModDest dest) noexcept { return labelFor(dest); }

int ModMatrixView::destIndex(dsp::ModDest dest) noexcept { return indexOf(dest); }

} // namespace opcoda