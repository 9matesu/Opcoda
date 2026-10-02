# 16. Arquitetura de implementação

Este documento registra as decisões de implementação que não estão em
`06-metodologia-arquitetura.md`, que descreve a arquitetura proposta na fase de
escrita do TG. Aqui está o que o código faz de fato.

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
limitado para impedir aliasing audível na afinação máxima de +24 semitons.

A entropia local modula o spray: região de alta entropia espalha mais, região
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

Cinco arquivos, cada um com uma responsabilidade só:

| Arquivo | Responsabilidade |
| --- | --- |
| `palette.h` | os tokens do `design/DESIGN-SYSTEM.md`, em hex único |
| `look_and_feel.h` | desenho do knob e bisel do botão |
| `led.h` | indicador com brilho, quatro estados |
| `boxed_label.h` | rótulo com fundo, usado no valor e nos chips |
| `knob.h` | rótulo, `juce::Slider` rotativo e caixa de valor |
| `display_panel.h` | fundo escuro e grade de 24 px |
| `plugin_editor.h/.cpp` | as três faixas e o layout |

O editor tem três faixas: header claro, display escuro, painel de parâmetros. É a
convenção do Ableton, e é também a resposta ao pedido de brilho: halo laranja
sobre superfície clara dá 2,4:1 e desaparece, então toda a energia de glow fica na
região escura.

**Os knobs são `juce::Slider` de verdade, não desenho customizado.** Isso não é
padrão, é decisão: `Slider` entrega foco por teclado, ajuste com setas e
`AccessibilityHandler` de graça, que são exatamente os critérios da
`docs/10-acessibilidade-w3c.md`. Desenhar o knob à mão custaria tudo isso. O
`SliderParameterAttachment` cuida do undo por parâmetro, e por isso exige a
referência do `RangedAudioParameter`, não a da árvore inteira.

O nome accessible de cada knob é a descrição completa em português
("Tamanho de grao, em milissegundos") e não o rótulo curto do mock ("SIZE"), que
seria inútil para quem navega por leitor de tela.

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
display e do título do módulo não comporta dois knobs com rótulo e valor, e eles
despencam para poucos pixels. Os seis ficam sempre em uma linha, como no mock, e
o diâmetro do knob é limitado dentro do componente, então encolhe com a janela
sem precisar de um segundo layout.

### O que não está feito

O display está vazio de propósito nesta etapa: as abas de seção, a curva de
entropia com brilho e a telemetria de pico entram depois da aprovação do visual
do chassi. A telemetria vai exigir que o `PeImage` deixe de ser descartado no
`ingest`, porque a interface precisa do nome, do tamanho e da entropia de cada
seção.

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

## O teste que fecha a S1

`tests/end_to_end_test.cpp` percorre a cadeia completa com `notepad.exe` do
sistema, sem JUCE e sem host: ler disco, `parse`, `toSamples`, motor granular,
DC-blocker, limiter. É ele que prova que um executável real vira som, e ele
usou o material real porque os dois bugs mais caros do projeto só apareciam com
arquivo de verdade.

O `notepad.exe` da máquina tem 200 KB, sete seções e byte médio de 96 em
`.text`, ou seja, offset de −0,245 em amostra normalizada: material que o
DC-blocker tem o que remover e que a leitura sintética não reproduzia.
