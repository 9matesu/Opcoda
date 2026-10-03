#pragma once

#include "boxed_label.h"
#include "palette.h"

#include "plugin_processor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace opcoda {

// Seletor de bytes: a faixa do display que escolhe que regiao do binario
// alimenta o motor granular.
//
// **E' um juce::Slider deitado, nao um Component com mouseDown.** Nao e' uma
// escolha de desenho: e' o que da foco por teclado, ajuste com setas, Home e
// End, e AccessibilityHandler com nome e valor, que sao os criterios 2.1.1 e
// 4.1.2 do WCAG 2.2. Um retangulo com mouseDown seria operavel so com rato.
//
// E' deitado porque e' uma regiao de um ficheiro, lida da esquerda para a
// direita como o mock do Stitch.
//
// **O seletor nao e' um parametro do host.** Os seis da Tabela 8 sao os seis, e
// a constitution e' explicita sobre isso; um setimo parametro automatizavel
// mudaria o que o ensaio T4 mede. O valor vive no Processor, entre em
// getStateInformation e setStateInformation ao lado do sourcePath, entao o
// projeto guarda e abre com a mesma regiao selecionada sem adicionar nada a
// tabela de parametros.
class ByteSelector : public juce::Component {
public:
    ByteSelector();

    // Chamado quando o utilizador mexe no seletor ou quando o processor muda a
    // regiao. O Processor e' o dono do valor; o seletor e' so a vista e o
    // comando, e nao guarda uma segunda copia.
    std::function<void(std::uint64_t start, std::uint64_t end)> onRangeChanged;

    // Chamado pelo duplo clique do rato e pela tecla Enter com o seletor em
    // foco. Sao duas formas de pedir a mesma operacao, e nao duas operacoes:
    // sem a tecla, o alinhamento seria impossivel so com o teclado.
    std::function<void()> onSnapRequested;

    // Le a regiao do Processor e escreve na escala, sem emitir onRangeChanged.
    // E' o que evita o ciclo: o Processor muda a regiao, o seletor reflete, e
    // o seletor nao volta a pedir a mudanca.
    void showRange(std::uint64_t start, std::uint64_t end);

    void setFileSize(std::uint64_t sizeBytes);

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void paintSectionBar(juce::Graphics& g);

    // Slider interno do seletor. A unica diferenca para um Slider normal e'
    // receber o duplo clique e a tecla de ativacao: o duplo clique padrao do
    // Slider escreveria um valor de retorno e estragaria o inicio da regiao.
    struct RangeSlider : public juce::Slider {
        std::function<void()> onSnapRequested;

        RangeSlider()
            : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox) {
        }

        void mouseDoubleClick(const juce::MouseEvent& event) override {
            if (onSnapRequested) {
                onSnapRequested();
                return;
            }
            juce::Slider::mouseDoubleClick(event);
        }

        bool keyPressed(const juce::KeyPress& key) override {
            if (key.getKeyCode() == juce::KeyPress::returnKey &&
                !key.getModifiers().isAnyModifierKeyDown() && onSnapRequested) {
                onSnapRequested();
                return true;
            }
            return juce::Slider::keyPressed(key);
        }
    };

    // Declarado antes dos dados para que o onValueChange, montado no construtor
    // depois dos dois, veja um objeto ja completo.
    RangeSlider slider_;
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t rangeStart_ {0};
    std::uint64_t rangeEnd_ {0};
};

} // namespace opcoda
