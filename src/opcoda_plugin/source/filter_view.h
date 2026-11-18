#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>

namespace opcoda {

class FilterView final : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener {
public:
    explicit FilterView(juce::AudioProcessorValueTreeState& state);
    ~FilterView() override;

    void paint(juce::Graphics&) override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // corte ja' modulado, publicado pelo audio. zero = ainda nao ha' leitura.
    void setLive(float cutoff1, float cutoff2);

    // espectro do material (dB, 20 Hz .. 20 kHz, log). O ponteiro e' do
    // telemetry do processador e vive enquanto o plugin viver.
    void setSpectrum(const std::atomic<float>* bins, int count) noexcept {
        spectrum_ = bins;
        spectrumCount_ = count;
        repaint();
    }

private:
    void parameterChanged(const juce::String&, float) override { repaint(); }

    enum class Handle : std::uint8_t { none, first, second };

    struct Response {
        juce::Rectangle<float> plot;
        float cutoff1 {1000.0f};
        float cutoff2 {20000.0f};
        float q1 {0.7f};
        float q2 {0.7f};
        int type1 {0};
        int type2 {0};
        bool slope24 {false};
        double sampleRate {44100.0};
    };

    [[nodiscard]] Response response() const;
    [[nodiscard]] Handle handleAt(juce::Point<float> point) const;
    [[nodiscard]] float xForHz(float hz, const juce::Rectangle<float>& plot) const;
    [[nodiscard]] float hzForX(float x, const juce::Rectangle<float>& plot) const;
    [[nodiscard]] float yForDb(float db, const juce::Rectangle<float>& plot) const;
    [[nodiscard]] static float responseDb(const Response& geometry, float hz, float cutoff1,
                                          float cutoff2);
    [[nodiscard]] float valueOf(const char* id) const;
    void setValue(const char* id, float value);
    void beginGesture(Handle handle);
    void endGesture();

    juce::AudioProcessorValueTreeState& state_;

    Handle active_ {Handle::none};
    Handle hover_ {Handle::none};
    bool gestureOpen_ {false};
    const char* gestureA_ {nullptr};
    const char* gestureB_ {nullptr};

    float startCutoff_ {0.0f};
    float startQ_ {0.0f};

    float liveCutoff1_ {0.0f};
    float liveCutoff2_ {0.0f};

    const std::atomic<float>* spectrum_ {nullptr};
    int spectrumCount_ {0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterView)
};

} // namespace opcoda
