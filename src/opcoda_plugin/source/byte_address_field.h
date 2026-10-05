#pragma once

#include "boxed_label.h"
#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>

namespace opcoda {

// Campo de endereco: o "Go to address" do Ghidra.
//
// **E' o caminho de teclado para a regiao.** O seletor de 30 px que existia
// antes dava setas, Home, End e Enter; a grelha que o substitui e' so de rato.
// O que se perde e' escrito em docs/10-acessibilidade-w3c.md, e o que fica e'
// isto: um TextEditor de verdade, que tem foco, selecao, e um nome accessivel.
//
// O campo escreve o endereco e nao a fracao de leitura. Sao coisas diferentes:
// o endereco escolhe que bytes sao o material, e o POSITION escolhe que byte
// deles se le. Um arrasta de deslize trocava o primeiro, e um clique na grelha
// mexe no segundo.
//
// Aceita "1F4", "0x1F4" e "000001F4", em qualquer caixa. Enter confirma, Esc
// volta ao valor anterior, e sair do campo confirma.
class ByteAddressField : public juce::Component {
public:
    ByteAddressField();

    // Endereco confirmado. O editor e' que decide o que se faz com ele, porque
    // so ele sabe o comprimento da regiao atual.
    std::function<void(std::uint64_t address)> onAddressEntered;

    // Endereco mais alto que existe. Acima disso o campo corrige para o ultimo
    // byte em vez de escrever um endereco que nao ha.
    void setHighestAddress(std::uint64_t highest);

    // Escreve o endereco sem emitir o callback. E' o caminho de leitura, e e' o
    // que impede que o Processor a reagir ao que ele proprio acabou de fazer.
    void showAddress(std::uint64_t address);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Concentra o foco no campo. Chamado pelo editor quando o material muda, senao
    // quem esta a escrever perde o foco do meio de uma digitacao.
    void grabFocusOnField() { editor_.grabKeyboardFocus(); }

private:
    void commit();
    void revert();
    [[nodiscard]] std::uint64_t typedAddress() const;
    void nudge(const juce::KeyPress& key);

    // TextEditor com as quatro teclas de navegacao retiradas.
    //
    // Nao se deixa o campo repassar as setas para o componente que o contem: o
    // que consome uma tecla no TextEditor depende do cursor estar ou nao no fim
    // do texto, e um campo em foco que responde a setas as vezes e' um defeito
    // que so aparece quando o campo ja tem conteudo. Aqui a captura e' a mesma
    // nos dois casos, porque a tecla nunca chega ao TextEditor.
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
    juce::Font monoFont_ {juce::FontOptions {11.0f, juce::Font::plain}};
    std::uint64_t highestAddress_ {0};
    std::uint64_t shownAddress_ {0};
    bool updating_ {false};
};

} // namespace opcoda