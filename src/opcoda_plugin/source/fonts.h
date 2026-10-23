#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "OpcodaFonts.h"

namespace opcoda {

// tipografia do Opcoda: Inter Display para texto e numeros, entre 9 e 11 px.
// e' o corte da familia com altura de x para texto pequeno, e le-se melhor a 10 px
// do que a Inter.
//
// **sem monoespacada.** a JetBrains Mono saiu porque os numeros pareciam
// apertados. os numeros vao na mesma Inter, alinhados a direita em caixa de
// largura fixa: nao ha `tnum` no JUCE para travar a largura dos digitos.
//
// **a fonte vem por Typeface::Ptr e nao por nome**, para nao depender de busca por
// nome e depois por estilo. se faltar um ficheiro, a fonte do sistema serve e
// ninguem avisa -- um binario que nao abre por causa de uma fonte e' pior.
struct Fonts {
    juce::Typeface::Ptr sansRegular;
    juce::Typeface::Ptr sansStrong;

    [[nodiscard]] bool embedded() const noexcept {
        return sansRegular != nullptr;
    }

    // a fabrica de que a interface depende: todas as fontes passam por aqui. era
    // metodo de BoxedLabel, que morreu; ficou por ser o unico caminho para a fonte
    // embebida.
    [[nodiscard]] juce::Font sans(float height, bool strong = false) const {
        if (const auto& face = strong ? sansStrong : sansRegular) {
            return juce::Font {juce::FontOptions {face}.withHeight(height)};
        }
        return fallbackSans(height, strong);
    }

private:
    // o caminho sem typeface embebida: nome do sistema. FontOptions e' imutavel e
    // nao tem setTypefaceName, por isso o nome vai no construtor.
    [[nodiscard]] static juce::Font fallbackSans(float height, bool strong) {
        return juce::Font {juce::FontOptions {juce::Font::getDefaultSansSerifFontName(), height,
                                              strong ? juce::Font::bold : juce::Font::plain}};
    }
};

// carrega uma vez e devolve sempre a mesma. o resultado vive num static para o
// typeface ficar registado enquanto o plugin viver: createSystemTypefaceFor
// desregista quando o ultimo Ptr morre, e um Font sem o typeface cai na
// substitute.
inline const Fonts& fonts() {
    static const Fonts loaded = [] {
        const auto load = [](const char* data, int size) {
            return (data != nullptr && size > 0)
                       ? juce::Typeface::createSystemTypefaceFor(data, static_cast<std::size_t>(size))
                       : nullptr;
        };

        return Fonts {load(BinaryData::InterDisplayMedium_ttf, BinaryData::InterDisplayMedium_ttfSize),
                       load(BinaryData::InterDisplaySemiBold_ttf,
                            BinaryData::InterDisplaySemiBold_ttfSize)};
    }();

    return loaded;
}

} // namespace opcoda