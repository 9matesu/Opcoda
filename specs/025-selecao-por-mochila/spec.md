# 025 — Seleção por mochila

**Feature Branch**: `025-selecao-por-mochila`
**Criada**: 2026-10-22
**Status**: Implementado (núcleo, testes e painel OPT); falta ensaio auditivo
**Objetivos atendidos**: OE4/OE5 (escolha de material por entropia)
**Ensaios**: portões B, C, D e E

**Alvo**: F025, do mapa da emenda 1.3.0

## Contexto

A região que toca é uma janela contínua do binário. A entropia já modula o
spray, mas não escolhe *quais* partes do ficheiro entram na sonificação. A
seleção por mochila faz essa escolha: divide a região em blocos, mede cada
bloco por entropia de Shannon e proporção de zeros, e escolhe um subconjunto
que maximize o valor total sem passar da capacidade.

A referência é o aplicativo C# `src/OpcodaOtimizacao/`, que implementa os três
métodos sobre um problema de mochila 0/1. A F025 porta o algoritmo para o
núcleo C++20 e liga o resultado ao motor existente. O aplicativo C# não é
dependência de runtime nem é distribuído com o plugin.

## Cenários

### História 1 — Escolher os blocos mais informativos (P1)

Como produtor, quero que a sonificação use os blocos com mais entropia e menos
bytes zerados, para ouvir variedade em vez de silêncio.

**Por que P1**: é a feature. Sem a escolha, o painel OPT não tem o que publicar.

**Teste independente**: `Knapsack.PrefersHighEntropyBlocks` constrói uma região
com um bloco de alta entropia e outro de zeros e confirma que o primeiro é
escolhido.

### História 2 — Controlar o custo da seleção (P2)

Como produtor, quero limitar o tamanho do bloco e a capacidade total, para que
a otimização caiba no tempo e no material que quero ouvir.

**Teste independente**: `Knapsack.ClampsCapacityFraction` e
`Knapsack.EveryMethodRespectsCapacity` confirmam que a capacidade é limitada a
`[0, 1]` e que nenhum método devolve solução acima dela.

### História 3 — Escolher o método de busca (P3)

Como produtor, quero alternar entre subida de encosta, multi-tentativas e
têmpera simulada, para comparar qualidade e custo.

**Teste independente**: `Knapsack.MultiRestartIsNotWorseThanSingleClimb`
confirma que a multi-tentativas não devolve valor inferior à subida pura com a
mesma semente.

### História 4 — Ouvir o material escolhido (P4)

Como produtor, quero que o botão OTIMIZAR substitua a região atual pelo material
concatenado dos blocos escolhidos, mantendo o caminho de áudio em tempo real.

**Teste independente**: `Knapsack.ConcatenatesSelectedBlocksInOrder` confirma
que a concatenação respeita a ordem dos blocos na região e que o buffer tem
exatamente a soma dos tamanhos escolhidos.

## Requisitos

1. **Núcleo puro.** `src/opcoda_core/optimization/knapsack.{h,cpp}` não inclui
   JUCE e compila no alvo `opcoda_core`.
2. **Problema a partir da região atual.** O pool de candidatos é `byteRange_`
   (a região que está a soar), não o ficheiro inteiro. O tamanho de bloco é
   configurável (default 1 KiB), com no máximo 512 blocos; se a região exceder
   esse limite, o bloco efetivo cresce para caber.
3. **Valor e peso.** O valor de cada bloco é
   `entropiaShannon × (1 − proporçãoDeZeros)`; o peso é `tamanhoBytes / 1024`.
   A capacidade é `fração × peso total`, com a fração limitada a `[0, 1]`.
4. **Três métodos.** Subida de encosta, multi-tentativas (TMAX) e têmpera
   simulada (Ti, Tf, alfa), todos determinísticos dada a mesma semente.
5. **Solução sempre viável.** O resultado tem peso menor ou igual à capacidade,
   ou é inválido (`Result::valid == false`), sem exceção; nenhum método pode
   devolver solução acima da capacidade.
6. **Concatenação antes do motor.** Os blocos escolhidos são concatenados na
   ordem da região, num buffer novo, e publicados pela fila SPSC existente. O
   callback de áudio não aloca, não trava e não faz I/O.
7. **Painel OPT.** A interface ganha a aba `OPT` com método, `BLK`, `CAP`,
   `TMAX`, `SEED`, `TI`, `TF`, `ALFA`, botão `OTIMIZAR`, estado e métricas
   (valor, peso, blocos, iterações e tempo). Campos fora da faixa são limitados
   aos valores válidos; falha da otimização aparece em texto, nunca só por cor.
8. **41 parâmetros continuam fechados.** A otimização não acrescenta parâmetros
   à APVTS; as configurações do painel vivem na sessão da interface.
9. **Destaque dos blocos.** A grelha de bytes destaca os blocos escolhidos; um
   novo carregamento ou uma nova região limpa o destaque e o estado da
   otimização.
10. **Sem dependência do C#.** O aplicativo `src/OpcodaOtimizacao/` permanece
    como referência e não entra no build do plugin.

## Critérios de aceite

- [x] `build.ps1` compila o núcleo e roda a suíte completa;
- [x] `Knapsack.BuildsOneItemPerBlock` e `KeepsLastPartialBlock` provam a
      divisão da região, inclusive o último bloco parcial;
- [x] `Knapsack.RejectsInvalidRegion` e `InvalidProblemProducesInvalidResult`
      devolvem resultado inválido em vez de exceção;
- [x] `Knapsack.BuildsProblemForSubRegion` prova que o pool respeita
      `start`/`end` e não o ficheiro inteiro;
- [x] `ClampsCapacityFraction` limita a fração a `[0, 1]`;
- [x] `EveryMethodRespectsCapacity` cobre os três métodos;
- [x] `SameSeedProducesSameSelection` prova o determinismo da têmpera;
- [x] `PrefersHighEntropyBlocks` prova a fórmula de valor;
- [x] `MultiRestartIsNotWorseThanSingleClimb` prova que TMAX não piora o
      resultado da subida pura;
- [x] `ConcatenatesSelectedBlocksInOrder`, `ConcatenateRejectsMismatchedSolution`
      e `ConcatenateRejectsInvalidInput` provam a publicação do material;
- [x] o guard de alocação continua verde: a otimização corre na message thread
      e o callback só consome a fila;
- [x] `build.ps1 -Release` gera VST3 e Standalone e passa a guarda de CRT;
- [ ] **por ouvir**: comparar os três métodos num host real e confirmar que a
      seleção soa diferente da região contínua (sessão T4).

## Não-objetivos

- **Mochila exata por programação dinâmica.** Os três métodos são busca local,
  como na referência C#; a solução ótima não é prometida.
- **Novos parâmetros automatizáveis.** A tabela continua em 41.
- **Guardar a solução no preset.** O destaque e as métricas são de sessão; o
  estado do projeto continua a guardar o ficheiro e a região.
- **Otimizar o ficheiro inteiro.** O pool é a região atual.
- **Playhead mapeado por bloco.** Com a otimização ativa, o transporte percorre
  o material concatenado, mas o display continua a mostrar a região original;
  o destaque identifica os blocos escolhidos. O mapeamento fino do playhead
  para o bloco concatenado fica para uma iteração de UI.

## Limitações declaradas

- **Custo na message thread.** Com 512 blocos e os valores por omissão, a
  subida fica abaixo de 20 ms e a multi-tentativas abaixo de 200 ms; `TMAX` no
  máximo chega a cerca de 2 s e a interface espera. Não há thread de trabalho
  nesta iteração.
- **Fila de retorno cheia.** `drainIncomingQueue` ignora um `push` recusado,
  comportamento anterior à F025. O buffer antigo permanece em `owned_` até
  `releaseResources`; com o editor aberto, a fila é drenada a 60 Hz e o caso é
  teórico.
- **Falha com seleção ativa.** O material e as métricas anteriores ficam,
  porque o que está a soar não mudou; o erro aparece em texto no painel.

## Testes

| Teste | O que prova |
| --- | --- |
| `Knapsack.BuildsOneItemPerBlock` | um item por bloco, com valor e peso |
| `Knapsack.GrowsBlockToHonourMaxBlocks` | bloco efetivo cresce para caber em `maxBlocks` |
| `Knapsack.BuildsProblemForSubRegion` | offsets e limites respeitam `start`/`end` |
| `Knapsack.KeepsLastPartialBlock` | último bloco parcial não é descartado |
| `Knapsack.RejectsInvalidRegion` | região vazia ou fora do buffer |
| `Knapsack.ClampsCapacityFraction` | capacidade presa a `[0, 1]` |
| `Knapsack.EveryMethodRespectsCapacity` | viabilidade nos três métodos |
| `Knapsack.SameSeedProducesSameSelection` | determinismo com a mesma semente |
| `Knapsack.PrefersHighEntropyBlocks` | fórmula entropia × (1 − zeros) |
| `Knapsack.MultiRestartIsNotWorseThanSingleClimb` | TMAX não piora |
| `Knapsack.ConcatenatesSelectedBlocksInOrder` | ordem e tamanho do material |
| `Knapsack.ConcatenateRejectsMismatchedSolution` | solução de outro problema |
| `Knapsack.ConcatenateRejectsInvalidInput` | buffer nulo ou bloco fora do ficheiro |
| `Knapsack.OptimizeAcceptsEmptyProblem` | problema válido sem blocos |
| `Knapsack.InvalidProblemProducesInvalidResult` | inválido, sem exceção |

## Integração

- `src/opcoda_plugin/source/plugin_processor.cpp`: `optimizeSelection`,
  `publishMaterial` e `makeEntropyCurve`; o resultado é publicado pela mesma
  fila usada pela ingestão.
- `src/opcoda_plugin/source/plugin_editor.cpp`: painel OPT, leitura dos campos,
  execução e refresh das métricas.
- `src/opcoda_plugin/source/hex_grid.cpp`: `setOptimizedBlocks` e pintura dos
  blocos escolhidos.
- `src/opcoda_plugin/source/byte_display.cpp`: encaminha o destaque para a
  grelha.
