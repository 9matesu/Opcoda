#pragma once

#include "boxed_label.h"
#include "palette.h"

#include "plugin_processor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace opcoda {

// Abas de secao PE, proporcionais ao tamanho de cada secao.
//
// A largura proporcional e' o que faz o display dizer algo: a aba .text de um
// binario real ocupa quase toda a barra e o .rsrc aparece como um risco. Uma
// barra de abas de largura igual esconde exatamente a informacao que a barra de
// secoes do PE traz.
class SectionTabs : public juce::Component {
public:
    SectionTabs() { setOpaque(false); }

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        selected_ = 0;
        repaint();
    }

    void setSelected(int index) {
        if (selected_ != index) {
            selected_ = index;
            repaint();
        }
    }

    [[nodiscard]] int selected() const noexcept { return selected_; }

    void mouseDown(const juce::MouseEvent& event) override {
        const auto index = indexAt(event.position.toFloat());
        if (index >= 0) {
            setSelected(index);
        }
    }

    void paint(juce::Graphics& g) override;

private:
    // Secoes sem dados brutos, como .bss, ocupam largura zero. Sem isso o
    // cursor seria invisivel e a aba impossivel de acertar.
    [[nodiscard]] int indexAt(const juce::Point<float>& position) const {
        float x = 0.0f;
        for (std::size_t i = 0; i < sections_.size(); ++i) {
            const auto width = widthFor(sections_[i]);
            if (position.x >= x && position.x < x + width) {
                return static_cast<int>(i);
            }
            x += width;
        }
        return -1;
    }

    [[nodiscard]] float widthFor(const PluginProcessor::SourceInfo::SectionInfo& section) const {
        return static_cast<float>(section.rawSize) * pixelsPerByte_;
    }

    void paintTab(juce::Graphics& g,
                  const juce::Rectangle<float>& bounds,
                  const juce::String& label,
                  bool isSelected);

    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    int selected_ {0};
    float pixelsPerByte_ {0.0f};
};

} // namespace opcoda