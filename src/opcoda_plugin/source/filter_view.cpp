#include "filter_view.h"

#include "opcoda_core/dsp/biquad.h"

#include <cmath>

namespace opcoda {
namespace {

constexpr float kMinHz {20.0f};
constexpr float kMaxHz {20000.0f};
constexpr float kTopDb {18.0f};
constexpr float kBottomDb {-48.0f};
constexpr int kSamples {96};
constexpr float kHandleRadius {4.5f};
constexpr float kHitRadius {11.0f};

juce::String formatHz(float hz) {
    if (hz >= 1000.0f) {
        return juce::String {hz / 1000.0f, hz >= 10000.0f ? 0 : 1} + "k";
    }
    return juce::String {juce::roundToInt(hz)};
}

dsp::FilterType typeOf(int index) {
    return dsp::sanitizeFilterType(static_cast<dsp::FilterType>(juce::jlimit(0, 3, index)));
}

// |H(e^jw)| do biquad, em dB. mesma conta do filtro: coeficientes normalizados
// por a0, resposta avaliada no circulo unitario.
float biquadDb(const dsp::BiquadCoeffs& c, float hz, double sampleRate) {
    const auto w = juce::MathConstants<double>::twoPi * static_cast<double>(hz) / sampleRate;
    const auto cosw = std::cos(w);
    const auto sinw = std::sin(w);
    const auto cos2w = std::cos(2.0 * w);
    const auto sin2w = std::sin(2.0 * w);

    const auto numRe = c.b0 + c.b1 * cosw + c.b2 * cos2w;
    const auto numIm = -(c.b1 * sinw + c.b2 * sin2w);
    const auto denRe = 1.0 + c.a1 * cosw + c.a2 * cos2w;
    const auto denIm = -(c.a1 * sinw + c.a2 * sin2w);

    const auto num = std::hypot(static_cast<double>(numRe), static_cast<double>(numIm));
    const auto den = std::hypot(denRe, denIm);
    if (den < 1.0e-12) {
        return kBottomDb;
    }
    const auto magnitude = num / den;
    if (magnitude < 1.0e-6) {
        return kBottomDb;
    }
    return static_cast<float>(20.0 * std::log10(magnitude));
}

} // namespace

FilterView::FilterView(juce::AudioProcessorValueTreeState& state) : state_(state) {
    setName("Filter response");
    setHelpText("Arraste as pegas para mudar o corte (horizontal) e o Q (vertical) "
                "de cada filtro.");
    setWantsKeyboardFocus(false);
}

FilterView::Response FilterView::response() const {
    Response geometry;
    geometry.plot = getLocalBounds().toFloat().reduced(10.0f, 10.0f).withTrimmedBottom(12.0f);

    geometry.cutoff1 = juce::jlimit(kMinHz, kMaxHz, valueOf("f1cutoff"));
    geometry.cutoff2 = juce::jlimit(kMinHz, kMaxHz, valueOf("f2cutoff"));
    geometry.q1 = juce::jlimit(0.5f, 12.0f, valueOf("f1q"));
    geometry.q2 = juce::jlimit(0.5f, 12.0f, valueOf("f2q"));
    geometry.type1 = juce::roundToInt(valueOf("f1type"));
    geometry.type2 = juce::roundToInt(valueOf("f2type"));
    geometry.slope24 = juce::roundToInt(valueOf("f1slope")) != 0;

    geometry.sampleRate = 44100.0;
    if (const auto rate = state_.processor.getSampleRate(); rate > 0.0) {
        geometry.sampleRate = rate;
    }
    return geometry;
}

float FilterView::responseDb(const Response& geometry, float hz, float cutoff1, float cutoff2) {
    const auto sr = juce::jmax(8000.0, geometry.sampleRate);
    const auto safeHz = juce::jlimit(kMinHz, static_cast<float>(sr * 0.49), hz);

    const auto coeffs1 = dsp::makeBiquadCoeffs(typeOf(geometry.type1), cutoff1, geometry.q1, sr);
    auto db = biquadDb(coeffs1, safeHz, sr);
    if (geometry.slope24) {
        // 24 dB e' o mesmo biquad aplicado duas vezes: a magnitude ao quadrado.
        db *= 2.0f;
    }

    const auto coeffs2 = dsp::makeBiquadCoeffs(typeOf(geometry.type2), cutoff2, geometry.q2, sr);
    db += biquadDb(coeffs2, safeHz, sr);
    return juce::jlimit(kBottomDb, kTopDb, db);
}

float FilterView::valueOf(const char* id) const {
    if (const auto* parameter = state_.getParameter(id)) {
        return parameter->convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
}

void FilterView::setValue(const char* id, float value) {
    if (auto* parameter = state_.getParameter(id)) {
        const auto& range = parameter->getNormalisableRange();
        parameter->setValueNotifyingHost(
            range.convertTo0to1(juce::jlimit(range.start, range.end, value)));
    }
}

float FilterView::xForHz(float hz, const juce::Rectangle<float>& plot) const {
    const auto t = std::log(juce::jlimit(kMinHz, kMaxHz, hz) / kMinHz)
                   / std::log(kMaxHz / kMinHz);
    return plot.getX() + plot.getWidth() * static_cast<float>(t);
}

float FilterView::hzForX(float x, const juce::Rectangle<float>& plot) const {
    const auto t = juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
    return kMinHz * std::pow(kMaxHz / kMinHz, t);
}

float FilterView::yForDb(float db, const juce::Rectangle<float>& plot) const {
    const auto t = (juce::jlimit(kBottomDb, kTopDb, db) - kBottomDb) / (kTopDb - kBottomDb);
    return plot.getBottom() - plot.getHeight() * t;
}

FilterView::Handle FilterView::handleAt(juce::Point<float> point) const {
    const auto geometry = response();
    const auto near = [point](juce::Point<float> target) {
        return point.getDistanceFrom(target) <= kHitRadius;
    };

    const auto y1 = yForDb(responseDb(geometry, geometry.cutoff1, geometry.cutoff1,
                                     geometry.cutoff2),
                           geometry.plot);
    if (near({xForHz(geometry.cutoff1, geometry.plot), y1})) {
        return Handle::first;
    }

    const auto y2 = yForDb(responseDb(geometry, geometry.cutoff2, geometry.cutoff1,
                                     geometry.cutoff2),
                           geometry.plot);
    if (near({xForHz(geometry.cutoff2, geometry.plot), y2})) {
        return Handle::second;
    }

    return Handle::none;
}

void FilterView::beginGesture(Handle handle) {
    switch (handle) {
        case Handle::first:
            gestureA_ = "f1cutoff";
            gestureB_ = "f1q";
            break;
        case Handle::second:
            gestureA_ = "f2cutoff";
            gestureB_ = "f2q";
            break;
        case Handle::none:
        default:
            return;
    }

    if (gestureOpen_) {
        return;
    }
    for (const char* id : {gestureA_, gestureB_}) {
        if (auto* parameter = state_.getParameter(id)) {
            parameter->beginChangeGesture();
        }
    }
    gestureOpen_ = true;
}

void FilterView::endGesture() {
    if (!gestureOpen_) {
        return;
    }
    for (const char* id : {gestureA_, gestureB_}) {
        if (auto* parameter = state_.getParameter(id)) {
            parameter->endChangeGesture();
        }
    }
    gestureOpen_ = false;
    gestureA_ = nullptr;
    gestureB_ = nullptr;
}

void FilterView::setLive(float cutoff1, float cutoff2) {
    const auto changed = std::abs(cutoff1 - liveCutoff1_) > 1.0f
                         || std::abs(cutoff2 - liveCutoff2_) > 1.0f;
    liveCutoff1_ = cutoff1;
    liveCutoff2_ = cutoff2;
    if (changed) {
        repaint();
    }
}

void FilterView::mouseDown(const juce::MouseEvent& event) {
    active_ = handleAt(event.position);
    if (active_ == Handle::none) {
        return;
    }

    startCutoff_ = valueOf(active_ == Handle::first ? "f1cutoff" : "f2cutoff");
    startQ_ = valueOf(active_ == Handle::first ? "f1q" : "f2q");
    beginGesture(active_);
    repaint();
}

void FilterView::mouseDrag(const juce::MouseEvent& event) {
    if (active_ == Handle::none) {
        return;
    }

    const auto geometry = response();
    const auto dx = static_cast<float>(event.getDistanceFromDragStartX());
    const auto dy = static_cast<float>(event.getDistanceFromDragStartY());

    // o eixo x e' logaritmico: a largura toda vale as tres decadas de 20 Hz a
    // 20 kHz, por isso um pixel vale a mesma proporcao em qualquer zona.
    const auto cutoff = juce::jlimit(
        kMinHz, kMaxHz,
        startCutoff_ * std::pow(kMaxHz / kMinHz, dx / juce::jmax(1.0f, geometry.plot.getWidth())));
    const auto q = juce::jlimit(0.5f, 12.0f, startQ_ * std::exp(-dy * 0.012f));

    setValue(active_ == Handle::first ? "f1cutoff" : "f2cutoff", cutoff);
    setValue(active_ == Handle::first ? "f1q" : "f2q", q);
    repaint();
}

void FilterView::mouseUp(const juce::MouseEvent&) {
    endGesture();
    active_ = Handle::none;
    repaint();
}

void FilterView::mouseMove(const juce::MouseEvent& event) {
    const auto next = handleAt(event.position);
    if (next != hover_) {
        hover_ = next;
        setMouseCursor(next == Handle::none ? juce::MouseCursor::NormalCursor
                                            : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
}

void FilterView::mouseExit(const juce::MouseEvent&) {
    if (hover_ != Handle::none) {
        hover_ = Handle::none;
        repaint();
    }
}

void FilterView::paint(juce::Graphics& g) {
    const auto geometry = response();
    const auto& plot = geometry.plot;

    const auto bezel = getLocalBounds().toFloat();
    g.setColour(palette::display);
    g.fillRoundedRectangle(bezel, 3.0f);
    g.setColour(palette::displayBorder);
    g.drawRoundedRectangle(bezel.reduced(0.5f), 3.0f, 1.0f);

    const auto buildPath = [&](float cutoff1, float cutoff2) {
        juce::Path path;
        for (int i = 0; i < kSamples; ++i) {
            const auto t = static_cast<float>(i) / static_cast<float>(kSamples - 1);
            const auto hz = kMinHz * std::pow(kMaxHz / kMinHz, t);
            const auto x = plot.getX() + plot.getWidth() * t;
            const auto y = yForDb(responseDb(geometry, hz, cutoff1, cutoff2), plot);
            if (i == 0) {
                path.startNewSubPath(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        return path;
    };

    // o corpo e a linha viva seguem o corte modulado -- e' o que se ouve. a linha
    // cheia fica nos knobs, para se ver de onde o corte saiu.
    const auto live1 = liveCutoff1_ > 0.0f ? liveCutoff1_ : geometry.cutoff1;
    const auto live2 = liveCutoff2_ > 0.0f ? liveCutoff2_ : geometry.cutoff2;
    const auto modulated = std::abs(live1 - geometry.cutoff1) > 0.5f
                           || std::abs(live2 - geometry.cutoff2) > 0.5f;

    const auto bodyPath = buildPath(modulated ? live1 : geometry.cutoff1,
                                    modulated ? live2 : geometry.cutoff2);

    // o corpo por baixo da curva. sem ele o ecra fica um campo escuro com um
    // risco em cima, porque o filtro de fabrica corta a 20 kHz e a resposta e'
    // plana quase toda a largura.
    {
        juce::Path filled {bodyPath};
        filled.lineTo(plot.getRight(), plot.getBottom());
        filled.lineTo(plot.getX(), plot.getBottom());
        filled.closeSubPath();
        g.setColour(palette::alpha(palette::accent, 0.16f));
        g.fillPath(filled);
    }

    // grelha por cima do corpo: por baixo, o banho de laranja apagava as linhas
    // de 0 e -12 dB, que sao as que se leem num filtro.
    g.setColour(palette::displayGrid);
    for (const auto hz : {100.0f, 1000.0f, 10000.0f}) {
        const auto x = xForHz(hz, plot);
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
    }
    for (const auto db : {0.0f, -12.0f, -24.0f, -36.0f}) {
        const auto y = yForDb(db, plot);
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    }

    const auto strokePath = [&](const juce::Path& path, juce::Colour ink, float thickness) {
        g.setColour(ink);
        g.strokePath(path, juce::PathStrokeType {thickness, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded});
    };

    if (modulated) {
        strokePath(bodyPath, palette::alpha(palette::ivory, 0.60f), 1.2f);
        strokePath(buildPath(geometry.cutoff1, geometry.cutoff2), palette::accentSoft, 2.0f);
    } else {
        strokePath(bodyPath, palette::accentSoft, 2.0f);
    }

    // -- pegas de corte --
    const auto hot = active_ != Handle::none ? active_ : hover_;
    const auto drawHandle = [&](float cutoff, bool bright, bool second) {
        const auto x = xForHz(cutoff, plot);
        const auto y = yForDb(responseDb(geometry, cutoff, geometry.cutoff1, geometry.cutoff2), plot);
        const auto centre = juce::Point<float> {x, y};

        g.setColour(palette::alpha(bright ? palette::ivory : palette::accentSoft, 0.55f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());

        const auto size = kHandleRadius * 2.0f;
        const auto disc = juce::Rectangle<float> {size, size}.withCentre(centre);
        g.setColour(bright ? palette::ivory : palette::accentSoft);
        if (second) {
            // filtro 2: aro vazado. com os dois cortes no mesmo sitio -- o caso
            // de fabrica, 20 kHz nos dois -- a pega cheia do filtro 1 escondia a
            // do filtro 2 e so' uma parecia existir.
            g.fillEllipse(disc);
            g.setColour(palette::display);
            g.fillEllipse(disc.reduced(2.0f));
            g.setColour(bright ? palette::ivory : palette::accentSoft);
            g.drawEllipse(disc, 1.4f);
        } else {
            g.fillEllipse(disc);
        }
        g.setColour(palette::display);
        g.drawEllipse(disc, 1.2f);
    };

    drawHandle(geometry.cutoff1, hot == Handle::first, false);
    drawHandle(geometry.cutoff2, hot == Handle::second, true);

    // -- leituras --
    g.setFont(fonts().sans(8.5f, true).withExtraKerningFactor(0.10f));
    g.setColour(palette::alpha(palette::textOnDarkSub, 0.75f));
    for (const auto hz : {100.0f, 1000.0f, 10000.0f}) {
        g.drawText(formatHz(hz),
                   juce::Rectangle<float> {xForHz(hz, plot) - 22.0f, plot.getBottom() + 1.0f, 44.0f,
                                           11.0f},
                   juce::Justification::centred, false);
    }

    if (hot != Handle::none) {
        const auto first = hot == Handle::first;
        const auto cutoff = first ? geometry.cutoff1 : geometry.cutoff2;
        const auto q = first ? geometry.q1 : geometry.q2;
        const auto text = juce::String {first ? "F1 " : "F2 "} + formatHz(cutoff) + " Hz  Q "
                          + juce::String {q, 2};
        g.setColour(palette::ivory);
        g.setFont(fonts().sans(10.0f, true));
        g.drawText(text, plot.withHeight(12.0f), juce::Justification::topRight, false);
    }
}

} // namespace opcoda
