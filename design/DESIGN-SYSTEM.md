# Design system do Opcoda

Extraído do projeto Stitch **Opcoda Granular Synth Interface** (ID 4838353471144449707),
em 01/10/2026, com o MCP do Stitch. Os arquivos-fonte estão em `design/stitch/`.

Este documento é a fonte da verdade visual da interface. A implementação em JUCE
segue os tokens daqui, não os valores do HTML.

## Origem

| Arquivo | Tela |
| --- | --- |
| `01-ableton-style.png` / `.html` | Opcoda Granular Synthesizer, Ableton Style |
| `02-disassembly-scanner.png` / `.html` | Opcoda, Disassembly & Byte Scanner |

## Paleta

O chassis é **claro** e o display é **escuro**. É a convenção de device do Ableton
Live, e é o que separa a área de parâmetros da área de visualização.

### Chassis e superfícies

| Token | Valor | Uso |
| --- | --- | --- |
| `chassis` | `#c2c6c9` | corpo do device |
| `chassisDark` | `#b2b6ba` | borda inferior do chassis |
| `chassisBorder` | `#52575c` | divisórias estruturais, 1 px |
| `header` | `#d2d6d9` | barra de título |
| `panelBg` | `#b6babd` | painel de módulo |
| `subPanel` | `#a8acad` | sub-painel e campo de valor |
| `valBox` | `#d5d8da` | caixa de valor numérico |

### Display

| Token | Valor | Uso |
| --- | --- | --- |
| `displayDark` | `#0e1013` | fundo do LCD |
| `displayPanel` | `#14181c` | barra de abas de seção |
| `displayGrid` | `rgba(255,255,255,0.05)` | grade do osciloscópio, 24 px |
| `displayBorder` | `#252b31` | divisória interna do display |

### Texto

| Token | Valor | Uso |
| --- | --- | --- |
| `textDark` | `#1b1e22` | texto sobre superfície clara |
| `textSub` | `#383d42` | texto secundário sobre claro |
| `textOnDark` | `#c6cacc` | texto sobre o display |
| `textOnDarkSub` | `#8e98a1` | aba inativa, metadado |
| `textMuted` | `#6b7176` | rótulo desabilitado |
| `textField` | `#555a60` | dica de campo |

### Acentos

| Token | Valor | Uso |
| --- | --- | --- |
| `accent` | `#ff9a00` | laranja Ableton: aba ativa, ponteiro de knob, pico do espectro |
| `accentSoft` | `#ffc83b` | amarelo do cabeçalho; meio da escala de entropia |
| `accentWarn` | `#ffb833` | amarelo de atenção |
| `waveLow` | `#35c4dc` | ciano: base da escala de entropia (0–3 bits/byte) |
| `waveHigh` | `#ea7070` | rosa: topo da escala de entropia (6–8 bits/byte) |
| `ok` | `#4caf50` | estado pronto, "STEREO GRAINS" |
| `okBright` | `#82d64a` | botão de ativação ligado |
| `error` | `#e05373` | estado de recusa |
| `pink` | `#ea7070` | série secundária do espectro |

O laranja `#ff9a00` sobre `#c2c6c9` dá contraste de 1,24:1, e **não** atinge 4,5:1.
Onde o laranja carrega texto, ele vai sobre preto (`#0e1013` dá 8,96:1), e os anéis
de foco usam `focusRing` (`#1b1e22`, 9,73:1 sobre o chassi). Regra: laranja é
preenchimento e indicador, nunca portador de texto pequeno sobre superfície clara
— e nunca indicador de foco sobre superfície clara.

## Tipografia

Duas famílias, escala densa de device.

| Papel | Família | Tamanho | Peso |
| --- | --- | --- | --- |
| Título do device | Inter | 13 px | 600 |
| Subtítulo | Inter | 10 px | 400 |
| Rótulo de parâmetro | Inter | 10 px | 500 |
| Metadado numérico | Inter | 10–11 px | 400–500 |
| Aba de seção | Inter | 9 px | 700, maiúscula |
| Estado do display | Inter | 9 px | 500, maiúscula |

Uma família só. A JetBrains Mono saiu porque a leitura numérica parecia apertada
nas caixas do rodapé e da grelha. Regra: **todo número é Inter**, alinhado ao
centro com a caixa que tinha, e a borda direita fica quieta porque a caixa tem
largura fixa. Isso é o que faz a leitura de valor parecer instrumento e não
página web. Não há `tnum` no JUCE para travar a largura dos dígitos; o
alinhamento em caixa fixa é o que segura a leitura sem ele.

## Layout

Três faixas, de cima para baixo, com 1 px de `#52575c` entre elas.

### 1. Cabeçalho

Botão de ativação (círculo com fenda vertical, verde `#82d64a` quando ligado),
nome do device, ícone de pasta para carregar, nome do ficheiro em Inter, e à
direita `[64-bit PE]` e o tamanho em texto puro — sem caixas nem molduras em
nenhum dos três.

**Não há mais texto no cabeçalho.** Saíram o subtítulo `Granular Synthesizer`, o
contador `VOICES 8`, a dica `DRAG & DROP BINARY` e o título do módulo
`1 - GRANULAR ENGINE`, porque cada um repetia informação escrita noutro sítio: o
título repetia o nome do device, o contador repetia o rodapé — e escrevia o máximo
em vez do valor real —, a dica repetia a linha de estado e o "1" prometia um
segundo módulo que nunca existiu. Regra geral: **um rótulo que repete outro rótulo
é ruído**, por mais bem desenhado que esteja.

### 2. Display

Fundo `#0e1013`, com 1 px de `#252b31` na moldura. A faixa de display mede entre
200 e 320 px, metade da altura da janela, e a janela mínima é 480×410.

**O display tem três vistas**, porque são três perguntas sobre a mesma região e
porque o OE7 pede duas delas. A região é a unidade nas três: mostrar o ficheiro
inteiro ao lado de uma região estreita mente sobre o que está a soar.

De cima para baixo:

- **Linha de estado**, 14 px: LED e `PRONTO / ficheiro` ou `RECUSADO / E_CODIGO` à
  esquerda; à direita o campo hexadecimal do endereço da região e o transporte
  como glifo ▶/■, sem texto e sem cromo. Sem botão `ALINHAR`: tecla A e
  duplo-clique cobrem, e o terceiro controle empurrava o texto para debaixo do
  campo. Quando o host está parado o glifo fica em `textOnDarkSub` e a linha diz
  `TRANSPORTE DO HOST PARADO` — desativado e não invisível, porque um botão que
  aceita o toque e não produz nada é pior do que um que recusa o toque.
- **Medidor de saída**, 3 px no topo do display: pico com faixa de −60 dB a 0 dB. No
  topo e não no rodapé porque o rodapé é do editor e está lá a telemetria escrita.
- **Vista do material**, o corpo do display:
  - **forma de onda**: envelope mínimo-máximo fraco e núcleo de RMS forte, com a
    linha do núcleo colorida por entropia — ciano no previsível, amarelo no meio,
    rosa no aleatório — e glow em pilha de alfa. O mínimo e o máximo quase não
    servem para mostrar bytes — o texto de um `.exe` tem quase todos os valores
    de `0x00` a `0xFF`, e o envelope fica com altura quase inteira em todas as
    colunas. O RMS é o que varia, e é o que diz se o material é denso ou
    repetitivo; a cor diz do que ele é feito. Três Paths por faixa em vez de um
    stroke por coluna, com subpath novo a cada descontinuidade para não ligar
    colunas distantes com diagonais;
  - **grelha de bytes**: endereço de 8 dígitos hexadecimais à esquerda, dezasseis
    bytes por linha em Inter 11 px, e a coluna ASCII à direita. Cabeçalho de colunas
    de 14 px com `00` a `0F`. Linhas de 24 px, com um filete de 1 px entre elas. A
    região ativa leva fundo laranja a 22 % **e** um filete de 1 px em cima e em
    baixo da linha. Os bytes nulos saem num tom à parte. Os limites de secção
    aparecem no gutter, na linha onde a secção começa, com uma barra de 2 px a toda
    a altura da linha e o nome à direita. Abaixo de 520 px a coluna **ASCII é
    omitida**. O gutter **cresce com o ficheiro**;
  - **curva de entropia**: 0 a 8 bits por byte com o **eixo legível em 0, 4 e 8** e
    a unidade escrita como `bits/byte`. Uma curva sem eixo não tem leitura: 7,5 é o
    valor de um `.exe` normal e 3,5 o de um repositório de bytes iguais. A linha
    usa a mesma cor-por-entropia da onda, pelo mesmo helper;
  - **grelha com barras**: a vista hex acopla uma faixa de barras de entropia à
    direita (30 % da largura, some abaixo de 120 px). Cada barra é o máximo do
    grupo de colunas, na cor da faixa — sem glow, sem eixo. O `HexGrid` encolhe;
    scroll, clique e duplo-clique não mudam.
- **Cabeças**: a de leitura é uma barra branca de 2 px com realce por baixo; a de
  reprodução é um **triângulo** em cima com barra fina, e é a única peça do display
  com uma forma própria — o que a identifica mesmo sem cor, como o 1.4.1 exige. A
  caret de teclado só aparece com o foco do teclado, senão seriam três barras
  verticais no mesmo ecrã.
- **Rodapé de telemetria**, 16 px: entropia em bits por byte na janela da cabeça de
  leitura, `TP` com a cabeça de reprodução e o instante e a duração da volta,
  `POS`, `REG`, `PK` e `VOICES`, tudo em texto puro sem caixa nem moldura. Tudo
  em `textOnDark`. O rodapé degrada em vez de espremer, e a ordem de sacrifício
  é o valor: primeiro o pico, a entropia é a última a cair. A taxa saiu da faixa
  — constante na sessão, foi para o tooltip do título.

**Não há banda da região nas vistas novas**, e a diferença é intencional: no hex a
região é uma faixa sobre células de largura fixa porque o que se vê é o ficheiro
inteiro; na forma de onda e na curva o que se vê já é a região. No lugar ficam as
marcas dos limites de secção, que são filetes de 1 px — um sinal que não depende da
cor.

### 3. Painel de parâmetros

Abas verticais de modo à esquerda e, à direita, módulos agrupados em caixas
`panelBg` com borda 1 px `chassisBorder`.

Cada módulo tem título em maiúscula, e dentro dele os controles com rótulo
acima e valor em texto puro abaixo — sem `valBox`.

## Controles

Knob circular com ponteiro laranja e arco de valor, 1 px de arco de fundo
`chassisBorder`. **O corpo é desenhado em código e não vem de nenhuma imagem**:
disco liso com luz suave de cima, aro fino de 1 px e ponto de origem — sem
serrilhado, que virava ruído cinzento em diâmetro útil. Rótulo em Inter 10 px
acima, valor em Inter 10 px abaixo, sem caixa.

**Por que a peça fotografada saiu.** `knob-md.png` eram 186×192 píxeis para pintar
um disco de 62, e `button-large.png` obrigava a desenhar a peça quadrada ao lado do
texto porque esticar um bisel de 2 px o deixava com 1 px de um lado e 4 px do outro.
Com `Path` o corpo escala, o bisel tem a espessura que se pede, e não há um
descodificador de PNG a correr no arranque.

Botão sem cromo: ícone ou palavra solta sobre a superfície, sem fundo, sem bisel,
sem sombra. O componente continua `TextButton` — teclado, foco e
`AccessibilityHandler` de graça — só o desenho achatou. O transporte é ▶/■
desenhado como `Path`, com halo branco no display escuro: a mesma luz dos grãos,
porque o botão que comanda o som tem a energia do som a acontecer. A troca de
*forma* é a segunda pista e a que funciona sem ver cor, pelo motivo que criou a
troca de palavra `PLAY`/`STOP`. A aba ativa leva peso forte, filete de acento e
halo fraco em baixo; o texto carrega o estado, o resto é redundância. Hover e
premido são banhos de alfa; o foco continua o anel `focusRing` (claro) ou
`textOnDark` (no display escuro, onde o transporte mora). Arrastar na onda e na
curva varre a cabeça de leitura pelo caminho do clique, com um endereço pendente
descarregado no tick de 60 Hz; no hex o arrasto é do `HexGrid`.

Alvos de toque: 24 px de diâmetro no mínimo, para o critério 2.5.8 do WCAG 2.2. **O
critério foi fechado em 06/10/2026** sem aumentar o alvo — a célula de byte continua
a 17 px de largura, porque a densidade é a função —: o `ByteDisplay` é focalizável e
tem uma caret de navegação, e a operação completa por teclado acontece sobre o
display inteiro sem passar por nenhum outro controlo.

O desenho é sempre código. Quem opera o controlo é um `juce::Slider`, porque é ele
que dá foco por teclado, ajuste com setas e `AccessibilityHandler`.

## Animação

Tudo o que se mexe interpola por **meia-vida exponencial**, não por mola e não por
linha por quadro:

```
valor += (alvo - valor) * (1 - 2^(-dt / meiaVida))
```

A meia-vida precisa só do valor e do alvo, e o mesmo aspecto aparece a 20 Hz e a
144 Hz. Uma interpolação linear por quadro parece igual a 60 e a 120 Hz e o dobro de
rápida a 240 Hz — e 240 Hz é um monitor que existe. Uma mola precisa de velocidade,
e velocidade precisa de estado: se um quadro atrasar, o valor salta e a mola passa
do alvo.

| O que | Meia-vida | Porquê |
| --- | --- | --- |
| cabeça de reprodução | 30 ms | é a única que se mexe sem a mão do utilizador |
| cabeça de leitura | 60 ms | segue o knob |
| transição de vista | 90 ms | o suficiente para se ler que mudou |
| medidor | 80 ms | sobe depressa e cai devagar, como um medidor de verdade |

A primeira leitura de cada valor é um salto e as seguintes deslizam: sem essa
distinção, carregar um ficheiro faria a cabeça varrer o ecrã inteiro.

O temporizador do editor corre a **60 Hz**, e o delta é medido com
`getMillisecondCounterHiRes` — o contador inteiro tem 15 ms de resolução e um quadro
a 144 Hz dura 7 ms, o que daria um delta alternado de 0 e 15 ms.

**Todo valor animado tem um equivalente textual no ecrã**: a cabeça de leitura tem o
`POS`, a de reprodução tem o `TP`, e o medidor tem o `PK`. O critério 1.4.1 não é só
sobre cor — é sobre ter mais do que uma pista, e uma barra animada sem número ao
lado seria a única forma de ler o que se ouve.

## Adaptação ao JUCE

| Conceito do HTML | Equivalente em JUCE |
| --- | --- |
| Tailwind `bg-[#c2c6c9]` | `juce::Colour {0xffc2c6c9}` |
| Inter | `fonts().sans()` com a fonte embarcada, ou a fonte do sistema |
| Knob com ponteiro | `juce::Slider` `RotaryHorizontalVerticalDrag` com `LookAndFeel` próprio |
| Grelha de bytes | `HexGrid`, um `Component` com `paint()` e `cellRect()` partilhada entre o desenho e o clique |
| Vista de forma de onda e de entropia | `ByteDisplay`, um `Component` com três caminhos de `paint()` e `HexGrid` como filho na vista hex |
| Redução do material | `pe::reduceToColumns`, no núcleo, sem JUCE e com quinze ensaios |
| Curva de entropia por região | a coluna `entropyBits` da mesma redução, com o eixo escrito em `paintEntropy` |
| Medidor de saída | `ByteDisplay::paintOutputLevel` mais a leitura `PK` no rodapé |
| Caret de teclado | `ByteDisplay::caret_`, com `setWantsKeyboardFocus` e `paintFocusRing` |
| Campo de endereço | `juce::TextEditor` com `AddressEditor` a retirar as setas |
| Faixa de arquivo | `juce::Label` + `juce::TextButton` |

**Não há peças de arte.** Não existe ficheiro de imagem em `resources/`: knob e
botão são `juce::Path` desenhados em código, e o texto é sempre desenhado em
código. A regra do harness antigo — não gerar texto, porque texto gerado por
modelo sai com letra errada — mantém-se, mas deixou de ser a única restrição:
agora também não há imagem.

JUCE não embarca fonte por padrão. Inter Display vai embutida como dados binários
(alvo `OpcodaFonts`, 183 KB — só os dois pesos que a interface escreve, gerados
por `tools/vendor_fonts.py`): usar a fonte do sistema (`Segoe UI`) economizaria
isso ao custo de não bater exatamente com o mock. A JetBrains Mono saiu junto com
a regra do número mono. Decisão fechada pelo subset embarcado.

## Conflito com o escopo documentado

A tela do Stitch tem cerca de dezesseis controles: `Size`, `Density`, `Position`,
`Spray`, `Pitch`, `Distance`, `Freq`, `Attack`, `Release`, `Drive`, `Cutoff`,
`Resonance`, `Shape`, `Volume`, `Dry/Wet`, `Voices`.

O MVP documentado tinha **seis** parâmetros (Tabela 8 do artigo); a constituição
v1.1.0 fechou o instrumento em **25** parâmetros em cinco módulos (GRAIN,
ENVELOPE, FILTER, MOD, OUT), com os IDs dos seis originais intactos e os nomes
exibidos em inglês. O número de vozes é fixo em 8.

A interface adota a linguagem visual e a arquitetura de informação, com os 25
parâmetros documentados. `Voices` vira leitura, não controle. O bloco
`STATE FILTER` virou o módulo FILTER (dois biquads em série por voz); `MOD`
cobre o LFO e `OUT` o volume. `Dry/Wet`, `Distance`, `Drive` e `Shape` do Stitch
continuam fora: o que não está na tabela de 25 não entra.

**O botão de reprodução não é um sétimo parâmetro**, e é preciso escrever isto porque
a tentação existe: `PLAY` comanda o motor granular pela mesma porta que as notas
usam, e poderia ser um `AudioParameterFloat` com automating. Não é, porque o OE4
fixa seis parâmetros e o ensaio T4 mede seis. A audição é momentânea e não faz parte
do estado do projeto: `getStateInformation` guarda `sourcePath`, `byteStart` e
`byteEnd`, e nada de reprodução.
