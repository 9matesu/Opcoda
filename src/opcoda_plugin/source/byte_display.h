#pragma once

#include "boxed_label.h"
#include "hex_grid.h"
#include "palette.h"
#include "plugin_processor.h"

#include "opcoda_core/pe/byte_range.h"
#include "opcoda_core/pe/column_reduction.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <vector>

namespace opcoda {

// O display do material, em tres leituras.
//
// Sao tres vistas porque sao tres perguntas diferentes sobre a mesma regiao, e
// porque o OE7 pede duas delas: "forma de onda navegavel" e "curva de entropia em
// versao enxuta". Nenhuma existed antes desta feature — o commit e99d089 trocou o
// seletor de 30 px por uma grelha de bytes e levou a curva com ele, e o display
// passou a mostrar bytes e nada mais.
//
//   - **forma de onda**: que som e' este. E' a unica vista que responde sem somar;
//   - **hex**: que bytes sao estes. Nao e' alterada: a geometria foi verificada por
//     captura em tres tamanhos de janela e nao ha razao para a mexer;
//   - **entropia**: o quanto o material e' aleatorio. E' o que a dispersao do grao
//     consome, e ate agora era invisivel.
//
// **A regiao e' a unidade nas tres.** A curva removida chegou a mostrar o ficheiro
// inteiro ao lado de uma regiao estreita, o que mente sobre o que esta a soar.
//
// A forma de onda e a entropia sao desenhadas aqui; o hex e' o HexGrid, que
// continua a ser um Component independente e vive como filho. Reutiliza-lo em vez de
// o reescrever e' deliberado: a sua geometria tem tres capturas atras e um
// redesenho seria o caminho de perder as tres.
class ByteDisplay : public juce::Component {
public:
    enum class ViewMode {
        waveform,  // 1
        hex,       // 2
        entropy,   // 3
    };

    ByteDisplay();

    // Os bytes crus. Mesmo contrato do HexGrid: o ponteiro e' para o vector do
    // Processor e fica valido enquanto o Processor viver.
    void setSource(const std::vector<std::uint8_t>* bytes, std::uint64_t fileSize);

    void setSections(const std::vector<PluginProcessor::SourceInfo::SectionInfo>& sections) {
        sections_ = sections;
        repaint();
    }

    void setRegion(std::uint64_t start, std::uint64_t end);

    void setReadHead(std::uint64_t address) {
        readHead_ = address;
        hex_.setReadHead(address);
        repaint();
    }

    // Cabeca de reproducao, em fracao da regiao, tal como a publicada pela thread
    // de audio. Fica separada da cabeca de leitura porque sao coisas diferentes: a
    // primeira anda sozinha com o transporte, a segunda responde ao knob.
    void setPlayhead(float fraction, bool visible) {
        playheadFraction_ = fraction;
        playheadVisible_ = visible;
        repaint();
    }

    void setMode(ViewMode mode);
    [[nodiscard]] ViewMode mode() const noexcept { return mode_; }

    // Endereco escolhido por clique nas vistas novas. Quem decide o que se escreve
    // com ele e' o editor, como no hex.
    std::function<void(std::uint64_t address)> onAddressActivated;
    std::function<void()> onSnapRequested;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

    // Para o editor escrever o modo sem conhecer o enum.
    static juce::String viewName(ViewMode mode);

private:
    // A cache de colunas, com a chave que a invalida.
    //
    // **A chave e' (regiao, numero de colunas) e nada mais.** Os bytes mudam com o
    // ingest, e o ingest chama setSource, que limpa a cache. Uma coluna por pixel e'
    // o que torna a chave depender da largura, e por isso um redimensionar reconstrói
    // — o que e' o comportamento certo, porque mudar a largura muda o desenho.
    void rebuildColumnsIfNeeded();
    [[nodiscard]] std::uint32_t columnCountForWidth() const noexcept;

    // Aplica o que decorre do modo. Chamado pelo construtor E por setMode, e nao
    // so por setMode: um estado derivado que so se aplica numa das duas entradas
    // fica errado sempre que a outra e' a primeira a correr.
    void applyModeVisibility();

    void paintWaveform(juce::Graphics& g);
    void paintEntropy(juce::Graphics& g);
    void paintSectionTicks(juce::Graphics& g);
    void paintReadHead(juce::Graphics& g, const juce::Rectangle<float>& area);
    void paintPlayhead(juce::Graphics& g, const juce::Rectangle<float>& area);

    // Fracao da regiao em que um endereco cai, em [0, 1]. Devolve 1,0 para um
    // endereco fora da regiao, e quem chama recorta.
    [[nodiscard]] float fractionOf(std::uint64_t address) const noexcept;

    // Endereco sob um ponto, ou o maximo de uint64 se o ponto nao for material.
    [[nodiscard]] std::uint64_t addressAt(juce::Point<float> position) const;

    // Linha da regiao acima da banda. Existe porque a regiao vem por endereco e o
    // desenho e' por fracao, e a conversao e' do nucleo.
    [[nodiscard]] double fractionFor(std::uint64_t address) const noexcept;

    static constexpr float kPadding {6.0f};
    static constexpr float kAxisHeight {12.0f};
    static constexpr float kAxisInset {22.0f};

    HexGrid hex_;

    const std::vector<std::uint8_t>* bytes_ {nullptr};
    std::vector<PluginProcessor::SourceInfo::SectionInfo> sections_;
    std::uint64_t fileSize_ {0};
    std::uint64_t regionStart_ {0};
    std::uint64_t regionEnd_ {0};
    std::uint64_t readHead_ {0};
    float playheadFraction_ {0.0f};
    bool playheadVisible_ {false};

    ViewMode mode_ {ViewMode::waveform};

    // Resolucao das vistas novas: quantas colunas por pixel. A roda muda isto e nao
    // o tamanho do componente, porque o resized do editor e' o dono da geometria e
    // um setSize de dentro de um paint fightaria com ele.
    float columnScale_ {1.0f};

    std::vector<pe::Column> columns_;
    std::uint64_t cachedRegionStart_ {0};
    std::uint64_t cachedRegionEnd_ {0};
    std::uint32_t cachedColumnCount_ {0};
    bool cacheValid_ {false};

    juce::Font monoFont_ {juce::FontOptions {10.0f, juce::Font::plain}};
};

} // namespace opcoda
