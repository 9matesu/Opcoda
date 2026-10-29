# 16. Arquitetura de implementação

Este documento registra as decisões de implementação que não estão em
`06-metodologia-arquitetura.md`, que descreve a arquitetura proposta na fase de
escrita do TG. Aqui está o que o código faz de fato.

O mapa com a linha exata de cada coisa está em `docs/atlas/`, com os
diagramas de contexto, contentores e fluxo de dados, e com as tabelas de
entradas e saídas. Onde este documento e o código divergem, o código manda.

## Divisão em camadas

```
src/opcoda_core/    C++20 puro, sem JUCE
  pe/               parser PE com verificação de limites
  entropy/          entropia de Shannon
  dsp/              motor granular, janelas, DC-blocker, limiter
  rt/               fila SPSC, troca de amostra, guard de alocação
src/opcoda_ui/      interface (etapa S4)
src/opcoda_plugin/  AudioProcessor e formatos (etapa S4)
```

O núcleo não inclui cabeçalho do JUCE. Isso não é estilo: é o que permite rodar
sanitizers e testes sobre o parser e o DSP sem o framework no caminho, e o
build falha se alguém incluir algo de fora.

## O caminho de áudio não usa FFT

A entropia é frequência de símbolos, não análise espectral: `shannon_entropy`
conta ocorrências de byte. A FFT existe apenas em `tests/t1_spectral_test.cpp`,
que mede a saída do motor para o ensaio T1. Nenhuma dependência de FFT entra no
plugin.

## Cadeia de sinal

```
bytes -> float normalizado -> motor granular (8 vozes, overlap-add)
      -> DC-blocker R = 0,9983 -> limiter -1 dBFS
```

### DC-blocker

`y[n] = x[n] - x[n-1] + R · y[n-1]`, com R = 0,9983, que corta em 12 Hz a
44,1 kHz.

**Divergência corrigida:** `05-fundamentacao.md`, o artigo, `04-objetivos.md` e
`06-metodologia-arquitetura.md` indicavam R = 0,995 com corte entre 10 e 15 Hz.
Os dois números não são compatíveis: R = 0,995 corta em 35,2 Hz a 44,1 kHz. O
código usa o valor que produz o corte citado, porque o ensaio T1 mede a faixa de
frequência e não o coeficiente, e o texto foi corrigido nos quatro lugares com a
relação R = exp(−2·π·f_c/f_s) explicitada.

### Limiter

O ganho é suavizado assimetricamente, ataque rápido e relaxação lenta. O
ganho sozinho deixa o sinal passar acima do teto na amostra em que o ataque
ainda não convergiu, então há um clamp final. É a troca clássica de limitador sem
lookahead: garante o teto ao custo de alguns dB de distorção no ataque. Sem o
clamp, o limitador não limita, e o teto é exatamente o que T1 mede.

## Agendamento dos grãos

A densidade é um spawn por amostra derivado dos grãos por segundo. O motor
mantém uma fração de fase, de modo que um bloco de 128 e outro de 512 produzem a
mesma densidade, e espalha os grãos ao longo do bloco em vez de nascê-los todos
no início. Cada grão guarda o índice de escrita onde nasceu, porque a sobreposição
precisa começar na amostra certa.

Um grão começa a ler com a janela no índice zero, o que zera a amplitude nas
bordas e evita o clique na sobreposição. O interpolador é linear, com passo
limitado a 0,0625..16 (quatro oitavas para cada lado) para impedir aliasing
audível na afinação máxima de ±48 semitons.

**O grão que nasce no fim morria na primeira amostra.** A posição 1,0 aponta para
o último byte, e a janela precisa de `grainLength` amostras à frente; sem margem,
o grão lia zeros e o instrumento calava-se — era o que acontecia ao clicar no
canto direito do mapa de entropia, e parecia avaria. `startGrain` passou a puxar
o início para trás pelo comprimento do grão, depois de conhecido o `readStep`:
`position = min(position, 1 - min(grainLength * readStep, 1))`. A posição que o
utilizador escolheu perde apenas o que a janela não teria como ler, e o fim do
material deixa de ser um silêncio. Numa região curta com pitch alto o grão não
cabe em sítio nenhum e o tecto caía para zero — todos os grãos no primeiro byte,
knob de posição morto. Passa a haver um segundo limite: o grão nunca ocupa mais
de metade do material, e a janela acompanha o comprimento que sobrou.

**A janela deixou de ser construída no comprimento do grão.** Até 27/10/2026 havia
duas tabelas do tamanho do grão, refeitas em `rebuildWindow` a cada mudança de
tamanho. Com o tecto a 2 s, um grão de 48 kHz pediria 96 mil amostras por tipo, e
uma tabela nova a cada tique do knob no caminho do áudio. Passaram a ser quatro
tabelas de 4096 pontos — Hann, gaussiana, Hamming e Blackman —, preenchidas uma
vez no `prepare`; o grão lê a tabela com passo `(4096−1)/(grainLength−1)`, de modo
que o primeiro índice cai em zero e o último em 4095. A escala antiga,
`4096/grainLength`, deixava a última amostra de um grão de 2 amostras no pico da
Hann, e o corte ouvia-se. `rebuildWindow` passou a ser uma troca de ponteiro
(nenhum `cos` no callback), e cada grão guarda a tabela com que nasceu: mudar de
janela não salta o envelope de um grão de 2 s a meio. `activeWindowLength_`
desapareceu porque o comprimento vive no grão que está a soar.

O spray e' dispersao pura dos graos, independente da entropia local; a entropia so' o modula quando roteada pela matriz: região
repetitiva fica mais ancorada. É o Entropy-to-CV descrito na fundamentação.

## Verificação de limites no parser

Toda leitura passa por `inBounds(offset, length, total)`, que confere a soma em
aritmética de 64 bits para que `offset + tamanho` nunca transborde antes da
comparação. `SizeOfRawData` declarado pelo cabeçalho não é confiável: é
recortado para o que existe no arquivo, e um valor declarado maior que o
disponível retorna `E_OOB`.

`NumberOfSections` acima de 96 é rejeitado em vez de alocar uma tabela do
tamanho que o cabeçalho pedir.

**Bug encontrado pelos testes:** o parser lia `NumberOfSections` e
`SizeOfOptionalHeader` em `eLfanew + 2`, que pertence à assinatura `PE\0\0`, e
não no COFF header, que começa em `eLfanew + 4`. O resultado era `E_BAD_PE` em
todo arquivo válido. O erro estava no código e não na especificação do formato,
e só apareceu quando um PE mínimo válido foi construído em memória pelo
`tests/pe_builder.h`.

## Concorrência

A fila SPSC usa um índice de escrita e um de leitura, cada um em uma linha de
cache separada, com `memory_order_release` na publicação e `acquire` na leitura.
O par é o mínimo necessário para o contrato entre interface e áudio sem barreira.

A troca de material entre interface e áudio usa **duas filas em sentidos
opostos**, e não uma. A interface publica o ponteiro novo e guarda o buffer em
`owned_`. A thread de áudio troca o ponteiro ativo e devolve o antigo pela fila
de retorno. Só a interface remove o buffer de `owned_`, e só depois que a thread
de áudio confirmou que terminou de usá-lo.

A fila carrega **ponteiro cru, nunca `shared_ptr`**, por duas razões que
apontam para a mesma escolha. `SpscRing` exige tipo trivial para copiar o slot
sem contagem de referência, e decrementar um contador na thread de áudio pode
rodar o `operator delete` ali dentro, o que é violação de tempo real.

Uma fila só seria mais curta e incorreta: sem a volta, a interface não sabe
quando é seguro liberar, e liberar cedo é use-after-free. O teste
`OldBufferIsOnlyReleasedAfterEngineMovedOn` cobre a invariante.

`SampleSwap`, que fazia double-buffer com flip atômico para o mesmo problema,
foi removido na S2 junto com seus cinco testes. A dupla fila resolve com menos
peças reaproveitando o `SpscRing` que já estava testado, e `sample_exchange_test.cpp`
cobre a invariante com duas threads de verdade.

## Guard de alocação

O `operator new` global é substituído por uma versão que conta alocações
dentro da janela de áudio. O teste do portão C marca a janela e falha se
`processBlock` alocar, em cinco segundos de playback sintético no pior caso
(densidade, spray e passo de leitura máximos). A alocação, se existir, acontece
em qualquer bloco; os cinco minutos contínuos são o ensaio T2 no host, que mede
p99 e xruns e não o `operator new`. Isto é o critério de tempo real virando
código, e não revisão manual.

## Correções pendentes na documentação

| Onde | O que está errado | Situação |
| --- | --- | --- |
| `05-fundamentacao.md`, artigo, `04-objetivos.md`, `06` | R = 0,995 com corte de 10 a 15 Hz | corrigido para R = 0,9983 nos quatro lugares |
| artigo, seção 1.3 | biquad de coloração sem parâmetro na Tabela 8 | pendente de decisão: estágio fixo ou sétimo parâmetro |
| `04-objetivos.md` e `06` | não listam o biquad | alinhamento depende da decisão acima |
| `06-metodologia-arquitetura.md` | FFT como dependência do plugin | removida: a FFT é só da medição T1 |
| `08-cronograma.md` | contingência de trocar JUCE por iPlug2 | desativada: JUCE 8.0.14 compila no toolset v145 |
| `src/opcoda_plugin/CMakeLists.txt` | `IS_SYNTH FALSE` publicava `Sub Categories: ["Fx"]` | corrigido na S2: agora `["Instrument", "Synth"]` |
| `PluginProcessor` | barramento de entrada estéreo declarado e nunca usado | removido na S2: instrumento sem entrada |
| `PeImage::Section::entropy` | campo declarado e nunca preenchido | corrigido na S2: medido por seção |

## Ingestão

`pe::toSamples` é a fronteira entre o binário e o motor: byte vira amostra
normalizada em [−1, 1], com 127.5 no zero. Fica no núcleo e não na interface nem
no teste, para que o plugin e o ensaio T1 chamem a mesma função. Antes ela
existia duplicada dentro do teste, e a divergência já produziu uma leitura 57
bytes além do buffer, detectada pelo AddressSanitizer.

## Agendamento dos grãos

A densidade é um spawn por amostra derivado dos grãos por segundo, com fração de
fase para que um bloco de 128 e outro de 512 produzam a mesma densidade. Os
grãos nascem espaçados ao longo do bloco pelo intervalo entre dois grãos, e cada
um guarda o índice de escrita onde nasceu, porque a sobreposição precisa começar
na amostra certa.

Um grão que atravessa o bloco recomeça em zero no índice de escrita: o índice é
a posição dentro do bloco corrente, não um contador global de amostras. Sem
esse reset a voz trava, entra no próximo bloco com índice maior que o tamanho do
bloco e nunca mais escreve.

O avanço por amostra é a taxa de reprodução dividida pelo comprimento da fonte,
porque `position` é normalizada em [0, 1]. Somar a taxa crua fazia o grão varrer
o arquivo inteiro em duas amostras e morrer, com pico de saída da ordem de
1e-7. O ensaio T1 de 40 dB continuava verde, porque uma asserção `> 0.0f` é
satisfeita por 5,6e-7.

## Entropia por seção

`PeImage::Section::entropy` era um campo declarado e nunca preenchido. A S2
passou a medi-lo na ingestão, que é o que a OE5 pede: entropia por seção.

A implementação encontrou um bug real no caminho. O overload de três argumentos
de `shannonBitsPerByte(data, size, offset)` mede **do offset até o fim do
arquivo**. Para uma seção isso é errado: o fim é `rawOffset + rawSize`, e
`rawSize` é `min(declarado, disponível)`, então costuma ser menor que o resto do
arquivo. Um `.text` de 256 bytes cheios de zero seguido de 256 bytes de dados
uniformes media 4,98 em vez de 0.

A correção foi adicionar o overload de quatro argumentos, com comprimento
explícito, e deixar o de três chamando-o com `size - offset`. O comportamento
antigo continua coberto por `ShannonEntropy.OffsetVersionMatchesSlice`.

Seção sem dados brutos, como `.bss`, tem `rawSize` zero e fica com entropia zero.
Medir a partir do offset declarado daria a entropia do material seguinte.

## A interface

Cada arquivo tem uma responsabilidade só:

| Arquivo | Responsabilidade |
| --- | --- |
| `palette.h` | os tokens do `design/DESIGN-SYSTEM.md`, em hex único |
| `look_and_feel.h` | desenho do knob, do botão e dos ícones, todos em `juce::Path` |
| `animator.h` | meia-vida exponencial, a única forma de interpolação da interface |
| `boxed_label.h` | rótulo com fundo, usado no valor e nos chips |
| `knob.h` | rótulo com glifo, `juce::Slider` rotativo e valor |
| `choice_button.h` | parâmetro de escolha como chip escuro com valor, chevron e `PopupMenu` |
| `display_panel.h` | fundo escuro com gradiente, brilho de topo e sombra interna, sem grelha |
| `byte_display.h/.cpp` | as três vistas do material, a caret e o *preview* |
| `hex_grid.h/.cpp` | a vista de bytes: endereços de 8 dígitos, 16 colunas num bloco só, ASCII, ASM com cores por símbolo e barra de deslocamento |
| `mod_matrix_view.h/.cpp` | a vista MATRIX: oito rotas com combos e barra bipolar, cada célula um attachment |
| `output_meter.h` | medidor stereo de saída, com pico por canal e escala em dB |
| `plugin_editor.h/.cpp` | o chassi fixo, as abas, o pé do display com o transporte, o teclado e o layout |

**Não há `assets.h` nem ficheiro de arte.** Desde 06/10/2026 o knob e o botão são
`Path` desenhados em código, e `resources/` guarda só matéria-prima de duas
espécies: as duas Inter Display em `resources/fonts` e os PNG do ícone em
`resources/icons`. A peça fotografada
`knob-md.png` eram 186×192 píxeis para pintar um disco de 62, e `button-large.png`
obrigava a desenhar a peça quadrada ao lado do texto porque esticar um bisel de 2 px
o deixava com 1 px de um lado e 4 px do outro. Com `Path` o corpo escala, o bisel
tem a espessura que se pede e não há descodificador de PNG no arranque.

**O `HexGrid` continua a ser um Component independente, dentro do `ByteDisplay`.**
A geometria dele tem três capturas atrás e um redesenho seria o caminho de perder as
três. O `ByteDisplay` é o dono do modo e pinta por cima a cabeça de reprodução, que
é a única peça que o `HexGrid` não conhece.

O editor tem três faixas de conteúdo — header claro, display escuro, painel de
parâmetros — mais a fileira de abas de módulo e a faixa do teclado ao fundo. É a
convenção do Ableton, e é também a resposta ao pedido de brilho: halo laranja
sobre superfície clara dá 2,4:1 e desaparece, então toda a energia de glow fica na
região escura.

**O teclado virtual não atravessa o `MidiBuffer`.** O `MidiKeyboardComponent`
escreve num `MidiKeyboardState` que o editor escuta, e o listener empurra um
`NoteTracker::Event` para uma `SpscRing` de 64 lugares; o callback drena-a antes
de ler os parâmetros e entrega os eventos à mesma `NoteTracker` do MIDI do host.
A alternativa canónica do JUCE — `MidiMessageCollector` + `processNextMidiBuffer`
— faz lock e pode alocar dentro do callback, que é o que o guard de alocação
proíbe. Com a fila, o caminho de áudio continua sem trava e sem alocação, e a
nota do teclado e a do host são indistinguíveis para o motor.

**Os knobs são `juce::Slider` de verdade, não desenho customizado.** Isso não é
padrão, é decisão: `Slider` entrega foco por teclado, ajuste com setas e
`AccessibilityHandler` de graça, que são exatamente os critérios da
`docs/10-acessibilidade-w3c.md`. Desenhar o knob à mão custaria tudo isso. O
`SliderParameterAttachment` cuida do undo por parâmetro, e por isso exige a
referência do `RangedAudioParameter`, não a da árvore inteira.

O nome acessível de cada controle é a descrição completa em inglês
("Grain size, in milliseconds") e não o rótulo curto ("SIZE"), que
seria inútil para quem navega por leitor de tela. Os **66 parâmetros** vivem em
seis módulos (GRAIN, ENV, FILTER, MOD, CLOUD, KEYS), com uma faixa visível de
cada vez e sem páginas internas: a janela fixa de 1180×760 dá a altura para os
controles numa linha. A aba MAIN saiu porque repetia os onze parâmetros de grão;
a aba OUT saiu porque volume e pan formam o master persistente na coluna escura
da direita, ao lado do medidor de saída. Cada controle aparece uma vez, no campo
semântico a que pertence: GRAIN separa grão, playback e mix; ENV separa tempos e
curvas; FILTER separa o primeiro filtro, o carácter e o segundo; MOD separa o LFO
do roteamento. A legenda e o filete desenham o campo, e a largura de cada um
segue o número de controles que contém.

Os parâmetros de escolha deixaram de ser knobs. Window, tipos e inclinação do
filtro, alvo, forma, sincronismo e divisão do LFO, escala e tônica passam a
campos de escolha: um chip escuro, baixo e largo, com o valor escrito e um
chevron à direita. O clique abre o `PopupMenu` com os rótulos reais e o
`ParameterAttachment` mantém o campo em sincronia com o host. O chip fica na
altura do disco do knob, mas não ocupa a célula inteira: o quadrado que aqui
estava lia-se como um disco sem escala. A troca derruba o número de discos de
trinta e nove para vinte e nove sem perder parâmetro nenhum, e resolve a leitura
de "Low-pass" num campo onde antes cabia "Lo". Cada legenda leva ainda um glifo
em `juce::Path`, desenhado pela mesma função que serve os ícones dos módulos.

### Três decisões que deram errado na primeira tentativa

**`Rectangle::removeFromLeft` não consome o rectângulo.** Ele devolve um
retângulo novo e deixa o original intacto. Encadear chamadas para avançar um
cursor sobrepõe as peças: o texto de status ficou com 6 px de largura e
invisível, e o wordmark ficou por baixo da faixa de arquivo. Agora o layout usa
um `int x` explícito.

**Não existe `BlendMode` aditivo no JUCE 8.** O brilho do LED é um gradiente
radial que vai do centro opaco para a borda transparente: em blend normal, a
queda de alpha sobre fundo escuro dá o mesmo resultado de uma luz que se dissolve
no fundo.

**Quebrar os knobs em duas linhas não funciona.** A altura disponível depois do
display e das abas não comporta dois knobs com rótulo e valor, e eles
despencam para poucos pixels. Cada módulo fica sempre em uma linha, como no mock, e
o diâmetro do knob é limitado dentro do componente, então encolhe com a janela
sem precisar de um segundo layout. Por isso as abas trocam a faixa em vez de
empilhar: cinco fileiras pediriam ~700 px e não caberiam na janela fixa. A
revisão de 27/10/2026 reforçou a decisão: os campos semânticos dividem a mesma
linha e os parâmetros de escolha, ao virarem chips baixos, não pedem altura de
disco. No filtro, o gráfico cede um pouco da largura para os dez controlos
caberem com o rótulo inteiro.

### A janela fixa e as quatro vistas

O chassi fecha em 1180×760 e não aceita redimensionamento (`setResizable(false,
false)`). O display ocupa a faixa superior de 380 px, com uma rail vertical de
62 px à direita para as vistas e o zoom; as seis abas de módulo encostam
directamente no display, sem tira de navegação pelo meio; os controles e a coluna
mestra de 168 px preenchem o meio; o teclado de 84 px e o rodapé fecham o
desenho. O layout foi escrito para estes números e não tem um caminho
alternativo — a guarda de `resized()` contra o primeiro rectângulo 0×0 é a única
excepção. A coluna mestra cresceu de 116 para 168 px quando pan e volume passaram
a dividi-la, e as leituras de POS e REG subiram para a tira superior do visor,
deixando o pé do ecrã com o transporte e as páginas do hex.

A grelha hexadecimal é contínua, com 16 bytes por linha num bloco só (o vão de
12 px que partia a linha em dois grupos de oito saiu), endereços de 8 dígitos,
coluna ASCII própria, coluna ASM e uma barra de deslocamento de 10 px. O topo do
hex é do utilizador: `topByte_` só muda por roda, clique ou teclado, e a cabeça de
reprodução não arrasta a vista. O mesmo gesto na grelha move a posição. A parede
de dígitos lê-se toda no mesmo marfim; o âmbar fica para o byte da cabeça de
leitura, que é a selecção. O endereço e os bytes levam uma linha de separação que
morre na margem das caixas de leitura, e as colunas ASCII e ASM são superfícies
opacas próprias — antes eram faixas translúcidas por linha e a grade aparecia por
dentro delas.

O transporte tem play/pause e `HOLD` no pé do display, ao lado das leituras de
POS e REG, e o `HOLD` congela o avanço (`transport_.advance(held ? 0 :
numSamples, playing)`), não o áudio. Tirar a tira de navegação devolveu 40 px ao
display e 8 px ao painel, e deixou o transporte dentro do que ele governa: o
olho que segue a cabeça de leitura não tem de atravessar a janela para carregar
no play. O medidor stereo da coluna mestra lê `peakLeftDb` e `peakRightDb` da
telemetria, sobe depressa e cai devagar, e usa a escala de 0 a -48 dB, com
rótulos de 12 em 12 dB. O render é vetorial 2-D; não há OpenGL nem animação
ambiente.

A quarta vista é a **matriz de modulação** (`ModMatrixView`, tecla 4): oito
linhas com fonte, destino e amount, cada célula um componente de verdade com
attachment ao `m{n}src`/`m{n}dst`/`m{n}amt`. O trio de zoom esconde-se nela,
porque escala não se aplica.

A **largura estéreo** (`STEREO`, na linha de grão entre PAN e LEVEL) é mid/side
na saída: `mid` fica intocado, `side` escala de 0 a 2, e a rampa corre dentro
do bloco para a automação não zumbir. O destino `Stereo Width` da matriz usa a
mesma escala multiplicativa do motor.

A roda acelera por gesto: cada entalhe seguido dentro de 320 ms multiplica o
passo por 1,7 no hex (até 32×) e o expoente do zoom nas vistas de onda e de
entropia até 8×; uma pausa volta ao passo de base. Ícones de 30 px sem moldura:
o desligado é só o glifo mais fraco, sem contorno de 1 px a fingir botão de web.

### Abas de pasta, marcadores e as correções de 26/10/2026

As seis abas de módulo deixaram a fileira solta e passaram a ser pastas
assentes na borda do painel de parâmetros. O componente `ModuleFolderTab` põe
o glifo no lugar da palavra; a aba ativa é da cor do painel, desce 6 px sobre
ele e leva o filete âmbar, e as inativas ficam 2 px mais altas, com a linha de
pé a lembrar o separador de arquivo. A largura caiu de 92 para 44 px; o nome
falado continua no tooltip e no leitor de tela. O painel cedeu os 6 px da
cauda, e a fileira de knobs ficou com o espaço que a palavra ocupava.

O teclado cobre C1 a C7 — 43 teclas brancas, com o dó central em C4
(`setOctaveForMiddleC(4)`). A largura da tecla é calculada em `resized()` a
partir da baía, e não fixada: com uma constante, a última oitava ficava fora do
vidro e a seta de scroll cortava o C7. O `PITCH` acompanha em ±48 semitons, e
`applyModMatrix` limita no mesmo teto.

O envelope da onda deixou de ser riscado por coluna. São duas passagens: uma
reamostragem para um máximo e um mínimo por pixel (`resampleEnvelope`, ao dobro
da largura em pixéis) e um máximo corrido de ±2 px sobre o resultado. A primeira
mata o zigzag sub-pixel que abria riscos verticais no preenchimento; a segunda
fecha as valas de 1 px que faziam o material aleatório parecer um pente. A forma
é uma só tinta — marfim a 15 % no corpo e um filete a 78 % no contorno —, e a
meia altura é 47 % do gráfico: com o envelope a encostar ao limite, o filete era
cortado e só os entalhes do ruído apareciam. O gradiente quente, a sombra
projectada e o núcleo de rms saíram; eram três camadas sobre a mesma forma, e o
ruído virava uma nuvem sem contorno.

O `DRIVE` ganhou o clipping descrito em `SOUND_ENGINE.md` — `tanh` por grão e
teto duro no barramento, de 1,0 a 0,40 — e a largura estéreo ganhou o Haas:
acima de 1,0 o canal direito atrasa até 12 ms, que é o spread que o material
mono não tem. O atraso e a largura rampam no mesmo laço, com a derivada do
atraso limitada a um quarto de amostra por amostra: a rampa de bloco inteiro
fazia o ponteiro andar para trás e o canal direito tocava ao contrário.

Os marcadores (`bookmarks_`) são endereços absolutos, não parâmetros: alt+clique
acrescenta ou tira a bandeira no endereço apontado, o clique sobre a bandeira
salta para ele, e a bandeira também se desenha no mapa. Vivem na sessão e morrem
com o material novo — não são estado do projeto, e por isso não entram no
`APVTS`.

### A rodada de acabamento de 26/10/2026

**O transporte desceu para o pé do display e a tira de navegação desapareceu.**
Play/pause e `HOLD` passaram a ser botões de 74×20, com glifo e palavra, na
mesma linha de POS e REG e dos quatro botões de página do hex; o `PLAY` troca
para `PAUSE` quando corre. A tira de 32 px que os alojava foi apagada, o display
cresceu de 340 para 380 px e o painel ganhou 8 px. A rail de vistas foi de 36
para 96 px: os quatro botões passaram a rectângulos de 88×34 com ícone e nome
(`WAVE`, `HEX`, `ENTROPY`, `MATRIX`), e o trio de zoom, `IN`/`FIT`/`OUT`, usa o
mesmo desenho.

**O clique deixou de roubar o foco.** Todos os controlos recebem
`setMouseClickGrabsKeyboardFocus(false)`, o foco pertence ao teclado virtual, e
um `MouseListener` global devolve-lho depois de qualquer clique que o leve para
fora. As setas, `PageUp`/`PageDown`, `Home`, `End` e `Enter` passaram a ser
tratadas pelo editor e encaminhadas para o cursor do display, de modo que trocar
de vista ou arrastar um knob não cala as teclas de nota. O `keyStateChanged`
encaminha a tecla que sobe para o teclado quando este perdeu o foco, e nenhuma
nota fica presa. O anel de foco laranja sobreviveu apenas à travessia por `Tab`:
o `ByteDisplay` trocou-o por um caret branco.

**O botão de play não pisca mais.** A condição de activação incluía
`hostTransportIsKnownToBeStopped()`; no standalone, o playhead alterna entre
"parado" e "desconhecido" e o botão apagava e acendia ao ritmo do callback.
Ficou só `hasSource() && hasPlayableRegion()`: o transporte que o botão comanda é
o do instrumento. A leitura de `VOICES` saiu do rodapé — era ela que tremia — e o
número continua na telemetria, sem caixa onde piscar.

**As agulhas do visor são brancas, rectas e opacas.** O `preview` do display
deixou de desenhar barrinhas laranja arredondadas: a densidade vira um comb de
2,5 px de espessura com 3 a 16 dentes, o spray um banho com dois filetes, a
posição uma linha de 2,6 px a toda a altura e o grão a silhueta da janela
escolhida — Hann, Gaussian, Hamming ou Blackman, com as mesmas expressões do
motor. O pitch inclina a silhueta, o pitch spread desenha duas cópias mais
fracas, o level manda na opacidade e o pan desloca o ponto do topo. O desenho é
limitado à área visível, para a silhueta não aparecer meio cortada na posição 0.

**A escala do medidor saiu do aperto.** O piso desceu de -60 para -48 dB e os
rótulos passaram a 0, -12, -24, -36 e -48, centrados no traço e limitados dentro
da pista. O volume da coluna mestra cedeu 18 px ao medidor: com a pista antiga, 0
e -6 dB ficavam a 8 px de distância e os números encostavam uns aos outros. Em
27/10/2026 os rótulos voltaram a encostar — saíam por cima da coluna esquerda — e
o grupo inteiro passou a ser medido de uma vez e centrado: coluna de 22 px, 7 px
entre as colunas, 9 px até a escala e 26 px de escala. O corpo da escala desceu
para 7,5 px e a altura do medidor passou a 45 % da coluna, presa entre 64 e 104
px, para o grupo não dançar quando a janela muda de altura.

### O display F008

As abas de seção, a curva de entropia e a telemetria entraram na F008. O
`ingest` deixou de descartar o `PeImage`: publica em `SourceInfo` o nome, o
tamanho e, por seção, o nome, o offset, o tamanho em disco e a entropia. A curva
de entropia em janelas foi acrescentada depois, saiu com o componente antigo e
voltou na revisão de 24/10/2026 como faixa clicável na base do display; ver "A
grelha de bytes".

O `PeImage` continua a ser local ao `ingest`, mas **os bytes crus não**: são
guardados, porque a grelha precisa de os mostrar e porque o `Processor` precisa
de converter uma janela nova sem reler o disco. A distinção é deliberada e está
escrita em `plugin_processor.h`: o parse sai, os bytes ficam.

A telemetria de pico, vozes ativas e estado de som sai de `processBlock` por
`std::atomic`, lida pela thread de interface a 20 Hz. `refreshTelemetry` só
reescreve um `Label` quando o texto muda, porque trocar o texto dispara um
evento de acessibilidade e a 20 Hz isso vira ruído para quem usa leitor de tela.

### Quatro defeitos que só a captura de ecrã apanhou

Nenhum destes aparece em teste nenhum: são de layout e de contraste, e só
mostram com o binário a correr.

**A cor de texto do rodapé vinha da superfície clara.** As cinco leituras de
telemetria são `BoxedLabel`, que nunca define cor de texto e portanto herda a
quase preta do `Label`. Sobre o display escuro "48.0 kHz" ficava ilegível, o que
reprova o critério 1.4.3. Passaram a usar `textOnDark`.

**O rodapé empilhava caixas sobrepostas.** O helper media cada caixa a partir
da margem esquerda a cada passo, em vez de encostar na caixa anterior, então
todas ocupavam o mesmo intervalo e só a última pintada aparecia: o pico, a
entropia e o offset estavam debaixo das outras. Agora são larguras fixas
empilhadas da direita para a esquerda.

**Uma caixa vazia parece defeito.** Sem ficheiro não há seções para listar, e a
moldura das abas e das leituras vazias desenha um retângulo sem nada dentro. As
abas e as duas leituras que dependem de material escondem-se quando não têm
texto. A linha de estado não se move quando isso acontece: um alvo que salta ao
carregar um binário é pior do que um espaço constante.

**Um `int cursor` local colide com `Component::cursor`.** O `/W4` trata a
ocultação de membro como erro, e o build parou. Passou a chamar-se `edge`.

**Uma coordenada não é um tamanho.** A grelha recebeu
`Rectangle {x, y, largura, footerY}`, em que `footerY` é a coordenada absoluta
do rodapé no ecrã e não uma altura. Com a janela a 540 px de altura a diferença
são 54 px, e a grelha invadia o rodapé: a última linha de bytes era desenhada
por cima das leituras de pico e de vozes, e o resultado era texto ilegível
empilhado. Passou a `footerY - gridTop`.

**A linha de estado estava dentro da área da grelha.** O texto estava a 7 px do
topo do display, que é onde a grelha começa, e o fundo opaco da grelha tapava
`PRONTO` e `RECUSADO`. Deixava de ser visível exactamente o que o critério 3.3.1
exige que se veja. Agora a faixa de estado tem 14 px próprios no topo e a grelha
começa abaixo dela.

Nenhum dos três aparece em teste nenhum, e nenhum dos três é de lógica: são
medidas. Só uma captura de ecrã os mostra, e só depois de o valor já estar
errado no ecrã.

### A grelha de bytes

O seletor de 30 px foi substituído por uma grelha de bytes no formato de um hex
dump: endereço à esquerda, dezasseis bytes por linha em hexadecimal, coluna
ASCII à direita. A faixa antiga era um mapa do ficheiro inteiro com um cursor, e
um ficheiro de 12 MB não cabe em grelha. O que a grelha dá em troca é o byte.

**O clique escreve o parâmetro `position`.** POSITION é um dos seis da Tabela 8,
e a leitura de posição é o que o clique responde: escolher um byte é escolher de
onde se lê. Se o byte estiver fora da região, o editor **puxa a região para lá
antes** de escrever o POSITION, e a ordem não é arbitrária: o POSITION é uma
fração da região, e escrevê-lo antes de a região mudar apontaria para o sítio
errado. O comprimento da região preserva-se, que é a regra que o seletor antigo
usava.

O mapeamento entre o endereço e a fração vive em
`src/opcoda_core/pe/byte_to_position.{h,cpp}`, no núcleo, e não no componente
da grelha, pelo mesmo motivo que a `ByteRange`: o núcleo não depende de JUCE e
é o que os sanitizers alcançam. Tem duas armadilhas que estão em teste:

**O último byte não tem posição própria.** Com `B = end - 1` vem `k = length - 1`,
fração 1.0, e o motor desliga a voz logo a seguir, porque em
`granular_engine.cpp` a condição é `index + 1 >= sourceCount`. Um clique no
último byte seria um clique em silêncio. O mapeamento é meio aberto em cima
também, e o endereço mais alto com som é `end - 2`.

**O caminho inverso arredonda, não trunca.** O motor trunca o índice para
escolher a amostra; truncar também no display punha o cursor sistematicamente um
byte à esquerda do que foi clicado, em 63 enderecos em mil num teste. Arredondar
ao mais próximo centra o erro em zero: o endereço clicado volta exato e o desvio
do índice que o motor trunca é de no máximo meio byte.

A fração é um `float` de 24 bits de mantissa, que é a resolução real do
parâmetro: distingue cada byte até uma região de 16 MB, e acima disso o cursor
salta vários bytes. Não há correção possível sem mudar o tipo do parâmetro.

**A grelha e a faixa de entropia partilham o display.** O gutter marca o início
de cada seção com uma barra de 2 px; a faixa de 58 px na base mostra a entropia
por coluna do ficheiro inteiro, com barras largas e um contorno laranja na
região. A entropia da janela já não se lê no rodapé: o rodapé diz POS e REG, e
o medidor stereo ocupa a coluna mestra. O cache de colunas do hex e o
da entropia reutilizam `pe::reduceToColumns`, e nenhum dos dois aloca no
callback.

**A região é um filete, não um fundo.** O topo e a base da linha da região levam
1 px laranja; o preenchimento laranja a 3 px e o véu ficam reservados para o
endereço tocado, que é o único que a barra marca. A distinção não depende de
cor: o endereço tem também a barra de 3 px.

### Os dois caminhos de teclado

A grelha é operada pelo rato e pelo teclado do display. As células são pintadas
e não são componentes, e um `Component` só não tem filhos acessíveis: não há
como dar nome a 200 células. O caminho de teclado não é a mesma coisa desenhada
outra vez, são dois controlos com nome e valor próprios:

- **POSITION**, o knob, que é um `juce::Slider` de verdade, move a cabeça de
  leitura;
- **o display**, com ou sem foco, responde a PageUp/PageDown (uma página),
  Home/End (início e fim do ficheiro) e `A` (alinha a região à seção). O editor
  encaminha as setas para o cursor, porque o foco vive no teclado virtual; os
  quatro botões de página, no pé do display, chamam os mesmos métodos e só
  aparecem no modo hex.

O duplo clique na grelha e a tecla `A` alinham a região à seção mais próxima; o
duplo clique passa pelo mesmo teste de célula que o clique simples: um segundo
clique no vão entre colunas não alinha nada, em vez de alinhar a seção a partir
de um byte vizinho.

O alvo de clique é a linha de 26 px. Nas células ASCII a largura é de cerca de
14 px, abaixo dos 24 px do critério 2.5.8; o caso está declarado como exceção em
`docs/10-acessibilidade-w3c.md`.

**O endereço mede-se pelo ficheiro carregado.** Um PE acima de 4 GB tem endereços
com mais de oito dígitos, e a coluna é alargada em vez de cortar o número a
meio. A janela fixa não tem refluxo: a coluna ASCII ocupa 26 % da largura e não
é omitida, porque perder os caracteres é perder contexto. O que se preserva é a
regra de não partir o endereço; acima de oito dígitos a gutter cresce.

### O que o `ingest` deixou de publicar

O `PeImage` continua a ser local ao `ingest`, e os bytes crus continuam guardados
porque a grelha precisa deles e o seletor precisava deles. O que saiu foi
`SourceInfo::entropyCurve` e o respetivo `buildEntropyCurve`; a faixa de
entropia do display calcula as colunas a partir dos bytes crus e guarda-as em
cache, reutilizando `pe::reduceToColumns`. O rodapé deixou de medir a entropia da
janela: diz POS e REG.

### Defeito observado que não é desta alteração

**Alguns parâmetros arrancam com valores que não são os defaults declarados.**
Na captura de ecrã do Standalone, SIZE a 1 ms, DENSITY a 28.756 /s, POSITION a
6.8 % — e os valores mudam entre execuções. Foi confirmado em `c8006ae`,
compilado num worktree separado: os mesmos valores, logo não vem da grelha.

**A causa é o estado de sessão do Standalone, não memória por inicializar.** O
primeiro palpite foi memória não inicializada e estava errado; o que está
estabelecido é o seguinte, por esta ordem:

1. **Na saída do construtor os valores estão certos.** Instrumentado com
   `fprintf`: `grain=0.393939`, `density=0.095477`, `position=0.500000`, que são
   exatamente os defaults normalizados. O `AudioParameterFloat` aplica o default
   como deve.
2. **Sem `%APPDATA%\Opcoda\Opcoda.settings`, tudo está certo.** Apagado o
   ficheiro, o Standalone abre com 40 ms, 20 /s, 50 %, 0 %, 0.0 st e 0.0 dB —
   os defaults de `createParameterLayout`, verificados por captura duas vezes.
3. **A ida e volta do estado preserva os valores.** Uma sessão sem material grava
   o estado e sai com limpeza; o arranque seguinte restaura os mesmos defaults.
   Portanto `copyState` e `replaceState` não estão partidos.
4. **O Standalone do JUCE 8 persiste o estado entre sessões.** Em
   `%APPDATA%\Opcoda\Opcoda.settings`, na chave `filterState`. E no arranque
   chama `setStateInformation` a partir de
   `juce::StandalonePluginHolder::init` — confirmado pelo AddressSanitizer, com
   pilha completa:

   ```
   opcoda::PluginProcessor::setStateInformation      plugin_processor.cpp:415
   juce::StandalonePluginHolder::init                juce_StandaloneFilterWindow.h:122
   juce::StandalonePluginHolder::StandalonePluginHolder
   ```

Isto explica também um mistério que custou tempo: um `.exe` de `System32` que
appeared na janela em todas as execuções. **Não era o ambiente** — era o
`sourcePath` que o plugin tinha guardado, reingerido por `setStateInformation` na
linha seguinte à reposição do estado.

**O palpite de memória não inicializada está refutado.** Com o plugin compilado
em `RelWithDebInfo` e Symbols, o Dr. Memory 2.6 reporta 35
`UNINITIALIZED READ` e **nenhum tem um frame `opcoda::`** — as 35 são de
fronteiras JUCE e do sistema operativo. As 3447 `INVALID HEAP ARGUMENT` são de
caminhos de destruição, do JUCE. Não é leitura por inicializar no nosso código.

**O que fica por explicar** é qual sessão escreveu valores errados no estado.
Os candidatos nomeados, por ordem: os CCs 74 e 71 mapeados a densidade e
posição a chegar de um controlador MIDI ligado — o próprio `Opcoda.settings`
nomeia um *Arturia MiniLab mkII* como entrada MIDI —, ou automação do host.
Não foi reproduzido numa sessão controlada com material, porque a automação do
diálogo de ficheiro não é fiável o suficiente para servir de teste.

**A reprodução controlada passou a ser possível em 19/10/2026**, com o host
licenciado: a sessão pode ter automação real, CCs reais e um projeto guardado e
reaberto, que é precisamente o que faltava para separar as duas hipóteses. A
hipótese dos CCs testa-se com o mesmo controlador ligado e o mapeamento ativo; a
do host, com automação escrita no projeto. Se nenhuma reproduzir, o registo de
quem escreveu os valores continua aberto e o contorno abaixo continua a valer.

**Contorno prático, e é real:** apagar `%APPDATA%\Opcoda\Opcoda.settings` traz
os defaults de volta. É o que o passo 2 demonstra e o que qualquer pessoa pode
fazer semesperar por uma correção.

Não foi corrigido aqui: a origem dos valores errados não está identificada, e
mexer às cegas num caminho que já escreve seis parâmetros seria trocar um sintoma
por outro.

### `String {valor, 0}` não arredonda — e estava nos seis formatadores

Defeito pré-existente, **corrigido**. As caixas de valor mostravam a precisão
toda — `28.756 /s` em vez de `29 /s` — e ninguém reparava porque um número com
seis dígitos é plausível numa densidade.

A causa está no JUCE, em `juce_String.cpp`: `writeDouble` só aplica a precisão ao
stream quando `numDecPlaces > 0`. Com zero, o `o << n` sai com a precisão por
omissão do stream, que é 6 dígitos significativos. `String(28.756f, 0)` dá
`"28.756"`.

`String` não tem construtor de float arredondado, portanto os seis formatadores
de `kSpecs` passaram a usar `asInteger()`, que faz `roundToInt` explícito. Com uma
casa decimal — PITCH e VOLUME — o `if` entra e o formato já estava certo; esses
dois não se mexeram. O helper existe para a razão ficar escrita uma vez em vez
de seis.

### Peças desenhadas em código

**Não há mais harness, nem foto, nem sprite.** Ver a secção "A interface" acima para
o porquê. Fica escrito o que a regra do harness chegou a ensinar e que continua
valendo: **o harness não gerava texto**, e todo o texto — rótulos, valores, escalas —
é desenhado em código, porque texto gerado por modelo sai com letra errada e não há
como corrigir sem regenerar. A regra passou a aplicar-se a tudo, não só ao texto.

**O corpo do knob são três círculos.** Aro serrilhado, face recuada e ponto de
origem. São eles que separam "disco" de "botão rotativo": o aro lê-se como pega, a
face mais escura recua, e o ponto no centro marca a origem, que é o que se vê num
knob real. A luz vem de cima e da esquerda e o gradiente é diagonal, não radial a
partir do centro — um radial simétrico daria um disco sem direção, e sem direção não
se lê como superfície redonda.

**O serrilhado é um `Path` memorizado por posição**, não 36 chamadas de `drawLine`
por knob. São seis knobs a 60 Hz, e 216 chamadas de linha por quadro para desenhar
textura fixa é o trabalho que o `Path` existe para evitar. A comparação do cache é
dos dois valores e não só do raio: o mesmo raio com o centro noutro sítio é um
serrilhado na posição errada.

**O estado ligado do botão não é só cor.** `#ff9a00` sobre `#c2c6c9` dá 2,4:1, e
sem segunda pista o botão de transporte falha o 1.4.3. O ligado ganha uma barra de
1 px à esquerda, e o `PLAY`/`STOP` troca a palavra — que é a pista que funciona sem
ver cor.

### O que não está feito

**Não há tinta de entropia por linha.** O `design/stitch/03-byte-heatmap.png`
mostra a entropa como cor de fundo das células, e é o passo natural a seguir:
`entropy::shannonCurve` já dá a curva e a grelha já tem a linha. Fica de fora
porque cor de fundo que codifica um número é o primeiro sinal a falhar o
critério 1.4.1, e por isso precisa de uma segunda leitura, não só de um alfa.

**A resolução do cursor é a do parâmetro.** Com uma região acima de 16 MB o
`float` de 24 bits de mantissa já não distingue cada byte, e o cursor salta
vários. O valor é exato, mas a resolução não é nossa.

A validação no Ableton continua por fazer, e com ela o T2 e o T4.

## Categoria de instrumento

Com `IS_SYNTH FALSE`, o `moduleinfo.json` publicava `Sub Categories: ["Fx"]` e o
Ableton listava o Opcoda em Audio Effects, com uma entrada de áudio que o plugin
nunca usa. A S2 trocou para `IS_SYNTH TRUE` e removeu o barramento de entrada. O
bundle agora declara `["Instrument", "Synth"]`.

`live_` também era lido pela thread de interface enquanto a thread de áudio o
escrevia. Os dois ponteiros têm o mesmo tipo, então o compilador não reclamava,
mas era uma corrida. Passou a ser `std::atomic`.

O diálogo de arquivo é **assíncrono**. `browseForFileToOpen` não existe por
padrão no JUCE, porque `JUCE_MODAL_LOOPS_PERMITTED` é 0: travar a message
thread atrapalha o host. `launchAsync` é o caminho correto para plugin.

O arrasto usa a API do JUCE 8, que não é a do JUCE 5:
`isInterestedInFileDrag` decide se os demais callbacks chegam e é chamado várias
vezes enquanto o mouse se move, então precisa ser barato; `fileDragEnter` e
`fileDragExit` dão o feedback visual; `filesDropped` entrega. Arrastar uma pasta
traz dezenas de caminhos, então o editor tenta em ordem e para no primeiro que o
parser aceita, em vez de recusar o arrasto inteiro.

## O plugin que o Ableton reconhecia e não abria

O preset se chamava `release` mas não tinha `CMAKE_BUILD_TYPE`, e com o gerador
NMake, que é single-config, o CMake caía em Debug. O VST3 linkava contra
`MSVCP140D.dll`, `VCRUNTIME140D.dll`, `VCRUNTIME140_1D.dll`, `ucrtbased.dll` e
`dbghelp.dll`.

O Ableton lê `moduleinfo.json` do disco para montar a lista, então reconhecia o
plugin normalmente. Só para abrir ele faz `LoadLibrary`, que falha com
`ERROR_MOD_NOT_FOUND` onde essas DLL não existem. Reconhecer e não abrir é a
assinatura exata de uma dependência de runtime ausente.

Duas linhas no preset resolveram, e o `build.ps1` agora falha se o VST3 linkar
contra qualquer DLL de debug. Detalhes em `docs/17-guia-de-build.md`.

## Estado do plugin

`getStateInformation` e `setStateInformation` estavam vazios. A consequência é
silenciosa: ao reabrir um projeto no Ableton, os knobs voltavam ao valor
default e a automatização do usuário sumia.

A implementação usa `copyState`/`replaceState` da APVTS, que é o caminho que o
próprio host usa para empurrar parâmetros de automação de volta para dentro, e
não uma segunda via. O binário carregado entra no estado como uma propriedade da
mesma árvore, pelo mesmo motivo.

`setStateInformation` **reingere o arquivo pelo mesmo `ingest` do arrasto**,
verificando antes se ele ainda existe. Se sumiu, o erro é `E_SOURCE_MISSING` em
vez de estado vazio silencioso.

### Gate de notas

O motor granular ponta a ponta assim que havia ficheiro carregado, sem que
exigisse nota nenhuma. Num instrumento isso é errado: o Ableton põe o plugin
numa pista MIDI, o utilizador carrega um `.exe` e o som começa sem ele tocar
nada.

`GranularEngine::setSounding` liga e desliga o motor. A rampa é **linear, de
5 ms**, e isso não é detalhe estético: abrir a saída de uma vez produz um degrau
no sinal, e um degrau é um transiente largo em frequência, audível mesmo com
release curto. A rampa custa duas multiplicações por amostra.

O gate entra **antes** do limiter, para que o comportamento do limiter não
dependa de quantas notas estão a soar.

Sete testes cobrem a rampa: que fica em silêncio sem nota, que abre e fecha, que
o nível avança a meio caminho depois de um bloco de 128 amostras (onde um gate
binário já estaria em 1,0), que o tempo pedido é respeitado, e que `reset`
fecha tudo. `reset` também limpa os valores de CC, porque reset significa
esquecer tudo e deixar valor antigo invisível é o tipo de estado que só se
descobre depois.

## Notas e CCs

`rt::NoteTracker` está no núcleo, sem JUCE, pela mesma razão que
`pe::toSamples`: **é a regra que tem testes**. O plugin limita-se a traduzir
`MidiMessage` em `Event`, que é a parte fina.

O modelo separa **teclas premidas** de **notas presas pelo pedal**. Com o
pedal premido, largar o teclado não desliga o som; soltar o pedal não abafa as
teclas que continuam premidas. Um contador único não consegue expressar isso.

Há um piso em zero na contagem. Uma `note-off` sem `note-on` correspondente vem
de hosts que reenviam o estado inicial, e sem esse piso `sounding()` ficaria
falso para sempre: o instrumento carregava, o knob mexia, e nunca mais saía som
sem reiniciar o plugin.

### Onde os CCs são aplicados, e porquê

Os dois CCs mapeados são **CC74** (Brightness) → DENSITY e **CC71**
(Resonance) → POSITION. São os dois com nome de brightness e resonance em
qualquer teclado de palco, então não precisam de tabela para serem descobertos.

O valor lido no callback vai para `NoteTracker` e é aplicado ao parâmetro
**na thread de interface**, a partir do timer do editor.

O caminho alternativo — escrever direto no parâmetro da thread de áudio — foi
descartado de propósito. `setValueNotifyingHost` notifica os listeners, e o
`SliderParameterAttachment` é um deles: seria acesso cruzado ao GUI a partir do
callback, exatamente a classe de defeito que o guard de alocação não apanha.

O custo é a latência do timer, cerca de 50 ms. Em troca o parâmetro continua a
ser a fonte única da verdade: o host é notificado, o knob segue, e o estado do
projeto fica certo sem caminho paralelo. Os dois valores fazem parte dos
`NoteTracker` para poderem ser testados sem JUCE.

### A matriz de modulação é parâmetro

Desde 24/10/2026 as oito rotas da F020 são **parâmetro** (`m1src`/`m1dst`/
`m1amt` ... `m8amt`), e não o nó `MODMATRIX` que antes vivia ao lado dos
`PARAM`. A consequência é a que a constituição pede: o host vê as 24 células,
automatiza, e elas viajam no preset e no `.opcoda` porque o XML é o mesmo. A
vista MATRIX do display (tecla 4) edita por attachment: dois
`ComboBoxParameterAttachment` e um `SliderParameterAttachment` por rota, e o
audio lê os 24 valores crus por bloco.

Três caminhos foram considerados para atravessar a fronteira das threads:

1. ler a árvore no callback — proibido: `ValueTree` bloqueia e aloca;
2. um `SpinLock` com o callback à espera — proibido: o callback pode esperar;
3. **ler os raws por ponteiro pre-resolvido na construção** — escolhido. A
   primeira versão usava 24 inteiros atómicos e um contador de geração; o
   snapshot inteiro por geração trocou-se por 24 leituras de `std::atomic`
   soltas, a mesma tolerância que os 41 parâmetros de som já tinham. Ler os
   raws por id no bloco construiria `juce::String` no thread de audio, e é por
   isso que os ponteiros ficam em `routeRaw_`.

Projetos gravados antes trazem o nó `MODMATRIX`; `migrateLegacyModMatrix`
copia as rotas para os parâmetros e remove o nó, depois de cada `replaceState`.

`amount` é `AudioParameterFloat` de -1 a +1. O audio recorta com
`sanitizeModAmount` — rota com fonte ou destino inválido é lida como rota
vazia, e estado corrompido abre em silêncio.

A matriz é aplicada a uma **cópia** dos parâmetros por bloco. Sem a cópia, a
soma das rotas acumulava de bloco para bloco e o corte derivava; com ela, cada
bloco parte do valor declarado e recebe a soma uma vez.

### O filtro com caráter

O filtro 1 ganhou quatro parâmetros, e cada um tem uma decisão que não é óbvia:

- **slope 24 dB** acrescenta um segundo biquad em série com **estado próprio**.
  Chamar `process` duas vezes no mesmo biquad não é uma cascata: a segunda
  chamada reutiliza a memória do estágio e a resposta deixa de ser a de dois
  filtros idênticos. O `filterA2_` existe por isso.
- **drive** é `tanh(x·g)/g`, e não `tanh(x·g)/tanh(g)`. A segunda normaliza para
  escala cheia e levanta 12 dB o material baixo; a primeira garante
  `|tanh(g·x)/g| ≤ |x|`, e o pico nunca sobe. Em `drive = 0` o caminho é
  desligado, não saturado: um `tanh` a 0 dB já comprime.
- **keytrack** e **env** movem o corte em oitavas, antes do `makeBiquadCoeffs`.
  O envelope é o nível do bloco anterior: o gate fecha no fim do bloco, e usar o
  valor corrente exigiria recalcular coeficientes por amostra.
- os coeficientes continuam a ser recalculados **no máximo uma vez por bloco**,
  e a comparação usa o corte já modulado — comparar o cru recalcularia a cada
  bloco com o LFO ligado, mesmo parado.

### As curvas do envelope

Com as três curvas a zero, o motor usa o laço antigo, amostra a amostra. Não é
preguiça: os ensaios do ADSR comparam com `EXPECT_FLOAT_EQ` (4 ULP), e trocar a
aritmética por `pow` mudaria os últimos bits de todos eles. Com qualquer curva
não nula, a fase do estágio sobe a `2^(3·curva)`:

| curva | expoente | forma |
| --- | --- | --- |
| −1 | 1/8 | sobe depressa, aproxima devagar |
| 0 | 1 | rampa linear |
| +1 | 8 | fica baixo, fecha depressa |

O sinal do expoente estava trocado na primeira versão: a frase da spec pedia
"rápido no início em −1" e a fórmula ao lado dela, `2^(-3·curva)`, dava o
contrário. Vale a frase, que é a convenção do Serum que a `docs/19` declara, e a
fórmula passou a `2^(+3·curva)`.

### A nuvem

Três parâmetros, todos aplicados no nascimento do grão, nenhum no caminho
quente:

- **`cloudsize`** sorteia um atraso uniforme em `[0, 0,5 s]` e o grão ocupa o
  slot **sem escrever** enquanto espera. O que resta do atraso passa para o
  bloco seguinte, e o grão começa no meio do bloco quando o atraso acaba ali —
  não no início do bloco, que seria quantizar a nuvem ao tamanho do bloco.
- **`pitchspread`** soma um desvio simétrico ao detune antes da razão de
  semitons.
- **`grainjit`** sorteia um fator em `[0,5×, 1,5×]` e recorta a 1–2000 ms, o mesmo
  intervalo do knob de tamanho. A janela **não é reconstruída por grão**: a tabela
  é uma por tipo, preenchida no `prepare`, e os oito slots partilham-na, por isso
  o índice é escalado por `(kWindowTableSamples − 1) / (grainLength − 1)`. O grão
  guarda a tabela com que nasceu, e não a que o knob aponta agora.

**Nenhum dos três sorteia quando está a zero.** Um sorteio a mais mudaria a
sequência do gerador e a saída com os knobs a zero deixaria de reproduzir a
anterior — que é o critério de aceite da F021, verificado comparando zeros
explícitos com os defaults.

A semente continua fixa (`kDefaultSeed`). Semear com o relógio em produção
daria uma nuvem diferente em cada corrida e quebraria a reprodutibilidade de T1
e T2; `setSeed()` fica disponível para quem queira variar as tomadas.

### A lacuna do teste de tempo real

O guard de alocação corre sobre o núcleo e **não vê o `processBlock` do
plugin**, onde o MIDI é lido. `readNotes` é uma passagem por um buffer de
mensagens já contadas, sem alocação previsível, mas isso é raciocínio e não
medição.

O que fecha esta lacuna é o T2 no Ableton, com buffers de 128 amostras e
tráfego MIDI real, na etapa S3b. Está declarado em `docs/07-plano-testes.md`
como dependência de DAW, e não como coisa já feita.

O `tests/` é núcleo puro sem JUCE, por decisão de portão: o AddressSanitizer
roda sem framework de áudio no caminho. A consequência é que
`PluginProcessor::getStateInformation` **não tem teste automatizado**, e é
código novo.

Não há como contornar sem criar um alvo de teste com JUCE, o que traria
framework de áudio para dentro do portão de tempo real. A verificação fica
manual e declarada: no Ableton, mover um knob, guardar o projeto, fechar e
reabrir. Se o knob voltar ao default, o defeito está no round-trip de XML.

Isto é a mesma classe de buraco que as outras duas falhas de host: código
que compila, passa em todos os testes, e só falha no DAW. A diferença é que
aqui a lacuna está declarada em vez de ser descoberta depois.

`tests/end_to_end_test.cpp` percorre a cadeia completa com `notepad.exe` do
sistema, sem JUCE e sem host: ler disco, `parse`, `toSamples`, motor granular,
DC-blocker, limiter. É ele que prova que um executável real vira som, e ele
usou o material real porque os dois bugs mais caros do projeto só apareciam com
arquivo de verdade.

O `notepad.exe` da máquina tem 200 KB, sete seções e byte médio de 96 em
`.text`, ou seja, offset de −0,245 em amostra normalizada: material que o
DC-blocker tem o que remover e que a leitura sintética não reproduzia.

## A janela própria do standalone (26/10/2026)

O `StandaloneFilterWindow` do JUCE trazia o botão *Options* dentro da barra do
sistema, um menu com Save/Load/About e o `AudioDeviceSelectorComponent` num
`DialogWindow`. Três coisas erradas de uma vez: a barra do sistema ficava com o
desenho do Windows, o menu oferecia um Save que neste projeto não existe (o
estado do plugin é do *host*; no standalone, do `Opcoda.settings`) e o seletor de
placa, quando aberto, tapava o instrumento com a grelha de etiquetas do sistema.

`StandaloneWindow` substitui-a: `DocumentWindow` com barra de 36 px desenhada
pelo `OpcodaLookAndFeel` — grafite um tom abaixo do chassis, marca à esquerda,
minimizar e fechar em teclas planas de 34 px — e um corpo que tem o editor e, por
cima dele, o `SettingsScreen`. A janela mede 1182×798: os 1180×760 do editor, mais
a moldura de 1 px de cada lado e a barra; não é redimensionável, e a posição fica
em `windowX`/`windowY` do mesmo `Opcoda.settings`, reposta no arranque.

O `SettingsScreen` é uma folha de 820×580 sobre um véu de carvão a 62 %, com duas
páginas — `AUDIO` (driver, saída, taxa, buffer) e `MIDI` (entradas em linhas de
estado) —, CPU e latência no pé e o erro do último pedido de dispositivo escrito
à esquerda do `CLOSE`, em vermelho de erro. Não há `APPLY`: a mudança aplica-se ao
escolher, e um pedido que falhe deixa o dispositivo anterior a tocar. O véu não
tapa o instrumento de propósito, e as teclas que o ecrã não usa (o mapa
`A W S E...`) são entregues ao editor, para se poder escolher a placa com uma nota
presa; fechar é um clique fora, `Escape` ou `CLOSE`, e o foco volta ao teclado
virtual.

O ícone da barra de tarefas é aplicado ao *peer* depois de a janela estar no
desktop: `DocumentWindow::setIcon` guarda só uma cópia para o título, e este
LookAndFeel não desenha título. O menu de contexto do display perdeu o cromo do
sistema: carvão, itens de 11 px com realce âmbar e separadores de 1 px.

## A marca, os visualizadores e o desfazer (26/10/2026)

### A marca saiu do header e o ícone passou a ser texto

O nome do instrumento morava no header do editor, na mesma fila do preset e do
ficheiro. Três blocos numa linha de 64 px empurravam o grupo para a esquerda: o
vazio ficava todo de um lado e o olho lia o título antes do que se opera. O nome
saiu de lá e subiu para a barra da janela, ao lado da marca, na fonte do título a
17 px; o preset e a origem ficaram num bloco único, centrado, e a largura do nome
do ficheiro passou a ser fixa em 250 px para a linha não saltar a cada ingestão.

O ícone deixou de ser três grãos num quadrado de 16 px — a 16 px ninguém lê três
grãos — e passou a ser o texto "Op" dentro da caixa laranja, na mesma Inter
Display SemiBold do nome. `OpcodaLookAndFeel::paintMark` desenha a marca e tira o
corpo da letra da altura da caixa, de sorte que a mesma função serve a barra da
janela (22 px), o cabeçalho do ecrã de ajustes (16 px) e o ícone da barra de
tarefas (224 px). Uma assinatura, um desenho. No VST3 não há barra da janela: o
editor fica sem assinatura, e é o preço de tirar o nome da única superfície que
existe nas duas formas.

Duas correções vieram de arrasto. O fundo da tecla `bar` era um banho de carvão a
45 % e deixava a chapa aparecer por trás; sobre a barra da janela o texto ficava
com duas sombras. A mistura passou a ser feita na cor, não na transparência. E o
glifo de ajustes tinha os três cursos com 0,11 de lado de intervalo — a 14 px os
puxadores fundiam-se num borrão; com 0,32 de intervalo e traço de 1,1 px os três
leem-se.

### As abas de módulo passaram a retângulos com nome

A pasta de 44 px com o glifo só, e o nome no tooltip, foi economia errada: seis
módulos com glifos de 13 px não se distinguem sem os ler, e o tooltip só chega
depois da dúvida. As seis teclas passaram a dividir a largura da chapa — 165 px
cada, com 6 px de folga — e a levar glifo e nome, como as teclas da rail. O par
glifo+nome é medido em `resized()`, com `GlyphArrangement`, e centrado como bloco;
medir em `paint` seria medir a cada quadro. A linguagem de pasta ficou: a ativa é
da cor do painel, desce a cauda de 6 px sobre ele e leva o filete âmbar; as
inativas ficam 2 px mais altas, com a linha de pé e a tinta fraca.

Isto substitui a largura de 44 px registrada em "Abas de pasta, marcadores e as
correções de 26/10/2026".

### O envelope e o filtro desenhados

Dois módulos deixaram de ser sete e dez knobs com o nome da função. `ENV` e
`FILTER` abrem com um ecrã afundado à esquerda, de 42 % do painel (entre 300 e
460 px), e os knobs recuam para a direita, menores: sete numa fila no envelope,
cinco mais cinco no filtro, para o nome de cada um caber.

No envelope, o eixo do tempo é comprimido pela raiz do valor — com ataque de
5 ms e decay de 2 s, a rampa linear faria do ataque meio pixel — e o patamar de
sustain leva 20 % da largura móvel. Os três segmentos amostram a mesma
`dsp::curveShape` que o motor usa, 40 pontos cada, e é por isso que a forma
desenhada é a que se ouve, e não uma parábola de enfeite. Três pegas de canto
mudam ataque, decay/sustain e release; três pegas a meio de cada segmento dobram
a curva do estágio. O arrasto horizontal dos tempos é relativo
(`valor · e^0,012·dx`), porque em escala absoluta um ataque curto não sai do
sítio. O ponto vivo vem do áudio — `gateLevel`, fase do estágio e progresso do
estágio —, publicado no `Telemetry` uma vez por bloco, e não de um cronômetro da
interface: o ponto mostra o envelope que está a soar, com o atraso de um bloco.

No filtro, a curva sai de `dsp::makeBiquadCoeffs` com o tipo, o corte e o Q dos
knobs, avaliada em 96 pontos log-espaçados de 20 Hz a 20 kHz e em dB entre +18 e
−48; 24 dB é a mesma magnitude ao quadrado, que é o que o motor faz ao passar o
biquad duas vezes. As duas pegas mudam corte (x) e Q (y). A pega do filtro 2 é um
aro vazado: com os dois cortes no mesmo sítio — 20 kHz nos dois é o estado de
fábrica — a pega cheia do filtro 1 escondia a do filtro 2. Uma linha fina desenha
a resposta com o corte já modulado por keytrack, envelope e LFO, que é o que o
áudio está a fazer.

O custo é repintura a 60 Hz com 96 pontos e dois biquads por ponto, no thread de
interface, e só quando o visor está aberto; nenhuma conta nova entrou no
`processBlock`. Fica de fora: envelope por voz (o motor aplica o ADSR ao mix, e o
desenho mostra o mix), automação das curvas por gesto no visor (as curvas são
parâmetros, automatizáveis pelo host como os outros) e o teste `Ui.*`
correspondente, que continua não implementado.

### O desfazer da interface

`Ctrl+Z` desfaz pelo editor, com um retrato do estado por gesto: 32 passos, e o
que separa dois gestos é 400 ms de silêncio nos eventos de parâmetro. O retrato é
o `stateXml()` inteiro — parâmetros, caminho do binário e região — e o passo
guardado é sempre o de antes do gesto começar, por isso desfazer devolve o valor
anterior e não o penúltimo tique de um arrasto.

Na reposição, o caminho é escolhido pelo custo: se o binário e a região do
retrato são os atuais, basta `replaceState` nos parâmetros; se mudaram, o retrato
é aplicado por `loadStateXml` e o material é relido. A guarda `restoringUndo_`
impede que a própria reposição entre na pilha. Não há refazer, a pilha morre com
o editor, e no Ableton quem trata do `Ctrl+Z` é o host — a pilha local existe para
o *standalone*, que não tem outra.

### O MIDI do host acende o teclado

O teclado virtual mostrava o que se tocava com o rato e com as teclas do
computador, não o que chegava do *host*. O áudio publica agora um bitmask de 128
teclas premidas, em dois `uint64` (um `atomic` de 128 bits não existe), e só o
publica em blocos que trouxeram MIDI; a interface lê a 60 Hz e espelha no
`MidiKeyboardState` o que mudou. O pedal não entra no bitmask: a tecla que subiu
com o pedal em baixo continua a soar e não pode continuar acesa. O espelho escreve
no estado com uma guarda (`mirroringNotes_`) para o listener não devolver a nota ao
motor, que já a recebeu pelo `MidiBuffer` — sem ela, cada nota do *host* soaria
duas vezes. `NoteTracker::isHeld` passou a público para a pergunta ser a mesma nos
dois lados, e tem ensaio próprio.

### O que ficou por provar

A verificação destes três desenhos foi por captura de ecrã e por build, não por
teste: os testes `Ui.*` da F023 continuam não implementados, e o alvo `tests/` é
núcleo puro sem JUCE. O que ganhou ensaio foi o que está do lado do núcleo — o
`isHeld` do `NoteTracker` —, e o resto do caminho, do bitmask ao espelho, foi
lido e não medido.

### O ícone, a grelha de bytes e o zoom da entropia (27/10/2026)

O ícone é a onda que os bytes fazem: silhueta espelhada laranja (`palette::accent`,
`#D8792F`) sobre a chapa preta do display (`#0D0C0A`), cantos arredondados, e mais
nada — sem linha de entropia, sem cabeça de leitura, sem marcadores. A forma vem de
uma tabela fixa de 32 amplitudes que imita o que o display mostra num `.exe`: o
início baixo dos cabeçalhos, o pico do cabeçalho PE, o corpo de código denso e a
cauda de dados a morrer. O `tools/make-icon.ps1` desenha os PNG a partir dessa
tabela, e o build tem dois caminhos para o mesmo desenho: o `ICON_BIG` e o
`ICON_SMALL` no `juce_add_plugin` fazem o `juceaide` montar o `.ico` (16, 32, 48 e
256) e injectá-lo no executável, e o PNG de 256 vai por *binary data* para o
`makeWindowIcon` do standalone, que é o ícone da janela e da barra de tarefas. O
alvo de dados chama-se `OpcodaAssets` e tem espaço de nomes próprio: os dois alvos
de *binary data* declaram `namedResourceListSize` no mesmo espaço e a redefinição
partia a compilação de quem incluía os dois cabeçalhos.

A grelha hexadecimal perdeu a faixa de entropia de 58 px. A faixa era um segundo
mapa do ficheiro inteiro, e o ficheiro inteiro já vive no *overview* do pé do
display; o que ela fazia, na prática, era partir o ecrã em duas metades que
competiam pela mesma largura. Sem ela, o hex ocupa o ecrã todo, os 16 bytes correm
num bloco contínuo — o vão a meio da linha sugeria dois grupos que não existem — e
entram duas colunas: ASCII e ASM.

A coluna ASM desmonta a partir do topo da janela. A instrução que cruzou a
fronteira do topo sai cortada e a que continua na linha de baixo mostra só `...`,
como em qualquer *dump*; não há tentativa de adivinhar o alinhamento antes do topo.
A coluna só aparece em material de 64 bits, decidido pelo `formatTag` do parser:
em PE32 os comprimentos seriam mentira a partir da primeira instrução com opcode de
32 bits.

O descodificador é novo e vive no núcleo, em `src/opcoda_core/disasm`: C++20 puro,
sem JUCE, sem excepção, sem alocação — o texto sai num `std::array<char, 80>` e o
comprimento nunca passa de 15 nem lê para além do buffer. Cobre o conjunto inteiro
(aritmética, mov, lea, cmp, test, shifts, grupos 3 e 5), o controlo de fluxo
(call, jmp, jcc, loop, ret), os `cmov`/`setcc`, o `movsxd`, o `movzx`/`movsx` e os
movimentos SSE mais comuns. AVX (VEX e EVEX) e os escapes `0F 38`/`0F 3A` caem em
`db` e a leitura volta a sincronizar no próximo opcode que reconhece; o
comentário do cabeçalho diz isso mesmo, porque uma coluna de orientação não se
pode vender como desmontador de referência. O ensaio prende o que interessa: 12
casos de comprimento e mnemónica exactos, e duas varreduras — 4000 buffers
aleatórios onde o comprimento tem de caber no buffer, e uma varredura linear que
tem de avançar sempre, para o desenho nunca ficar preso num comprimento zero.

A vista de entropia tinha o zoom quebrado, e a causa era aritmética. A entropia de
Shannon de uma coluna de um byte é zero por definição: com o zoom fundo, a
redução passava a uma coluna por byte e a curva inteira desabava para a base,
exactamente quando a pessoa se aproximava para ler mais. A meio zoom o problema
era mais discreto e igualmente falso: uma coluna de 14 bytes nunca chega a 8 bits,
e o eixo de 0 a 8 mentia para baixo em todo o gráfico. A vista passou a ter
colunas próprias, com um mínimo de 256 bytes cada (`log2(256) = 8`, o topo do eixo
continua alcançável em qualquer zoom) e cache separada do cache da forma de onda,
que quer resolução máxima para o envelope. A curva desenha-se pelos centros das
colunas com as pontas esticadas até às bordas do gráfico: com três colunas o
desenho continua a cobrir a largura toda, em vez de morrer a meio.

O que ficou por provar nesta ronda: a coluna ASM, nos ficheiros com AVX no
caminho quente, mostra `db` e volta a sincronizar; isso é orientação ao olho, não
engenharia reversa.

### Superfícies de leitura, onda de uma tinta e ASM com cor (27/10/2026)

A grelha de bytes tinha três desenhos a competir na mesma linha. A grade do
endereço e dos bytes corria de margem a margem, e as colunas ASCII e ASM eram
faixas translúcidas pintadas por linha: a linha atravessava-as por dentro, e o
resultado lia-se como uma tabela com buracos em vez de três superfícies. Agora a
grade morre na margem do bloco de bytes, e ASCII e ASM são caixas opacas com
contorno arredondado, pintadas uma vez em `paintReadingColumns`, por baixo das
linhas. Nenhuma linha lhes passa por trás.

As linhas de bytes também perderam a cor. Saíram as listras alternadas, o
escurecimento do que estava fora da região e os dois filetes laranja que
delimitavam a região; a parede de dígitos lê-se toda no mesmo marfim, e o âmbar
fica reservado ao byte da cabeça de leitura — a selecção. Quem diz onde começa e
acaba cada secção é o nome da secção no endereço, e o mapa do ficheiro no pé do
display.

A coluna ASM passou a ler-se como um desmontador. A monoespacada vem do sistema
(`Font::getDefaultMonospacedFontName()`, Consolas no Windows) porque a largura
fixa é a informação: o mnemónico alinha na vertical de linha para linha. O texto
é decomposto em símbolos e cada classe leva a sua tinta — mnemónico em âmbar,
registos em marfim, números e destinos em verde-pálido, pontuação apagada, e
`db`/`bad` em cinza, para o que o descodificador não reconheceu não se confundir
com código lido. A fonte embebida continua a ser só a Inter Display: uma coluna
de texto não justifica juntar um TTF ao binário.

A forma de onda perdeu as camadas. Havia gradiente quente, sombra projectada,
contorno de marfim e um núcleo de rms sobre a mesma silhueta; em material de
ruído, tudo junto lia-se como uma nuvem laranja sem borda. Ficou uma tinta só: o
corpo a 15 % de marfim e um filete a 78 % a fechar a forma, como o envelope de um
clip no Ableton. A reamostragem passou a correr ao dobro da largura em pixéis, e a
meia altura desceu para 47 % do gráfico — com o envelope a encostar ao limite, o
filete era cortado pelo topo e só os entalhes apareciam. O `pixelRms_` e o
`paintRmsSilhouette` saíram com o resto.

A rail de vistas voltou a ser uma tira de teclas simples: 38×32, só o glifo, sem
o nome ao lado. A largura caiu de 96 para 54 px, com 10 px de compasso entre
teclas, e os px foram para o display. O nome falado continua no tooltip e no
leitor de ecrã, e o alvo não desceu abaixo dos 24 px do WCAG 2.5.8. O mesmo
desenho serve as quatro vistas e o trio de zoom.

A marca "Op" saiu de toda a interface: da barra da janela (que passa a dizer só
"Opcoda") e do cabeçalho dos ajustes. A caixa laranja era uma segunda assinatura
ao lado do nome e do ícone da janela, e a identidade já vem dos dois. Os quatro
parafusos decorativos dos cantos do chassi também saíram - `paintScrew` e as
quatro chamadas.

O `ChoiceControl` dá o passo seguinte na hierarquia: cada parâmetro discreto
passa a ter um componente que guarda um `juce::Slider` invisível como fonte de
valor — o `SliderParameterAttachment` continua a ser o único caminho para o
parâmetro, o componente escreve nele e as teclas apenas espelham o índice. Há
quatro modos, a saber, tira de ícones, botões de texto curto, chave de dois
estados e cursor com dentes, e nenhum deles abre estado paralelo ao APVTS. A
escolha de um `Slider` oculto em vez de um `ButtonParameterAttachment` é
deliberada: o attachment de botão só mapeia 0 e 1, e uma escolha de quatro ou
cinco valores ficaria a meio. O `Knob` guarda a variante antiga, com menu, para
os casos em que o rótulo longo não cabe numa tira de glifos.

### O grão até 2 s e o piso do transporte (27/10/2026)

O `SIZE` parava em 100 ms. Não havia dead zone: o teto do knob era o teto do
motor, e os dois números viviam casados em `kMaxGrainMs` e no `NormalisableRange`
de `grain`. Quem quisesse um grão de meio segundo não tinha para onde girar. O
teto passa a 2 s, que é o que se espera de um granular moderno, e o knob ganhou
escala em milissegundos com centro em 100 ms (`setSkewForCentre`), senão os 40 ms
do default moravam nos primeiros 2 % do curso e o resto da viagem era só material
longo. O valor guardado, automatizado e nos presets continua em milissegundos, e
tudo até 100 ms lê exatamente como antes.

No motor, `kMaxGrainSamples` passou a 768 mil — 2 s mesmo a 384 kHz, a maior taxa
que um host entrega —, e o limite que o utilizador vê é o de milissegundos. O
recorte de "no máximo metade do material" não mudou: um grão de 2 s numa fonte de
3 s continua a encolher, e o envelope acompanha.

O transporte tinha o piso amarrado ao maior grão por um `static_assert`: 100 ms de
piso porque 100 ms era o grão. Com 2 s, o piso herdaria 2 s e a região de 64 bytes
— 1,3 ms — seria esticada mil e quinhentas vezes, o que anula o critério de
duração natural da F008. O piso de 0,1 s é uma decisão de audição e ficou fixo; o
`static_assert` saiu, porque prender outra vez os dois números voltaria a acoplar
o que precisa de divergir.

`getTailLengthSeconds` passa a devolver 2 s. É o tempo que um grão já nascido tem
para acabar depois de o gate fechar; sem a cauda declarada, o host cortava o som
do `STOP` a meio do grão.

**O grão de 2 s não durava 2 s.** Ao medir a vida do grão apareceu um defeito
antigo: o `processBlock` renderizava as vozes antes de semear as novas e voltava a
renderizar todas depois. A voz que já estava ativa via `writeIndex` voltar a zero
e avançava um segundo bloco inteiro, somado ao primeiro. Cada grão consumia dois
blocos de vida por bloco — um grão de 1,5 s morria em 0,75 s, e a saída somava
dois trechos distintos do material. Com grãos de 100 ms o erro passava por "grão
curto"; com 2 s ficou impossível não ver. O segundo passe só renderiza as vozes
que nasceram no bloco (`bornThisBlock`), e `AGrainLivesAsLongAsItsLength` conta os
blocos de um grão de 200 ms — cerca de 35 blocos de 256 amostras — para o defeito
não voltar.

No mesmo dia, `releaseResources` passou a soltar a fonte do motor
(`setSource(nullptr, 0)`) antes de `owned_.clear()`. O motor guarda um ponteiro
cru para o buffer de bytes, e libertar o buffer deixava o próximo
`prepareToPlay` a ler memória que já não era dele.

**A nuvem estava congelada.** O mesmo trabalho de medição mostrou que o spray, o
detune e o spread não variavam: `startGrain` montava o estado do gerador como
`seed_ + voz` a cada grão, então cada voz repetia exatamente os mesmos três
sorteios. Com oito vozes, a "nuvem" era uma roda de oito valores. Passou a haver
um serial por grão (`grainSerial_`, reiniciado no `prepare` e no `setSeed`), e o
estado sai de `seed_ + ouro × (voz + serial × 8 + 1)`. A mesma semente continua a
dar a mesma nuvem; o que muda é que agora ela varia no tempo.

Três ensaios acompanharam o teto. `RespectsGrainSizeBounds` pede 5 s com uma
fonte de 5 s — uma fonte curta escondia o recorte de milissegundos atrás do
recorte de metade do material — e exige 88200 amostras a 44,1 kHz, os 2 s
literais; escrito a partir da constante, aceitaria qualquer teto novo.
`LongGrainStaysAliveForTheWholeLength` mede a 1,9 s do arranque, 0,9 s depois do
único grão que nasceu, e falha se o teto voltar atrás ou se o motor voltar a
consumir dois blocos por bloco; `WindowScaleClosesTheEnvelopeOnTheLastSample`
percorre os comprimentos de 2 a 4096 e exige que o último índice caia em 4095.
`factory_presets` passou a aceitar até 2000 ms. Os dois ensaios de drive
(`CeilingStaysOpenBelowSixDb`, `CeilingInterpolatesWithTheKnob`) passaram a usar
uma nuvem densa e a medir só depois de o ganho de drive assentar: com um grão
renderizado por bloco a soma já não chegava ao tecto, e medir a subida apanhava
um pico anterior ao próprio tecto. O `character_test` foi o único que apanhou a mudança de forma
inesperada: media a terça harmónica do `DRIVE` com um grão de 400 ms que o teto
antigo recortava para 100, e com 400 ms reais os grãos sobrepunham-se e a razão
caía. O teste passou a pedir 100 ms de propósito — ele mede o `tanh` do filtro, e
não a sobreposição de janelas, e assim não muda se o teto crescer outra vez.
