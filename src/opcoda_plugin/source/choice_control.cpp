#include "choice_control.h"

#include <cmath>

namespace opcoda {
namespace {

void dressButton(juce::Button& button,
                 const juce::String& name,
                 const char* icon,
                 std::vector<std::unique_ptr<PressHover>>& hovers) {
    button.setName(name);
    button.setTooltip(name);
    button.setClickingTogglesState(true);
    button.getProperties().set("uiLook", "icon");
    button.getProperties().set("uiIcon", icon != nullptr ? juce::String {icon} : juce::String {});
    attachButtonHover(button, hovers);
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
    title_.setFont(fonts().sans(10.0f, true));
    title_.setColour(juce::Label::textColourId, palette::textSub);
    addAndMakeVisible(title_);

    valueLabel_.setJustificationType(juce::Justification::centred);
    valueLabel_.setFont(fonts().sans(10.0f));
    valueLabel_.setColour(juce::Label::textColourId, palette::textDark);
    addAndMakeVisible(valueLabel_);

    auto* parameter = state.getParameter(paramId);
    const auto* choice = dynamic_cast<const juce::AudioParameterChoice*>(parameter);
    if (choice != nullptr) {
        labels_ = choice->getAllValueStrings();
    } else if (!items_.empty()) {
        // parametro booleano (ou nao-choice): os nomes vem dos itens da vista.
        // Sem isto o controlo ficava vazio, porque so' sabia ler AudioParameterChoice.
        labels_.clear();
        for (const auto& item : items_) {
            labels_.add(item.shortLabel.isNotEmpty() ? item.shortLabel : item.label);
        }
    }

    const auto count = static_cast<int>(labels_.size());
    jassertquiet(parameter != nullptr && count > 0);

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
        dressButton(*button, spokenName, nullptr, hovers_);
        button->onClick = [this] { choose(currentIndex() == 0 ? 1 : 0); };
        buttons_.push_back(std::move(button));
        addAndMakeVisible(*buttons_.back());
    } else if (mode_ != Mode::detent) {
        for (int i = 0; i < itemCount; ++i) {
            auto button = std::make_unique<juce::TextButton>();
            const auto& item = items_[static_cast<std::size_t>(i)];
            const auto name = item.label.isNotEmpty() ? item.label : labels_[i];
            if (mode_ == Mode::icons) {
                dressButton(*button, name, item.icon, hovers_);
            } else {
                dressButton(*button, name, nullptr, hovers_);
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
    const bool tight = getHeight() < 88;
    title_.setBounds(area.removeFromTop(tight ? 12 : 14));

    const bool withValueLabel =
        mode_ != Mode::text && mode_ != Mode::toggle && mode_ != Mode::icons;
    valueLabel_.setVisible(withValueLabel);
    if (withValueLabel) {
        valueLabel_.setBounds(area.removeFromBottom(tight ? 15 : 17));
    }

    if (mode_ == Mode::detent) {
        value_.setBounds(area.reduced(0, 2));
        return;
    }

    if (mode_ == Mode::toggle) {
        if (!buttons_.empty()) {
            const int width = juce::jmin(area.getWidth() - 8, 84);
            const int height = juce::jmin(area.getHeight(), 30);
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

    constexpr int kMaxButtonSize {30};
    const int buttonSize =
        juce::jmin(kMaxButtonSize, juce::jmin(cellWidth - 2, cellHeight - 2));
    const int switchWidth = juce::jmin(26, cellWidth - 3);
    const int switchHeight = juce::jmin(13, cellHeight - 3);

    for (int i = 0; i < count; ++i) {
        const auto cell = juce::Rectangle<int> {
            area.getX() + (i % columns) * cellWidth,
            area.getY() + (i / columns) * cellHeight,
            cellWidth, cellHeight};
        auto& button = *buttons_[static_cast<std::size_t>(i)];
        if (compactSwitches_) {
            button.setBounds(cell.withSizeKeepingCentre(switchWidth, switchHeight));
        } else {
            button.setBounds(cell.withSizeKeepingCentre(buttonSize, buttonSize));
        }
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
        const auto on = static_cast<int>(i) == index;
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
