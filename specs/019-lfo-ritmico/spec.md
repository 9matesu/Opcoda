# 019 · LFO rítmico

**Feature Branch**: `019-lfo-ritmico`
**Criada**: 2026-10-19
**Status**: Implementado — ensaios verdes; falta confirmar o seguimento do BPM ao vivo na sessão T4
**Objetivos atendidos**: OE4 (modulação)
**Ensaios**: portões B e C

**Alvo**: F019, do mapa da emenda 1.2.0

## Contexto

O LFO atual (`lfo.h`) corre em tempo absoluto e em Hz: `lforateHz`, mais
profundidade, destino e forma. Quatro parâmetros, cinco formas incluindo S&H. Em
estúdio, um LFO em Hz obriga o músico a calcular a divisão de memória — 1/8 a
120 BPM é 4 Hz, 1/8 pontuado é 6 Hz — e não acompanha a mudança de andamento.

O núcleo não conhece JUCE e não pode consultar o transporte. O caminho é o que a
emenda fixou: o plugin lê BPM e posição em ppq **uma vez por bloco**, e entrega-os
ao motor como dois doubles. O resto é aritmética no núcleo.

`lfosteps` é o que separa este LFO de um LFO de sintetizador dos anos 90: quantiza
a forma em degraus, o que produz o padrão "stepped" que o Vital popularizou e que
em sample-and-hold não é o mesmo — S&H sorteia por ciclo, `steps` divide o ciclo
em N patamares determinísticos.

## Cenários

### História 1 — Sincronizar com o andamento (P1)

Como produtor, quero escolher 1/8 e que o LFO acompanhe o andamento do projeto,
para que a modulação fique no tempo da música quando eu mudo o BPM.

**Por que P1**: é a razão de existir da feature. Um LFO que não acompanha o BPM é
o que já existe.

**Teste independente**: `Lfo.SyncedEighthAt120BpmIsFourHertz` e
`Lfo.SyncedEighthAt140BpmIsFourPointSixSixSevenHertz` confirmam a conversão com
tolerância de 1e-3 Hz.

### História 2 — Pontuada e tercina (P2)

Como produtor, quero 1/8 pontuado e 1/8 tercina, porque é onde nasce o groove.

**Teste independente**: `Lfo.DottedIsThreeHalves` e `Lfo.TripletIsTwoThirds`
verificam os fatores sobre a divisão base.

### História 3 — Degraus (P2)

Como produtor, quero quantizar o LFO em 4 ou 8 degraus, para obter um padrão
repetitivo em vez de uma onda suave.

**Teste independente**: `Lfo.StepsQuantizesIntoFlatPlateaus` verifica que, com
`steps = 4`, a saída assume exatamente 4 valores distintos por ciclo.

### História 4 — Sem transporte (P3)

Como utilizador do Standalone sem transporte, quero que o LFO sincronizado caia
para 120 BPM em vez de parar.

**Teste independente**: `Lfo.SyncWithoutTransportUsesFallbackBpm` confirma 120 e
que o LFO continua a oscilar.

## Requisitos

1. **Dois doubles por bloco.** `setTransport(bpm, ppqPosition)` no motor, chamado
   no início do `processBlock`. Sem alocação, sem chamada ao host depois disso.
2. **Modo.** `lfosync = Free` usa `lforateHz`; `lfosync = Sync` usa `lfodiv` e o
   BPM. Mudar de modo não reinicia a fase.
3. **Divisões.** `1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64` mais `1/4., 1/8.` e
   `1/4T, 1/8T, 1/16T`. Fatores: `1,5` para pontuada, `2/3` para tercina.
4. **Degraus.** `lfosteps` de 1 a 16; 16 significa "sem quantização" (valor
   contínuo). A quantização é `floor(phase * steps) / steps`, aplicada à forma
   depois de gerada.
5. **Fase contínua.** A fase avança por amostra a partir do tempo, e não é
   reiniciada por nota: um LFO que reinicia a cada nota não é um LFO, é um
   envelope.
6. **Deriva declarada.** Com `Sync`, a fase é derivada de `ppq`, o que faz o LFO
   saltar se o host reposicionar o transporte. É o comportamento pretendido, e é
   testado.
7. **Sem transporte.** `bpm <= 0` ou `ppq` inválido cai para 120 BPM e tempo
   absoluto, sem exceção e sem divisão por zero.

## Critérios de aceite

- [x] 1/8 a 120 BPM = 4,000 Hz; a 140 BPM = 4,667 Hz;
- [x] 1/8 pontuado = 2,667 Hz a 120 BPM; 1/8 tercina = 6,000 Hz a 120 BPM;
- [x] `steps = 4` produz 4 patamares por ciclo;
- [x] mudar de `Free` para `Sync` não produz descontinuidade audível na fase;
- [x] sem transporte, o LFO continua a correr a 120 BPM;
- [x] nenhuma alocação no callback (guard verde).

## Não-objetivos

- **Múltiplos LFOs.** Só existe um, e a matriz da F020 pode encaminhá-lo.
- **Rampa e fase ajustável.** Fica para depois; a fase é sempre derivada do tempo.
- **Sync por nota.** O LFO é global, como o envelope.

## Testes

| Teste | O que prova |
| --- | --- |
| `Lfo.SyncedEighthAt120BpmIsFourHertz` | conversão base |
| `Lfo.SyncedEighthAt140BpmIsFourPointSixSixSevenHertz` | acompanha o BPM |
| `Lfo.DottedIsThreeHalves` | pontuada |
| `Lfo.TripletIsTwoThirds` | tercina |
| `Lfo.StepsQuantizesIntoFlatPlateaus` | degraus |
| `Lfo.SyncWithoutTransportUsesFallbackBpm` | ausência de transporte |
| `Lfo.ModeChangeKeepsPhaseContinuous` | continuidade |
| `Lfo.SyncFollowsTheHostPosition` | reposicionamento do transporte |
