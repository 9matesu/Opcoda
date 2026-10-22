#pragma once

#include "palette.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "OpcodaFonts.h"

namespace opcoda {

// Tipografia do Opcoda: Inter Display para tudo, texto e numeros.
//
// **Inter Display e nao Inter.** Inter Display e' o corte da mesma familia com
// altura de x e contraste concebidos para texto pequeno, e esta interface passa a
// vida entre 9 e 11 px — o rodape, os rotulos dos knobs, os digitos do endereco.
// A 10 px le-se melhor, e a letra e' mais aberta sem ser maior.
//
// **Sem monoespacada.** A JetBrains Mono saiu porque a leitura numerica parecia
// apertada nas caixas do rodape e da grelha. Os numeros vao na mesma Inter,
// alinhados a direita em caixa de largura fixa — a borda direita fica quieta e o
// valor nao respira. Nao ha `tnum` no JUCE para travar a largura dos digitos, e
// alinhar a direita e' o que segura a leitura sem ele.
//
// **A fonte vem por Typeface::Ptr e nao por nome.** Os dois ficheiros reduzidos
// partilhavam nome Postscript antes de tools/vendor_fonts.py o reescrever. Uma
// busca por nome e depois por estilo resolveria o conflito de forma implicita e
// fragil; um ponteiro para o typeface nao tem nada para resolver.
//
// **Se um ficheiro faltar, a fonte do sistema serve e nada avisa.** Um aviso em
// consola numa janela que o utilizador fecha sem ler e' ruido, e um binario que
// nao abre por causa de uma fonte e' pior do que uma interface com a letra do
// sistema. A interface fica legivel nas duas situacoes.
struct Fonts {
    juce::Typeface::Ptr sansRegular;
    juce::Typeface::Ptr sansStrong;

    [[nodiscard]] bool embedded() const noexcept {
        return sansRegular != nullptr;
    }

    // A fabrica de que a interface inteira depende. Todas as fontes do plugin
    // passam por aqui, e nao ha um segundo sitio onde se escreve uma.
    //
    // Era metodo de BoxedLabel; o componente morreu (caixas fora) e a fabrica
    // ficou, porque continua a ser o unico caminho para a fonte embebida.
    [[nodiscard]] juce::Font sans(float height, bool strong = false) const {
        if (const auto& face = strong ? sansStrong : sansRegular) {
            return juce::Font {juce::FontOptions {face}.withHeight(height)};
        }
        return fallbackSans(height, strong);
    }

private:
    // O caminho de recurso. FontOptions e' imutavel e nao tem setTypefaceName, e o
    // construtor com nome e' a forma de o escrever.
    [[nodiscard]] static juce::Font fallbackSans(float height, bool strong) {
        return juce::Font {juce::FontOptions {juce::Font::getDefaultSansSerifFontName(), height,
                                              strong ? juce::Font::bold : juce::Font::plain}};
    }
};

// Carrega uma vez e devolve sempre a mesma. O resultado vive num static para que o
// typeface fique registado enquanto o plugin viver: createSystemTypefaceFor
// desregista quando o ultimo Ptr morre, e um Font guardado sem o typeface seria
// desenhado com a substitute.
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