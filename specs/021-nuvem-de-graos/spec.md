# 021 · Nuvem de grãos

**Feature Branch**: `021-nuvem-de-graos`
**Criada**: 2026-10-19
**Status**: Implementado (DSP); falta a visualização, que é F023
**Objetivos atendidos**: OE4 (dispersão e aleatoriedade dos grãos)
**Ensaios**: portões B e C

**Alvo**: F021, do mapa da emenda 1.2.0

## Contexto

`spray` hoje desloca a posição de leitura de cada grão dentro de um raio, e o
resultado é uma textura instável, mas ainda um fluxo: os grãos chegam ao ritmo da
densidade e o único acaso é onde cada um lê. A entropia modula o spray, e isso
está testado — mas o que falta é a **nuvem**: um conjunto de grãos que chega
junto, espalhado no tempo e na altura, em vez de um fluxo regular com posição
aleatória.

A diferença é musical e não é subtil. Um fluxo regular com spray soa a scrub; uma
nuvem soa a textura. O Vital e o Serum têm ambos o conceito, e o artigo pode
descrevê-lo sem inventar: a nuvem é o mesmo motor, com três graus de liberdade a
mais no momento em que o grão nasce.

## Cenários

### História 1 — A nuvem alarga o ataque (P1)

Como produtor, quero que um disparo produza um conjunto de grãos espalhados no
tempo, para obter textura em vez de um ponto.

**Por que P1**: é a feature. Sem isto, os outros dois parâmetros não têm onde se
aplicar.

**Teste independente**: `Cloud.SizeSpreadsOnsetsWithinWindow` confirma que, com
`cloudsize = 1`, os inícios dos grãos de um disparo se distribuem por até 500 ms,
e que com `cloudsize = 0` coincidem com o comportamento atual.

### História 2 — Dispersão de afinação (P2)

Como produtor, quero que cada grão da nuvem tenha uma afinação ligeiramente
diferente, para que a nuvem soe espessa.

**Teste independente**: `Cloud.PitchSpreadStaysWithinRange` verifica que nenhum
grão sai do intervalo declarado e que a distribuição é simétrica.

### História 3 — Jitter de duração (P3)

Como produtor, quero que os grãos da nuvem tenham durações diferentes, para
evitar o efeito de "metralhadora" quando a densidade é alta.

**Teste independente**: `Cloud.GrainJitterVariesDuration` mede o desvio-padrão
das durações com `grainjit = 1` e confirma zero com `grainjit = 0`.

## Requisitos

1. **`cloudsize`** de 0 a 1 mapeia para uma janela de 0 a 500 ms. O início de
   cada grão é deslocado uniformemente dentro da janela, e o grão seguinte não
   herda o deslocamento.
2. **`pitchspread`** de 0 a 24 semitons: deslocamento uniforme em
   `[-spread, +spread]`, somado a `pitch`, `pitchrand` e à nota, antes da
   conversão para taxa.
3. **`grainjit`** de 0 a 1: duração multiplicada por um fator uniforme em
   `[1 - 0.5·jit, 1 + 0.5·jit]`, com piso de 1 ms e teto de 100 ms.
4. **Zero é o comportamento atual.** Com os três a zero, o motor produz
   exatamente a mesma saída que antes da feature, o que é verificado por
   comparação amostra a amostra.
5. **Determinismo controlável.** O gerador pseudoaleatório tem semente fixa no
   teste e semente do relógio em produção; a semente é reiniciável para que o
   ensaio seja reprodutível. **Desvio declarado:** em produção a semente
   continua fixa (`kDefaultSeed`, a mesma de antes da F021). Semear com o
   relógio faria a mesma fonte e os mesmos knobs darem um som diferente em cada
   corrida, e é isso que T1 e T2 comparam; `setSeed()` existe para quem queira
   a nuvem diferente entre tomadas.
6. **Sem alocação.** Tudo avaliado no momento em que o grão nasce, com estado
   pré-alocado no motor.
7. **Entropia continua a modular o spray.** A nuvem não substitui a ligação
   existente; soma-se a ela.

## Critérios de aceite

- [x] `cloudsize = 0`, `pitchspread = 0`, `grainjit = 0` → saída idêntica à
      anterior, amostra a amostra (o teste compara zeros explícitos com os
      defaults e confirma que nenhum sorteio é consumido);
- [x] `cloudsize = 1` espalha inícios por até 500 ms;
- [x] `pitchspread = 12` não produz nenhum grão fora de ±12 st, e a
      distribuição é simétrica;
- [x] `grainjit = 1` produz durações entre metade e uma vez e meia, com piso e
      teto (a spec dizia "dobro"; o requisito 3 e o código usam 1,5×, e é essa
      a frase que vale);
- [x] a mesma semente produz a mesma nuvem em duas execuções;
- [x] guard de alocação verde com os três no máximo (o caminho de áudio só
      consome o gerador já existente e copia inteiros);
- [ ] **por ouvir**: a nuvem num host real (sessão T4). O binário mede atrasos,
      dispersão e durações; ninguém ouviu a textura ainda.

## Não-objetivos

- **Nuvem como segundo motor.** É o mesmo motor com três graus de liberdade.
- **Distribuição gaussiana.** Uniforme é o que se consegue testar sem discussão
  estatística; gaussiana fica como possível evolução.
- **Visualização da nuvem antes da F023.** A F021 entrega o DSP.

## Testes

| Teste | O que prova |
| --- | --- |
| `Cloud.SizeSpreadsOnsetsWithinWindow` | janela temporal, medida no atraso que o motor aplicou |
| `Cloud.ZeroIsByteIdenticalToPrevious` | ausência de regressão e nenhum sorteio a mais |
| `Cloud.PitchSpreadStaysWithinRange` | intervalo de afinação, simetria e uso da faixa |
| `Cloud.GrainJitterVariesDuration` | jitter de duração com média e desvio |
| `Cloud.SameSeedSameCloud` | determinismo e sensibilidade à semente |
