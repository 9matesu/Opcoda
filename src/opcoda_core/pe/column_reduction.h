#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opcoda::pe {

// Reducao de um intervalo de bytes a colunas, uma por pixel do display.
//
// **Fica no nucleo e nao no plugin** pela mesma razao que a ByteRange e a
// positionForByte: e' aritmetica com casos de bordo - uma regiao cujo comprimento
// nao divide o numero de colunas - e dentro de um paint() nao ha onde a testar.
// E' tambem o unico sitio onde a conversao byte->amostra do desenho pode divergir
// do motor, e o ensaio T1 ja usa a mesma funcao.
//
// O que uma coluna carrega, e porque sao tres coisas e nao uma:
//
//   - **minimo e maximo** sao o envelope que se desenha. E' o que mostra amplitude
//     sem ler o numero;
//   - **rms** e' o nucleo do envelope. Sem ele uma regiao de bytes aleatorios e
//     uma regiao de bytes constantes parecem a mesma coisa, e soa diferente;
//   - **entropia** em bits por byte e' a curva do OE7, que e' o que diz se o
//     material e' repetitivo ou denso.
struct Column {
    float minimum {0.0f};
    float maximum {0.0f};
    float rms {0.0f};
    float entropyBits {0.0f};

// Bytes que a coluna representa. E' de 64 bits e nao de 32 porque uma coluna
    // pode cobrir mais de 4 G num ficheiro enorme com poucas colunas, e truncar em
    // silencio partiria o invariante de que a soma das contagens da o comprimento
    // da regiao — que e' o invariante que o ensaio usa para provar a particao.
    std::uint64_t byteCount {0};

    [[nodiscard]] bool silent() const noexcept { return minimum >= 0.0f && maximum <= 0.0f; }
};

// Tecto de amostras por coluna.
//
// **E' o que impede que a vista seja inutilizavel num ficheiro grande.** Reduzir
// 12 MB a 900 colunas sem teto sao 12 milhoes de operacoes por reconstrucao, e a
// reconstrucao acontece sempre que a regiao muda — portanto a cada quadro de
// arrasto. Com o teto, o custo e' O(colunas * kMaxSamplesPerColumn) e nao depende
// do tamanho do ficheiro.
//
// **E o valor tem de ser MAIOR do que 256, e nao por afinacao.** A entropia de uma
// amostra de N bytes uniformes de 256 valores distintos aproxima-se de log2(N), e nao
// de 8. Com 64 amostras por coluna, log2(64) = 6 bits, e a curva nunca leria acima
// de 6 num material que vale 7 — um eixo de 0 a 8 que mostra sempre o material como
// repetitivo. Com 512, log2(512) = 9, acima do topo da escala, e o limite da
// medicao desaparece.
//
// 512 e' o ponto de equilibrio: 900 colunas vezes 512 e' 460 800 amostras, cerca de
// dois milissegundos de reconstrucao, contra os 250 milissegundos que 12 MB
// custariam sem teto.
inline constexpr std::uint32_t kMaxSamplesPerColumn {512};

// Reduz [start, end) a, no maximo, `columns` colunas.
//
// Devolve false sem mexer em `out` quando o intervalo nao e' usavel: `data` nula,
// intervalo vazio ou invertido, ou `end` a sair do buffer. A verificacao e' o
// principio III da constitution - `offset + tamanho` em aritmetica que nao
// transborda - e e' aqui que um regiao corrompido tem de ser recusado e nao
// lido a mais.
//
// **O numero de colunas pode sair menor do que o pedido.** Com quatro bytes e novecentas
// colunas, ha quatro colunas de um byte e nao novecentas de meio byte: uma coluna
// sem byte nenhum nao tem minimo, nem maximo, nem entropia, e seria um rectangulo
// vazio que se le como defeito. Ha um tecto de 65536 colunas pelo mesmo motivo que o
// parser recusa acima de kMaxSections: um `resize` que lance dentro de uma funcao
// `noexcept` termina o processo, e o host vai abaixo connosco. `out.size()` e' a
// verdade.
//
// Aloca em `out`, que e' a thread de interface. Nao aloca em nenhum sitio da
// thread de audio: esta funcao nunca e' chamada de la.
[[nodiscard]] bool reduceToColumns(const std::uint8_t* data,
                                   std::size_t size,
                                   std::uint64_t start,
                                   std::uint64_t end,
                                   std::uint32_t columns,
                                   std::vector<Column>& out) noexcept;

} // namespace opcoda::pe