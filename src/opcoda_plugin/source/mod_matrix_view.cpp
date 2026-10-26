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
    g.setColour(palette::displayPanel);
    g.fillRect(bounds);

    // cabecalho: o que a vista e' e como se le. a coluna do amount e' bipolar:
    // negativo a' esquerda, positivo a' direita, o centro e' nada.
    g.setFont(fonts().sans(10.0f, true));
    g.setColour(palette::alpha(palette::textOnDark, 0.55f));
    g.drawText("MOD MATRIX",
               juce::Rectangle<float> {bounds.getX() + 8.0f, bounds.getY() + 5.0f,
                                       120.0f, 13.0f},
               juce::Justification::centredLeft, false);

    g.setFont(fonts().sans(9.0f));
    g.setColour(palette::alpha(palette::textOnDarkSub, 0.70f));
    const auto headerY = bounds.getY() + 6.0f;
    // o rotulo da fonte e' o proprio combo: so' destino e amount precisam de
    // cabecalho, e o titulo fica a' esquerda.
    g.drawText("DESTINATION",
               juce::Rectangle<float> {bounds.getX() + 262.0f, headerY, 200.0f, 12.0f},
               juce::Justification::centredLeft, false);
    g.drawText("AMOUNT",
               juce::Rectangle<float> {bounds.getX() + 486.0f, headerY, 120.0f, 12.0f},
               juce::Justification::centredLeft, false);

    // indice e seta de cada linha, pintados: sao rotulo, nao controle.
    g.setFont(fonts().sans(10.0f));
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
        g.setColour(palette::alpha(palette::accent, 0.70f));
        g.drawText(juce::String::fromUTF8("\xE2\x86\x92"),
                   juce::Rectangle<float> {row.getX() + 214.0f, row.getY(), 18.0f,
                                           row.getHeight()},
                   juce::Justification::centred, false);
    }
}

void ModMatrixView::resized() {
    layoutRows();
}

void ModMatrixView::layoutRows() {
    // margem esquerda 26 px para o indice, 22 em cima para o cabecalho.
    const auto area = getLocalBounds().reduced(8, 4).withTrimmedTop(22).withTrimmedLeft(26);
    if (area.isEmpty()) {
        rowBounds_ = {};
        return;
    }

    constexpr int kRowGap {4};
    const int rowHeight = juce::jmax(18, (area.getHeight() - kRowGap * 7) / 8);
    const int gridHeight = rowHeight * 8 + kRowGap * 7;
    int y = area.getY() + juce::jmax(0, (area.getHeight() - gridHeight) / 2);

    const int comboWidth = juce::jmax(120, juce::jmin(200, area.getWidth() * 22 / 100));
    const int gap = 8;
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        auto line = area.withTop(y).withHeight(rowHeight);
        rowBounds_[i] = line;

        row.source_.setBounds(line.removeFromLeft(comboWidth).reduced(0, 1));
        line.removeFromLeft(20 + gap);
        row.destination_.setBounds(line.removeFromLeft(comboWidth).reduced(0, 1));
        line.removeFromLeft(gap);
        row.amount_.setBounds(line.reduced(0, 3));
        y += rowHeight + kRowGap;
    }
}

} // namespace opcoda
