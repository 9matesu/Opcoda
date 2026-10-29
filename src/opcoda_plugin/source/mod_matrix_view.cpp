#include "mod_matrix_view.h"

namespace opcoda {

ModMatrixView::ModMatrixView() {
    setName("Modulation matrix");
    setHelpText("Oito rotas de modulacao: fonte, destino e amount. Cada celula "
                "e' um parametro automatizavel.");
    setWantsKeyboardFocus(false);

    for (auto& row : rows_) {
        // tinta do combo: o desenho do corpo fica no LookAndFeel; aqui so' a cor
        // do texto e o fim das cores de contorno do JUCE, que nunca seria visto.
        for (auto* combo : {&row.source_, &row.destination_}) {
            combo->setColour(juce::ComboBox::textColourId, palette::textOnDark);
            combo->setColour(juce::ComboBox::backgroundColourId,
                             juce::Colours::transparentBlack);
            combo->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
            combo->setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
            combo->setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
            addAndMakeVisible(*combo);
        }
        row.amount_.setSliderStyle(juce::Slider::LinearHorizontal);
        row.amount_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(row.amount_);
    }
}

void ModMatrixView::attach(juce::AudioProcessorValueTreeState& state) {
    if (attached_) {
        return;
    }
    attached_ = true;

    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        const auto n = juce::String {static_cast<int>(i) + 1};
        const auto srcId = ("m" + n + "src");
        const auto dstId = ("m" + n + "dst");
        const auto amtId = ("m" + n + "amt");

        auto* srcParameter = state.getParameter(srcId);
        auto* dstParameter = state.getParameter(dstId);
        auto* amtParameter = state.getParameter(amtId);
        // falta parametro e' erro de contracto entre layout e vista: morre em
        // Debug, e em Release a linha nasce vazia em vez de rebentar.
        jassertquiet(srcParameter != nullptr && dstParameter != nullptr && amtParameter != nullptr);
        if (srcParameter == nullptr || dstParameter == nullptr || amtParameter == nullptr) {
            return;
        }

        // os rotulos vem do parametro: a ordem dos choices e' contrato e a vista
        // nao repete a lista. Sem parametro, o combo fica vazio e o attachment
        // nulo, que e' o estado inofensivo.
        if (const auto* parameter = dynamic_cast<const juce::AudioParameterChoice*>(srcParameter)) {
            row.source_.addItemList(parameter->getAllValueStrings(), 1);
        }
        if (const auto* parameter =
                dynamic_cast<const juce::AudioParameterChoice*>(dstParameter)) {
            row.destination_.addItemList(parameter->getAllValueStrings(), 1);
        }

        row.source_.setName("Route " + n + " source");
        row.source_.setTooltip("Fonte de modulacao da rota " + n);
        row.destination_.setName("Route " + n + " destination");
        row.destination_.setTooltip("Destino de modulacao da rota " + n);
        row.amount_.setName("Route " + n + " amount");
        row.amount_.setTooltip("Profundidade da rota " + n + ", de -1 a +1");

        row.sourceAttachment_ =
            std::make_unique<juce::ComboBoxParameterAttachment>(*srcParameter, row.source_);
        row.destinationAttachment_ = std::make_unique<juce::ComboBoxParameterAttachment>(
            *dstParameter, row.destination_);
        row.amountAttachment_ =
            std::make_unique<juce::SliderParameterAttachment>(*amtParameter, row.amount_);
    }
}

void ModMatrixView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(palette::display);
    g.fillRect(bounds);

    // cabecalho: o que a vista e' e como se le. as tres colunas sao desenhadas
    // onde as celulas estao, e nao em numeros soltos: o cabecalho e' a legenda
    // da coluna, tem de cair em cima dela.
    g.setFont(fonts().sans(10.0f, true));
    g.setColour(palette::alpha(palette::textOnDark, 0.60f));
    g.drawText("MOD MATRIX",
               juce::Rectangle<float> {bounds.getX() + 10.0f, bounds.getY() + 5.0f,
                                       120.0f, 13.0f},
               juce::Justification::centredLeft, false);

    g.setFont(fonts().sans(9.0f, true));
    g.setColour(palette::alpha(palette::textOnDarkSub, 0.75f));
    const auto headerY = bounds.getY() + 7.0f;
    if (!columnBounds_.empty()) {
        const auto headerFor = [&](std::size_t column) {
            return juce::Rectangle<float> {static_cast<float>(columnBounds_[column].getX()),
                                           headerY,
                                           static_cast<float>(columnBounds_[column].getWidth()),
                                           12.0f};
        };
        g.drawText("SOURCE", headerFor(0), juce::Justification::centredLeft, false);
        g.drawText("DESTINATION", headerFor(1), juce::Justification::centredLeft, false);
        g.drawText("AMOUNT", headerFor(2), juce::Justification::centredLeft, false);
    }

    // indice de cada linha, pintado: e' rotulo, nao controle.
    g.setFont(fonts().sans(10.0f, true));
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        const auto row = rowBounds_[i].toFloat();
        if (row.isEmpty()) {
            continue;
        }
        g.setColour(palette::alpha(palette::textOnDarkSub, 0.55f));
        g.drawText(juce::String {static_cast<int>(i) + 1},
                   juce::Rectangle<float> {row.getX() - 24.0f, row.getY(), 16.0f,
                                           row.getHeight()},
                   juce::Justification::centredRight, false);
        g.setColour(palette::alpha(palette::accent, 0.75f));
        const auto arrowX = columnBounds_.empty() ? row.getX() + 214.0f
                                                  : static_cast<float>(columnBounds_[0].getRight());
        g.drawText(juce::String::fromUTF8("\xE2\x86\x92"),
                   juce::Rectangle<float> {arrowX, row.getY(), 22.0f, row.getHeight()},
                   juce::Justification::centred, false);
    }
}

void ModMatrixView::resized() {
    layoutRows();
}

void ModMatrixView::layoutRows() {
    // margem esquerda 26 px para o indice, 22 em cima para o cabecalho.
    const auto area = getLocalBounds().reduced(10, 4).withTrimmedTop(24).withTrimmedLeft(28);
    if (area.isEmpty()) {
        rowBounds_ = {};
        columnBounds_ = {};
        return;
    }

    constexpr int kRowGap {5};
    const int rowHeight = juce::jmax(20, (area.getHeight() - kRowGap * 7) / 8);
    const int gridHeight = rowHeight * 8 + kRowGap * 7;
    int y = area.getY() + juce::jmax(0, (area.getHeight() - gridHeight) / 2);

    // tres colunas: fonte, destino e amount. a barra bipolar ocupa o resto da
    // largura, porque e' ela que se le de relance.
    const int sourceWidth = juce::jmax(120, juce::jmin(210, area.getWidth() * 20 / 100));
    const int destinationWidth =
        juce::jmax(120, juce::jmin(210, area.getWidth() * 20 / 100));
    const int arrow = 26;
    const int gap = 10;
    const int amountWidth =
        juce::jmax(120, area.getWidth() - sourceWidth - destinationWidth - arrow - gap * 3);

    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        auto line = area.withTop(y).withHeight(rowHeight);
        rowBounds_[i] = line;

        const auto source = line.removeFromLeft(sourceWidth);
        line.removeFromLeft(arrow + gap);
        const auto destination = line.removeFromLeft(destinationWidth);
        line.removeFromLeft(gap);
        const auto amount = line.removeFromLeft(amountWidth);

        if (i == 0) {
            columnBounds_ = {source, destination, amount};
        }

        row.source_.setBounds(source.reduced(0, 1));
        row.destination_.setBounds(destination.reduced(0, 1));
        row.amount_.setBounds(amount.reduced(0, 3));
        y += rowHeight + kRowGap;
    }
}

} // namespace opcoda
