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

inline const juce::Image& sliderCap() {
    static const juce::Image image {decode(BinaryData::slidercap_png,
                                           BinaryData::slidercap_pngSize)};
    return image;
}

} // namespace detail

} // namespace opcoda::assets
