# Design system do Opcoda

Reescrito em 16/11/2026 a partir de `src/opcoda_plugin/source/palette.h`,
`fonts.h`, `look_and_feel.h`, `knob.h`, `ui_animation.h` e `plugin_editor.cpp`. A
versão anterior descrevia uma interface que já não existia: falava de âmbar e
teal (o tema escuro é monocromático desde 13/11/2026) e de um display de 200–320
px (a janela é 1180×764 com o teclado aberto e display de 396).

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

Inter Display, embarcada como dados binários (`OpcodaFonts`, gerada por
`tools/vendor_fonts.py` a partir de Medium e Semibold). A JetBrains Mono saiu do
corpo e voltou só para a coluna ASM, onde a largura fixa é a informação.

| Papel | Tamanho | Peso |
| --- | --- | --- |
| Rótulo de parâmetro | 9,5–10 px | Semibold |
| Valor | 8–12,5 px, ajustado ao disco | Médio |
| Título de módulo | 9 px, caixa alta, tracking .09em | Semibold |
| Leitura de display | 9 px | Médio |

**Nada de tracking em rótulo de knob.** O `withExtraKerningFactor(0.10)` que
existia em `knob.h` espaçava as letras como um selo e era o detalhe que mais
datava a interface. Em caixa alta de título o tracking fica; em texto de rótulo
não.

## Layout

Janela de projeto 1180×764 com o teclado aberto e 1180×716 com ele fechado
(escala por `uiScale_`), quatro faixas sobre uma grelha de 8 px.

### 1. Cabeçalho — 48 px

Gradiente vertical de 3 % (`toolbarTop` → `toolbarBottom`), filete de 1 px
embaixo. **Sem sombra:** a profundidade vem do filete. Três peças, três pesos: a
marca à esquerda, a 16 px da borda (depois das luzes, no standalone), o campo do
ficheiro no centro exacto da faixa e o preset à direita, discreto. O campo é um
sulco com pasta, nome, tamanho, formato e chevron, como no preview; o clique é em
todo o rectângulo.

### 2. Ecrã — 396 px

O ecrã é uma chapa do mesmo material do painel de parâmetros — raio de 6 px,
filete de 1 px — com o vidro preto por dentro. A sidebar ocupa o rail de 96 px à
esquerda: 16 de padding, os três botões de 72×32 e 8 de folga até ao filete
vertical que fecha a coluna. O rail é do mesmo preto do vidro; o filete é que
separa a coluna. O conteúdo começa aos 104 px da borda, na mesma coluna em que
arrancam os knobs do painel.

A onda carro-chefe mede os 396 px de largura e dispensa bisel: o vidro ocupa a
região toda, sem sombra interna. A silhueta leva o corpo a 10 %, o núcleo de RMS
a 30 % e a textura das amostras a 34 % — a onda tem de se ler cheia, não como um
vidro apagado. As amostras cruas são desenhadas uma a uma, 2 400 pontos à largura
do vidro, por cima de um envelope a 5 % e de um núcleo de RMS a 16 %; é esse traço
que converte o bloco de ruído numa leitura. O navegador só aparece quando há
janela para navegar — com o material todo à vista a miniatura seria o mesmo
desenho outra vez, e a faixa vai para a onda. O rodapé de 28 px leva a legenda de
entropia à esquerda (132 px, do cinza ao branco), o transporte ao centro — dois
ícones de 24 px em botões de 32×28 —, P.RAND e Scan à direita dos ícones e a
paginação do hex no canto direito, com 18 px por tecla.

### 3. Painel de parâmetros — 176 px

O painel mede 176 px: 16 de padding em cima, quatro módulos de 32 px na sidebar
(128 no total, centrados) e 16 em baixo — sobram 8 px de folga de cada lado. Os
módulos ficam dentro da chapa do painel, não no vidro. Os
(GRAIN/ENV/FILTER/MOD) ocupam a coluna de 72 px à esquerda dos knobs — a escolha
pertence ao painel que ela altera, e a coluna mantém o olho na mesma zona. Os
glifos são os mesmos da barra de modo do Granulator III; o nome sobrevive no
rótulo, no tooltip e no leitor de ecrã. O master ocupa 216 px à direita, com o
rótulo Master, os quatro knobs (Volume, Pan, Stereo, Level) numa grelha 2×2 e o
medidor vertical de 24 px na coluna da direita.

Margens: `kMargin 16`, `kPanelPaddingX/Y 16`, `kGap 16`. A grelha enche a largura,
sem teto de célula, e o disco tem teto de 64 px. As células separam-se por filetes
de 1 px (`palette::hair`), recuados 8 px nas pontas. A janela do grão são quatro
linhas empilhadas na mesma célula, sem vão; o LFO tem quatro colunas (Rate/DIV,
Sync, Depth, Steps) com Target e Wave a ocupar duas cada; o intervalo entre o
gráfico e os controlos é de 16 px.

### 4. Teclado — 80 px

O teclado é um cartão dentro do chassis, com a largura do corpo, raio de 6 px e
filete de 1 px, separado do painel pelo mesmo vão de 16. As teclas entram 8 px
para dentro do cartão e o interruptor de 20×20 fica no canal de 44 px à direita.
Fechado, o cartão reduz-se a uma tira de 32 px com a palavra MIDI e o `+`, e a
janela encolhe 48 px.


## Controles

### Knob — arco, ponto e valor dentro

Saiu o corpo. O disco com gradiente, o aro e as onze marcas fixas davam ao painel
o ar de pedalboard e, a 96 px, pesavam mais que o valor. Saiu também a linha do
valor por baixo do disco: custava 22 px de altura a cada célula e era ela que
obrigava a fila a 148 px. O que fica é leitura:

- arco de fundo em `knobTrack` (`rgba(255,255,255,.22)`), com o peso
  `max(1,6 ; raio × 0,045)`;
- arco de valor em `accent`, 1,3× o peso do fundo;
- ponto cheio no fim do arco, 2,1× o peso;
- valor tabular **dentro do arco**, centrado, com o corpo ajustado ao disco
  (`jlimit(8 ; 12,5 ; diâmetro × 0,17)`);
- halo de hover a 4,5 % de `ivory` e arco 12 % mais grosso, ambos animados;
- anel de foco em `focusRing` quando há foco de teclado.

A célula desce para 92 px e o disco tem teto de 64 px. O controle
continua a ser um `juce::Slider`: foco por teclado, setas e
`AccessibilityHandler` vêm dele, não do desenho. Abaixo de 84 px de altura o
rótulo sai, e o nome fica no tooltip e no leitor de ecrã.

### Detent dial — até cinco posições

Com a propriedade `uiDetents` > 0, o knob vira dial: as posições aparecem **por
fora** do arco, em traço fino; a ativa é um **ponto cheio**, não um risco mais
grosso, para o formato dizer qual está ativa mesmo sem cor. O arco recolhe 6,5 px
para lhes dar lugar.

**Só em parâmetros de até cinco opções.** Acima disso o dropdown é melhor: girar
um knob de doze paradas é pior que ler a lista, e essa decisão já estava fechada
do lado do menu. O `Knob` decide sozinho pelo tamanho de `choiceLabels`.

### Teclado — flat

Teclas de uma cor só: branca `#dcdcde`, preta `#161618`, separadas por um filete
de 1 px. Sem gradiente, sem sombra e sem nome de nota; o estado lê-se pela luz
(branca apaga para cinza, preta acende para carvão). O desenho é
`OpcodaKeyboard::drawWhiteNote` e `drawBlackNote`, não do LookAndFeel.

### Botões

Tecla retangular de raio 4–5, filete fino, inativo neutro e ativo com filete ou
banho de `accent`. O transporte é ▶/■ desenhado em `Path`. O hover e o premido
são grandezas animadas entre 0 e 1 (`ui_animation.h`), não booleanos: o banho
mistura-se por `hoverOf` ao longo de 45 ms de meia-vida e o botão afunda 0,6 px
no clique. Sem animador instalado, o desenho recai no booleano do JUCE.

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

`paintMark` desenha o logo a partir de `resources/brand/logo-light.svg`, lido
como dados binários (`OpcodaBrand`) e convertido em `Drawable` no primeiro uso. A
tinta `#e8e8e8` do ficheiro é trocada por `palette::accent` antes de o XML ser
lido, e o cache é invalidado quando o tema muda. É o mesmo desenho na title bar,
no cabeçalho e no estado vazio do display. O SVG tem de vir sem BOM: o parser de
XML do JUCE recusa-o e a marca desaparece sem dar erro.

## Janela do standalone

**Sem barra própria.** A janela não reserva faixa: o conteúdo ocupa tudo, e as
luzes de fechar e minimizar ficam sobrepostas ao cabeçalho, em círculos de 10 px.
Uma região do Windows recorta os 12 px exteriores, de sorte que o canto
arredondado pertence à janela e não só ao desenho. Como a pega desapareceu com a
barra, o cabeçalho passou a arrastar a janela — em qualquer direção, mas só a
partir da faixa de 48 px do cabeçalho, medida em coordenadas de ecrã. O seletor
de ficheiro ocupa 470×32 px no centro dessa faixa. A engrenagem
dos temas senta-se à esquerda do campo, colada a ele; `#setOnThemeRequested` liga o botão ao
ecrã de ajustes no `standalone` e deixa-o escondido no VST3, onde não há tema
para trocar.

## Marca e nome

O nome vem em letra normal — Opcoda, não OPCODA — e o mesmo vale para os
rótulos dos parâmetros, dos módulos e dos botões. Ao passar o rato sobre a
marca, o cabeçalho troca o par de crescentes pelo logótipo da Bloodworm Trax e
escreve `bloodworm trax | Opcoda`; o SVG da marca continua a ser a fonte do
desenho, e o PNG da editora entra como dados binários do plugin.

## Agulha e parâmetros

A densidade entra no desenho como escala linear de 0 a 100, convertida a partir
do intervalo de 1 a 200 grãos por segundo do parâmetro. O pente ganha um dente
por passo, a poça de luz alarga-se na mesma razão e o corpo da agulha engrossa —
a resposta chega em 35 ms de meia-vida, em vez dos 70 ms anteriores. O controlo
deixa de saturar aos 20 grãos: o que o utilizador vê acompanha o que o motor
ouve.

## Mod matrix

A matriz é uma tabela seca: cabeçalho Source / Target / Amount e oito linhas.
Sem índices, sem pontos, sem setas e sem filetes por baixo de cada linha — só os
combos de texto com chevron e o amount bipolar com o zero ao centro. A leitura é
a do Ableton: a rota e a profundidade, nada mais.

## Tokens

A escala de espaçamento é uma grelha de 8 px com um único passo de 4: 4, 8, 16,
24 e 32 px, em `src/opcoda_plugin/source/ui_tokens.h`. O ícone padrão mede 24×24
px, o pequeno 18 px e a altura de controlo 22 px. Os raios são dois — 6 px nos
painéis primários, 3 px nos componentes aninhados — e as bordas são sempre de
1 px com o mesmo alfa (0,09). A divisão vertical fecha em 652 px: cabeçalho de
48, margem de 16, ecrã de 396, vão de 16 e painel de 176. Ver
`docs/UX-REFINING.md`.

## Temas

O tema troca o instrumento inteiro, do chassis ao último rótulo, e fica guardado
em `%APPDATA%\Opcoda\theme.txt`. São cinco, em `palette.h`:

| Tema | Superfície | Acento |
| --- | --- | --- |
| Grafite | preto e branco | branco |
| Clássico | creme e carvão | laranja queimado |
| Claro | papel e grafite | azul cobalto |
| Âmbar | grafite quente | âmbar |
| Ciano | grafite frio | ciano |

O ecrã fica preto em todos: é onde a onda é desenhada a branco, e um vidro claro
apagá-la-ia. A cor vive no acento, não no fundo.

## Filtro e analisador

A vista do filtro mostra a resposta dos dois biquads e, por trás, o espectro do
material em tempo real. O analisador é uma FFT de 512 amostras com janela de Hann,
agrupada em 64 bandas logarítmicas entre 20 Hz e 20 kHz. A altura e o brilho da
barra dizem a energia da banda, e o contorno claro dá o contraste. A suavização é
de ataque rápido e descida lenta, para as barras não tremerem a cada bloco.

## Linguagem dos botões

Nenhum botão de ação tem chapa, contorno ou estado deprimido. A seleção vive na
tinta do glifo — `accentSoft` quando ativo —, e o único rasto do rato é um banho
de 5 % durante o hover. Essa decisão retira da interface a pilha de caixas que
competia com a onda e aproxima o desenho do princípio de menos é mais.

A exceção é a coluna de navegação. As abas de vista e de módulo precisam de uma
área de toque legível, e a caixa de 72×32 resolve isso sem contorno nem chapa: no
repouso não há fundo nenhum — a coluna é uma lista contínua; o hover põe um banho
de 5 % de ivory; o estado activo põe um banho de 10 % e um filete de 2 px na
esquerda, com o glifo e o rótulo em ivory. O acento é da coluna, não do botão, e
é isso que faz o item activo parecer parte da sidebar.

Os botões só de ícone seguem a mesma gramática: quando estão ligados, ganham um
banho de 12 % de ivory e um filete de 30 %. É o estado activo das formas do LFO,
da lista de janelas e do Play, e é o que responde à pergunta "o que está
seleccionado?" sem depender da cor do glifo.

## Entropia

A entropia usa menos colunas quando o material todo está à vista: as barras ficam
largas e a leitura é de medidor. Com o zoom perto, uma coluna por pixel e as
barras afinam. O ícone da vista é um hexágono cheio, do mesmo tamanho dos outros
glifos.

## Vista hex

Ícone `0x` na tecla da vista, não um hexágono. O arrasto com o botão esquerdo
seleciona um intervalo de bytes e escreve a região; o clique simples continua a
mover a cabeça. No topo direito, um selector com as secções do PE
(`.text`, `.rdata`, `.data`...) salta para o endereço da secção.

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
| efeitos do preview | 70 ms |
| respiração da onda | 70 ms |
| hover de botão | 45 ms |
| pressão de botão | 30 ms |

A onda respira com o nível do envelope e a agulha leva uma poça de luz que
acende com a mesma grandeza; nenhuma das duas anima quando o material está em
silêncio. O temporizador do editor corre a 60 Hz e o delta vem de
`getMillisecondCounterHiRes`. Os animadores de hover e de pressão têm
temporizador próprio, a 90 Hz, que só existe enquanto o valor se move; com o rato
parado o custo é zero. Todo valor animado tem equivalente textual no ecrã.

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
| Hover e pressão | `PressHover` em `ui_animation.h`, lido pelo LookAndFeel por `uiHover`/`uiPress` |

**Não há peças de arte.** `resources/` só tem o logo e os PNGs do ícone da
aplicação; tudo o resto é desenhado em código.

## Pendente

- Re-medir contraste de `docs/10-acessibilidade-w3c.md` sobre o tema DARK.
- Cabeçalho: marca e preset desenhados; falta o menu (Arquivo/Editar/Ver).
- Teclado com o gradiente do preview.
- Células do painel com separadores de 1 px, quando a altura sobrar.
