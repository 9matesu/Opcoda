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
| `accentSoft` | `#ffc83b` | amarelo do cabeçalho |
| `accentWarn` | `#ffb833` | amarelo de atenção |
| `ok` | `#4caf50` | estado pronto, "STEREO GRAINS" |
| `okBright` | `#82d64a` | botão de ativação ligado |
| `error` | `#e05373` | estado de recusa |
| `pink` | `#ea7070` | série secundária do espectro |

O laranja `#ff9a00` sobre `#c2c6c9` dá contraste de 2,4:1, e **não** atinge 4,5:1.
Onde o laranja carrega texto, ele vai sobre preto (`#0e1013` ou `#1b1e22`), o que dá
10,8:1. Regra: laranja é preenchimento e indicador, nunca portador de texto pequeno
sobre superfície clara.

## Tipografia

Duas famílias, escala densa de device.

| Papel | Família | Tamanho | Peso |
| --- | --- | --- | --- |
| Título do device | Inter | 12 px | 700, `tracking-tight` |
| Subtítulo | Inter | 10 px | 400 |
| Rótulo de parâmetro | Inter | 10 px | 500 |
| Metadado numérico | JetBrains Mono | 9–10 px | 500–600 |
| Aba de seção | JetBrains Mono | 9 px | 700, maiúscula |
| Estado do display | Inter | 9 px | 500, maiúscula |

Regra: **todo número é JetBrains Mono**, alinhado à direita. Isso é o que faz a
leitura de valor parecer instrumento e não página web.

## Layout

Três faixas, de cima para baixo, com 1 px de `#52575c` entre elas.

### 1. Cabeçalho

Botão de ativação (círculo com fenda vertical, verde `#82d64a` quando ligado),
nome do device, e à direita a faixa de arquivo: botão `LOAD`, nome em mono,
`[64-bit PE]`, tamanho em chip, e a dica `DRAG & DROP BINARY`.

### 2. Display

Fundo `#0e1013`, com 1 px de `#252b31` na moldura. A faixa de display mede entre
200 e 320 px, metade da altura da janela, e a janela mínima subiu de 560×380 para
560×420 porque uma grelha de bytes precisa de seis linhas de 24 px para mostrar
o endianço de um ficheiro inteiro.

De cima para baixo:

- **Grelha de bytes**: endereço de 8 dígitos hexadecimais à esquerda, dezasseis
  bytes por linha em mono 11 px, e a coluna ASCII à direita. Cabeçalho de colunas
  de 14 px com `00` a `0F`. Linhas de 24 px, com um filete de 1 px entre elas.
  A viewport é ancorada num múltiplo de 16, para os endereços caírem em
  round numbers como num hex dump.
  - A **região ativa** leva fundo laranja a 22 % **e** um filete de 1 px em cima
    e em baixo da linha: o fundo diz a quem vê cor, o filete diz a quem não vê.
  - A **cabeça de leitura** é uma barra branca de 2 px na margem esquerda da
    célula, com fundo claro a 16 % por baixo do texto.
  - Os **bytes nulos** saem num tom a parte, como num hex dump, para se ver o
    preenchimento de uma seção sem ler as duas colunas.
  - Os **limites de seção** aparecem no gutter, na linha onde a seção começa, com
    uma barra de 2 px a toda a altura da linha e o nome da seção à direita. Substitui
    o mapa de seções de 18 px.
  - Abaixo de 520 px de largura a coluna **ASCII é omitida**. Perder colunas de
    hexadecimal partiria os endereços ao meio, e um endereço ao meio é pior do que
    nenhum.
  - O gutter **cresce com o ficheiro**: um PE acima de 4 GB tem endereços com mais
    de oito dígitos, e o número não é cortado a meio.
- **Linha de estado**, 14 px: `PRONTO / ficheiro` ou `RECUSADO / E_CODIGO`, com
  LED à esquerda. À direita, o campo hexadecimal do endereço da região e o botão
  `ALINHAR`, que substitui a tecla `Enter` que o seletor removido usava para o
  alinhamento de seção.
- **Rodapé de telemetria**, 16 px: entropia em bits por byte na janela da cabeça
  de leitura, `POS` com o endereço da cabeça, `REG` com o intervalo da região, `PK`,
  taxa e `VOICES`. Tudo em `textOnDark`, nunca nos tokens do chassi.

O display deixou de ter a barra de 30 px do seletor, o mapa de seções e a curva
de entropia. A faixa de 30 px era um mapa do ficheiro inteiro com um cursor, e um
ficheiro de 12 MB não cabe em grelha: o que a grelha dá em troca é o byte.

### 3. Painel de parâmetros

Abas verticais de modo à esquerda e, à direita, módulos agrupados em caixas
`panelBg` com borda 1 px `chassisBorder`.

Cada módulo tem título em maiúscula, e dentro dele os controles com rótulo
acima e valor em `valBox` abaixo.

## Controles

Knob circular com ponteiro laranja e arco de valor, 1 px de arco de fundo
`chassisBorder`. O corpo vem do asset harness como peça física e **não é rodado**:
é uma fotografia top-down com a luz de estúdio assente, e rodar o PNG faria o
brilho andar com o controle. Rótulo em Inter 10 px acima, valor em `valBox`
abaixo, com fonte mono.

Alvos de toque: 24 px de diâmetro no mínimo, para o critério 2.5.8 do WCAG 2.2.
A célula da grelha de bytes é a exceção, e está declarada em
`docs/10-acessibilidade-w3c.md`: tem 24 px de altura e cerca de 17 de largura, e a
densidade é a função.

O PNG é sempre só o desenho. Quem opera o controle é um `juce::Slider`, porque é
ele que dá foco por teclado, ajuste com setas e `AccessibilityHandler`.

## Adaptação ao JUCE

| Conceito do HTML | Equivalente em JUCE |
| --- | --- |
| Tailwind `bg-[#c2c6c9]` | `juce::Colour {0xffc2c6c9}` |
| Inter | `juce::Font {16.0f}` com a fonte do sistema, ou `Inter` embarcada |
| JetBrains Mono | `juce::Font` monoespaçada, ou `JetBrains Mono` embarcada |
| Knob com ponteiro | `juce::Slider` `RotaryHorizontalVerticalDrag` com `LookAndFeel` próprio |
| Grelha de bytes | `juce::Component` com `paint()` e `cellRect()` partilhada entre o desenho e o clique |
| Campo de endereço | `juce::TextEditor` com `AddressEditor` a retirar as setas |
| Faixa de arquivo | `juce::Label` + `juce::TextButton` |

As peças físicas vêm de `I:\TG_I\opcoda-asset-harness` e são embutidas com
`juce_add_binary_data`. O harness **não gera texto**: rótulos, valores e escalas
são sempre desenhados em código, porque texto gerado por modelo sai com letra
errada e não há como corrigir sem regenerar.

JUCE não embarca fonte por padrão. Embarcar Inter e JetBrains Mono adiciona cerca
de 400 KB ao binário; usar a fonte do sistema (`Segoe UI`) e a monoespaçada do
sistema (`Consolas`) economiza isso ao custo de não bater exatamente com o mock.
Decisão pendente, com padrão sendo a fonte do sistema.

## Conflito com o escopo documentado

A tela do Stitch tem cerca de dezesseis controles: `Size`, `Density`, `Position`,
`Spray`, `Pitch`, `Distance`, `Freq`, `Attack`, `Release`, `Drive`, `Cutoff`,
`Resonance`, `Shape`, `Volume`, `Dry/Wet`, `Voices`.

O MVP documentado tem **seis** parâmetros (Tabela 8 do artigo) e o ensaio T4
verifica "mover os 6 parâmetros". O número de vozes é fixo em 8 no MVP.

A interface adota a linguagem visual e a arquitetura de informação, com os seis
parâmetros documentados. `Voices` vira leitura, não controle. O bloco
`STATE FILTER` corresponde ao biquad de coloração que o artigo descreve e que não
está na Tabela 8: a decisão de torná-lo sétimo parâmetro ou estágio fixo continua
aberta, e muda o texto publicado.
