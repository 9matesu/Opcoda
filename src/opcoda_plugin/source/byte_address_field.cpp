#include "byte_address_field.h"

#include <algorithm>
#include <limits>

namespace opcoda {
namespace {

// Um digito hexadecimal e' ASCII, entao o teste e' sobre o caractere e nao sobre
// uma tabela: um caractere acima de 0x7F nunca e' um digito e cai fora.
//
// Os helpers recebem char e nao o juce_wchar que a iteracao da String devolve.
// Narrowing de wchar_t para char e' seguro aqui porque so interessam os ASCII:
// qualquer caractere de outra gama chega negativo e reprova o teste do digito.
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

// Numero hexadecimal sem prefixo. Devolve false se sobrar um caractere que nao
// seja digito, e nesse caso o campo volta ao valor anterior: aceitar "1F4Z"
// seria escrever um endereco que o utilizador nao digitou.
bool parseHex(const juce::String& text, std::uint64_t& out) {
    auto digits = text.trim();
    if (digits.startsWithIgnoreCase("0x")) {
        // O prefixo e' ignorado em vez de fazer parte do numero, porque o campo
        // ja o desenha a esquerda. Aceitar "0x1F4" e nao "1F4" so por causa
        // disso seria uma assimetria gratuita para quem digita.
        digits = digits.substring(2).trim();
    }
    if (digits.isEmpty()) {
        // So o prefixo, ou campo vazio, e' o byte zero do ficheiro: um
        // endereco que existe.
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

        // A multiplicacao transborda antes de a soma. O ficheiro nao tem 17
        // digitos hexadecimais, e um campo com 17 e' digito a mais.
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
    // Alinhar a direita faz o numero crescer para a esquerda, como um contador
    // de enderecos, em vez de empurrar o resto do campo para a direita.
    editor_.setJustification(juce::Justification::centredRight);

    // 4.1.2 Nome, funcao e valor. O TextEditor e' o campo de texto do JUCE e tem
    // AccessibilityHandler; o que falta e' dizer o que ele escreve e em que
    // unidades. O valor em hexadecimal e' o valor.
    editor_.setName("Endereco do inicio da regiao de bytes");
    editor_.setHelpText(
        "Endereco hexadecimal do primeiro byte do material sintetizado. Escreva "
        "o valor e pressione Enter para mover a regiao para la; Esc volta ao "
        "valor anterior. As setas sobem e descem um byte, e PageUp e PageDown "
        "saltam dezasseis.");

    editor_.onReturnKey = [this] { commit(); };
    editor_.onEscapeKey = [this] { revert(); };
    // Sair do campo confirma, porque o mais comum e' digitar o endereco e passar
    // a outra coisa. Um valor mal formado e' que nao passa, porque o commit
    // volta ao ultimo endereco valido.
    editor_.onFocusLost = [this] { commit(); };
    editor_.onNavigationKey = [this](const juce::KeyPress& key) {
        nudge(key);
        return true;
    };

    addAndMakeVisible(editor_);
}

void ByteAddressField::setHighestAddress(std::uint64_t highest) {
    highestAddress_ = highest;
    // O valor mostrado e' corrigido a seguinte: o ficheiro pode ter encolhido e o
    // campo ficaria a mostrar um endereco que ja nao existe.
    if (shownAddress_ > highest) {
        showAddress(highest);
    }
}

void ByteAddressField::showAddress(std::uint64_t address) {
    shownAddress_ = address;

    // updating_ trava o onTextChange de reescrever o campo a meio da leitura.
    // Sem ele, o cursor de texto saltava para o inicio a cada tique do timer de
    // 20 Hz.
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

    // 2.4.7 Foco visivel. Anel laranja a volta do campo quando ele tem o foco:
    // e' o unico sinal de que as setas mexem no endereco e nao no knob que
    // estiver em foco ao lado.
    if (editor_.hasKeyboardFocus(true)) {
        g.setColour(palette::accent);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.5f);
    } else {
        g.setColour(palette::displayBorder);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 2.0f, 1.0f);
    }

    // O prefixo e' desenhado aqui e nao faz parte do texto: dentro do TextEditor,
    // um Ctrl+A seguido de escrita apagava o 0x e o valor seguinte seria lido
    // como decimal.
    g.setColour(palette::textOnDarkSub);
    g.setFont(font_);
    g.drawText("0x", bounds.reduced(5.0f, 0.0f).withWidth(16.0f), juce::Justification::centredLeft,
               false);
}

void ByteAddressField::resized() {
    // O prefixo ocupa a fatia da esquerda; o editor fica com o resto.
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

    // As setas partem do que esta escrito e nao do ultimo valor confirmado. Assim
    // quem esta a meio de um endereco o corrige so com uma seta, sem reescrever
    // o numero todo.
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
    // Um valor que nao e' hexadecimal nao vira endereco: volta ao ultimo que era.
    // Sem esta volta, "1F4Z" ficava escrito no campo a prometer um endereco que
    // nao existe.
    showAddress(shownAddress_);
}

} // namespace opcoda