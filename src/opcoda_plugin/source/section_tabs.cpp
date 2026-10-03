#include "section_tabs.h"

namespace opcoda {

void SectionTabs::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (bounds.isEmpty() || sections_.empty()) {
        return;
    }

    // A escala vem do total de bytes em secoes, para a barra ocupar sempre a
    // largura toda independentemente do tamanho do binario.
    std::uint32_t total = 0;
    for (const auto& section : sections_) {
        total += section.rawSize;
    }
    if (total == 0) {
        return;
    }
    pixelsPerByte_ = bounds.getWidth() / static_cast<float>(total);

    float x = bounds.getX();
    for (std::size_t i = 0; i < sections_.size(); ++i) {
        const auto width = widthFor(sections_[i]);
        if (width >= 1.0f) {
            paintTab(g,
                     {x, bounds.getY(), width, bounds.getHeight()},
                     sections_[i].name,
                     static_cast<int>(i) == selected_);
        }
        x += width;
    }
}

void SectionTabs::paintTab(juce::Graphics& g,
                           const juce::Rectangle<float>& bounds,
                           const juce::String& label,
                           bool isSelected) {
    if (isSelected) {
        g.setColour(palette::accent);
        g.fillRect(bounds);

        g.setColour(juce::Colours::black);
    } else {
        g.setColour(palette::displayPanel);
        g.fillRect(bounds);

        // Divisoria de 1 px entre abas inativas, mais visivel que a borda do
        // painel porque o fundo e' quase preto.
        g.setColour(palette::displayBorder);
        g.drawVerticalLine(juce::roundToInt(bounds.getRight()) - 1,
                           bounds.getY(),
                           bounds.getHeight());
        g.setColour(palette::textOnDarkSub);
    }

    // O texto so aparece quando a aba tem espaco para ele. Uma aba de 3 px com
    // texto e' ruido visual e nao informacao.
    if (bounds.getWidth() < 28.0f) {
        return;
    }

    g.setFont(BoxedLabel::monoFont(9.0f));
    g.drawText(label.toUpperCase(),
               bounds.reduced(4.0f, 0.0f),
               juce::Justification::centredLeft,
               true);
}

} // namespace opcoda