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
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

namespace opcoda {

// letras e a linha dos numeros nao tem codigo nomeado no JUCE: chegam com keyCode
// zero e o caracter em getTextCharacter(). KeyPress::aKey nao existe.
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

// o display do material, em tres leituras: forma de onda, hex e entropia.
//
// **a forma de onda e' o protagonista.** o desenho passou a ter corpo: envelope
// preenchido com gradiente, nucleo de rms, silhueta colorida por entropia e um
// banho de profundidade. sem grelha por baixo, o material le-se como audio e nao
// como um grafico.
//
// a vista HEX e' o HexGrid, que vive como filho e ocupa o ecra todo: endereco,
// bytes, ASCII e a coluna ASM. a faixa de entropia que morava por baixo saiu --
// o mapa do ficheiro inteiro ja' vive no overview, e a faixa partia o ecra em
// duas metades que competiam pela mesma largura.
//
// a vista ENTROPY leva colunas proprias com um minimo de 256 bytes cada: a
// entropia de Shannon de uma coluna menor nunca chega ao topo do eixo, e no
// zoom fundo a curva desabava toda para zero.
class ByteDisplay : public juce::Component {
public:
    enum class ViewMode {
        waveform,  // tecla 1
        hex,       // tecla 2
        entropy,   // tecla 3
        matrix,    // tecla 4
    };

    // o que os knobs estao a pedir, desenhado sobre a onda. nao e' uma segunda
    // fonte da verdade: sao os valores lidos do estado, a 60 Hz, para o material
    // mostrar spray, densidade, tamanho do grao e posicao antes de se ouvir.
    struct ParamPreview {
        float position {0.5f};
        float spray {0.0f};
        float density {20.0f};
        float grainMs {40.0f};
        float pitchSpread {0.0f};
        float pan {0.0f};
        float sampleRate {48000.0f};
        // a janela e' a forma do grao; sem ela o preview mostrava onde os graos
        // nascem mas nao como soam. pitch e nivel moldam a mesma silhueta.
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
        repaint();
    }

    void setRegion(std::uint64_t start, std::uint64_t end);
    void setReadHead(std::uint64_t address);

    void setPlayhead(float fraction, bool visible) {
        playheadVisible_ = visible;

        if (!playheadAnimated_ || !visible) {
            playheadFraction_.jumpTo(fraction);
        } else {
            playheadFraction_.set(fraction);
        }
        playheadAnimated_ = visible;

        repaint();
    }

    void setGrains(const std::array<dsp::GrainView, dsp::GrainTelemetry::kMaxVoices>& views,
                   int count);

    void setParamPreview(const ParamPreview& preview);

    void setMode(ViewMode mode);
    [[nodiscard]] ViewMode mode() const noexcept { return mode_; }

    // a coluna ASM do hex so' faz sentido em material de 64 bits; o editor liga
    // a chave quando o formato do material o diz.
    void setDisassemblyEnabled(bool enabled) { hex_.setDisassemblyEnabled(enabled); }

    // liga as 24 celulas da matriz aos parametros do APVTS. uma vez, no
    // construtor do editor, depois de a arvore existir.
    void attachModParameters(juce::AudioProcessorValueTreeState& state) {
        matrix_.attach(state);
    }

    // marcadores de sessao: alt+click alterna no endereco, click perto da
    // bandeira salta. nao sao estado do projeto; morrem com o material novo.
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

    // colunas da vista ENTROPY: com o minimo de bytes por coluna, o eixo de
    // 8 bits continua honesto em qualquer zoom.
    void rebuildEntropyColumnsIfNeeded();

    void applyModeVisibility();

    void paintWaveform(juce::Graphics& g);
    void paintEntropy(juce::Graphics& g);

    void paintEntropyLine(juce::Graphics& g,
                          const std::vector<pe::Column>& columns,
                          const std::function<float(std::size_t)>& xAt,
                          const std::function<float(std::size_t)>& yAt);

    [[nodiscard]] static int entropyBucket(float bits) noexcept;

    // reamostra o envelope a' resolucao da tela: um maximo e um minimo por
    // pixel. com uma coluna por byte em zoom-out o path fazia zigzag sub-pixel
    // e o preenchimento abria riscos verticais.
    void resampleEnvelope(int pixels);

    // bandeiras dos marcadores: triangulo no topo e fio ate' a base, nas vistas
    // de onda e de entropia, e no overview.
    void paintBookmarks(juce::Graphics& g, const juce::Rectangle<float>& area);
    [[nodiscard]] std::uint64_t bookmarkAt(
        float x, const juce::Rectangle<float>& area) const noexcept;

    // o preview dos parametros: banda de spray, barrinhas de grao com a largura
    // do tamanho e o espacamento da densidade, e a posicao. so' na forma de onda.
    void paintParamPreview(juce::Graphics& g, const juce::Rectangle<float>& area);

    void layoutHex();
    void paintSectionTicks(juce::Graphics& g);
    void paintGrains(juce::Graphics& g, const juce::Rectangle<float>& area);
    void paintGrainsInHex(juce::Graphics& g);

    void paintReadHead(juce::Graphics& g, const juce::Rectangle<float>& area);
    void paintPlayhead(juce::Graphics& g, const juce::Rectangle<float>& area);

    static constexpr float kPlayheadHalfLife {0.030f};
    static constexpr float kReadHeadHalfLife {0.060f};
    static constexpr float kModeHalfLife {0.090f};

    static constexpr float kGrainCoreWidth {2.0f};
    static constexpr float kGrainGlowWidth {7.0f};

    void paintGrain(juce::Graphics& g,
                    const juce::Rectangle<float>& area,
                    const view::GrainTrailSet::Trail& trail,
                    float gain);

    Eased readHeadFraction_;
    Eased playheadFraction_;
    Eased modeFade_;

    view::GrainTrailSet grainTrails_ {};

    bool readHeadAnimated_ {false};
    bool playheadAnimated_ {false};

    [[nodiscard]] std::uint64_t highestUsableAddress() const noexcept;
    [[nodiscard]] float fractionOf(std::uint64_t address) const noexcept;
    [[nodiscard]] float regionFractionAt(float x, float areaX, float areaWidth) const noexcept;
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;
    [[nodiscard]] double fractionFor(std::uint64_t address) const noexcept;
    [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> viewByteRange() const noexcept;

    static constexpr float kPadding {10.0f};
    static constexpr float kAxisHeight {12.0f};
    static constexpr float kAxisInset {22.0f};
    static constexpr float kRulerHeight {16.0f};
    static constexpr float kOverviewHeight {30.0f};
    static constexpr std::uint64_t kMinViewBytes {64};
    // a entropia de uma coluna de menos de 256 bytes nunca chega a 8 bits: com
    // menos que isto o eixo da vista mente e o zoom desinfla a curva.
    static constexpr std::uint64_t kEntropyColumnBytes {256};

    HexGrid hex_;
    ModMatrixView matrix_;

    // marcadores: enderecos absolutos, ordenados. sessao, nao projeto.
    std::vector<std::uint64_t> bookmarks_;

    // envelope reamostrado por pixel, reaproveitado entre repaints.
    std::vector<float> pixelMax_;
    std::vector<float> pixelMin_;
    std::vector<float> hullMax_;
    std::vector<float> hullMin_;

    const std::vector<std::uint8_t>* bytes_ {nullptr};
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t regionStart_ {0};
    std::uint64_t regionEnd_ {0};
    std::uint64_t readHead_ {0};
    std::uint64_t caret_ {0};
    bool caretMoved_ {false};

    std::uint64_t pendingScrub_ {std::numeric_limits<std::uint64_t>::max()};
    float playheadVisible_ {false};

    ViewMode mode_ {ViewMode::waveform};

    // velocidade da roda nas vistas: consecutiva multiplica, pausa devolve. o
    // multiplicador e' o mesmo para zoom e deslocamento, para a mao nao ter de
    // reaprender o gesto entre vistas.
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

    // o primeiro material carregado centraliza o hex na cabeca uma vez. depois
    // disso o topo e' do utilizador (era o follow por grao que fazia o ecra saltar).
    bool hexHeadScrolled_ {false};

    float viewStart_ {0.0f};
    float viewLength_ {1.0f};

    bool panning_ {false};
    float lastPanX_ {0.0f};
    bool overviewDrag_ {false};

    [[nodiscard]] juce::Rectangle<float> overviewBounds() const noexcept;
    [[nodiscard]] juce::Rectangle<float> plotBounds() const noexcept;
    [[nodiscard]] juce::Rectangle<float> entropyPlotBounds() const noexcept;

    std::vector<pe::Column> overviewColumns_ {};
    std::uint64_t overviewRegionStart_ {0};
    std::uint64_t overviewRegionEnd_ {0};
    bool overviewValid_ {false};
    void rebuildOverviewIfNeeded();

    // cache da vista ENTROPY: refeita quando a regiao, a largura ou o zoom
    // mudam. independente do cache da onda, que quer resolucao maxima.
    std::vector<pe::Column> entropyColumns_ {};
    std::uint64_t entropyRegionStart_ {0};
    std::uint64_t entropyRegionEnd_ {0};
    std::uint32_t entropyColumnCount_ {0};
    float entropyViewStart_ {-1.0f};
    float entropyViewLength_ {-1.0f};
    bool entropyCacheValid_ {false};

    void paintRuler(juce::Graphics& g, const juce::Rectangle<float>& plotArea);
    void paintOverview(juce::Graphics& g);
    void paintSourceTag(juce::Graphics& g, const juce::Rectangle<float>& plotArea);

    void paintSampleDots(juce::Graphics& g,
                         const juce::Rectangle<float>& plotArea,
                         std::uint64_t viewStartByte,
                         std::uint64_t viewEndByte);
    [[nodiscard]] bool sampleDotsVisible() const noexcept;

    std::vector<pe::Column> columns_;
    std::uint64_t cachedRegionStart_ {0};
    std::uint64_t cachedRegionEnd_ {0};
    std::uint32_t cachedColumnCount_ {0};
    float cachedViewStart_ {-1.0f};
    float cachedViewLength_ {-1.0f};
    bool cacheValid_ {false};

    ParamPreview preview_ {};
    bool previewValid_ {false};

    juce::Font font_ {fonts().sans(10.0f)};
};

} // namespace opcoda
