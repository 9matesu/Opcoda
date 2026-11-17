#pragma once

#include "fonts.h"
#include "palette.h"

#include "plugin_processor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace opcoda {

class HexGrid : public juce::Component {
public:
    HexGrid();

    std::function<void(std::uint64_t address)> onCellActivated;

    std::function<void(std::uint64_t start, std::uint64_t end)> onRegionSelected;

    // duplo clique: alterna entre a regiao exacta e a secao PE mais proxima.
    std::function<void()> onSnapRequested;

    std::function<void()> onUserScrolled;

    void setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize);

    void setDisassemblyEnabled(bool enabled);

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    void setRegion(std::uint64_t start, std::uint64_t end);

    // cabeca de leitura. nao move a janela: quem move a janela e' a pessoa.
    void setReadHead(std::uint64_t address);

    // rolagem publica: teclado, botoes de navegacao e roda.
    void scrollToAddress(std::uint64_t address);
    void scrollByLines(int lines);
    void scrollToFraction(float fraction);

    [[nodiscard]] juce::Rectangle<float> cellRectForAddress(std::uint64_t address) const;

    [[nodiscard]] bool isAddressVisible(std::uint64_t address) const noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

private:
    struct Metrics {
        float cellWidth {0.0f};
        float cellAdvance {0.0f};
        float gutterWidth {0.0f};
        float asciiWidth {0.0f};
        float asciiCharWidth {0.0f};
        float asmWidth {0.0f};
        int gutterDigits {8};
        int rows {1};
        bool showAscii {true};
        bool showAsm {false};
        bool showRail {false};
        juce::Rectangle<float> bytes;
        juce::Rectangle<float> gutter;
        juce::Rectangle<float> ascii;
        juce::Rectangle<float> asmColumn;
        juce::Rectangle<float> rail;
    };

    struct AsmItem {
        std::uint64_t address {0};
        std::uint32_t length {0};
        juce::String text;
        std::uint64_t target {0};
        bool hasTarget {false};
    };

    void rebuildMetrics();
    void rebuildAsmIfNeeded();
    void clampViewport();
    [[nodiscard]] std::uint64_t rowAddress(int row) const;
    [[nodiscard]] std::uint64_t highestTop() const noexcept;
    [[nodiscard]] juce::Rectangle<float> thumbRect() const noexcept;
    void paintScrollRail(juce::Graphics& g);

    [[nodiscard]] juce::Rectangle<float> cellRect(std::uint64_t column, int row) const;

    [[nodiscard]] juce::String formatAddress(std::uint64_t address) const;

    // endereco sob um ponto, ou uint64 max quando o ponto nao e' celula.
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;

    void paintColumnHeader(juce::Graphics& g);
    void paintReadingColumns(juce::Graphics& g);
    void paintRow(juce::Graphics& g, int row);
    void paintGutter(juce::Graphics& g, int row);
    void paintBytes(juce::Graphics& g, int row);
    void paintAscii(juce::Graphics& g, int row);
    void paintAsm(juce::Graphics& g, int row);
    // uma instrucao com cores por classe de simbolo, na monoespacada.
    void paintAsmText(juce::Graphics& g, const juce::String& text,
                      juce::Rectangle<float> area) const;

    [[nodiscard]] const PluginProcessor::SourceInfo::SectionInfo* sectionAt(
        std::uint64_t address) const;

    static constexpr std::uint64_t kBytesPerRow {16};
    static constexpr float kRowHeight {26.0f};
    static constexpr float kHeaderHeight {16.0f};
    static constexpr float kPadding {8.0f};
    static constexpr float kInnerGap {14.0f};
    static constexpr float kRailWidth {10.0f};
    static constexpr float kRailGap {10.0f};

    const std::vector<std::uint8_t>* bytes_ {nullptr};
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t regionStart_ {0};
    std::uint64_t regionEnd_ {0};
    std::uint64_t readHead_ {0};

    bool asmEnabled_ {false};
    std::vector<AsmItem> asmItems_;
    std::uint64_t asmTopByte_ {0};
    int asmRows_ {0};

    std::uint64_t topByte_ {0};

    bool draggingRail_ {false};

    // seleccao de intervalo: a ancora fica no byte onde o arrasto comecou.
    std::uint64_t anchor_ {std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t selStart_ {0};
    std::uint64_t selEnd_ {0};
    bool selecting_ {false};

    juce::Font font_ {fonts().sans(12.0f)};
    Metrics metrics_;
    double wheelAccumulator_ {0.0};
    double wheelBoost_ {1.0};
    std::uint32_t lastWheelMs_ {0};
};

} // namespace opcoda
