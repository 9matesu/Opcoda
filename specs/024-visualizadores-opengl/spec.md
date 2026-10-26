# 024 · Visualizadores OpenGL com queda para software

**Feature Branch**: `024-visualizadores-opengl`
**Criada**: 2026-10-19
**Status**: Recusada em 23/10/2026, antes da implementação. A revisão de redesign
(`I:\Opcoda_Redesign_Especificacao.docx`, secção 8) assenta o render vetorial 2D
como baseline e dispensa o OpenGL: o contexto com `setContinuousRepainting`
repintava a janela inteira a 60 Hz contra o critério de *idle* da secção 9, e a
combinação contexto + chapa creme produzia ecrã preto. O orçamento de desenho
passa a ser cumprido por invalidação de região no pintor de software. Esta spec
fica como registro da alternativa avaliada; nenhum requisito está em vigor.
**Objetivos atendidos**: OE7 (interface)
**Ensaios**: portões A, B e E

**Alvo**: F024, do mapa da emenda 1.2.0

## Contexto

A waveform da F022 desenha min/max por coluna com `Path` e o visualizador do
envelope desenha 64 pontos. Em software, isso é confortável. Com zoom muito
próximo, ou com a nuvem da F021 a animar 200 grãos por quadro, o desenho em
software começa a competir com a thread de mensagem do host — e no Ableton a
thread de mensagem é a mesma que desenha a interface inteira da DAW.

OpenGL resolve isso, mas traz um risco que não pode ser escondido: em máquinas
sem GPU, em sessões RDP e em alguns drivers, o contexto não ativa. O JUCE, nesse
caso, cai silenciosamente para software. A queda silenciosa é aceitável **desde
que o utilizador saiba**, e é isso que esta feature acrescenta: um indicador no
rodapé que diz qual caminho está ativo.

A decisão de usar 3D real, e não gradientes que imitam volume, foi tomada com o
custo à frente: `juce_opengl` entra no alvo do plugin, o contexto é criado e
anexado na message thread, e o componente tem de continuar a pintar corretamente
quando não há contexto.

## Cenários

### História 1 — Desenhar com GPU quando existe (P1)

Como utilizador com GPU, quero que os visualizadores usem aceleração, para que a
interface não dispute CPU com o áudio.

**Por que P1**: é a razão da feature.

**Teste independente**: `OpenGl.ContextAttachesWhenAvailable` confirma, num
contexto de teste, que o componente reporta `isActive()` e que o desenho acontece
no caminho acelerado.

### História 2 — Continuar a funcionar sem GPU (P1)

Como utilizador em RDP ou numa máquina sem driver, quero que a interface continue
a desenhar, mais devagar, e que o rodapé diga que está em software.

**Por que P1**: uma interface que não desenha é pior do que uma interface lenta.

**Teste independente**: `OpenGl.FallsBackToSoftwareWithoutContext` força a falha
de contexto e confirma que o componente pinta e que o indicador mostra
"software".

### História 3 — O contexto não toca no áudio (P1)

Como host, não quero que o contexto OpenGL seja criado, destruído ou usado fora
da message thread.

**Teste independente**: `OpenGl.NoContextCallsFromAudioThread` verifica, por
instrumentação do construtor de contexto, que nenhuma chamada acontece no
`processBlock`.

## Requisitos

1. **Dependência.** `juce_opengl` entra no `target_link_libraries` apenas de
   `opcoda_plugin`. O núcleo continua sem JUCE.
2. **Ciclo de vida.** O contexto é membro do editor, criado em
   `parentHierarchyChanged` e destruído em `~Component`, ambos na message thread.
   Nunca em `prepareToPlay`, `processBlock` ou `releaseResources`.
3. **Anexação condicional.** `context.attachTo(*component)` só depois de
   `context.isActive()`; se não ativar, o componente continua a usar `paint()`.
4. **Indicador honesto.** O rodapé mostra `GPU` ou `Software`, lido de
   `context.isActive()`, e a mudança é refletida sem reiniciar o editor.
5. **Sem estado partilhado com o áudio.** Os buffers de desenho são da message
   thread; a telemetria chega por atómicos, como já acontece.
6. **Limpeza.** `detach()` antes de destruir o componente, e o contexto não
   sobrevive ao editor.
7. **Orçamento.** Com 200 grãos animados a 60 Hz, o tempo de desenho por quadro
   não excede 8 ms em software e 4 ms com GPU, medido no ensaio manual do T2.
8. **Sem regressão de acessibilidade.** O conteúdo desenhado em OpenGL não é
   acessível por si; as informações que ele acrescenta continuam a existir em
   texto ou em controle acessível.

## Critérios de aceite

- [ ] numa máquina com GPU, o indicador mostra `GPU` e o desenho é acelerado;
- [ ] sem contexto, o indicador mostra `Software` e tudo desenha;
- [ ] nenhuma chamada de contexto parte do thread de áudio;
- [ ] fechar e reabrir o editor não deixa contexto pendente nem memória a subir;
- [ ] 200 grãos a 60 Hz ficam dentro do orçamento declarado;
- [ ] o release não introduz dependência nova além de `juce_opengl`;
- [ ] `dumpbin` continua sem `*D.dll` (o guard do `build.ps1 -Release`).

## Não-objetivos

- **Shaders próprios.** Usa-se o `OpenGLContext` do JUCE com o desenho atual.
- **3D com geometria complexa.** O "3D" é profundidade e brilho nos
  visualizadores, não uma cena.
- **OpenGL no núcleo.** Impossível por constitution, e sem sentido.
- **Fallback para Direct2D.** O caminho de software do JUCE já cobre.

## Testes

| Teste | O que prova |
| --- | --- |
| `OpenGl.ContextAttachesWhenAvailable` | caminho acelerado |
| `OpenGl.FallsBackToSoftwareWithoutContext` | queda |
| `OpenGl.NoContextCallsFromAudioThread` | contrato de thread |
| `OpenGl.EditorReopenDoesNotLeak` | ciclo de vida |
| `OpenGl.IndicatorMatchesActiveState` | honestidade do indicador |
