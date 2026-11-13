#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "OpcodaFonts.h"

namespace opcoda {

// tipografia do Opcoda: Switzer para texto e numeros, entre 9 e 11 px. o corte
// Medio e' o corpo e o Semibold os rotulos; a familia e' um grotesco suico com
// formas fechadas, e a 9 px a linha cabe onde a Inter Display precisava de folga.
//
// **a monoespacada voltou, so' para o desmontador.** a JetBrains Mono tinha
// saido porque os numeros do mostrador pareciam apertados, e continuam na
// Switzer: a coluna ASM e' o unico lugar onde a largura fixa e' a informacao
// (mnemonicos e operandos alinham na vertical). vem do sistema, como reserva
// declarada, para nao juntar um binario de fonte ao programa por causa de uma
// coluna de texto.
//
// Switzer vem da Fontshare sob a FFL (resources/fonts/Switzer-FFL.txt) e chega
// reduzida a'quilo que a interface escreve por tools/vendor_fonts.py.
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

    // coluna ASM: avanco fixo, o que faz a instrucao alinhar linha a linha.
    [[nodiscard]] juce::Font mono(float height, bool strong = false) const {
        return juce::Font {juce::FontOptions {juce::Font::getDefaultMonospacedFontName(),
                                              height,
                                              strong ? juce::Font::bold : juce::Font::plain}};
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

        return Fonts {load(BinaryData::SwitzerMedium_ttf, BinaryData::SwitzerMedium_ttfSize),
                       load(BinaryData::SwitzerSemibold_ttf,
                            BinaryData::SwitzerSemibold_ttfSize)};
    }();

    return loaded;
}

} // namespace opcoda