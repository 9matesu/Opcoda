#include "lfo_view.h"

#include <cmath>

namespace opcoda {
namespace {

constexpr int kPoints {256};
constexpr int kCycles {2};

} // namespace

LfoView::LfoView(juce::AudioProcessorValueTreeState& state) : state_(state) {
    setName("Modulation");
    setHelpText("A forma do LFO, a profundidade e o ponto vivo do audio. Os "
                "knobs ao lado escolhem onda, ritmo, alvo e profundidade.");
    setWantsKeyboardFocus(false);

    for (const char* id : {"lfowave", "lfotarget", "lfodepth", "lfosteps", "lforate",
                           "lfosync", "lfodiv"}) {
        state_.addParameterListener(id, this);
    }
}

LfoView::~LfoView() {
    for (const char* id : {"lfowave", "lfotarget", "lfodepth", "lfosteps", "lforate",
                           "lfosync", "lfodiv"}) {
        state_.removeParameterListener(id, this);
    }
}

float LfoView::valueOf(const char* id) const {
    if (const auto* parameter = state_.getParameter(id)) {
        return parameter->convertFrom0to1(parameter->getValue());
    }
    return 0.0f;
}

float LfoView::rateHz() const {
    const auto rate = juce::jlimit(0.01f, 20.0f, valueOf("lforate"));
    if (valueOf("lfosync") <= 0.5f) {
        return rate;
    }
    const auto div = juce::roundToInt(valueOf("lfodiv"));
    return static_cast<float>(dsp::syncedRateHz(div, 120.0));
}

juce::Colour LfoView::inkFor(dsp::LFOTarget target) const noexcept {
    switch (target) {
        case dsp::LFOTarget::kCutoff:
            return palette::accentSoft;
        case dsp::LFOTarget::kDensity:
            return palette::okBright;
        case dsp::LFOTarget::kPosition:
            return palette::brass;
        case dsp::LFOTarget::kPitch:
        default:
            return palette::ivory;
    }
}

void LfoView::setLive(float phase, float value) {
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto delta = lastLiveMs_ > 0.0
                           ? juce::jlimit(0.0, 0.1, (now - lastLiveMs_) / 1000.0)
                           : 1.0 / 60.0;
    lastLiveMs_ = now;

    const auto wrapped = phase - std::floor(phase);
    float step = wrapped - rawPhasePrev_;
    rawPhasePrev_ = wrapped;
    if (step > 0.5f) {
        step -= 1.0f;
    } else if (step < -0.5f) {
        step += 1.0f;
    }

    if (!liveSeen_) {
        phaseContinuous_ = wrapped;
        phaseEased_.jumpTo(phaseContinuous_);
        valueEased_.jumpTo(value);
        liveSeen_ = true;
    } else {
        if (std::abs(step) > 1e-7f) {
            phaseContinuous_ += step;
        } else {
            // fase parada (sem audio a correr): avanca ao ritmo do LFO.
            phaseContinuous_ += rateHz() * static_cast<float>(delta);
        }
        phaseEased_.set(phaseContinuous_);
        valueEased_.set(value);
    }

    const auto beforePhase = livePhase_;
    const auto beforeValue = liveValue_;
    phaseEased_.tick(static_cast<float>(delta), 0.012f);
    valueEased_.tick(static_cast<float>(delta), 0.012f);
    livePhase_ = phaseEased_.value();
    liveValue_ = valueEased_.value();

    if (std::abs(livePhase_ - beforePhase) > 0.0005f ||
        std::abs(liveValue_ - beforeValue) > 0.0005f) {
        repaint();
    }
}

void LfoView::paint(juce::Graphics& g) {
    const auto bezel = getLocalBounds().toFloat();
    const auto plot = bezel.reduced(10.0f, 10.0f).withTrimmedBottom(12.0f);
    if (plot.getWidth() <= 0.0f || plot.getHeight() <= 0.0f) {
        return;
    }

    // -- ecra afundado --
    g.setColour(palette::display);
    g.fillRoundedRectangle(bezel, 6.0f);

    const auto wave = dsp::sanitizeLFOWave(
        static_cast<dsp::LFOWave>(juce::roundToInt(valueOf("lfowave"))));
    const auto target = dsp::sanitizeLFOTarget(
        static_cast<dsp::LFOTarget>(juce::roundToInt(valueOf("lfotarget"))));
    const auto depth = juce::jlimit(0.0f, 1.0f, valueOf("lfodepth"));
    const auto steps = juce::roundToInt(valueOf("lfosteps"));

    const auto centreY = plot.getCentreY();
    const auto shapeAmplitude = plot.getHeight() * 0.42f;
    const auto liveAmplitude = shapeAmplitude * depth;
    // a onda corre com a fase viva: dois ciclos visiveis, o de agora a' esquerda.
    const auto scroll = liveSeen_
                            ? std::fmod(static_cast<float>(livePhase_),
                                        static_cast<float>(kCycles))
                            : 0.0f;
    const auto yFor = [centreY](float value, float amplitude) {
        return centreY - value * amplitude;
    };

    g.setColour(palette::displayGrid);
    const juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(plot.toNearestInt());
    for (int i = 1; i < 4; ++i) {
        const auto y = plot.getY() + plot.getHeight() * static_cast<float>(i) / 4.0f;
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    }
    g.setColour(palette::alpha(palette::textOnDarkSub, 0.28f));
    g.drawHorizontalLine(juce::roundToInt(centreY), plot.getX(), plot.getRight());

    juce::Path shape;
    for (int i = 0; i < kPoints; ++i) {
        const auto t = static_cast<float>(i) / static_cast<float>(kPoints - 1);
        const auto phase = scroll + t * static_cast<float>(kCycles);
        const auto value = dsp::LFO::shapeAt(wave, dsp::quantizeLFOPhase(phase, steps));
        const auto x = plot.getX() + t * plot.getWidth();
        const auto y = yFor(value, shapeAmplitude);
        if (i == 0) {
            shape.startNewSubPath(x, y);
        } else {
            shape.lineTo(x, y);
        }
    }
    g.setColour(palette::alpha(palette::textOnDarkSub, 0.45f));
    g.strokePath(shape, juce::PathStrokeType {1.4f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded});

    // a curva viva: o que a profundidade deixa passar.
    juce::Path path;
    for (int i = 0; i < kPoints; ++i) {
        const auto t = static_cast<float>(i) / static_cast<float>(kPoints - 1);
        const auto phase = scroll + t * static_cast<float>(kCycles);
        const auto value = dsp::LFO::shapeAt(wave, dsp::quantizeLFOPhase(phase, steps));
        const auto x = plot.getX() + t * plot.getWidth();
        const auto y = yFor(value, liveAmplitude);
        if (i == 0) {
            path.startNewSubPath(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    juce::Path filled {path};
    filled.lineTo(plot.getRight(), centreY);
    filled.lineTo(plot.getX(), centreY);
    filled.closeSubPath();
    g.setColour(palette::alpha(inkFor(target), 0.14f));
    g.fillPath(filled);

    g.setColour(palette::alpha(inkFor(target), 0.92f));
    g.strokePath(path, juce::PathStrokeType {2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded});
}

} // namespace opcoda
