# 003 · Troca de material entre interface e áudio

**Feature Branch**: `003-troca-material`
**Criada**: 2026-10-01
**Status**: Implementado
**Objetivos atendidos**: OE2 (completado), OE3, OE8
**Ensaios**: portão C

## Contexto

A F002 entregou o caminho do binário ao som, mas com uma mentira: o plugin
chamava `engine_.setSource` direto da thread de interface. Isso viola o Princípio
II da constitution, que exige que nada do estado do DSP seja tocado fora do
caminho de áudio. A `SpscRing` existia, testada, e sem uso.

Esta feature liga a fila no caminho real e dá ao Standalone um jeito de carregar
um arquivo. Fecha a entrega de áudio da S1: agora o instrumento é usável por
uma pessoa, e não só por um teste.

## Cenários

### História 1 — Carregar um binário pelo Standalone (P1)

Como usuário, quero escolher um `.exe` numa janela e ouvir o resultado, para usar
o Opcoda sem passar por um DAW.

**Teste independente**: `cmake --build --preset standalone` gera `Opcoda.exe` com
o editor, e a cadeia de áudio é a de `EndToEnd.RealExecutableBecomesAudio`.

**Aceitação**:

1. **Dado** o Standalone aberto, **quando** clico em "Abrir .exe ou .bin" e
   escolho `notepad.exe`, **então** o rótulo mostra o nome do arquivo em até um
   segundo
2. **Dado** um arquivo inválido, **quando** o escolho, **então** o rótulo mostra
   o código, como `Recusado: E_BAD_MZ`, e o material anterior continua tocando
3. **Dado** um arquivo válido, **quando** a thread de áudio roda, **então** o
   material novo substitui o anterior sem clique e sem parar

### História 2 — A troca não aloca na thread de áudio (P1)

Como mantenedor, quero que trocar de material não custo alocação, porque
alocar no callback derruba o host.

**Teste independente**: `SampleExchange.AudioThreadAllocatesNothingDuringSwap`
roda duas threads reais, 200 trocas de material e um processBlock por iteração,
com o guard de alocação ativo.

**Aceitação**:

1. **Dado** a janela de áudio marcada, **quando** troco o material e processo
   200 blocos, **então** o contador de alocações é zero
2. **Dado** a fila de entrada cheia, **quando** a interface publica, **então**
   recebe `E_BUSY` em vez de crescer
3. **Dado** um buffer antigo, **quando** a thread de áudio troca de material,
   **então** o ponteiro antigo só é liberado depois, pela interface

### História 3 — Os seis parâmetros chegam ao host (P2)

Como produtor, quero automatizar os parâmetros no DAW, porque é assim que um
instrumento se usa em produção.

**Teste independente**: a tabela de parâmetros é exposta pelo
`AudioProcessorValueTreeState`, e o VST3 carrega com seis parâmetros
automatizáveis.

**Aceitação**:

1. **Dado** o VST3 carregado no DAW, **quando** inspeciono os parâmetros,
   **então** vejo tamanho de grão, densidade, posição, spray, afinação e volume
2. **Dado** um parâmetro alterado pelo host, **quando** o próximo bloco roda,
   **então** o motor usa o valor novo

## Fora de escopo

- Arrasto e soltar, mapa de seções e curva de entropia na interface: F008
- MIDI learn e CC: F009
- Telemetria de vozes ativas para o rótulo: F007
- Visual Ableton-style com design system do Stitch: F008

## Decisões e armadilhas

**A fila carrega ponteiro cru, não `shared_ptr`.** Duas restrições apontam
para a mesma escolha, e obedecer as duas dá a propriedade. `SpscRing` exige
tipo trivial, e a asserção de `std::is_trivially_copyable` recusou o
`shared_ptr` na primeira tentativa: a asserção estava certa. E decrementar um
contador de referência na thread de áudio pode rodar o `operator delete` ali
dentro, o que é violação de tempo real.

**A memória é da thread de interface, com devolução em duas filas.** A
interface publica o ponteiro novo e guarda o buffer em `owned_`. A thread de
áudio troca o ponteiro ativo e devolve o antigo por uma segunda fila. Só a
interface, em `releaseReturnedBuffers`, remove o buffer de `owned_`, e só depois
que a thread de áudio confirmou que terminou.

Uma fila só seria mais curta e incorreta: sem a volta, a interface não sabe
quando é seguro liberar, e liberar cedo é use-after-free. O teste
`OldBufferIsOnlyReleasedAfterEngineMovedOn` verifica a invariante pelo lado de
quem segura a referência, porque a outra direção é o próprio bug: ler o buffer
depois de liberado, que foi exatamente o erro que a primeira versão do teste
cometeu e o AddressSanitizer pegou.

**`releaseReturnedBuffers` roda a 20 Hz, no timer do editor.** O
`AudioProcessorEditor` não herda `juce::Timer`, que é da `juce_events`; a
herança explícita é o que habilita `startTimerHz`.

**O diálogo é assíncrono, não modal.** `browseForFileToOpen` não existe por
padrão, porque `JUCE_MODAL_LOOPS_PERMITTED` é 0: travar a message thread
atrapalha o host. `launchAsync` é o caminho correto para plugin, com callback,
e `getResults()` no lugar de `getResult()` porque é a variante assíncrona que
devolve uma lista.

**O `SampleSwap` do núcleo ficou sem uso.** A dupla fila resolve a troca com
menos peças e reaproveita o `SpscRing` que já estava testado. Ele é
documentação de um padrão que não usamos, e código morto é dívida: a remoção
fica para a limpeza da F010, com o mesmo cuidado de apagar o que a
documentação ainda citar.
