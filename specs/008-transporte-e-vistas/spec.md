# 008 · Transporte de audição e três leituras do material

**Feature Branch**: `008-transporte-e-vistas`
**Criada**: 2026-10-05
**Status**: Draft
**Objetivos atendidos**: OE7 (forma de onda navegável e curva de entropia, hoje
não cumpridos), OE5 (parcial: a curva passa a ser mostrada)
**Ensaios**: portões B, C e E; portão D por extensão, porque a redução lê
intervalos de bytes e tem de recusar os limites da mesma forma que o parser

**Alvo**: F008, a interface e a acessibilidade

## Contexto

Esta feature fecha uma lacuna que já está escrita no objetivo e não no código.

`docs/04-objetivos.md:23` pede, no OE7, três coisas na interface: "mapa de
seções do PE, **forma de onda navegável** e controles automatizáveis no DAW.
**Curva de entropia em versão enxuta**". O commit `e99d089` trocou o seletor de
30 px por uma grelha de bytes e, no mesmo movimento, apagou a curva de entropia e
o mapa de seções como peças separadas. A troca foi defensável na altura — a barra
de 30 px mostrava a posição relativa de um byte num ficheiro de 12 MB, que não é
informação — mas o resultado é que hoje o display mostra bytes, e **nenhuma das
duas leituras que o OE7 nomeia existe**.

A segunda metade da lacuna é de audição. O Opcoda é um instrumento: soa porque
alguém carregou o host e segurou uma nota. Não há como ouvir o material sem
teclado MIDI, o que torna impossível responder à pergunta mais b
sintetizador de fonte — *que som é este?*. O botão de transporte desta feature
existe para isso.

Esta feature entrega assim um display com três leituras — o som, os bytes, e o
quanto o material é aleatório — e um botão que percorre a região inteira sem
nota nenhuma.

O que também muda, e é deliberado: as peças físicas do asset harness saem do
binário e passam a ser desenhadas em código, e os rótulos que repetem informação
já presente noutro sítio saem do ecrã. As duas decisões têm a mesma origem do
pedido original, que é o de que a interface seja legível e não decorativa.

## Cenários

### História 1 — Auditar a região inteira sem nota MIDI (P1)

Como músico, quero carregar num botão e ouvir a região inteira percorrer-se de
início a fim, para saber o que estou a sintetizar sem precisar de um teclado MIDI
nem de automação.

**Por que P1**: é o objetivo de audição de um sintetizador de fonte, e hoje não
existe forma de o atingir. Uma pessoa que abre o Opcoda pela primeira vez não
ouve nada, e não tem como descobrir o que o plugin faz. Nenhuma outra entrega
desta feature é tão bloqueante.

**Teste independente**: `Transport.FullRegionIsTraversedWithinTheCap` percorre a
região num núcleo sem JUCE e exige que a posição completa exatamente uma volta no
tempo anunciado, e `EndToEnd.TransportPlayProducesAudioWithoutMidi` arranca o
material, toca o transporte sem uma única nota e exige pico audível.

**Cenários de aceitação**:

1. **Dado** um binário carregado, **quando** aciono o botão de reprodução,
   **então** o material soa em até 300 ms, sem nota MIDI e com o host em reprodução
2. **Dado** uma região de 4 KB, **quando** o transporte a percorre, **então** a
   duração é `bytes / taxaAmostragem`, porque 4 KB cabem em menos de um segundo e
   esse é o tempo natural
3. **Dado** uma região de 12 MB, **quando** o transporte a percorre, **então** a
   duração é 30 s, e não os 262 s que 12 MB a 48 kHz teriam em playback direto
4. **Dado** uma região de 64 bytes, o menor tamanho que o editor aceita, **quando**
   o transporte a percorre, **então** a duração tem um piso de 0,1 s, porque
   1,3 ms não chegam para um grão
5. **Dado** o transporte a tocar, **quando** movo o knob POSITION, **então** a
   leitura reaninja imediatamente e o knob continua vivo e automatizável
6. **Dado** o transporte a tocar, **quando** a região é movida com o campo de
   endereço, **então** a duração é recalculada para a nova região sem reiniciar
7. **Dado** nenhum material carregado, **quando** a interface é desenhada, **então**
   o botão de reprodução está desativado e nomeado, e não é um controlo morto
8. **Dado** o transporte em reprodução, **quando** o host pergunta o estado do
   projeto, **então** o estado **não** contém o transporte: audição é momentâneo,
   não é parte do projeto
9. **Dado** o transporte a tocar, **quando** o callback de áudio corre, **então**
   o contador de alocações do guard é zero

### História 2 — Ver a forma de onda navegável (P1)

Como usuário, quero ver a forma de onda da região selecionada e poder navegar
nela, porque é a única vista que responde "que som é este" sem somar um segundo.

**Por que P1**: é a expressão literal do OE7, hoje por cumprir. E é a vista que
torna o botão da História 1 legível: uma cabeça de leitura que anda sobre uma
forma de onda mostra o que a reprodução está a fazer, e o mesmo botão em cima de
uma grelha de bytes não mostra nada.

**Teste independente**: `WaveformCache.KnownPatternGivesExactEnvelope` reduz um
padrão de bytes conhecido e exige o mínimo e o máximo exatos por coluna, e
`WaveformCache.PartialLastColumnReadsEveryByteExactlyOnce` exige que uma região
cujo comprimento não divide o número de colunas não salte nem repita byte.

**Cenários de aceitação**:

1. **Dado** uma região selecionada, **quando** a mudo para a vista de forma de
   onda, **então** vejo o envelope mínimo-máximo da região inteira, e não um
   recorte de uma janela
2. **Dado** uma região selecionada, **quando** a amplitude cai a zero numa coluna,
   **então** o envelope colapse a uma linha, porque é assim que um silêncio
   aparece
3. **Dado** uma região grande, **quando** a desenho, **então** o custo depende do
   número de colunas e não do tamanho do ficheiro, porque o display tem 900
   colunas e o ficheiro pode ter 12 milhões de bytes
4. **Dado** uma região de 12 MB, **quando** mudo a região com o campo de endereço,
   **então** a vista continua a desenhar ao ritmo do ecrã, porque a redução é
   amostrada com passo e não byte a byte
5. **Dado** o transporte a tocar, **quando** a vista de forma de onda está ativa,
   **então** a cabeça de reprodução move-se sobre o envelope sem saltos
6. **Dado** a vista de forma de onda ativa, **quando** mudo a largura da janela,
   **então** a vista reflui sem distorcer a forma de onda

### História 3 — Ler a entropia em versão enxuta (P2)

Como pesquisador, quero ver a entropia da região selecionada como uma curva, para
saber que parte do material é repetitiva e que parte é densa, porque a dispersão
do grão é modulada por isso e hoje isso é invisível.

**Por que P2**: é a segunda cláusula do OE7 não cumprida, e a curva foi removida
em `e99d089`. É P2 e não P1 porque responde a uma pergunta mais rara do que a
História 1, e porque uma curva sem rótulo e sem padrão é decoração.

**Teste independente**: `WaveformCache.EntropyOfConstantBytesIsZero` e
`WaveformCache.EntropyOfAllByteValuesIsEight` fixam as duas pontas da escala, e a
inspeção do eixo 0/4/8 e da marca não cromática é feita por captura, como o resto
da interface.

**Cenários de aceitação**:

1. **Dado** uma região selecionada, **quando** mudo para a vista de entropia,
   **então** vejo a curva de 0 a 8 bits por byte da região, com o eixo legível
2. **Dado** uma região de bytes constantes, **quando** a curva a mostra, **então**
   ela está colada a 0 bits por byte, que é o resultado correto e não um defeito
3. **Dado** a região ativa, **quando** a curva a marca, **então** a marca não é só
   cor, porque uma faixa laranja não diz onde acaba a região a quem não distingue
   as cores
4. **Dado** a curva de entropia a ser mostrada, **quando** mudo o tamanho da
   janela, **então** a curva reflui e mantém a escala legível

### História 4 — Operar o Opcoda só com teclado (P2)

Como alguém que não usa rato, quero fazer tudo o que faço com o rato no teclado,
porque uma grelha de células pintadas não tem como ser acedida por quem navega
por teclado ou por leitor de ecrã.

**Por que P2**: é o portão E, "operação completa por teclado", e é a remoção de
uma exceção que a documentação hoje declara. A exceção está escrita em
`docs/10-acessibilidade-w3c.md:42-71` e em `hex_grid.h:29-39`: a grelha é de rato
porque as células não são componentes. Isso é verdade e é a razão de ser do
desenho atual, e também é a razão de a exceção existir.

**Teste independente**: cada tecla do mapa tem um caso que verifica a posição da
cabeça de leitura depois e antes de a carregar, nos dois sentidos e nos dois
extremos da região, e a captura mostra o cursor de foco.

**Cenários de aceitação**:

1. **Dado** o editor com foco no display, **quando** pressiono `Espaço`, **então**
   o transporte começa ou pára, e o botão diz `PLAY` ou `STOP`
2. **Dado** o display com foco, **quando** pressiono `←` e `→`, **então** a cabeça
   de leitura move-se um byte
3. **Dado** o display com foco, **quando** pressiono `Shift` com `←` ou `→`,
   **então** a cabeça move-se dezasseis bytes, que é uma linha da grelha
4. **Dado** o display com foco, **quando** pressiono `↑` e `↓`, **então** a cabeça
   move-se uma linha
5. **Dado** o display com foco, **quando** pressiono `Página cima` e `Página baixo`,
   **então** a cabeça move-se uma página visível
6. **Dado** o display com foco, **quando** pressiono `Início` e `Fim`, **então** a
   cabeça vai para o início e para o fim utilizável da região
7. **Dado** o display com foco, **quando** pressiono `Enter`, **então** o byte sob
   a cabeça é ativado, como o clique na grelha faz
8. **Dado** o editor, **quando** pressiono `1`, `2` ou `3`, **então** a vista passa
   a forma de onda, ao hex ou à entropia
9. **Dado** o editor, **quando** pressiono `A`, **então** a região alterna entre a
   exata e a secção PE mais próxima
10. **Dado** o editor, **quando** pressiono `L`, **então** o diálogo de arquivo abre
11. **Dado** o campo de endereço com o foco do teclado, **quando** pressiono as
    setas, **então** continuam a mover o endereço e não chegam ao display, porque
    o campo consome-as de propósito
12. **Dado** o display com o foco do teclado, **quando** o foco está nele, **então**
    há um cursor visível, porque o critério 2.4.7 pede foco visível
13. **Dado** a interface, **quando** percorro tudo com `Tab`, **então** chego ao
    display, e o display não é um beco sem saída

### História 5 — Peças desenhadas em código (P2)

Como mantenedor, quero que o binário não embuta fotografias de controlos, porque
uma imagem de 186×192 píxeis para desenhar um disco de 62 píxeis não é uma peça
de interface, é um resíduo do asset harness dentro do artefacto.

**Por que P2**: não é uma falha de interface, é peso morto e uma dependência
externa. `resources/assets/` traz duas peças de um harness que vive em
`I:\TG_I\opcoda-asset-harness`, e o plugin não pode compilar sem elas. O desenho
vetorial já existe como caminho de recurso em `look_and_feel.h:164-177`, escrito
como rede para o caso de a peça faltar; esta feature promove-o a caminho único.

**Teste independente**: o alvo `OpcodaAssets` desaparece do build, `resources/`
fica sem PNG, e a captura mostra knob e botão desenhados com os mesmos estados.

**Cenários de aceitação**:

1. **Dado** o build, **quando** configuro o projeto, **então** não há ficheiro
   PNG na lista de recursos e não há alvo de dados embebidos
2. **Dado** o knob, **quando** o desenho, **então** corpo, serrilhado, face e
   marcador são desenhados em código e escalam com a janela
3. **Dado** o botão, **quando** o estado muda, **então** repouso, sobreposto,
   premido e ligado são distinguíveis, e ligado não é só cor
4. **Dado** o knob ou o botão, **quando** tem o foco do teclado, **então** o anel
   de foco continua desenhado
5. **Dado** o binário, **quando** arranca, **então** não descodifica nenhuma
   imagem

### História 6 — Display animado e sem rótulos repetidos (P3)

Como usuário, quero que o display se mova e não me repita, porque um cursor que
salta de 50 em 50 milissegundos não parece vivo e quatro rótulos que dizem o que
já está escrito noutro sítio são ruído.

**Por que P3**: é o acabamento da interface, e é o que faz a leitura parecer
instrumento em vez de formulário. P3 porque nenhuma das duas coisas acrescenta
capacidade.

**Teste independente**: a inspeção visual por captura, que é a forma como os
defeitos de layout deste projeto apareceram — quatro deles só foram vistos por
captura, registados em `docs/16-arquitetura-implementacao.md:252-293`.

**Cenários de aceitação**:

1. **Dado** o display, **quando** a cabeça de leitura salta de endereço, **então**
   desliza em vez de saltar, com meia-vida curta
2. **Dado** o transporte a tocar, **quando** a cabeça de reprodução anda, **então**
   o movimento é contínuo a 60 Hz
3. **Dado** a vista a mudar, **quando** a transição acontece, **então** há
   esbatido cruzado, e não um corte
4. **Dado** a saída a tocar, **quando** o pico muda, **então** um medidor de pico
   com RMS responde, com retenção e queda, e não tremula
5. **Dado** a interface, **quando** a leio, **então** o subtítulo, o contador de
   vozes do cabeçalho, o título do módulo e a dica de arrasto já não estão, porque
   cada um deles repetia informação escrita noutro sítio
6. **Dado** o motor a tocar, **quando** a animação está a correr, **então** nenhum
   valor animado é a única forma de o ler, porque cor só não passa o critério 1.4.1

## Fora de escopo

- Sétimo parâmetro automatizável, como duração de reprodução: o OE4 fixa seis
  parâmetros e o ensaio T4 mede seis. O botão de reprodução não é parâmetro.
- Produzir som com o host parado. Sem `processBlock` não há motor, e isto não é
  uma limitação do Opcoda: o botão de reprodução comanda o motor, e o motor só
  existe enquanto o host chama o callback. O que a interface faz é **dizer** isso:
  quando `getPlayHead()` é nulo ou `canPlayNow()` é falso, o botão fica
  desativado e o estado mostra que o transporte do host está parado. Um botão que
  aceita o toque e não produz nada é pior do que um botão que recusa o toque.
- Gravação do transporte pelo host, ou exportação do áudio renderizado para
  ficheiro.
- Path direto de reprodução de amostras, fora do motor granular. O transporte
  comanda o motor que já existe; uma segunda via de áudio seria uma segunda
  superfície de testes em tempo real sem ganho.
- Mapa de secções do PE como peça separada de 18 px. Os limites de secção
  continuam visíveis, agora no gutter da grelha e como marcas na forma de onda.
- Embutir Inter e JetBrains Mono. A decisão pendente de `design/DESIGN-SYSTEM.md:177-180`
  mantém-se: fonte do sistema.
- Curva de entropia por secção, com realce ao passar o rato. A versão enxuta do
  OE7 é a da região.

## Requisitos funcionais

### Transporte

- **FR-001**: `src/opcoda_core` MUST fornecer `rt::Transport`, sem incluir
  cabeçalho do JUCE e sem alocar, travar ou fazer I/O
- **FR-002**: a duração de uma passagem MUST ser
  `clamp(bytesDaRegião / taxaAmostragem, 0,1 s, 30 s)`, e o piso MUST ser igual ao
  comprimento do maior grão que o motor produz, verificado por `static_assert`
- **FR-003**: `advance(n)` MUST mover a posição exatamente `passosPorSegundo × n`
  e embrulhar em zero ao atingir o fim da região
- **FR-004**: a posição devolvida MUST NEVER alcançar 1,0, porque 1,0 é silêncio
- **FR-005**: `play`, `stop` e `seekToFraction` MUST ser chamáveis de qualquer
  thread sem leitura rasgada nem fração fora de `[0, 1)`
- **FR-006**: o motor MUST receber o gate aberto enquanto o transporte toca, sem
  nota MIDI
- **FR-007**: o valor de POSITION MUST servir de âncora de busca, escrito pelo
  editor apenas quando o usuário o move
- **FR-008**: o estado do projeto MUST NOT conter o transporte, e o ensaio T4
  MUST continuar a medir seis parâmetros
- **FR-009**: sem material ou com região vazia o botão MUST estar desativado,
  nomeado, e a linha de estado MUST dizer porquê, e não ser um controlo morto
- **FR-010**: `setRegionLength(0)` MUST NOT produzir NaN nem fração fora de `[0, 1)`
- **FR-011**: com o transporte do host parado o botão MUST ficar desativado e a
  linha de estado MUST dizer porquê, por `getPlayHead()` e não por adivinhação

### Leituras do material

- **FR-012**: o display MUST ter três modos, um por tecla `1`, `2` e `3`
- **FR-013**: a geometria da grelha hex MUST NOT mudar, porque foi verificada por
  captura em três tamanhos de janela
- **FR-014**: a vista de forma de onda MUST desenhar o envelope mínimo-máximo e um
  núcleo de RMS por coluna
- **FR-015**: a vista de entropia MUST desenhar a curva de 0 a 8 bits por byte com
  eixo legível e marca não cromática na região
- **FR-016**: a redução MUST ser memorizada por `(início, fim, colunas)` e
  reconstruída uma vez por mudança de chave
- **FR-017**: a redução MUST amostrar com passo quando os bytes por coluna
  ultrapassam o tecto, de modo que o custo não dependa do tamanho do ficheiro
- **FR-018**: uma região cujo comprimento não divide o número de colunas MUST NOT
  saltar nenhum byte nem ler nenhum byte duas vezes
- **FR-019**: as três vistas MUST mostrar a extensão da região, a cabeça de leitura
  e a cabeça de reprodução
- **FR-020**: a conversão byte→amostra da vista MUST ser a mesma de
  `pe::toSamples`, para que o desenho e o som não divirjam

### Teclado

- **FR-021**: o display MUST ser focalizável e ter cursor visível
- **FR-022**: o mapa de teclas MUST estar escrito em `docs/10-acessibilidade-w3c.md`
- **FR-023**: as setas MUST continuar a ser consumidas pelo campo de endereço
  quando ele tem o foco, e MUST NOT ser interpretadas pelo display nesse caso
- **FR-024**: `Enter` MUST ativar o byte sob a cabeça e `A` MUST alternar o
  alinhamento à secção
- **FR-025**: a exceção ao critério 2.5.8 MUST ser removida de `docs/10` e de
  `hex_grid.h`

### Peças vetoriais

- **FR-026**: o build MUST NOT embeter PNG e MUST NOT ter alvo de dados binários
- **FR-027**: knob e botão MUST ser desenhados com `juce::Path` em código
- **FR-028**: o estado ligado do botão MUST ser distinguível sem cor
- **FR-029**: os anéis de foco MUST continuar desenhados

### Animação e limpeza

- **FR-030**: a animação MUST correr só na thread de interface, a 60 Hz, e MUST
  NOT entrar no caminho de áudio
- **FR-031**: a interpolação MUST ser por meia-vida exponencial, independente da
  taxa de quadros, e MUST NOT alocar dentro de `paint()`
- **FR-032**: todo o valor animado MUST ter um equivalente textual no ecrã
- **FR-033**: o cabeçalho MUST NOT repetir voz, arrasto nem subtítulo, e o painel
  MUST NOT repetir o título de um módulo que é o único

## Entidades

- **Transporte**: posição de leitura dentro da região, duração de uma passagem e
  âncora de busca. Vive no núcleo; é possuído pela thread de áudio e iniciado pela
  thread de interface.
- **Coluna de redução**: mínimo, máximo, RMS e entropia de um intervalo de bytes,
  correspondente a um píxel do display.
- **Vista**: modo de leitura do display, com a geometria e o desenho próprios.
- **Peça**: nome acessível, rótulo curto e desenho de um controlo, sem imagem.

## Critérios de sucesso

| Critério | Verificado por |
| --- | --- |
| A região inteira é percorrida no tempo anunciado, uma volta | `Transport.*` |
| Tocar sem nota MIDI produz pico audível | `EndToEnd.TransportPlayProducesAudioWithoutMidi` |
| A posição de transporte nunca chega a 1,0 | `Transport.PositionNeverReachesSilence` |
| Sem alocação no callback durante a reprodução | `opcoda_alloc_guard_test` |
| A forma de onda tem envelope exato | `WaveformCache.KnownPatternGivesExactEnvelope` |
| Nenhum byte salta ou repete na redução | `WaveformCache.PartialLastColumnReadsEveryByteExactlyOnce` |
| A entropia tem 0 e 8 como extremos reais | `WaveformCache.Entropy*` |
| Cada tecla do mapa move a cabeça como declarado | `ByteDisplay.*Keyboard*` |
| O binário não embute PNG | ausência do alvo e de `resources/assets/*.png` |
| Contraste ≥ 4,5:1 no texto | `palette` e captura |
| Testes verdes com os casos novos | `ctest --preset dev` |

## Premissas

- **O host está em reprodução.** O botão de reprodução comanda o motor, e o motor
  só existe quando o host chama `processBlock`. Fora disso não há som, e a
  interface não vai fingir que há.
- **A região pode ter qualquer tamanho, de 64 bytes a um ficheiro inteiro.** Daí o
  piso e o tecto da duração.
- **O display tem largura variável**, entre 480 e 4096 píxeis, e o número de
  colunas muda com ela.
- **O temporizador da interface passa a 60 Hz.** A 20 Hz cada quadro dura 50 ms, e
  uma meia-vida de 60 ms dá menos de dois quadros por meia-vida: isso é degrau, e
  não suavização.
- **`docs/18-processo-sdd.md:57` está errado hoje.** Diz que o display de
  entropia está pronto; `e99d089` apagou-o. Esta feature corrige o código e a
  linha.

## Decisões e armadilhas

**A posição 1,0 é silêncio, e isso ata o lado de cima do transporte.**
`granular_engine.cpp:223` desliga a voz quando `index + 1 >= sourceCount`, e
`pe::positionForByte` já é meio aberto em cima por causa disso: o endereço mais
alto que produz som é `end − 2` (`byte_to_position.h:19-24`). Um transporte que
embrulha em 1,0 exato leva a última coluna de cada volta a audibilidade zero, e
isso ouve-se como um clique a cada 30 s. A posição devolvida é recortada a
`[0, 1 − 2/bytes)`. Com 64 bytes o epsilon é 0,031, folgadamente representável.

**A duração natural é 262 s, e o piso não é vaidade.** Doze megabytes a 48 kHz dão
262 s de reprodução direta. A região de 64 bytes, o menor tamanho que
`moveRegionTo` aceita, dá 1,3 ms: menos do que um grão, e um botão que mal chega a
piscar. O clamp nos dois extremos é a mesma linha.

**O piso é o comprimento do maior grão, e não um número redondo.** Abaixo de
`GranularEngine::kMaxGrainMs` uma passagem nem dá para completar um grão: produz um
clique e não uma audição. Esse comprimento é 100 ms, e por isso o piso é 0,1 s — e
não os 0,25 s que esta spec propunha inicialmente. Com 250 ms, uma região de 4 KB
que dura 85 ms de forma natural era esticada quase três vezes, o que anulava o
critério 2 da História 1. Um `static_assert` em `transport.cpp` prende os dois
números, porque dois valores que precisam de concordar divergem no primeiro que
alguém mexer num deles.

**O tecto de amostras por coluna tem de ser maior do que 256, e não por
afinidade.** A entropia de uma amostra de N bytes uniformes de 256 valores aproxima-se
de log2(N), e não de 8. Com 64 amostras por coluna, log2(64) = 6, e a curva nunca
leria acima de 6 num material que vale 7 — um eixo de 0 a 8 que mostra sempre o
material como repetitivo é pior do que não ter curva. Com 512, log2(512) = 9 já
passa do topo da escala, e o limite da medição desaparece.

**O trabalho por quadro é a armadilha que mata esta feature.** A `HexGrid` pinta
a partir de `sourceBytes_` a 20 Hz, o que é aceitável para 24 linhas de 16
bytes. Uma forma de onda sobre a mesma região tem de reduzir a região a cerca de
900 colunas, e reduzir 12 milhões de bytes por quadro enquanto o usuário arrasta a
região são 12 milhões de operações por quadro. Por isso a redução é amostrada com
passo quando os bytes por coluna ultrapassam um tecto, e para um envelope de
mínimo-máximo isso é visualmente idêntico. Sem esta linha a vista não é lenta: não
funciona.

**A redução é de duas naturezas na mesma passagem.** Mínimo, máximo e RMS são
aritmética; a entropia é um histograma de 256 caixas finalizado por coluna. Uma
passagem só, com memória extra constante. Guardar 256 contadores por coluna daria
quase um megabyte para 900 colunas, que é o que uma implementação ingénua faz e o
que a torna visivelmente lenta.

**A fração de POSITION é da região, não do ficheiro.** O motor lê
`read = position × (sourceCount − 1)` e `sourceCount` é o comprimento da região,
porque `pe::toSamples` converte um byte numa amostra. Por isso o transporte
trabalha em espaço de fração e não em endereço absoluto, e a taxa é
`bytesDaRegião / (taxaAmostragem × duração)`. Escrever o endereço na telemetria é
questão do editor, com `pe::byteForPosition`, que já existe e já tem 29 testes.

**A âncora precisa de guarda, ou briga com a própria cabeça.** O editor escreve
`seekToFraction` a cada quadro para que o knob sirva de busca. Sem comparar com o
último valor escrito, o transporte ancorava em 0,5 a cada 16 ms e a cabeça nunca
passava do meio. Escreve-se só quando `|novo − último| > 1e-4`.

**O transporte faz-se `play` e `stop` de qualquer thread, e o `advance` só da thread
de áudio.** É a mesma disciplina da `SpscRing` e da `Telemetry`: atómicos para o
que atravessa threads, variáveis simples para o que é de uma thread só. O
`advance` devolve a posição do bloco, e a thread de áudio usa a mesma booleana
local para a posição e para o gate, porque cabeça e gate em desacordo é som sem
posição ou posição sem som.

**As setas nunca chegam ao display com o campo de endereço focado, e isso é
proposital.** `AddressEditor::keyPressed` (`byte_address_field.h:66-75`) retira as
quatro teclas de navegação ao `juce::TextEditor` de propósito, porque um campo que
responde a setas de forma diferente conforme o cursor está no fim do texto ou não
é um defeito que só aparece quando o campo já tem conteúdo. O display só vê as
setas quando tem o foco, e a divisão é a que o JUCE já impõe: o display
focado trata a navegação e o editor trata o transporte. Uma única função de
teclas no editor seria mais curta e errada.

**`Enter` muda de dono.** O botão `ALINHAR` foi criado para substituir o `Enter`
que o seletor removido usava (`plugin_editor.cpp:164-166`). Com o display
focalizável, `Enter` passa a ativar o byte sob a cabeça, que é o que o clique faz,
e o alinhamento passa para `A`. A dica do botão muda no mesmo commit, ou fica a
mentir.

**Retirar os PNG tira o único `include` explícito de `juce_graphics`.**
`assets.h:3-6` avisa exatamente para isto, e o aviso vai para `look_and_feel.h`
quando o ficheiro vai. O módulo chega transitivamente por
`juce_audio_processors`; a falha continuaria a ser do mesmo tipo e a coincidir
com o mesmo TU.

**A curva de entropia precisa de eixo e de marca não cromática.** A versão
removida em `e99d089` já tinha as duas coisas: eixo e uma barra preta de 2 px
dentro do laranja, porque a barra é o que fecha o 1.4.1 quando a cor não
distingue. O `constitution.md:73` exige rótulo ou padrão além da cor, e uma curva
laranja sobre fundo escuro sem marca não passa.

**O botão ligado não pode ser só cor.** O laranja `#ff9a00` sobre o chassis
`#c2c6c9` dá 2,4:1 (`design/DESIGN-SYSTEM.md:65`), que é menos do que o 4,5:1 do
critério 1.4.3. O estado ligado ganha uma barra de 1 px no lado esquerdo, e é por
isso que o botão de reprodução pode ser lido sem ver a cor.

**A 60 Hz, o texto continua a não vocalizar.** `setIfChanged`
(`plugin_editor.cpp:80-85`) só escreve quando o texto muda, porque trocar o texto
de um `Label` dispara um evento de acessibilidade e a 20 Hz isso já é ruído. A 60 Hz
seria pior sem essa guarda, e a guarda está. O custo da subida é um repaint do
display a mais por segundo, e o desenho custa O(colunas), não O(bytes), porque a
redução está memorizada.

**A região é a unidade, e o display mostra a região.** A curva removida chegou a
mostrar o ficheiro inteiro ao lado de uma região estreita, o que mente sobre o que
está a soar. As três vistas desenham a região, e a extensão dela é visível nas três.

**Uma nota sobre o que isto não vai consertar.** O defeito conhecido dos seis
parâmetros que arrancam fora dos valores por omissão
(`docs/16-arquitetura-implementacao.md:382-440`) é do estado de sessão do
Standalone em `%APPDATA%\Opcoda\Opcoda.settings` e não do display. Nada nesta
feature o toca, e vale a pena escrevê-lo agora para ninguém confundir as duas
coisas quando o knob ficar a um valor e a vista parecer errada.
