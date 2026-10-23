#include "byte_address_field.h"

#include <algorithm>
#include <limits>

namespace opcoda {
namespace {

// digito hexadecimal e' ascii, por isso o teste e' sobre o caractere e nao sobre
// uma tabela. os helpers recebem char e nao o juce_wchar da iteracao: so os ascii
// interessam, e o resto reprova o teste.
constexpr bool isHexDigit(char character) {
    return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
           (character >= 'A' && character <= 'F');
}

std::uint64_t digitValue(char character) {
    if (character >= '0' && character <= '9') {
        return static_cast<std::uint64_t>(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
        return static_cast<std::uint64_t>(character - 'a') + 10;
    }
    return static_cast<std::uint64_t>(character - 'A') + 10;
}

// numero hex, com ou sem prefixo. devolve false se sobrar um caractere que nao
// seja digito, e nesse caso o campo volta ao valor anterior: "1F4Z" nao vira
// endereco.
bool parseHex(const juce::String& text, std::uint64_t& out) {
    auto digits = text.trim();
    if (digits.startsWithIgnoreCase("0x")) {
        // "0x" e' ignorado: o campo ja o desenha a esquerda, e aceitar so uma das
        // formas seria uma assimetria gratuita.
        digits = digits.substring(2).trim();
    }
    if (digits.isEmpty()) {
        // so o prefixo, ou o campo vazio, e' o byte zero -- um endereco que
        // existe.
        out = 0;
        return true;
    }

    std::uint64_t value = 0;
    for (int i = 0; i < digits.length(); ++i) {
        const auto character = static_cast<char>(digits[i]);
        if (!isHexDigit(character)) {
            return false;
        }
        const auto digit = digitValue(character);

        // a multiplicacao transborda antes da soma: um ficheiro nao tem 17
        // digitos hexadecimais.
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 16) {
            return false;
        }
        value = value * 16 + digit;
    }

    out = value;
    return true;
}

} // namespace

ByteAddressField::ByteAddressField() {
    font_ = fonts().sans(11.0f);

    editor_.setFont(font_);
    editor_.setColour(juce::TextEditor::textColourId, palette::textOnDark);
    editor_.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    editor_.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor_.setColour(juce::TextEditor::focusedOutlineColourId, palette::accent);
    editor_.setColour(juce::TextEditor::highlightColourId, palette::alpha(palette::accent, 0.35f));
    editor_.setColour(juce::TextEditor::highlightedTextColourId, palette::textOnDark);
    editor_.setTextToShowWhenEmpty("0", palette::textOnDarkSub);
    // alinhado a direita: o numero cresce para a esquerda, como um contador de
    // enderecos, em vez de empurrar o resto do campo.
    editor_.setJustification(juce::Justification::centredRight);

    // 4.1.2 nome, funcao e valor. o TextEditor ja tem AccessibilityHandler; falta
    // dizer o que ele escreve e em que unidades. o valor em hexadecimal e' o valor.
    editor_.setName("Endereco do inicio da regiao de bytes");
    editor_.setHelpText(
        "Endereco hexadecimal do primeiro byte do material sintetizado. Escreva "
        "o valor e pressione Enter para mover a regiao para la; Esc volta ao "
        "valor anterior. As setas sobem e descem um byte, e PageUp e PageDown "
        "saltam dezasseis.");

    editor_.onReturnKey = [this] { commit(); };
    editor_.onEscapeKey = [this] { revert(); };
    // sair do campo confirma: o mais comum e' digitar o endereco e passar a outra
    // coisa. valor mal formado nao passa, porque o commit volta ao ultimo valido.
    editor_.onFocusLost = [this] { commit(); };
    editor_.onNavigationKey = [this](const juce::KeyPress& key) {
        nudge(key);
        return true;
    };

    addAndMakeVisible(editor_);
}

void ByteAddressField::setHighestAddress(std::uint64_t highest) {
    highestAddress_ = highest;
    // se o ficheiro encolheu, o valor mostrado desce para o ultimo byte: o campo
    // nao fica a mostrar um endereco que ja nao existe.
    if (shownAddress_ > highest) {
        showAddress(highest);
    }
}

void ByteAddressField::showAddress(std::uint64_t address) {
    shownAddress_ = address;

    // updating_ trava o onTextChange de reescrever o campo a meio da leitura; sem
    // ele o cursor saltava para o inicio a cada tique do timer de 20 Hz.
    if (updating_) {
        return;
    }

    updating_ = true;
    editor_.setText(
        juce::String::formatted("%llX", static_cast<unsigned long long>(address)),
        juce::dontSendNotification);
    updating_ = false;
    repaint();
}

void ByteAddressField::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(palette::display);
    g.fillRoundedRectangle(bounds, 2.0f);

    // 2.4.7 foco visivel: anel laranja a volta do campo. e' o sinal de que as
    // setas mexem no endereco e nao no knob em foco ao lado.
    if (editor_.hasKeyboardFocus(true)) {
        g.setColour(palette::accent);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.5f);
    } else {
        g.setColour(palette::displayBorder);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);
    }

    // "0x" desenhado aqui e fora do texto: dentro do TextEditor, um Ctrl+A
    // seguido de escrita apagava o prefixo e o valor lia-se como decimal.
    g.setColour(palette::textOnDarkSub);
    g.setFont(font_);
    g.drawText("0x", bounds.reduced(5.0f, 0.0f).withWidth(16.0f), juce::Justification::centredLeft,
               false);
}

void ByteAddressField::resized() {
    // o prefixo ocupa a fatia da esquerda; o editor fica com o resto.
    editor_.setBounds(getLocalBounds().reduced(5, 0).withTrimmedLeft(16));
}

std::uint64_t ByteAddressField::typedAddress() const {
    std::uint64_t parsed = 0;
    return parseHex(editor_.getText(), parsed) ? parsed : shownAddress_;
}

void ByteAddressField::nudge(const juce::KeyPress& key) {
    const auto code = key.getKeyCode();
    const bool upwards = code == juce::KeyPress::upKey || code == juce::KeyPress::pageUpKey;
    const auto step = (code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey)
                          ? std::int64_t {16}
                          : std::int64_t {1};

    // as setas partem do que esta escrito, nao do ultimo valor confirmado: quem
    // esta a meio de um endereco corrige-o com uma seta.
    const auto base = static_cast<std::int64_t>(typedAddress());
    const auto moved = base + (upwards ? step : -step);
    const auto clamped = static_cast<std::uint64_t>(juce::jlimit<std::int64_t>(
        0, static_cast<std::int64_t>(highestAddress_), moved));

    if (onAddressEntered != nullptr) {
        onAddressEntered(clamped);
    }
}

void ByteAddressField::commit() {
    std::uint64_t parsed = 0;
    if (!parseHex(editor_.getText(), parsed)) {
        revert();
        return;
    }

    const auto clamped = juce::jlimit<std::uint64_t>(0, highestAddress_, parsed);
    showAddress(clamped);

    if (onAddressEntered != nullptr) {
        onAddressEntered(clamped);
    }
}

void ByteAddressField::revert() {
    // um valor que nao e' hex nao vira endereco: volta ao ultimo que era, senao
    // "1F4Z" ficava escrito a prometer um endereco que nao existe.
    showAddress(shownAddress_);
}

} // namespace opcoda