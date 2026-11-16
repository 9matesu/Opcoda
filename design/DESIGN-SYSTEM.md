# Design system do Opcoda

Reescrito em 16/11/2026 a partir de `src/opcoda_plugin/source/palette.h`,
`fonts.h`, `look_and_feel.h` e `plugin_editor.cpp`. A versão anterior descrevia
uma interface que já não existia: falava de Inter (o código usa Switzer), de
âmbar e teal (o tema escuro é monocromático desde 13/11/2026) e de um display de
200–320 px (a janela é 1180×760 com display de 304).

**A fonte da verdade é o código.** Este documento descreve o que ele faz; quando
os dois divergirem, o código ganha e este ficheiro é que está errado.

## Origem

Da tela do Stitch (`design/stitch/`) ficou a arquitetura de informação: cabeçalho,
display com as vistas do material, painel de parâmetros em módulos, teclado. A
linguagem visual é a de uma estação de trabalho: superfícies planas, filetes de
1 px, tipografia pequena e densa, e a cor toda no conteúdo.

## Dois temas, os mesmos nomes

`palette.h` tem um `Tokens` ativo e cada nome é uma referência para o membro.
Quem pinta pede `palette::accent` e não sabe em que tema está. A troca é uma
atribuição (`palette::setTheme`) e vive em SETTINGS > THEMES, guardada em
`%APPDATA%/Opcoda/theme.txt`.

### DARK — grafite monocromático (padrão)

Não há matiz em lado nenhum. O que era verde-água passou a branco, e o que era
aviso ou erro distingue-se por **luminosidade** e por **forma**, nunca por cor.
Um ecrã sem matiz obriga a interface a dizer o estado por outros meios, e é isso
que a torna legível para quem não distingue cor.

| Token | Valor | Uso |
| --- | --- | --- |
| `chassis` | `#0e0e0e` | corpo do device |
| `chassisDark` | `#070707` | base do gradiente do corpo |
| `chassisBorder` | `#2c2c2c` | divisória estrutural, 1 px |
| `header` | `#151515` | barra do topo |
| `faceplate` | `#171717` | chapa do corpo |
| `faceplateLight` | `#1e1e1e` | topo do gradiente da barra |
| `panelBg` | `#1c1c1c` | painel de módulo |
| `subPanel` | `#232323` | face elevada |
| `display` | `#000000` | fundo do LCD |
| `displayBorder` | `#262626` | moldura interna |
| `displayGrid` | `rgba(255,255,255,.086)` | grelha do osciloscópio |
| `textDark` | `#f1f1f1` | texto primário |
| `textSub` | `#b8b8b8` | secundário |
| `textMuted` | `#8a8a8a` | decorativo, LED apagado |
| `textOnDark` | `#ededed` | texto sobre o display |
| `accent` | `#e8e8e8` | valor, aba ativa, anel de foco |
| `accentSoft` | `#f7f7f7` | topo da escala de entropia |
| `rust` | `#ededed` | arco de valor |
| `knobEdge` | `#5c5c5c` | arco de fundo do knob |
| `waveLow` | `#8c8c8c` | base da escala de entropia |
| `waveHigh` | `#e8e8e8` | topo da escala |
| `meterGreen/Amber/Red` | `#8a8a8a` / `#c4c4c4` / `#ffffff` | medidor por luminosidade |
| `focusRing` | `#e6e6e6` | anel de foco, 1,5 px |

### CLASSIC — chapa creme e laranja (2025 a 13/11/2026)

Preservado valor por valor para o instrumento poder voltar à cara com que foi
desenhado. `chassis #171512`, `faceplate #d6c9ae`, `accent #f2740d`,
`focusRing #f2b34d`, `waveLow #a9c09a`, `waveHigh #e0703a`.

**Contraste:** as medições de `docs/10-acessibilidade-w3c.md` são anteriores ao
tema escuro e precisam de ser refeitas sobre estes tokens. A regra que vale é a
de sempre: texto primário e secundário acima de 4,5:1 sobre a superfície em que
vivem, e o foco com forma (anel de 2 px) além da cor, pelo 1.4.1.

## Tipografia

Switzer, embarcada como dados binários (`OpcodaFonts`, gerada por
`tools/vendor_fonts.py` a partir de Medium e Semibold). A JetBrains Mono saiu do
corpo e voltou só para a coluna ASM, onde a largura fixa é a informação.

| Papel | Tamanho | Peso |
| --- | --- | --- |
| Rótulo de parâmetro | 9,5–10 px | Médio |
| Valor | 10,5–13 px | Médio |
| Título de módulo | 9 px, caixa alta, tracking .09em | Semibold |
| Leitura de display | 9 px | Médio |

**Nada de tracking em rótulo de knob.** O `withExtraKerningFactor(0.10)` que
existia em `knob.h` espaçava as letras como um selo e era o detalhe que mais
datava a interface. Em caixa alta de título o tracking fica; em texto de rótulo
não.

## Layout

Janela de projeto 1180×760 (escala por `uiScale_`), três faixas.

### 1. Barra do topo — 44 px

Gradiente vertical de 3 % (`faceplateLight` → `faceplate`), filete de 1 px
embaixo. **Sem sombra:** a profundidade vem do filete. Preset, ficheiro, formato
e tamanho ficam centrados na vertical, e um filete vertical separa o que é do
instrumento (preset) do que é do material (ficheiro).

### 2. Display — 304 px

Moldura de 6 px, vista do material, tira de vistas (`railBounds_`, 54 px) à
direita com as quatro vistas e os três zooms. Faixa de estado de 16 px no topo e
rodapé de 24 px com transporte, leituras e paginação do hex.

### 3. Painel de parâmetros

Cinco módulos (GRAIN, ENV, FILTER, MOD, KEYS) em abas de pasta. Cada módulo tem
cinco parâmetros, e o master vive fora das abas, no canto inferior direito.

Margens no grid de 8: `kBodyMargin 24`, `kBodyTopGap 8`.

## Controles

### Knob — arco e ponto

Saiu o corpo. O disco com gradiente, o aro e as onze marcas fixas davam ao painel
o ar de pedalboard e, a 96 px, pesavam mais que o valor. O que fica é leitura:

- arco de fundo a 45 % de `knobEdge`, com o peso `max(1,6 ; raio × 0,045)`;
- arco de valor em `accent`, 1,3× o peso do fundo;
- ponto cheio no fim do arco, 2,1× o peso;
- anel de foco em `focusRing` quando há foco de teclado.

O controle continua a ser um `juce::Slider`: foco por teclado, setas e
`AccessibilityHandler` vêm dele, não do desenho.

### Detent dial — até cinco posições

Com a propriedade `uiDetents` > 0, o knob vira dial: as posições aparecem **por
fora** do arco, em traço fino; a ativa é um **ponto cheio**, não um risco mais
grosso, para o formato dizer qual está ativa mesmo sem cor. O arco recolhe 6,5 px
para lhes dar lugar.

**Só em parâmetros de até cinco opções.** Acima disso o dropdown é melhor: girar
um knob de doze paradas é pior que ler a lista, e essa decisão já estava fechada
do lado do menu. O `Knob` decide sozinho pelo tamanho de `choiceLabels`.

### Botões

Tecla retangular de raio 4–5, filete fino, inativo neutro e ativo com filete ou
banho de `accent`. O transporte é ▶/■ desenhado em `Path`. Hover e premido são
banhos de alfa (6–18 %), sem halo.

## Ícones

Um sistema, não trinta desenhos. Todos nascem na mesma grelha de 24, são
tracados com o mesmo peso (1,5 unidade, com piso de 1,15 px para não desaparecer
a 11 px) e têm pontas e junções redondas. `paintGlyph` decide por nome e devolve
`false` quando não conhece o glifo, o que faz a tecla cair no texto — degradação,
não falha.

Os desenhos são nossos. A licença do SF Symbols da Apple não permite usar os
originais fora de plataforma Apple; a linguagem (traço fino, canto redondo, peso
único) é a mesma e não arrasta um binário de fonte de ícones.

## Marca

`paintMark` desenha o logo em `Path`: bloco de cantos redondos com o canto
inferior direito cortado na diagonal, e um ponto ao lado. É geométrico, por isso
escala de 16 a 256 px sem fonte. O SVG canónico está em
`resources/brand/logo.svg` (herda `currentColor`) e `logo-light.svg` (tinta
clara, para fundo escuro); os PNGs de ícone da aplicação saem dele.

## Animação

Tudo o que se mexe interpola por **meia-vida exponencial**, não por mola e não por
quadro:

```
valor += (alvo - valor) * (1 - 2^(-dt / meiaVida))
```

| O que | Meia-vida |
| --- | --- |
| cabeça de reprodução | 30 ms |
| cabeça de leitura | 60 ms |
| transição de vista | 90 ms |
| medidor | 80 ms |

O temporizador corre a 60 Hz e o delta vem de `getMillisecondCounterHiRes`. Todo
valor animado tem equivalente textual no ecrã.

## Adaptação ao JUCE

| Conceito | Equivalente |
| --- | --- |
| `bg-[#202326]` | `juce::Colour {0xff202326}` |
| Knob | `juce::Slider` `RotaryHorizontalVerticalDrag` + `LookAndFeel` |
| Detent dial | propriedade `uiDetents` no Slider, lida em `drawRotarySlider` |
| Ícone | `OpcodaLookAndFeel::paintGlyph` em `juce::Path`, sem imagem |
| Marca | `paintMark` em `juce::Path` |
| Grelha de bytes | `HexGrid` com `cellRect()` partilhada entre desenho e clique |
| Forma de onda e entropia | `ByteDisplay` com três caminhos de `paint()` |

**Não há peças de arte.** `resources/` só tem o logo e os PNGs do ícone da
aplicação; tudo o resto é desenhado em código.

## Pendente

- Re-medir contraste de `docs/10-acessibilidade-w3c.md` sobre o tema DARK.
- A abas de módulo ainda são abas de pasta desenhadas; o alvo seguinte é o
  segmented control central do Logic, que devolve largura ao painel.
