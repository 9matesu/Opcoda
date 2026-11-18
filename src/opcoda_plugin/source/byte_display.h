#pragma once

#include "animator.h"
#include "fonts.h"
#include "hex_grid.h"
#include "mod_matrix_view.h"
#include "palette.h"
#include "plugin_processor.h"

#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/column_reduction.h"
#include "opcoda_core/view/grain_trail.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

namespace opcoda {

[[nodiscard]] inline bool isTypedCharacter(const juce::KeyPress& key, char character) noexcept {
    if (key.getKeyCode() != 0) {
        return false;
    }
    const auto fold = [](int c) { return (c >= 'a' && c <= 'z') ? (c & 0xDF) : c; };
    return fold(key.getTextCharacter()) == fold(static_cast<int>(character));
}

// digitos vem por duas frentes: keyDown com keyCode, ou char com keyCode zero.
[[nodiscard]] inline bool isDigitKey(const juce::KeyPress& key, char digit) noexcept {
    if (digit < '0' || digit > '9') {
        return false;
    }
    const auto want = static_cast<juce::juce_wchar>(digit);
    if (key.getKeyCode() == 0) {
        return key.getTextCharacter() == want;
    }
    return static_cast<juce::juce_wchar>(key.getKeyCode()) == want;
}

class ByteDisplay : public juce::Component {
public:
    enum class ViewMode {
        waveform,  // tecla 1
        hex,       // tecla 2
        matrix,    // tecla 3
    };

    struct ParamPreview {
        float position {0.5f};
        float spray {0.0f};
        float density {20.0f};
        float grainMs {40.0f};
        float pitchSpread {0.0f};
        float pan {0.0f};
        float sampleRate {48000.0f};
        int window {0};
        float pitchSemitones {0.0f};
        float level {1.0f};

        [[nodiscard]] bool operator==(const ParamPreview& other) const noexcept {
            return position == other.position && spray == other.spray &&
                   density == other.density && grainMs == other.grainMs &&
                   pitchSpread == other.pitchSpread && pan == other.pan &&
                   sampleRate == other.sampleRate && window == other.window &&
                   pitchSemitones == other.pitchSemitones && level == other.level;
        }
    };

    ByteDisplay();

    void setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize);
    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        rebuildSectionCombo();
        repaint();
    }
    void setRegion(std::uint64_t start, std::uint64_t end);
    void setReadHead(std::uint64_t address);

    // eixo Y da mira: 0 = fundo, 1 = topo. Vem do parametro ligado (grain size).
    void setCrosshairY(float fraction) {
        const auto clamped = juce::jlimit(0.0f, 1.0f, fraction);
        if (std::abs(clamped - crosshairY_) > 0.002f) {
            crosshairY_ = clamped;
            crosshairYE_.set(clamped);
            repaint();
        }
    }

    // arrasto 2D no display: x = posicao, y = parametro vertical.
    std::function<void(float xFraction, float yFraction)> onCrosshairMoved;

    void setPlayhead(float fraction, bool visible) {
        const bool already = playheadVisible_ == visible &&
                             std::abs(playheadFraction_.value() - fraction) < 1.0e-6f &&
                             std::abs(playheadFraction_.target() - fraction) < 1.0e-6f;
        if (!visible) {
            playheadFraction_.jumpTo(fraction);
        } else if (!playheadAnimated_) {
            playheadFraction_.jumpTo(fraction);
        } else {
            playheadFraction_.set(fraction);
        }
        playheadVisible_ = visible;
        const bool wasAnimated = playheadAnimated_;
        playheadAnimated_ = visible;

        if (already && wasAnimated == visible) {
            return;
        }
        repaint();
    }

    void setParamPreview(const ParamPreview& preview) {
        if (previewValid_ && preview_ == preview) {
            return;
        }
        if (!previewValid_) {
            sprayE_.jumpTo(preview.spray);
            densityE_.jumpTo(preview.density);
            sizeE_.jumpTo(preview.grainMs);
            pitchSpreadE_.jumpTo(preview.pitchSpread);
            levelE_.jumpTo(preview.level);
            pitchE_.jumpTo(preview.pitchSemitones);
            panE_.jumpTo(preview.pan);
            previewValid_ = true;
        }
        preview_ = preview;
        sprayE_.set(preview.spray);
        densityE_.set(preview.density);
        sizeE_.set(preview.grainMs);
        pitchSpreadE_.set(preview.pitchSpread);
        levelE_.set(preview.level);
        pitchE_.set(preview.pitchSemitones);
        panE_.set(preview.pan);
        repaint();
    }

    // nivel do envelope publicado pela thread de audio: a onda respira com ele.
    void setLiveLevel(float level) {
        if (std::abs(level - liveLevel_) > 0.004f) {
            liveLevel_ = level;
            repaint();
        }
    }

    void setGrains(const std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices>& views,
                   int count);

    void setMode(ViewMode mode);
    [[nodiscard]] ViewMode mode() const noexcept { return mode_; }

    void setDisassemblyEnabled(bool enabled) { hex_.setDisassemblyEnabled(enabled); }

    void attachModParameters(juce::AudioProcessorValueTreeState& state) {
        matrix_.attach(state);
    }

    // a linha temporaria da matriz: o parametro que o focu do editor revela.
    void setSelectedModDest(dsp::ModDest dest) { matrix_.setSelectedDest(dest); }

    void toggleBookmarkAt(std::uint64_t address);

    void zoomIn();
    void zoomOut();
    void fitView();
    void zoomAround(float factor, float centreFraction);
    [[nodiscard]] float viewLength() const noexcept { return viewLength_; }

    // navegacao do hex, para os botoes da tira horizontal e para as teclas.
    void scrollHexByLines(int lines);
    void scrollHexPage(int pages);
    void scrollHexToStart();
    void scrollHexToEnd();

    void tickAnimation(float deltaSeconds);

    std::function<void(std::uint64_t address)> onAddressActivated;
    // clique na regua de enderecos: leva a cabeca de leitura e o transporte ao
    // byte escolhido, cada um na sua unidade.
    std::function<void(std::uint64_t address)> onAddressPinned;
    std::function<void(float regionFraction)> onTransportSeek;
    // clique no ecra vazio: abre o selector de ficheiro.
    std::function<void()> onEmptyClick;
    // intervalo seleccionado na grelha de bytes (end exclusivo).
    std::function<void(std::uint64_t start, std::uint64_t end)> onRegionSelected;
    std::function<void()> onSnapRequested;
    std::function<void()> onToggleTransport;
    std::function<void()> onSnapRequestedFromKey;

    void setCaret(std::uint64_t address);
    [[nodiscard]] std::uint64_t caret() const noexcept { return caret_; }

    void handleNavigationKey(const juce::KeyPress& key);
    bool handleTransportKey(const juce::KeyPress& key);

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(juce::Component::FocusChangeType cause) override;
    void focusLost(juce::Component::FocusChangeType cause) override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

private:
    void rebuildColumnsIfNeeded();
    [[nodiscard]] std::uint32_t columnCountForWidth() const noexcept;

    // a navegacao de seccoes da vista hex: um combo com as seccoes do PE.
    void rebuildSectionCombo();

    void applyModeVisibility();

    void paintWaveform(juce::Graphics& g);

    void paintBookmarks(juce::Graphics& g, const juce::Rectangle<float>& area);
    [[nodiscard]] std::uint64_t bookmarkAt(
        float x, const juce::Rectangle<float>& area) const noexcept;

    void layoutHex();
    void paintSectionTicks(juce::Graphics& g);
    void paintGrains(juce::Graphics& g, const juce::Rectangle<float>& area);
    void paintGrainsInHex(juce::Graphics& g);

    void paintParamPreview(juce::Graphics& g, const juce::Rectangle<float>& area);
    void paintParamHud(juce::Graphics& g);
    void dragTo(juce::Point<float> position);

    static constexpr float kPlayheadHalfLife {0.030f};
    static constexpr float kReadHeadHalfLife {0.060f};
    static constexpr float kModeHalfLife {0.090f};
    static constexpr float kPreviewHalfLife {0.035f};

    static constexpr float kGrainCoreWidth {2.0f};
    static constexpr float kGrainGlowWidth {7.0f};

    void paintGrain(juce::Graphics& g,
                    const juce::Rectangle<float>& area,
                    const view::GrainTrailSet::Trail& trail,
                    float gain);

    Eased readHeadFraction_;
    Eased playheadFraction_;
    Eased modeFade_;

    // mira 2D: y suave, segue o parametro ligado.
    float crosshairY_ {0.5f};
    Eased crosshairYE_;
    bool crosshairDrag_ {false};

    Eased sprayE_ {};
    Eased densityE_ {};
    Eased sizeE_ {};
    Eased pitchSpreadE_ {};
    Eased levelE_ {};
    Eased pitchE_ {};
    Eased panE_ {};

    view::GrainTrailSet grainTrails_ {};

    bool readHeadAnimated_ {false};
    bool playheadAnimated_ {false};

    [[nodiscard]] std::uint64_t highestUsableAddress() const noexcept;
    [[nodiscard]] float fractionOf(std::uint64_t address) const noexcept;
    [[nodiscard]] float regionFractionAt(float x, float areaX, float areaWidth) const noexcept;
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;
    [[nodiscard]] double fractionFor(std::uint64_t address) const noexcept;
    [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> viewByteRange() const noexcept;

    // 8 px: o mesmo passo da grelha do chassi. O conteúdo do ecrã começa aqui e
    // a régua, as barras e a agulha partilham esta caixa.
    static constexpr float kPadding {8.0f};
    static constexpr float kAxisHeight {12.0f};
    static constexpr float kAxisInset {22.0f};
    static constexpr float kRulerHeight {16.0f};
    static constexpr float kOverviewHeight {20.0f};
    static constexpr std::uint64_t kMinViewBytes {64};

    HexGrid hex_;
    ModMatrixView matrix_;
    juce::ComboBox sectionCombo_;

    // marcadores: enderecos absolutos, ordenados. sessao, nao projeto.
    std::vector<std::uint64_t> bookmarks_;

    const std::vector<std::uint8_t>* bytes_ {nullptr};
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t regionStart_ {0};
    std::uint64_t regionEnd_ {0};
    std::uint64_t readHead_ {0};
    std::uint64_t caret_ {0};
    bool caretMoved_ {false};

    std::uint64_t pendingScrub_ {std::numeric_limits<std::uint64_t>::max()};
    bool playheadVisible_ {false};

    ParamPreview preview_ {};
    bool previewValid_ {false};
    float liveLevel_ {0.0f};

    ViewMode mode_ {ViewMode::waveform};

    float wheelBoost_ {1.0f};
    std::uint32_t lastWheelMs_ {0};
    [[nodiscard]] float nextWheelBoost() noexcept {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastWheelMs_ > 320) {
            wheelBoost_ = 1.0f;
        }
        lastWheelMs_ = now;
        const auto boost = wheelBoost_;
        // 1.6 por entalhe ate' 8x: o zoom e a linha do hex aceleram de igual forma.
        wheelBoost_ = juce::jmin(wheelBoost_ * 1.6f, 8.0f);
        return boost;
    }

    bool hexHeadScrolled_ {false};

    float viewStart_ {0.0f};
    float viewLength_ {1.0f};

    bool panning_ {false};
    float lastPanX_ {0.0f};
    bool overviewDrag_ {false};

    [[nodiscard]] juce::Rectangle<float> overviewBounds() const noexcept;

    [[nodiscard]] bool overviewShown() const noexcept;
    [[nodiscard]] juce::Rectangle<float> plotBounds() const noexcept;

    std::vector<pe::Column> overviewColumns_ {};
    std::uint64_t overviewRegionStart_ {0};
    std::uint64_t overviewRegionEnd_ {0};
    bool overviewValid_ {false};
    void rebuildOverviewIfNeeded();

    void paintRuler(juce::Graphics& g, const juce::Rectangle<float>& plotArea);
    void paintOverview(juce::Graphics& g);

    std::vector<pe::Column> columns_;
    std::uint64_t cachedRegionStart_ {0};
    std::uint64_t cachedRegionEnd_ {0};
    std::uint32_t cachedColumnCount_ {0};
    float cachedViewStart_ {-1.0f};
    float cachedViewLength_ {-1.0f};
    bool cacheValid_ {false};

    juce::Font font_ {fonts().sans(10.0f)};
};

} // namespace opcoda