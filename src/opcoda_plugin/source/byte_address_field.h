#pragma once

#include "fonts.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>

namespace opcoda {

// campo de endereco: o "go to address" do Ghidra. e' o caminho de teclado para a
// regiao -- a grelha e' so de rato, e o que se perde esta' em
// docs/10-acessibilidade-w3c.md. o que fica e' um TextEditor de verdade, com
// foco, selecao e nome acessivel.
//
// escreve o endereco, nao a fracao de leitura: o endereco escolhe que bytes sao o
// material, o POSITION escolhe que byte deles se le.
//
// aceita "1F4", "0x1F4" e "000001F4", em qualquer caixa. Enter confirma, Esc
// volta ao valor anterior, e sair do campo confirma.
class ByteAddressField : public juce::Component {
public:
    ByteAddressField();

    // endereco confirmado. o editor decide o que se faz com ele, porque so ele
    // sabe o comprimento da regiao atual.
    std::function<void(std::uint64_t address)> onAddressEntered;

    // endereco mais alto que existe: acima disso o campo corrige para o ultimo
    // byte em vez de escrever um endereco que nao ha.
    void setHighestAddress(std::uint64_t highest);

    // escreve o endereco sem emitir o callback: e' o caminho de leitura, o que
    // impede o Processor de reagir ao que ele proprio acabou de fazer.
    void showAddress(std::uint64_t address);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // foco no campo. chamado pelo editor quando o material muda, senao quem esta
    // a escrever perde o foco a meio de uma digitacao.
    void grabFocusOnField() { editor_.grabKeyboardFocus(); }

private:
    void commit();
    void revert();
    [[nodiscard]] std::uint64_t typedAddress() const;
    void nudge(const juce::KeyPress& key);

    // TextEditor com as quatro teclas de navegacao retiradas.
    //
    // o que um TextEditor consome depende do cursor estar ou nao no fim do texto,
    // e um campo que responde a setas as vezes e' um defeito que so aparece com
    // conteudo. aqui a tecla nunca chega ao TextEditor, por isso a captura e' a
    // mesma nos dois casos.
    struct AddressEditor : juce::TextEditor {
        std::function<bool(const juce::KeyPress&)> onNavigationKey;

        bool keyPressed(const juce::KeyPress& key) override {
            if (onNavigationKey != nullptr) {
                const auto code = key.getKeyCode();
                if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey ||
                    code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey) {
                    return onNavigationKey(key);
                }
            }
            return juce::TextEditor::keyPressed(key);
        }
    };

    AddressEditor editor_;
    // pela fabrica e nao por FontOptions directo: um FontOptions directo aqui
    // voltava a ser a fonte do sistema.
    juce::Font font_ {fonts().sans(11.0f)};
    std::uint64_t highestAddress_ {0};
    std::uint64_t shownAddress_ {0};
    bool updating_ {false};
};

} // namespace opcoda