#pragma once

namespace opcoda::dsp {

class DcBlocker {
public:
    // R = 0,9983 corta em ~12 Hz a 44,1 kHz, dentro da faixa de 10 a 15 Hz que a
    // fundamentacao exige. R = 0,995, citado em 05-fundamentacao.md, cortaria em
    // 35 Hz; o ensaio T1 mede a faixa, nao o coeficiente.
    static constexpr float kDefaultR = 0.9983f;

    void setSampleRate(double sampleRate) noexcept;
    void prepare() noexcept { reset(); }
    void reset() noexcept;

    [[nodiscard]] float process(float sample) noexcept {
        const float y = sample - x1_ + r_ * y1_;
        x1_ = sample;
        y1_ = y;
        return y;
    }

    void processBlock(float* data, int numSamples) noexcept;

    [[nodiscard]] double cutoffHz() const noexcept;
    [[nodiscard]] double dcGainDb() const noexcept;
    [[nodiscard]] double gainDbAt(double frequencyHz) const noexcept;

private:
    float r_ {kDefaultR};
    float x1_ {0.0f};
    float y1_ {0.0f};
    double sampleRate_ {44100.0};
};

} // namespace opcoda::dsp
