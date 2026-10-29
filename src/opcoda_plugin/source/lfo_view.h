#pragma once

#include "animator.h"
#include "fonts.h"
#include "palette.h"

#include "opcoda_core/dsp/lfo.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>

namespace opcoda {

// forma do LFO, desenhada a partir dos mesmos parametros que o audio usa.
//
// **a mesma gramatica do ENV e do FILTER.** o modulo MOD tinha sete knobs e
// nenhuma leitura: a forma que o LFO aplica so' existia na cabeca de quem
// desenhou o preset. aqui a forma e' o objeto, a profundidade e' a altura e o
// ponto vivo mostra a fase que o motor esta' a percorrer.
//
// **nao e' decorativo.** a curva sai de dsp::lfoWaveValue com a onda, a fase e
// a profundidade lidas do estado; o ponto vem da telemetria do audio, com a
// mesma meia-vida de interpolacao dos outros visualizadores.
class LfoView final : public juce::Component {
public:
    explicit LfoView(juce::AudioProcessorValueTreeState& state);

    void paint(juce::Graphics&) override;

    // fase em voltas (0..1) e valor bipolar do ultimo bloco.
    void setLive(float phase, float value);

private:
    [[nodiscard]] float valueOf(const char* id) const;
    [[nodiscard]] juce::Colour inkFor(dsp::LFOTarget target) const noexcept;

    juce::AudioProcessorValueTreeState& state_;

    // fase suavizada e valor: o audio publica por bloco.
    Eased phaseEased_ {};
    Eased valueEased_ {};
    double lastLiveMs_ {0.0};
    bool liveSeen_ {false};

    float livePhase_ {0.0f};
    float liveValue_ {0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoView)
};

} // namespace opcoda
