# Refinamento de UX

## Escala de espaçamento

A interface usa uma escala de 4 px, a mesma do Atlassian Design System, do USWDS
e do Material: 4, 8, 12, 16, 24, 32, 48. Os valores vivem em
`src/opcoda_plugin/source/ui_tokens.h` (`tokens::space1` … `tokens::space7`) e
nenhum espaçamento fora desta lista entra no layout.

| Token | px | Uso |
| --- | --- | --- |
| `space1` | 4 | respiro entre teclas da mesma família |
| `space2` | 8 | margem superior do corpo, gap do ecrã |
| `space3` | 12 | margem inferior do ecrã, gap do corpo |
| `space4` | 16 | cabeçalho, controlos compostos |
| `space5` | 24 | margem lateral do corpo, ícone padrão |
| `space6` | 32 | colunas largas |
| `space7` | 48 | separações de secção |

## Controlos

- Ícone padrão: 24×24 px (`tokens::icon`). É o piso confortável para o rato e
  para o dedo, e o mesmo tamanho para todas as vistas.
- Ícone pequeno: 18 px (`tokens::iconSmall`), reservado à paginação do hex.
- Os três botões de zoom saíram da interface; a navegação do ecrã continua por
  roda do rato, teclado e duplo clique.
- Altura de controlo: 22 px (`tokens::controlHeight`).
- Raio: 8 px (`tokens::radius`), igual ao do painel e do vidro.

## Proporção

A divisão vertical entre o ecrã e o painel segue a proporção áurea
(`tokens::golden = 0,618`): o ecrã mede 410 px e o painel recebe o resto,
trinta por cento acima da divisão anterior. O ecrã é o objeto de maior peso
visual; os parâmetros são o painel de controlo, não o centro. O cabeçalho desceu
para 48 px e os elementos nele postos cresceram — campo de 470×32 px, preset de
168 px, setas de 22 px —, de sorte que a margem encolheu sem comprimir a leitura.

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

O painel encolheu de 314 para 180 px de chapa: a seção do master dita a altura
da faixa, e o painel acompanha. Os knobs do grain ficaram numa fila única de
seis — Size, Density, Position, Spray, Pitch, Glide — com a janela do grain na
sétima célula, ocupando a largura toda da chapa (~120 px por célula). A curva
do envelope, agora com a caixa a ocupar a altura toda da faixa (280×164),
vive só no módulo ENV. O P.RAND e o Scan
saíram dos knobs e viraram dois sliders lineares no rodapé do ecrã, ao lado do
Play — trilho fino com leitura à direita, como no Ableton. O Pan, o Stereo e o
Level desceram para a seção Master: ela passou a 198 px de largura, com os
quatro knobs (Volume, Pan, Stereo, Level) numa grelha 2×2 e o medidor vertical
na coluna da direita. O bloco de linhas do painel centra-se na altura
disponível, e os discos de 64 px juntam-se em células de ~82 px no envelope,
~97 no filtro e ~122 no LFO.

## Módulo LFO

No módulo LFO, o sync manda nos parâmetros: em Free ficam o Rate em Hz, em Sync
esse knob dá lugar ao DIV — o detent ocupa a célula do Rate e o Rate desaparece,
sem parâmetros mortos à vista. O grid tem três colunas sem células fundidas:
Rate, Depth e Steps em cima; Target, Wave e Sync em baixo — os cinco ícones de
forma colados num só compartimento de 150 px, longe das hairlines. A onda são
dois ciclos que correm com a fase viva do áudio: desenho dim na amplitude cheia
e curva viva na profundidade, tudo a passar da esquerda para a direita, e a
fase parada não congela o ecrã (avança ao ritmo do LFO). No filtro, o slope
12/24 dB desceu para a fila de baixo como dois botões de texto clicáveis — um
ou outro, com o ativo aceso.

## Fora da caixa

O teclado MIDI é uma gaveta fora do chassis: a chapa do instrumento termina no
filete de fim do corpo — e termina sempre aí, em 652 px de design, com o
teclado aberto ou fechado. A aba `MIDI - / MIDI +` fica colada por baixo, a
92×20 px, e nunca se move: abre e fecha para fora, para baixo. Aberta, o cartão
das teclas desdobra-se por baixo da aba até à borda da janela; fechada, resta
apenas a aba e a janela encolhe 78 px. A escala do interface é ancorada na
altura aberta, de modo que alternar a gaveta não reescala o resto. Na janela
standalone as luzes de X e risco ficam à direita do cabeçalho, com 14 px de
lado, e o trio de zoom foi suprimido por completo.

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
display de 14 px para estados vazios. O mono de 11 px serve apenas à coluna de
desmontagem. As tooltips, que o JUCE desenhava na fonte do sistema, passaram a
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
- Figma Community — *Golden Ratio Design Token Styler*.
- Vanity Ibex — *Rogo* (estudo de formas geradas por corda; referência visual
  para a linguagem blocky e monocromática).