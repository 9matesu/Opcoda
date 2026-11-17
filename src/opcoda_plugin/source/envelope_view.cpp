#include "envelope_view.h"

#include <cmath>

namespace opcoda {
namespace {

constexpr float kMinSeconds {0.001f};
constexpr float kMaxSeconds {2.0f};
constexpr float kSustainShare {0.20f};
constexpr int kSamplesPerSegment {40};
constexpr float kHandleRadius {4.5f};
constexpr float kHitRadius {9.0f};

juce::String formatSeconds(float seconds) {
    if (seconds < 0.9995f) {
        return juce::String {juce::roundToInt(seconds * 1000.0f)} + " ms";
    }
    return juce::String {seconds, 2} + " s";
}

} // namespace

EnvelopeView::EnvelopeView(juce::AudioProcessorValueTreeState& state) : state_(state) {
    setName("Envelope");
    setHelpText("Arraste as pegas para moldar ataque, decay, sustain e release. "
                "As pegas de curva dobram cada segmento.");
    setWantsKeyboardFocus(false);

    for (const char* id : {"attack", "decay", "sustain", "release", "atkcurve", "deccurve",
                           "relcurve"}) {
        state_.addParameterListener(id, this);
    }
}

EnvelopeView::~EnvelopeView() {
    for (const char* id : {"attack", "decay", "sustain", "release", "atkcurve", "deccurve",
                           "relcurve"}) {
        state_.removeParameterListener(id, this);
    }
}

EnvelopeView::Shape EnvelopeView::shape() const {
    const auto attack = juce::jlimit(kMinSeconds, kMaxSeconds, valueOf("attack"));
    const auto decay = juce::jlimit(kMinSeconds, kMaxSeconds, valueOf("decay"));
    const auto release = juce::jlimit(kMinSeconds, kMaxSeconds, valueOf("release"));
    const auto sustain = juce::jlimit(0.0f, 1.0f, valueOf("sustain"));

    Shape geometry;
    const auto compact = getHeight() < 60;
    geometry.plot = getLocalBounds().toFloat()
                        .reduced(10.0f, compact ? 3.0f : 10.0f)
                        .withTrimmedBottom(compact ? 1.0f : 4.0f);
    geometry.sustain = sustain;
    geometry.attackCurve = valueOf("atkcurve");
    geometry.decayCurve = valueOf("deccurve");
    geometry.releaseCurve = valueOf("relcurve");

    const auto wa = std::sqrt(attack);
    const auto wd = std::sqrt(decay);
    const auto wr = std::sqrt(release);
    const auto ws = kSustainShare * (wa + wd + wr);
    const auto total = juce::jmax(0.001f, wa + wd + ws + wr);
    const auto width = geometry.plot.getWidth();

    geometry.attackX = geometry.plot.getX() + width * wa / total;
    geometry.decayX = geometry.attackX + width * wd / total;
    geometry.releaseX = geometry.decayX + width * ws / total;
    geometry.sustainY = geometry.plot.getBottom() - sustain * geometry.plot.getHeight();
    return geometry;
}

juce::Point<float> EnvelopeView::livePoint(const Shape& geometry) const {
    const auto level = juce::jlimit(0.0f, 1.0f, liveLevel_);
    const auto y = geometry.plot.getBottom() - level * geometry.plot.getHeight();
    const auto phase = juce::jlimit(0.0f, 1.0f, liveStage_);

    switch (livePhase_) {
        case 1:
            return {geometry.plot.getX() + (geometry.attackX - geometry.plot.getX()) * phase, y};
        case 2:
            return {geometry.attackX + (geometry.decayX - geometry.attackX) * phase, y};
        case 3:
            return {(geometry.decayX + geometry.releaseX) * 0.5f, y};
        case 4:
            return {geometry.releaseX + (geometry.plot.getRight() - geometry.releaseX) * phase, y};
        default:
            return {};
    }
}

float EnvelopeView::valueOf(const char* id) const {
    if (const auto* parameter = state_.getParameter(id)) {
        return parameter->convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
}

void EnvelopeView::setValue(const char* id, float value) {
    if (auto* parameter = state_.getParameter(id)) {
        const auto& range = parameter->getNormalisableRange();
        parameter->setValueNotifyingHost(
            range.convertTo0to1(juce::jlimit(range.start, range.end, value)));
    }
}

void EnvelopeView::beginGesture(Handle handle) {
    switch (handle) {
        case Handle::attack:
            gestureA_ = "attack";
            break;
        case Handle::decay:
            gestureA_ = "decay";
            gestureB_ = "sustain";
            break;
        case Handle::release:
            gestureA_ = "release";
            break;
        case Handle::curveAttack:
            gestureA_ = "atkcurve";
            break;
        case Handle::curveDecay:
            gestureA_ = "deccurve";
            break;
        case Handle::curveRelease:
            gestureA_ = "relcurve";
            break;
        case Handle::none:
        default:
            return;
    }

    if (gestureOpen_) {
        return;
    }
    for (const char* id : {gestureA_, gestureB_}) {
        if (id != nullptr) {
            if (auto* parameter = state_.getParameter(id)) {
                parameter->beginChangeGesture();
            }
        }
    }
    gestureOpen_ = true;
}

void EnvelopeView::endGesture() {
    if (!gestureOpen_) {
        return;
    }
    for (const char* id : {gestureA_, gestureB_}) {
        if (id != nullptr) {
            if (auto* parameter = state_.getParameter(id)) {
                parameter->endChangeGesture();
            }
        }
    }
    gestureOpen_ = false;
    gestureA_ = nullptr;
    gestureB_ = nullptr;
}

EnvelopeView::Handle EnvelopeView::handleAt(juce::Point<float> point) const {
    const auto geometry = shape();
    const auto near = [point](juce::Point<float> target) {
        return point.getDistanceFrom(target) <= kHitRadius;
    };

    // a ordem importa: as pegas de curva ficam por baixo das de canto.
    if (near({geometry.attackX, geometry.plot.getY()})) {
        return Handle::attack;
    }
    if (near({geometry.decayX, geometry.sustainY})) {
        return Handle::decay;
    }
    if (near({geometry.releaseX, geometry.sustainY})) {
        return Handle::release;
    }

    const auto attackMidY = geometry.plot.getBottom()
                            - dsp::curveShape(0.5f, geometry.attackCurve) * geometry.plot.getHeight();
    if (near({(geometry.plot.getX() + geometry.attackX) * 0.5f, attackMidY})) {
        return Handle::curveAttack;
    }

    const auto decayLevel = 1.0f - (1.0f - geometry.sustain) * dsp::curveShape(0.5f, geometry.decayCurve);
    const auto decayMidY = geometry.plot.getBottom() - decayLevel * geometry.plot.getHeight();
    if (near({(geometry.attackX + geometry.decayX) * 0.5f, decayMidY})) {
        return Handle::curveDecay;
    }

    const auto releaseLevel = geometry.sustain * (1.0f - dsp::curveShape(0.5f, geometry.releaseCurve));
    const auto releaseMidY = geometry.plot.getBottom() - releaseLevel * geometry.plot.getHeight();
    if (near({(geometry.releaseX + geometry.plot.getRight()) * 0.5f, releaseMidY})) {
        return Handle::curveRelease;
    }

    return Handle::none;
}

juce::String EnvelopeView::labelFor(Handle handle) {
    switch (handle) {
        case Handle::attack:
            return "Attack";
        case Handle::decay:
            return "Decay / Sustain";
        case Handle::release:
            return "Release";
        case Handle::curveAttack:
            return "Attack curve";
        case Handle::curveDecay:
            return "Decay curve";
        case Handle::curveRelease:
            return "Release curve";
        case Handle::none:
        default:
            return {};
    }
}

void EnvelopeView::setLive(float level, float stage, int phase) {
    const auto changed = phase != livePhase_ || std::abs(stage - liveStage_) > 0.01f
                         || std::abs(level - liveLevel_) > 0.004f;
    liveLevel_ = level;
    liveStage_ = stage;
    livePhase_ = phase;
    if (changed) {
        repaint();
    }
}

void EnvelopeView::mouseDown(const juce::MouseEvent& event) {
    active_ = handleAt(event.position);
    if (active_ == Handle::none) {
        return;
    }

    startTime_ = active_ == Handle::decay ? valueOf("decay")
                : active_ == Handle::release ? valueOf("release")
                                             : valueOf("attack");
    startSustain_ = valueOf("sustain");
    startCurve_ = active_ == Handle::curveAttack    ? valueOf("atkcurve")
                  : active_ == Handle::curveDecay   ? valueOf("deccurve")
                                                    : valueOf("relcurve");
    beginGesture(active_);
    repaint();
}

void EnvelopeView::mouseDrag(const juce::MouseEvent& event) {
    if (active_ == Handle::none) {
        return;
    }

    const auto geometry = shape();
    const auto dx = static_cast<float>(event.getDistanceFromDragStartX());
    const auto dy = static_cast<float>(event.getDistanceFromDragStartY());

    const auto scaled = [dx](float seconds) {
        return juce::jlimit(kMinSeconds, kMaxSeconds, seconds * std::exp(dx * 0.012f));
    };

    switch (active_) {
        case Handle::attack:
            setValue("attack", scaled(startTime_));
            break;
        case Handle::decay:
            setValue("decay", scaled(startTime_));
            setValue("sustain",
                     juce::jlimit(0.0f, 1.0f,
                                  startSustain_ - dy / juce::jmax(1.0f, geometry.plot.getHeight())));
            break;
        case Handle::release:
            setValue("release", scaled(startTime_));
            break;
        case Handle::curveAttack:
        case Handle::curveDecay:
        case Handle::curveRelease: {
            const auto delta = -dy / juce::jmax(1.0f, geometry.plot.getHeight() * 0.6f);
            const auto curve = juce::jlimit(-1.0f, 1.0f, startCurve_ + delta);
            const char* id = active_ == Handle::curveAttack    ? "atkcurve"
                             : active_ == Handle::curveDecay   ? "deccurve"
                                                               : "relcurve";
            setValue(id, curve);
            break;
        }
        case Handle::none:
        default:
            break;
    }

    repaint();
}

void EnvelopeView::mouseUp(const juce::MouseEvent&) {
    endGesture();
    active_ = Handle::none;
    repaint();
}

void EnvelopeView::mouseMove(const juce::MouseEvent& event) {
    const auto next = handleAt(event.position);
    if (next != hover_) {
        hover_ = next;
        setMouseCursor(next == Handle::none ? juce::MouseCursor::NormalCursor
                                            : juce::MouseCursor::LeftRightResizeCursor);
        repaint();
    }
}

void EnvelopeView::mouseExit(const juce::MouseEvent&) {
    if (hover_ != Handle::none) {
        hover_ = Handle::none;
        repaint();
    }
}

void EnvelopeView::paint(juce::Graphics& g) {
    const auto geometry = shape();
    const auto& plot = geometry.plot;

    // -- ecra afundado --
    const auto bezel = getLocalBounds().toFloat();
    g.setColour(palette::display);
    g.fillRoundedRectangle(bezel, 6.0f);

    g.setColour(palette::displayGrid);
    for (int i = 1; i < 4; ++i) {
        const auto y = plot.getY() + plot.getHeight() * static_cast<float>(i) / 4.0f;
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    }

    // -- a forma --
    juce::Path path;
    path.startNewSubPath(plot.getX(), plot.getBottom());
    for (int i = 1; i <= kSamplesPerSegment; ++i) {
        const auto p = static_cast<float>(i) / static_cast<float>(kSamplesPerSegment);
        const auto level = dsp::curveShape(p, geometry.attackCurve);
        path.lineTo(plot.getX() + (geometry.attackX - plot.getX()) * p,
                    plot.getBottom() - level * plot.getHeight());
    }
    for (int i = 1; i <= kSamplesPerSegment; ++i) {
        const auto p = static_cast<float>(i) / static_cast<float>(kSamplesPerSegment);
        const auto level = 1.0f - (1.0f - geometry.sustain) * dsp::curveShape(p, geometry.decayCurve);
        path.lineTo(geometry.attackX + (geometry.decayX - geometry.attackX) * p,
                    plot.getBottom() - level * plot.getHeight());
    }
    path.lineTo(geometry.releaseX, geometry.sustainY);
    for (int i = 1; i <= kSamplesPerSegment; ++i) {
        const auto p = static_cast<float>(i) / static_cast<float>(kSamplesPerSegment);
        const auto level = geometry.sustain * (1.0f - dsp::curveShape(p, geometry.releaseCurve));
        path.lineTo(geometry.releaseX + (plot.getRight() - geometry.releaseX) * p,
                    plot.getBottom() - level * plot.getHeight());
    }

    juce::Path filled {path};
    filled.lineTo(plot.getRight(), plot.getBottom());
    filled.closeSubPath();
    g.setColour(palette::alpha(palette::accent, 0.20f));
    g.fillPath(filled);

    g.setColour(palette::accentSoft);
    g.strokePath(path, juce::PathStrokeType {2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded});

    g.setColour(palette::alpha(palette::textOnDarkSub, 0.60f));
    g.setFont(fonts().sans(8.5f, true).withExtraKerningFactor(0.14f));
    const auto stageMark = [&](float x0, float x1, const char* letter) {
        g.drawText(letter,
                   juce::Rectangle<float> {(x0 + x1) * 0.5f - 8.0f, plot.getBottom() - 11.0f, 16.0f,
                                           10.0f},
                   juce::Justification::centred, false);
    };
    if (getHeight() >= 60) {
        stageMark(plot.getX(), geometry.attackX, "A");
        stageMark(geometry.attackX, geometry.decayX, "D");
        stageMark(geometry.decayX, geometry.releaseX, "S");
        stageMark(geometry.releaseX, plot.getRight(), "R");
    }

    // -- pegas --
    const auto drawHandle = [&g](juce::Point<float> point, bool bright) {
        g.setColour(bright ? palette::ivory : palette::accentSoft);
        g.fillEllipse(juce::Rectangle<float> {kHandleRadius * 2.0f, kHandleRadius * 2.0f}
                          .withCentre(point));
        g.setColour(palette::display);
        g.drawEllipse(juce::Rectangle<float> {kHandleRadius * 2.0f, kHandleRadius * 2.0f}
                          .withCentre(point),
                      1.2f);
    };

    const auto hot = active_ != Handle::none ? active_ : hover_;

    drawHandle({geometry.attackX, plot.getY()}, hot == Handle::attack);
    drawHandle({geometry.decayX, geometry.sustainY}, hot == Handle::decay);
    drawHandle({geometry.releaseX, geometry.sustainY}, hot == Handle::release);

    const auto curveHandle = [&](float x0, float x1, float y) {
        g.setColour(palette::alpha(palette::accentSoft, 0.75f));
        g.drawEllipse(juce::Rectangle<float> {5.0f, 5.0f}
                          .withCentre({(x0 + x1) * 0.5f, y}),
                      1.2f);
    };
    curveHandle(plot.getX(), geometry.attackX,
                plot.getBottom() - dsp::curveShape(0.5f, geometry.attackCurve) * plot.getHeight());
    curveHandle(geometry.attackX, geometry.decayX,
                plot.getBottom()
                    - (1.0f - (1.0f - geometry.sustain) * dsp::curveShape(0.5f, geometry.decayCurve))
                          * plot.getHeight());
    curveHandle(geometry.releaseX, plot.getRight(),
                plot.getBottom()
                    - geometry.sustain
                          * (1.0f - dsp::curveShape(0.5f, geometry.releaseCurve))
                          * plot.getHeight());

    // -- ponto vivo --
    if (livePhase_ > 0) {
        const auto point = livePoint(geometry);
        g.setColour(palette::alpha(palette::accentSoft, 0.28f));
        g.fillEllipse(juce::Rectangle<float> {16.0f, 16.0f}.withCentre(point));
        g.setColour(palette::ivory);
        g.fillEllipse(juce::Rectangle<float> {7.0f, 7.0f}.withCentre(point));
    }

    // -- leitura da pega em foco --
    if (hot != Handle::none && getHeight() >= 60) {
        const auto label = labelFor(hot);
        const auto value = hot == Handle::attack     ? formatSeconds(valueOf("attack"))
                           : hot == Handle::decay    ? formatSeconds(valueOf("decay"))
                           : hot == Handle::release  ? formatSeconds(valueOf("release"))
                           : hot == Handle::curveAttack  ? juce::String {valueOf("atkcurve"), 2}
                           : hot == Handle::curveDecay   ? juce::String {valueOf("deccurve"), 2}
                                                         : juce::String {valueOf("relcurve"), 2};
        g.setColour(palette::textOnDarkSub);
        g.setFont(fonts().sans(9.0f, true).withExtraKerningFactor(0.12f));
        g.drawText(label, plot.withHeight(12.0f), juce::Justification::topLeft, false);
        g.setColour(palette::ivory);
        g.setFont(fonts().sans(10.5f, true));
        g.drawText(value, plot.withHeight(12.0f), juce::Justification::topRight, false);
    }
}

} // namespace opcoda
