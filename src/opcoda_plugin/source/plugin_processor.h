#pragma once

#include "opcoda_core/dsp/granular_engine.h"
#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/rt/note_tracker.h"
#include "opcoda_core/rt/spsc_ring.h"
#include "opcoda_core/rt/transport.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace opcoda {

using SampleBuffer = std::shared_ptr<const std::vector<float>>;
struct PublishedMaterial {
    const std::vector<float>* samples {nullptr};
    const float* entropyCurve {nullptr};
};

using SamplePtr = const std::vector<float>*;

class PluginProcessor : public juce::AudioProcessor {
public:
    PluginProcessor();

    const juce::String getName() const override { return JucePlugin_Name; }
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override {
        return static_cast<double>(dsp::GranularEngine::kMaxGrainMs) / 1000.0;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

void getStateInformation(juce::MemoryBlock& destData) override;
void setStateInformation(const void* data, int sizeInBytes) override;

void applyFactoryPreset(int index);

[[nodiscard]] std::unique_ptr<juce::XmlElement> stateXml();
bool loadStateXml(const juce::XmlElement& xml);
bool savePresetToFile(const juce::File& file);
bool loadPresetFromFile(const juce::File& file);

    bool ingest(const juce::String& path);

    [[nodiscard]] juce::String lastError() const noexcept { return lastError_; }
    [[nodiscard]] juce::String sourceName() const noexcept { return sourceName_; }

    [[nodiscard]] const juce::String& sourcePath() const noexcept { return sourcePath_; }

    struct SourceInfo {
        juce::String name;
        juce::String formatTag;  // "[64-bit PE]", igual ao mock
        std::int64_t sizeBytes {0};

        struct SectionInfo {
            juce::String name;
            std::uint32_t rawOffset {0};
            std::uint32_t rawSize {0};
            double entropy {0.0};
        };

std::vector<SectionInfo> sections;
    };

    using ByteRange = pe::ByteRange;

    // regiao corrente. Comeca na primeira secao com dados e o seletor move-a.
    [[nodiscard]] const ByteRange& byteRange() const noexcept { return byteRange_; }

    bool setByteRange(std::uint64_t start, std::uint64_t end, bool rememberExactRange = true);

bool snapByteRangeToSection();

// true quando a regiao esta' alinhada numa secao com dados brutos.
[[nodiscard]] bool byteRangeIsSectionAligned() const noexcept;

    struct Telemetry {
        std::atomic<float> peakDb {-100.0f};
        std::atomic<float> peakLeftDb {-100.0f};
        std::atomic<float> peakRightDb {-100.0f};
        std::atomic<int> activeVoices {0};
        std::atomic<bool> sounding {false};
        std::atomic<float> sampleRate {48000.0f};

        std::atomic<float> playheadFraction {0.0f};
        std::atomic<double> playDurationSeconds {0.0};
        std::atomic<bool> playing {false};
        std::atomic<bool> held {false};

        std::atomic<float> envelopeLevel {0.0f};
        std::atomic<float> envelopeStage {0.0f};
        std::atomic<int> envelopePhase {0};
        std::atomic<float> cutoff1Hz {0.0f};
        std::atomic<float> cutoff2Hz {0.0f};

        std::atomic<float> lfoPhase {0.0f};
        std::atomic<float> lfoValue {0.0f};
    };

    [[nodiscard]] const Telemetry& telemetry() const noexcept { return telemetry_; }

    struct PressedNotes {
        std::uint64_t low {0};
        std::uint64_t high {0};
    };

    [[nodiscard]] PressedNotes pressedNotes() const noexcept {
        return {pressedLow_.load(std::memory_order_relaxed),
                pressedHigh_.load(std::memory_order_relaxed)};
    }

    // Deriva de eventos MIDI: a interface usa para acender o LED do header.
    [[nodiscard]] std::uint32_t midiActivity() const noexcept {
        return midiActivity_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] const dsp::GrainTelemetry& grainTelemetry() const noexcept {
        return engine_.telemetry();
    }

    [[nodiscard]] bool isTransportPlaying() const noexcept { return transport_.isPlaying(); }

    [[nodiscard]] bool hasPlayableRegion() const noexcept { return transport_.hasRegion(); }

    void startTransport() noexcept { transport_.play(); }
    void stopTransport() noexcept { transport_.stop(); }
    void toggleTransport() noexcept {
        if (transport_.isPlaying()) {
            transport_.stop();
        } else {
            transport_.play();
        }
    }

    void setTransportHold(bool hold) noexcept {
        transportHold_.store(hold, std::memory_order_relaxed);
    }

    [[nodiscard]] bool isTransportHeld() const noexcept {
        return transportHold_.load(std::memory_order_relaxed);
    }

    void pushKeyboardNote(int noteNumber, float velocity, bool on) noexcept;

    void seekTransportTo(float fraction) noexcept { transport_.seekToFraction(fraction); }

    [[nodiscard]] float transportPosition() const noexcept {
        return transport_.positionFraction();
    }

    [[nodiscard]] double transportDurationSeconds() const noexcept {
        return transport_.durationSeconds();
    }

    [[nodiscard]] bool hostTransportIsKnownToBeStopped() const noexcept {
        const auto* head = getPlayHead();
        if (head == nullptr) {
            // sem host nao ha quem mande: false evita o botao travado num teste.
            return false;
        }

        const auto position = head->getPosition();
        if (!position.hasValue()) {
            return false; // host sem tempo: desconhecido, nao parado
        }
        return !position->getIsPlaying();
    }

    [[nodiscard]] const SourceInfo& sourceInfo() const noexcept { return sourceInfo_; }

    [[nodiscard]] const std::vector<std::uint8_t>& sourceBytes() const noexcept {
        return sourceBytes_;
    }

    void applyPendingControllerChanges();

    void drainKeyboardNotes() noexcept;

    [[nodiscard]] dsp::ModMatrix readModMatrix() const noexcept;

    void migrateLegacyModMatrix();

    void updateHostTransport() noexcept;
    [[nodiscard]] bool hasSource() const noexcept {
        return live_.load(std::memory_order_acquire) != nullptr;
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    [[nodiscard]] dsp::GranularParams currentParams() const noexcept;

    [[nodiscard]] juce::AudioProcessorValueTreeState& parameters() noexcept { return parameters_; }
    [[nodiscard]] const juce::AudioProcessorValueTreeState& parameters() const noexcept { return parameters_; }

    void releaseReturnedBuffers();

private:
    void drainIncomingQueue() noexcept;

    struct RouteRaw {
        const std::atomic<float>* source {nullptr};
        const std::atomic<float>* destination {nullptr};
        const std::atomic<float>* amount {nullptr};
    };
    std::array<RouteRaw, dsp::kMaxModRoutes> routeRaw_ {};

    void readNotes(const juce::MidiBuffer& midi) noexcept;

    void publishPressedNotes() noexcept;

    void publishTelemetry(const juce::AudioBuffer<float>& buffer) noexcept;

    std::vector<std::uint8_t> sourceBytes_;

    bool publishByteRange();

    opcoda::dsp::GranularEngine engine_;

    rt::SpscRing<PublishedMaterial, 4> incoming_;
    rt::SpscRing<PublishedMaterial, 4> returned_;

    rt::SpscRing<rt::NoteTracker::Event, 256> keyboardNotes_;

    std::atomic<double> hostBpm_ {120.0};
    std::atomic<double> hostPpq_ {-1.0};

    std::vector<SampleBuffer> owned_;

    std::vector<std::vector<float>> entropyCurves_;

    std::atomic<SamplePtr> live_ {nullptr};
    juce::String lastError_;
    juce::String sourceName_;
    SourceInfo sourceInfo_;
    ByteRange byteRange_;

    ByteRange lastExactRange_ {};

    juce::String sourcePath_;

    rt::NoteTracker notes_;

    rt::Transport transport_;

    // latch do transporte. escrito pela interface, lido no callback.
    std::atomic<bool> transportHold_ {false};

    // teclas premidas, escrito pelo audio e lido pela interface.
    std::atomic<std::uint64_t> pressedLow_ {0};
    std::atomic<std::uint64_t> pressedHigh_ {0};

    // contador de eventos MIDI: sobe a cada mensagem, a interface compara.
    std::atomic<std::uint32_t> midiActivity_ {0};

    Telemetry telemetry_;

    // Mapa de controladores. O MiniLab 3 manda CC74 no knob 1 e CC71 no knob 2,
    // e as pads vem no canal 10: a leitura e' omnichannel, sem filtro de canal.
    static constexpr int kCcSustain = 64;   // pedal
    static constexpr int kCcModWheel = 1;   // roda de modulacao, fonte da matriz
    static constexpr int kCcDensity = 74;   // Brightness
    static constexpr int kCcPosition = 71;  // Resonance
    static constexpr int kCcAllSoundOff = 120;
    static constexpr int kCcAllNotesOff = 123;

    enum ControllerSlot : int { kDensityController = 0, kPositionController = 1,
                                kModWheelController = 2, kControllerSlots = 3 };

    static constexpr const char* kModMatrixNode = "MODMATRIX";

    juce::AudioProcessorValueTreeState parameters_;
};

} // namespace opcoda
