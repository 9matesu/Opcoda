# 018 · Teclado, afinação e escala

**Feature Branch**: `018-teclado-e-afinacao`
**Criada**: 2026-10-19
**Status**: Em curso — DSP, escala e parâmetros prontos; teclado pendente (F023)
**Objetivos atendidos**: OE4 (afinação por nota), OE9 (MIDI), OE7 (teclado na
interface)
**Ensaios**: portões B, C e E

**Alvo**: F018, do mapa da emenda 1.2.0

## Contexto

Hoje a nota MIDI é só um gate. `NoteTracker::Event` guarda o `number`
(`note_tracker.h`), mas o motor nunca o lê: um Dó e um Fá produzem o mesmo som, e
o parâmetro `pitch` é a única forma de mudar a altura. Num sintetizador, isso é a
diferença entre um instrumento e um pedal.

O motor já aceita afinação em semitons (`GranularParams::pitchSemitones`), e o
grão já lê o material a uma taxa variável — a conversão de nota para taxa é
aritmética, não uma reescrita. O que falta é a ligação: nota → semitons → taxa, e
a política de mono com prioridade à última nota, que a emenda 1.2.0 fixou.

A escala entra no mesmo caminho e é o que torna o instrumento tocável por quem
não tem formação de teclado: quantizar a nota ao grau mais próximo de uma escala
escolhida, com tônica escolhida.

## Cenários

### História 1 — A nota define a altura (P1)

Como músico, quero que tocar Dó4 e Fá4 produza duas alturas diferentes, para
poder tocar uma linha melódica.

**Por que P1**: é a definição de instrumento musical. Sem isto, a F018 não tem
razão de existir.

**Teste independente**: `Pitch.NoteSixtyIsUnity`, `Pitch.NoteSeventyTwoIsOctave` e
`Pitch.NoteFortyEightIsHalf` confirmam que a razão de reprodução é 1,0 para MIDI
60, 2,0 para MIDI 72 e 0,5 para MIDI 48, com tolerância de 1e-9.

### História 2 — Última nota vence (P1)

Como instrumento mono, quero que, com três teclas presas, a última determine a
altura, e que ao soltar a última a anterior volte.

**Por que P1**: sem uma pilha de notas, o instrumento fica mudo ao soltar a
última tecla de um acorde, o que é o defeito clássico de um mono mal feito.

**Teste independente**: `NoteTracker.LastNoteWinsThenFallsBack` prende 60, 64, 67
e confirma 67; solta 67 e confirma 64; solta 64 e confirma 60.

### História 3 — A escala quantiza (P2)

Como produtor sem formação de teclado, quero escolher uma escala e uma tônica, e
que qualquer tecla caia num grau dessa escala, para não tocar notas erradas.

**Teste independente**: `Scale.MajorQuantizesToNearestDegree` e `Scale.TieGoesDown`
verificam notas contra a escala maior de Dó e confirmam a prioridade descendente
no empate.

### História 4 — Glide (P3)

Como músico, quero que a altura deslize entre notas em vez de saltar, com tempo
controlável.

**Teste independente**: `Glide.ReachesTargetInDeclaredTime` mede a chegada a 12
semitons no tempo declarado, com `glide = 50 ms`, e confirma que o valor ainda
está a caminho meio bloco antes.

## Requisitos

1. **Nota vira taxa.** `rate = 2^((note - 60 + pitch + pitchrand + lfo) / 12)`.
   A soma acontece em semitons e a exponencial é calculada uma vez por grão, não
   por amostra.
2. **Mono com pilha.** `NoteTracker` mantém uma pilha de notas presas com
   capacidade fixa (16), sem alocação. A nota ativa é o topo.
3. **Sustain.** O pedal de sustain não empilha: marca as notas como sustentadas,
   e o note-off não as remove até o pedal subir. Comportamento atual preservado.
4. **Escala.** `scale` e `root` quantizam a nota *antes* de virar taxa, com
   prioridade descendente no empate. `Chromatic` desliga a quantização.
5. **Glide.** Interpolação linear em semitons, com a travessia completa no tempo
   declarado por `glide` em segundos, avaliada por bloco. Exponencial foi
   descartada: nunca chega ao alvo, e o critério de aceite pede chegada em
   50 ms ± 5 ms. A duração não depende do intervalo — 50 ms é 50 ms de Dó a Sol
   e de Dó a Dó5.
6. **Sem alocação.** Pilha de notas e tabela de escala são arrays fixos.
7. **Teclado na interface.** Um `MidiKeyboardComponent` na faixa de teclado,
   escutado por um `MidiKeyboardState::Listener` que empurra a nota para uma
   `SpscRing` do processador; o callback drena a fila para a mesma `NoteTracker`
   do MIDI do host, sem `MidiMessageCollector` (que faz lock e pode alocar). As
   teclas do computador usam o mapa por omissão do JUCE
   (`A W S E D F T G Y H U J K`), o componente mostra a tecla premida, e `K` dá
   o foco ao teclado quando o foco está noutro componente. Fechar a janela manda
   `allNotesOff` antes de remover o listener, para não deixar nota presa.
8. **Acessibilidade.** O teclado tem nome acessível e entra na ordem de `Tab`.
   O anúncio da nota sob foco e a mudança de oitava por setas ficam por
   verificar no ensaio manual.

## Critérios de aceite

- [x] MIDI 60 → razão 1,0; MIDI 72 → razão 2,0; MIDI 48 → 0,5;
- [x] três notas presas, soltar a última devolve a penúltima;
- [x] com `scale = Major`, `root = C`, nenhuma nota tocada sai fora de C-D-E-F-G-A-B;
- [x] glide de 50 ms chega ao alvo em 50 ms ± 5 ms;
- [ ] o teclado toca e mostra a tecla com o rato e com o teclado do computador
      (código e compilação verdes; falta o ensaio manual);
- [x] nenhuma alocação no callback com tráfego MIDI real (guard verde).

## Não-objetivos

- **Polifonia.** A emenda 1.2.0 fixou mono. Oito vozes continuam a ser grãos.
- **Afinação microtonal.** Só 12-TET e escalas derivadas dela.
- **MPE.** Fica fora; o `NoteTracker` não transporta `pitch bend` por nota.
- **Aftertouch.** Não há destino definido para ele nesta versão.

## Testes

| Teste | O que prova |
| --- | --- |
| `Pitch.NoteSixtyIsUnity` | conversão base |
| `Pitch.NoteSeventyTwoIsOctave` | oitava acima |
| `Pitch.NoteFortyEightIsHalf` | oitava abaixo |
| `NoteTracker.LastNoteWinsThenFallsBack` | pilha de notas |
| `NoteTracker.RetriggerDoesNotDuplicateOnTheStack` | retrigger não duplica |
| `NoteTracker.SustainKeepsTheActiveNote` | pedal segura a altura |
| `NoteTracker.AllNotesOffResetsTheStack` | pânico limpa a pilha |
| `Scale.ChromaticPassesEveryNoteThrough` | desligar quantizador |
| `Scale.MajorQuantizesToNearestDegree` | quantização |
| `Scale.TieGoesDown` | empate desce |
| `Scale.RootTransposesTheWholeScale` | tônica move a escala |
| `Scale.PentatonicMinorDropsTheHalfSteps` | escala sem meios-tons |
| `Glide.ReachesTargetInDeclaredTime` | tempo do glide |
| `Glide.ZeroJumpsImmediately` | glide desligado |
| `Keyboard.ComputerKeysPlayNotes` | teclado do computador (ensaio manual na sessão T4) |
