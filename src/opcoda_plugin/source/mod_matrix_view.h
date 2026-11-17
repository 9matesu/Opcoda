#pragma once

#include "fonts.h"
#include "palette.h"

#include "opcoda_core/dsp/mod_matrix.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <vector>

namespace opcoda {

class ModMatrixView final : public juce::Component,
                            private juce::Timer,
                            private juce::AudioProcessorValueTreeState::Listener {
public:
    ModMatrixView();
    ~ModMatrixView() override;

    void attach(juce::AudioProcessorValueTreeState& state);
    void setSelectedDest(dsp::ModDest dest);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    struct SourceColumn {
        dsp::ModSource source;
        const char* label;
    };

    static constexpr std::array<SourceColumn, 5> kColumns {{
        {dsp::ModSource::kVelocity, "Amp"},
        {dsp::ModSource::kEnvelope, "Env"},
        {dsp::ModSource::kLfo, "LFO"},
        {dsp::ModSource::kKeytrack, "Key"},
        {dsp::ModSource::kModWheel, "Wheel"},
    }};

    [[nodiscard]] static const char* destName(dsp::ModDest dest) noexcept;
    [[nodiscard]] static int destIndex(dsp::ModDest dest) noexcept;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    void refreshRoutes();
    void rebuildRows();
    [[nodiscard]] int findRoute(dsp::ModSource source, dsp::ModDest dest) const noexcept;
    [[nodiscard]] int findEmptyRoute() const noexcept;
    void writeAmount(int route, float amount) const;
    void clearRoute(int route) const;
    void writeRoute(int route, dsp::ModSource source, dsp::ModDest dest) const;

    [[nodiscard]] bool cellAt(const juce::Point<int>& position, int& row, int& column) const;
    [[nodiscard]] juce::Rectangle<int> rowBoundsFor(int row) const;
    [[nodiscard]] juce::Rectangle<int> columnBoundsFor(int column) const;
    [[nodiscard]] juce::Rectangle<int> gridBounds() const;

    juce::AudioProcessorValueTreeState* state_ {nullptr};
    std::array<juce::RangedAudioParameter*, dsp::kMaxModRoutes> srcParams_ {};
    std::array<juce::RangedAudioParameter*, dsp::kMaxModRoutes> dstParams_ {};
    std::array<juce::RangedAudioParameter*, dsp::kMaxModRoutes> amtParams_ {};
    juce::StringArray listenerIds_;

    std::array<dsp::ModRoute, dsp::kMaxModRoutes> routes_ {};
    std::vector<dsp::ModDest> rows_;
    std::vector<juce::Rectangle<int>> rowBounds_;

    dsp::ModDest selected_ {dsp::ModDest::kNone};
    bool attached_ {false};

    int dragRow_ {-1};
    int dragColumn_ {-1};
    int dragRoute_ {-1};
    float dragStartAmount_ {0.0f};
    juce::Point<int> dragStart_ {};
    bool dragging_ {false};

    int hoverRow_ {-1};
    int hoverColumn_ {-1};

    // o brilho da celula quando um amount muda: resposta ao gesto, nao decoracao.
    float flash_ {0.0f};
    int flashRoute_ {-1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrixView)
};

} // namespace opcoda