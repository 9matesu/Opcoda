#include "choice_control.h"

#include <cmath>

namespace opcoda {
namespace {

// o botao de opcao e' uma tecla da chapa: apagado e' so' o glifo, ligado e' um
// bloco de carvao com o glifo em marfim. o mesmo cromo da rail, para a leitura
// nao depender do modulo.
void dressButton(juce::Button& button, const juce::String& name, const char* icon) {
    button.setName(name);
    button.setTooltip(name);
    button.setClickingTogglesState(true);
    button.getProperties().set("uiLook", "icon");
    button.getProperties().set("uiIcon", icon != nullptr ? juce::String {icon} : juce::String {});
}

} // namespace

ChoiceControl::ChoiceControl(juce::AudioProcessorValueTreeState& state,
                             const juce::String& paramId,
                             const juce::String& title,
                             const juce::String& spokenName,
                             std::vector<Item> items,
                             Mode mode,
                             int columns)
    : items_(std::move(items)), mode_(mode), columns_(columns) {
    setName(spokenName);
    setHelpText(spokenName);

    title_.setText(title, juce::dontSendNotification);
    title_.setJustificationType(juce::Justification::centred);
    title_.setFont(fonts().sans(12.0f, true));
    title_.setColour(juce::Label::textColourId, palette::textSub);
    addAndMakeVisible(title_);

    valueLabel_.setJustificationType(juce::Justification::centred);
    valueLabel_.setFont(fonts().sans(12.0f));
    valueLabel_.setColour(juce::Label::textColourId, palette::textDark);
    addAndMakeVisible(valueLabel_);

    // os rotulos vem do parametro: a ordem das escolhas e' contrato e o
    // controle nao repete a lista.
    auto* parameter = state.getParameter(paramId);
    const auto* choice = dynamic_cast<const juce::AudioParameterChoice*>(parameter);
    if (choice != nullptr) {
        labels_ = choice->getAllValueStrings();
    }

    const auto count = static_cast<int>(labels_.size());
    jassertquiet(parameter != nullptr && count > 0);

    // o slider e' o valor. em icons, text e toggle ele nao se ve: os botoes
    // escrevem nele e ele escreve nos botoes.
    value_.setRange(0.0, static_cast<double>(juce::jmax(1, count - 1)), 1.0);
    value_.setName(spokenName);
    if (mode_ == Mode::detent) {
        value_.setSliderStyle(juce::Slider::LinearHorizontal);
        value_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        value_.getProperties().set("uiLook", "detent");
        addAndMakeVisible(value_);
    }
    if (parameter != nullptr) {
        attachment_ = std::make_unique<juce::SliderParameterAttachment>(*parameter, value_);
    }
    value_.addListener(this);

    const auto itemCount = juce::jmin(count, static_cast<int>(items_.size()));
    if (mode_ == Mode::toggle) {
        // uma chave so': o botao muda de estado e escreve o nome do estado.
        auto button = std::make_unique<juce::TextButton>();
        dressButton(*button, spokenName, nullptr);
        button->onClick = [this] { choose(currentIndex() == 0 ? 1 : 0); };
        buttons_.push_back(std::move(button));
        addAndMakeVisible(*buttons_.back());
    } else if (mode_ != Mode::detent) {
        for (int i = 0; i < itemCount; ++i) {
            auto button = std::make_unique<juce::TextButton>();
            const auto& item = items_[static_cast<std::size_t>(i)];
            const auto name = item.label.isNotEmpty() ? item.label : labels_[i];
            if (mode_ == Mode::icons) {
                dressButton(*button, name, item.icon);
            } else {
                dressButton(*button, name, nullptr);
                button->setButtonText(item.shortLabel.isNotEmpty() ? item.shortLabel
                                                                  : labels_[i]);
            }
            button->onClick = [this, i] { choose(i); };
            buttons_.push_back(std::move(button));
            addAndMakeVisible(*buttons_.back());
        }
    }

    syncFromValue();
}

void ChoiceControl::paint(juce::Graphics& g) {
    static_cast<void>(g);
}

void ChoiceControl::resized() {
    auto area = getLocalBounds();
    const bool tight = getHeight() < 112;
    title_.setBounds(area.removeFromTop(tight ? 14 : 17));

    // no modo text os botoes ja' escrevem o nome da opcao: um valor em baixo
    // repetiria a mesma palavra duas vezes. na chave acontece o mesmo -- o
    // rotulo dentro do botao ja' diz o estado. nos icones a legenda roubava aos
    // glifos a altura que os torna legiveis: a tecla acesa ja' diz qual e'.
    const bool withValueLabel =
        mode_ != Mode::text && mode_ != Mode::toggle && mode_ != Mode::icons;
    valueLabel_.setVisible(withValueLabel);
    if (withValueLabel) {
        valueLabel_.setBounds(area.removeFromBottom(tight ? 18 : 20));
    }

    if (mode_ == Mode::detent) {
        value_.setBounds(area.reduced(0, 2));
        return;
    }

    if (mode_ == Mode::toggle) {
        if (!buttons_.empty()) {
            // **a chave nao ocupa a casa inteira.** esticada aos 130 px da celula
            // ela virava um bloco preto ao lado de knobs; uma tecla de 92x34 le-se
            // como chave e deixa o compasso igual ao das outras casas.
            const int width = juce::jmin(area.getWidth() - 8, 92);
            const int height = juce::jmin(area.getHeight(), 34);
            buttons_.front()->setBounds(
                juce::Rectangle<int> {width, height}.withCentre(area.getCentre()));
        }
        return;
    }

    if (buttons_.empty()) {
        return;
    }

    const auto count = static_cast<int>(buttons_.size());
    const int columns = juce::jmax(1, columns_ > 0 ? columns_ : count);
    const int rows = (count + columns - 1) / columns;
    const int cellWidth = area.getWidth() / columns;
    const int cellHeight = area.getHeight() / juce::jmax(1, rows);

    // **o icone nao cresce com a celula.** um glifo de 50 px dentro de uma casa
    // de 150 px e' um cartaz, nao um botao: o desenho cabe no tamanho de uma
    // tecla, e o espaco a volta e' o que separa as opcoes umas das outras.
    constexpr int kMaxButtonSize {46};
    const int buttonSize =
        juce::jmin(kMaxButtonSize, juce::jmin(cellWidth - 2, cellHeight - 4));

    for (int i = 0; i < count; ++i) {
        const auto cell = juce::Rectangle<int> {
            area.getX() + (i % columns) * cellWidth,
            area.getY() + (i / columns) * cellHeight,
            cellWidth, cellHeight};
        buttons_[static_cast<std::size_t>(i)]->setBounds(
            cell.withSizeKeepingCentre(buttonSize, buttonSize));
    }
}

void ChoiceControl::sliderValueChanged(juce::Slider* changed) {
    if (changed == &value_) {
        syncFromValue();
    }
}

int ChoiceControl::currentIndex() const noexcept {
    return juce::jlimit(0, juce::jmax(0, labels_.size() - 1),
                        juce::roundToInt(value_.getValue()));
}

void ChoiceControl::choose(int index) {
    value_.setValue(static_cast<double>(index), juce::sendNotificationSync);
}

void ChoiceControl::syncFromValue() {
    const auto index = currentIndex();

    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        auto& button = *buttons_[i];
        const auto on = mode_ == Mode::toggle || static_cast<int>(i) == index;
        if (button.getToggleState() != on) {
            button.setToggleState(on, juce::dontSendNotification);
        }
    }

    if (mode_ == Mode::toggle && !buttons_.empty()) {
        // a chave escreve o estado: o botao diz em que posicao esta'.
        const auto text = index < labels_.size() ? labels_[index] : juce::String {};
        if (buttons_.front()->getButtonText() != text) {
            buttons_.front()->setButtonText(text);
        }
        valueLabel_.setText(text, juce::dontSendNotification);
        return;
    }

    if (mode_ == Mode::detent) {
        const auto text = index < labels_.size() ? labels_[index] : juce::String {};
        if (valueLabel_.getText() != text) {
            valueLabel_.setText(text, juce::dontSendNotification);
        }
        return;
    }

    // o nome da opcao activa fica em baixo: os botoes mostram o desenho, o
    // texto diz o que o desenho e'.
    juce::String text;
    if (index < labels_.size()) {
        const auto itemIndex = juce::jmin(index, static_cast<int>(items_.size()) - 1);
        if (itemIndex >= 0 && items_[static_cast<std::size_t>(itemIndex)].shortLabel.isNotEmpty()) {
            text = items_[static_cast<std::size_t>(itemIndex)].shortLabel;
        } else {
            text = labels_[index];
        }
    }
    if (valueLabel_.getText() != text) {
        valueLabel_.setText(text, juce::dontSendNotification);
    }
}

} // namespace opcoda
