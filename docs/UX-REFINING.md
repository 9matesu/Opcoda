# Refinamento de UX

## Escala de espaçamento

A interface assenta numa grelha de 8 px, com um único passo de 4 px para o
ajuste entre ícone e rótulo: 4, 8, 16, 24 e 32. Os valores vivem em
`src/opcoda_plugin/source/ui_tokens.h` (`tokens::space1` … `tokens::space5`) e
nenhum espaçamento fora desta lista entra no layout. A revisão expulsou o 12 e o
48 que ainda sobreviviam da escala anterior: margem do chassis, vão entre secções
e respiro interno dos componentes passaram a ser o mesmo número, de sorte que
deixa de haver desalinhamentos de 2 ou 3 px a compensar à mão.

| Token | px | Uso |
| --- | --- | --- |
| `space1` | 4 | micro: ícone e rótulo, folga entre as setas do preset |
| `space2` | 8 | respiro interno de componente, folga do rail |
| `space3` | 16 | margem do chassis, padding dos painéis, vão entre secções |
| `space4` | 24 | coluna do medidor, separação entre blocos |
| `space5` | 32 | espaço estrutural exterior |

## Controlos

- Ícone padrão: 24×24 px (`tokens::icon`). É o piso confortável para o rato e
  para o dedo, e o mesmo tamanho para todas as vistas.
- Ícone pequeno: 18 px (`tokens::iconSmall`), reservado à paginação do hex.
- Os três botões de zoom saíram da interface; a navegação do ecrã continua por
  roda do rato, teclado e duplo clique.
- Altura de controlo: 22 px (`tokens::controlHeight`).
- Raios: dois valores e mais nada. Painéis primários a 6 px
  (`tokens::radiusPanel`) — ecrã, painel de parâmetros, master e cartão do
  teclado; componentes aninhados a 3 px (`tokens::radiusControl`).
- Bordas: 1 px em toda a parte, com o mesmo `tokens::borderAlpha` (0,09). Um
  painel nunca leva contorno duplicado: o filete pertence ao painel, não ao
  conteúdo que ele aloja.

## Sidebar

As duas colunas de navegação — WAVE/HEX/MATRIX no ecrã, GRAIN/ENV/FILTER/MOD no
painel — partilham a mesma caixa: 72×32 px (`sidebar::kWidth` e
`sidebar::kHeight`), sem vão entre itens, glifo e rótulo sempre visíveis. Não há
caixa no repouso: a coluna é uma lista contínua, não uma pilha de cartões a
flutuar. O hover põe um banho de 5% de ivory; o estado activo põe um banho de 10%
e um filete de 2 px na esquerda — o acento é da coluna, não do botão, e por isso o
item activo parece pertencer à sidebar em vez de ser um botão independente.

O rail do ecrã mede 96 px — 16 de padding, 72 de botão e 8 de folga — e um filete
vertical, recuado 16 px do topo e do fundo do painel, marca o fim da coluna. O
rail é do mesmo preto do vidro: a coluna separa-se pelo filete, não por um tom
mais claro. O conteúdo do ecrã começa 8 px depois do filete, aos 104 px da borda,
que é exactamente a coluna onde arrancam os knobs do painel. As duas metades da
interface ficam, assim, presas ao mesmo eixo vertical.

Os módulos ficam dentro da chapa do painel, não no vidro: a coluna de 72 px herda
o material do painel, e os quatro itens de 32 px centram-se na altura interior com
8 px de folga em cima e em baixo. O painel tem 176 px e a sidebar ocupa 128 — a
diferença é o respiro, não um transbordo.

## Leitura do ecrã

A forma de onda tem duas leituras no mesmo eixo. Em cima, o envelope em branco:
barra cheia, altura esticada entre a mais baixa e a mais alta, para a forma
aparecer mesmo quando o binário tem amplitudes parecidas. Em baixo, a entropia:
a altura e o brilho crescem juntos, de sorte que barra alta e clara é material
aleatório e barra baixa e escura é material repetitivo. A leitura é directa, sem
grelha de bits nem números por cima.

A régua de endereços partilha o sistema de coordenadas das barras: o mesmo
`pxPerByte`, a mesma origem. Um clique na régua leva a cabeça de leitura ao byte
exacto que está debaixo do cursor e salta o transporte para a mesma fracção da
região — a agulha vermelha e o seu traço horizontal usam essa mesma projecção, e
o primeiro byte da região fica acessível, com a agulha em cima dele.

A precisão é de byte, não de pixe: o endereço sai de `floor(screen × bytes da
vista)`, e não de um arredondamento ao mais próximo. Sem isso, clicar num byte
caía no byte seguinte. O mesmo vale para o arrasto: a agulha segue o cursor byte a
byte.

O ecrã vazio deixou de ter a marca da Opcoda no centro: ficam os quatro cantos da
moldura e o texto `No source`, que respira num fade in/out lento. Um clique em
qualquer ponto desse ecrã abre o selector de ficheiro.

Na vista HEX, mover a posição arrasta a grelha: se o byte ficar fora da página,
a vista rola para cima ou para baixo e o endereço fica à vista.

O rodapé tem a legenda da entropia à esquerda — um filete de 132 px que vai do
cinza ao branco, com 0 e 8 bits nas pontas —, o transporte ao centro e P.RAND e
Scan à direita dos ícones. Os botões de Play e Hold são ícones de 24 px, do mesmo
tamanho dos outros glifos, e acendem com o estado: o Play pinta o fundo quando
está a tocar.

## Rodapé e transporte

O Play e o Hold são dois botões de 32×28 px centrados no conteúdo, com o glifo a
24 px. P.RAND e Scan ficam imediatamente à direita do par, e a paginação do hex
fecha o canto direito quando a vista é HEX. A legenda de entropia, à esquerda,
diz a escala da metade de baixo da onda. O Hold é um cadeado — argola em
meia-lua sobre o corpo —, não um risco: a 24 px, a argola tem de fechar em cima
do corpo, senão lê-se como um alvo.

## Proporção

A divisão vertical do chassis deixou de ser aproximada. Cabeçalho de 48 px,
margem de 16, ecrã de 396, vão de 16 e painel de 176 somam exactamente os 652 px
em que o corpo fecha. O ecrã conserva o maior peso visual; os parâmetros são o
painel de controlo, não o centro. A proporção áurea que antes justificava os
410 px do ecrã cedeu lugar à grelha de oito, e o ganho de 14 px foi para o
painel. O cabeçalho mantém os 48 px e os elementos nele postos — campo de 470×32
px, preset de 168 px, setas de 22 px —, agora com a margem certa de 16.

## Matriz de modulação

A matriz abre por omissão e ocupa a área de desenho com uma grelha de cinco
colunas fixas — Amp, Env, LFO, Key e Wheel — sobre treze destinos. Cada
célula é a intersecção entre uma fonte e um destino e escreve diretamente o
amount da rota correspondente em `m1..m8`, sem estado próprio nem janela
adicional. A célula com rota acende com o amount em percentagem centrado e uma
barra de magnitude na base, positiva em acento e negativa em `textSub`. O
arrasto vertical ou horizontal ajusta o valor; o duplo clique e o botão direito
zeram a atribuição. A linha do parâmetro focado distingue-se pelo filete de
acento, e as linhas com rotas conservam o rótulo em peso maior. A grelha
permanece alinhada com qualquer número de destinos porque a altura da linha é
calculada a partir da área disponível e limitada entre 15 e 34 px.

## Painel de knobs

O painel mede 176 px de chapa: 16 de padding em cima, quatro módulos de 32 px na
sidebar (128 no total, centrados na altura interior) e 16 em baixo — sobram 8 px de
folga de cada lado, e é essa folga que mantém a coluna dentro do painel. Os knobs
do grain continuam numa fila única de seis — Size, Density,
Position, Spray, Pitch, Glide —, com a janela do grain na sétima célula. A curva
do envelope ocupa a altura toda da faixa (280×144) e vive só no módulo ENV. O
P.RAND e o Scan saíram dos knobs e são dois sliders lineares no rodapé do ecrã,
ao lado do Play — trilho fino com leitura à direita, como no Ableton. A janela do
grão deixou de ser uma fila de quatro interruptores minúsculos: são quatro linhas
empilhadas na mesma célula, sem vão, com o glifo a encher a linha e o estado
activo a pintar a linha toda.

A grelha de knobs recebe o resto da chapa: duas filas de 72 px quando o módulo
tem oito células, uma fila de 90 quando tem quatro. Os discos de 64 px centram-se
na célula e os filetes divisórios entram 8 px em cada ponta, para não tocarem o
contorno do painel. O master cresceu para 216 px de largura — interior de
184×144, com o rótulo em 16 px e duas filas de 64 — e leva os quatro knobs
(Volume, Pan, Stereo, Level) numa grelha 2×2, com o medidor vertical de 24 px na
coluna da direita, separado por 8. O bloco de linhas centra-se na altura
disponível, de modo que os discos de 64 px se juntam em células de ~82 px no
envelope, ~97 no filtro e ~122 no LFO.

## Módulo LFO

No módulo LFO, o sync manda nos parâmetros: em Free ficam o Rate em Hz, em Sync
esse knob dá lugar ao DIV — o detent ocupa a célula do Rate e o Rate desaparece,
sem parâmetros mortos à vista. O botão Sync é um booleano, e o texto dele diz o
estado (Free ou Sync) em vez de ficar em branco: sem os nomes, o botão existia e
não se via. O grid tem quatro colunas sem células fundidas: Rate (ou DIV), Sync,
Depth e Steps em cima; Target e Wave em baixo, cada um com duas colunas. O Sync
fica ao lado do slider do DIV, que é a chave que manda nele — e o slider, que
antes usava `SliderParameterAttachment` e ficava preso no primeiro detent, agora
tem a gama 0..N-1 e escreve o parâmetro pela conversão certa. Cada forma
acende com um banho claro e um filete quando está seleccionada, o mesmo
tratamento dos interruptores do filtro. A onda são
dois ciclos que correm com a fase viva do áudio: desenho dim na amplitude cheia
e curva viva na profundidade, tudo a passar da esquerda para a direita, e a
fase parada não congela o ecrã (avança ao ritmo do LFO). No filtro, o slope
12/24 dB desceu para a fila de baixo como dois botões de texto clicáveis — um
ou outro, com o ativo aceso.

## Fora da caixa

O teclado MIDI deixou de ser uma gaveta pendurada no exterior e passou a ser um
cartão do próprio chassis: largura do corpo, raio de 6 px, filete de 1 px e o
mesmo respiro de 16 px que separa o painel do cabeçalho. O cartão assenta no
`faceplate` do corpo, que agora se pinta até ao fundo do desenho — antes parava no
fim do painel e o teclado ficava sobre um rectângulo preto, que era o "fundo
preto" da secção. As teclas entram 8 px
para dentro do cartão e o conjunto termina num canal de 44 px que aloja o
interruptor de 20×20 — um `−` vectorial com o teclado aberto, um `+` com ele
fechado. O glifo é desenhado em `paintGlyph`, não escrito: o carácter Unicode do
menos chegava ao JUCE como três glifos de mojibake, porque `juce::String(const
char*)` interpreta os bytes acima de 127 como Latin-1.

Aberto, o cartão mede 80 px e as teclas ocupam a largura do conteúdo; fechado,
resta uma tira de 32 px com a palavra MIDI à esquerda e o interruptor à direita.
O LED de actividade MIDI é branco e fica acima do interruptor, dentro do cartão;
fechado, a tira não tem altura para os dois e a luz passa para o lado. A janela
acompanha: 764 px de design com o teclado aberto, 716 com ele fechado,
uma diferença de 48. O chassis, esse, fecha sempre em 652, e a escala do
interface continua ancorada na altura aberta, de modo que alternar a gaveta não
reescala o resto. Na janela standalone as luzes de X e risco ficam à direita do
cabeçalho, com 14 px de lado, e o trio de zoom foi suprimido por completo.

## Cabeçalho

A engrenagem dos temas senta-se à esquerda do campo de ficheiro, colada a ele, e
o grupo de presets fica no canto direito. O LED de entrada MIDI saiu do cabeçalho
e foi para o teclado.

## Marca no cabeçalho

Ao passar o rato pela marca, o símbolo e o texto "chegam-se" — a marca Opcoda
e o nome crescem 10% com suavização (o hover do botão já anima a 90 Hz). A
troca é sequencial, nunca em sobreposição: na primeira metade do hover a marca
e o "Opcoda" saem; na segunda entra o bicho da bloodworm trax em silhueta
branca sem fundo (alfa = 1 − luminância do PNG) com o nome da editora. Um
filete corre por baixo do nome. O kerning do "Opcoda" é o de sempre:
`titleFont` sem tracking extra.

## Padronização

A revisão seguinte assentou quatro contratos que antes viviam dispersos pelo
código. A escala tipográfica ficou em `fonts.h`: caption de 9,5 px para
etiquetas, body de 11 px para valores, title de 12,5 px para assinaturas e
display de 14 px para estados vazios. A família é uma só — Switzer, em três pesos
(Regular, Medium, Semibold) — e a coluna de desmontagem, que antes usava uma
monoespacada do sistema, passou a usar a mesma Switzer: os algarismos do desenho
têm avanço fixo, de sorte que os endereços continuam a alinhar sem uma segunda
letra no ecrã. As tooltips, que o JUCE desenhava na fonte do sistema, passaram a
usar o mesmo body; já não há uma superfície a falar outra língua.

A cor segue um contrato de estado: `accent` diz activo, `accentSoft` fica
reservado ao brilho transitório, `focusRing` ao foco, `textMuted` ao
desactivado e `hair` às divisórias. Os tons de branco soltos no mostrador
foram substituídos por `accentSoft`, e o amount negativo da matriz deixou de
usar `ivory` — sentido e sinal passam a ler-se pela barra, pelo sinal e por
`textSub`. Os knobs derivam a tinta no desenho, de sorte que a troca de tema
já não deixa rótulo claro sobre chapa clara.

A interação ganhou o que o rigor do estúdio pede: o knob recebe foco por
clique e responde às setas, a matriz navega-se com setas, Enter, Delete e
`+`/`-`, e a célula acende por 180 ms quando o amount muda. O medidor mostra
o pico retido depois de a barra cair, e o transporte diz o nome e a dica de
acordo com o estado em que está. Todas as animações partilham a mesma janela
de 45 ms e a mesma curva — o que antes era excepção passou a regra.

## Referências

- Atlassian Design System — *Spacing tokens* (escala de 4 px).
- U.S. Web Design System — *Design tokens*.
- Design System Checklist — *Tokens, type, components and spacing*.
- aiuxplayground — skills *Better Interface*, *Impeccable*, *Spacing Audit*.
- Vanity Ibex — *Rogo* (estudo de formas geradas por corda; referência visual
  para a linguagem blocky e monocromática).