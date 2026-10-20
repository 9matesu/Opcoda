#pragma once

// juce_graphics e' o que traz juce::Image e o descodificador de PNG.
// juce_gui_basics sozinho nao basta, e o erro so aparece no TU que inclui este
// cabecalho, o que torna a falha dificil de localizar.
#include <juce_graphics/juce_graphics.h>

#include "OpcodaAssets.h"

namespace opcoda::assets {

// Pecas fisicas vindas do asset harness, embutidas no binario.
//
// As imagens sao construidas uma vez e partilhadas: um juce::Image por knob
// seria copias de textura que nao compram nada, porque os seis knobs mostram o
// mesmo corpo. A diferenca entre eles e' so o angulo, e o angulo e' um
// parametro do desenho, nao um ficheiro.
//
// Nenhum texto, escala ou valor vem destas imagens. O corpo e o marcador sao
// fisicos; o arco de valor, o anel de foco, o rotulo e a caixa de valor sao
// desenhados em codigo, porque precisam de reagir a foco, teclado e estado.
namespace detail {

// Os nomes dos simbolos sao o nome do ficheiro sem separadores: o
// juce_add_binary_data tira o ponto e o hifen, por isso knob-md.png vira
// knobmd_png e nao knob_md_png.
//
// A carga e' por ImageFileFormat::loadFrom e nao por um atalho de PNG: e' o
// mesmo caminho que um PNG lido do disco percorreria, e devolve uma Image
// invalida em vez de lancar se os dados nao decodificarem.
inline juce::Image decode(const char* data, int size) {
    return size > 0 ? juce::ImageFileFormat::loadFrom(data, static_cast<std::size_t>(size))
                    : juce::Image {};
}

inline const juce::Image& knobMd() {
    static const juce::Image image {decode(BinaryData::knobmd_png, BinaryData::knobmd_pngSize)};
    return image;
}

inline const juce::Image& buttonLarge() {
    static const juce::Image image {decode(BinaryData::buttonlarge_png,
                                           BinaryData::buttonlarge_pngSize)};
    return image;
}

} // namespace detail

// Fontes. O mock usa tres familias: Chakra Petch no wordmark (a fonte "tech"),
// JetBrains Mono nos dados e Plus Jakarta Sans nos rotulos. As duas primeiras
// entram no binario porque sao a identidade do painel e um fallback do sistema
// mudava a largura das colunas do hex dump. Os rotulos continuam na sans do
// sistema: a 10 px em maiusculas a diferenca para Plus Jakarta nao se ve, e uma
// terceira fonte custava mais um ficheiro por cada peso.
//
// O Typeface e' criado uma vez e partilhado. O createSystemTypefaceFor faz o
// parse do TTF na primeira chamada; repeti-lo por knob seria parse do mesmo
// ficheiro seis vezes.
namespace fonts {
namespace detail {
inline juce::Typeface::Ptr load(const char* data, int size) {
    return juce::Typeface::createSystemTypefaceFor(data, static_cast<std::size_t>(size));
}
} // namespace detail

inline const juce::Typeface::Ptr& tech() {
    static const juce::Typeface::Ptr typeface {
        detail::load(BinaryData::ChakraPetchBold_ttf, BinaryData::ChakraPetchBold_ttfSize)};
    return typeface;
}

inline const juce::Typeface::Ptr& monoRegular() {
    static const juce::Typeface::Ptr typeface {
        detail::load(BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize)};
    return typeface;
}

inline const juce::Typeface::Ptr& monoBold() {
    static const juce::Typeface::Ptr typeface {
        detail::load(BinaryData::JetBrainsMonoBold_ttf, BinaryData::JetBrainsMonoBold_ttfSize)};
    return typeface;
}

// FontOptions com Typeface ignora o nome e os flags de estilo: o peso ja' vem
// do ficheiro. Pedir bold a um Typeface regular nao engrossa, cai no regular.
inline juce::Font tech(float height) {
    return juce::Font {juce::FontOptions {tech()}.withHeight(height)};
}

inline juce::Font mono(float height, bool bold = false) {
    return juce::Font {juce::FontOptions {bold ? monoBold() : monoRegular()}.withHeight(height)};
}

} // namespace fonts

} // namespace opcoda::assets
