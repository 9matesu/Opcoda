#pragma once

#include "boxed_label.h"
#include "palette.h"

#include "plugin_processor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Abas de secao PE, proporcionais ao tamanho de cada secao.
//
// As abas sao um mapa, e nao controlos. A selecao vive no ByteSelector, que e'
// operavel por rato e por teclado; duplicar o comando nas abas criaria dois
// estados para a mesma regiao e dois caminhos para o mesmo som. O mapa apenas
// mostra onde a regiao ativa caiu.
class SectionTabs : public juce::Component {
public:
    SectionTabs() {
        setOpaque(false);
        // Sem interacao propria: o seletor de bytes e' o comando, e este mapa
        // nao precisa de ser clicavel para dizer onde se esta.
        setInterceptsMouseClicks(false, false);
    }

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    // Regiao ativa do motor, em bytes do ficheiro. As secoes intersetadas pela
    // regiao ganham uma marca alem da cor; sem isto, uma aba "ativa" seria so
    // fundo laranja, que o criterio 1.1 do WCAG proibe como unico sinal.
    void setSoundingRange(std::uint64_t start, std::uint64_t end) {
        if (start == rangeStart_ && end == rangeEnd_) {
            return;
        }
        rangeStart_ = start;
        rangeEnd_ = end;
        repaint();
    }

    void paint(juce::Graphics& g) override;

private:
    [[nodiscard]] bool overlapsSoundingRange(
        const PluginProcessor::SourceInfo::SectionInfo& section) const noexcept {
        if (section.rawSize == 0 || rangeEnd_ <= rangeStart_) {
            return false;
        }
        const auto start = static_cast<std::uint64_t>(section.rawOffset);
        const auto end = start + static_cast<std::uint64_t>(section.rawSize);
        return rangeStart_ < end && rangeEnd_ > start;
    }

    [[nodiscard]] float widthFor(const PluginProcessor::SourceInfo::SectionInfo& section) const {
        return static_cast<float>(section.rawSize) * pixelsPerByte_;
    }

    void paintTab(juce::Graphics& g,
                  const juce::Rectangle<float>& bounds,
                  const juce::String& label,
                  bool isSounding);

    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t rangeStart_ {0};
    std::uint64_t rangeEnd_ {0};
    float pixelsPerByte_ {0.0f};
};

} // namespace opcoda