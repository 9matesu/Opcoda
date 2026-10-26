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
de metade do material, e a janela encurta com o envelope reescalado.

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
`processBlock` alocar, inclusive em 5 minutos de playback sintético. Isso é o
critério de tempo real virando código, e não revisão manual.

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
| `knob.h` | rótulo, `juce::Slider` rotativo, valor e dropdown dos *choices* |
| `display_panel.h` | fundo escuro com gradiente, brilho de topo e sombra interna, sem grelha |
| `byte_display.h/.cpp` | as três vistas do material, a caret, a faixa de entropia e o *preview* |
| `hex_grid.h/.cpp` | a vista de bytes: endereços de 8 dígitos, 16 colunas, ASCII e barra de deslocamento |
| `mod_matrix_view.h/.cpp` | a vista MATRIX: oito rotas com combos e barra bipolar, cada célula um attachment |
| `output_meter.h` | medidor stereo de saída, com pico por canal e escala em dB |
| `plugin_editor.h/.cpp` | o chassi fixo, as abas, o pé do display com o transporte, o teclado e o layout |

**Não há `assets.h` nem ficheiro de arte.** Desde 06/10/2026 o knob e o botão são
`Path` desenhados em código, e `resources/` foi apagado. A peça fotografada
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

O nome accessible de cada knob é a descrição completa em inglês
("Grain size, in milliseconds") e não o rótulo curto ("SIZE"), que
seria inútil para quem navega por leitor de tela. Os **66 parâmetros** vivem em
seis módulos (GRAIN, ENV, FILTER, MOD, CLOUD, KEYS), com uma faixa visível de
cada vez e sem páginas internas: a janela fixa de 1180×760 dá a altura para os
onze knobs de grão numa linha. A aba MAIN saiu porque repetia os onze parâmetros
de grão; a aba OUT saiu porque o volume é um knob mestre persistente na coluna
escura da direita, ao lado do medidor de saída. Cada controle aparece uma vez.
Os knobs de choice (window, tipos de filtro, inclinação do filtro, alvo, forma e
sincronismo do LFO, escala e tônica) usam o mesmo `SliderParameterAttachment` —
o JUCE entrega intervalo 0..N-1 com passo 1 para `AudioParameterChoice` — e o
caret ao lado do valor abre um `PopupMenu` com os rótulos reais; girar o knob
continua a responder à roda e às setas.

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
empilhar: cinco fileiras pediriam ~700 px e não caberiam na janela fixa.

### A janela fixa e as quatro vistas

O chassi fecha em 1180×760 e não aceita redimensionamento (`setResizable(false,
false)`). O display ocupa a faixa superior de 380 px, com uma rail vertical de
96 px à direita para as vistas e o zoom; as seis abas de módulo encostam
directamente no display, sem tira de navegação pelo meio; os knobs e a coluna
mestra de 116 px preenchem o meio; o teclado de 84 px e o rodapé fecham o
desenho. O layout foi escrito para estes números e não tem um caminho
alternativo — a guarda de `resized()` contra o primeiro rectângulo 0×0 é a única
excepção.

A grelha hexadecimal é contínua, com 16 bytes por linha, endereços de 8 dígitos,
coluna ASCII própria e uma barra de deslocamento de 10 px. O topo do hex é do
utilizador: `topByte_` só muda por roda, clique ou teclado, e a cabeça de
reprodução não arrasta a vista. A faixa de entropia, com 58 px, fecha o display
e é clicável e arrastável; o mesmo gesto na grelha move a posição. A barra
laranja marca apenas o endereço tocado; a região aparece como filete de 1 px no
topo e na base.

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
reamostragem para um máximo e um mínimo por pixel (`resampleEnvelope`) e um
máximo corrido de ±2 px sobre o resultado. A primeira mata o zigzag sub-pixel
que abria riscos verticais no preenchimento; a segunda fecha as valas de 1 px
que faziam o material aleatório parecer um pente. O corpo é uma silhueta com
gradiente quente, o rms é uma faixa cheia por dentro dela, e `waveLow` saiu de
todas as camadas da onda — o verde lia-se como ruído de outra tela.

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
rótulos passaram a 0, -12, -24, -36 e -48, com 8 px de corpo, centrados no traço
e limitados dentro da pista. O volume da coluna mestra cedeu 18 px ao medidor:
com a pista antiga, 0 e -6 dB ficavam a 8 px de distância e os números
encostavam uns aos outros.

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
- **`grainjit`** sorteia um fator em `[0,5×, 1,5×]` e recorta a 1–100 ms. A
  janela **não é reconstruída por grão**: a tabela é uma por bloco e os oito
  slots partilham-na, por isso o índice é escalado por
  `activeWindowLength / grainLength`. Com jitter 0 esse fator é exatamente 1,0
  e o índice é o de antes.

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
