# 023 · Interface v2: LookAndFeel, SVG, visualizadores e menu de contexto

**Feature Branch**: `023-interface-v2`
**Criada**: 2026-10-19
**Status**: Draft
**Objetivos atendidos**: OE7 (interface, acessibilidade e controles no DAW)
**Ensaios**: portões B e E

**Alvo**: F023, do mapa da emenda 1.2.0

## Contexto

A interface atual é um chassi de três faixas com knobs `Slider` desenhados pelo
LookAndFeel por omissão do JUCE, cinco módulos em abas e um display com três
vistas. Funciona, passa os critérios de acessibilidade e não tem identidade.

Com 41 parâmetros, o desenho atual também deixa de servir: uma faixa por módulo
não comporta onze controles de grão, e a solução de acrescentar fileiras faria a
janela crescer para além do mínimo de 480×434 que o plugin tem de suportar.

Esta feature faz três coisas ao mesmo tempo, e é por isso que é uma só:

1. **Identidade visual.** LookAndFeel próprio, ícones em SVG carregados como
   `Drawable`, knobs com arco e ponteiro, paleta e tipografia do design system.
2. **Escala.** Páginas dentro de cada módulo, com navegação por teclado.
3. **Leitura.** Visualizador do envelope, resposta do filtro, régua e marcas de
   seção — o que transforma 41 números em algo que se entende com os olhos.

O SVG é escolhido em vez de WebView2 porque o editor é embutido no host: uma
WebView2 acrescenta um processo, não é acessível por leitor de tela de forma
confiável, e não sobrevive ao redimensionamento do host. `Drawable` é acessível,
escala sem perda e não acrescenta processo.

## Cenários

### História 1 — Caber 41 parâmetros em 480×434 (P1)

Como utilizador num portátil, quero ver todos os controles sem scroll horizontal
nem janela maior que o ecrã.

**Por que P1**: é a razão de a interface ser redesenhada, e não apenas embelezada.

**Teste independente**: `Ui.EveryParameterReachableAtMinimumSize` percorre os 41
parâmetros por teclado e confirma que cada um recebe foco e é visível em 480×434.

### História 2 — Ver o envelope (P1)

Como produtor, quero ver a forma do envelope enquanto movo os knobs, porque um
ADSR em quatro números não diz o que se vai ouvir.

**Teste independente**: `Ui.EnvelopeCurveMatchesEngine` compara a curva desenhada
com a do motor em 64 pontos, com tolerância de 0,01.

### História 3 — Ver a resposta do filtro (P1)

Como produtor, quero ver a curva de resposta dos dois filtros, para perceber o
que o Q está a fazer.

**Teste independente**: `Ui.FilterResponseMatchesBiquad` compara a magnitude
desenhada com a resposta calculada em 32 frequências, com tolerância de 0,5 dB.

### História 4 — Automatizar pelo menu do host (P2)

Como produtor no Ableton, quero clicar com o botão direito num knob e ver as
opções de automação do host, em vez de procurar o parâmetro no navegador.

**Teste independente**: `Ui.ContextMenuDelegatesToHostWhenAvailable` confirma que,
com `HostProvidedContextMenu` disponível, o menu do host é usado; e
`Ui.ContextMenuFallsBackInStandalone` confirma que, sem host, aparecem repor,
copiar, colar e MIDI learn.

### História 5 — Tocar com o teclado do computador (P2)

Como utilizador sem teclado MIDI, quero tocar com as teclas do computador, porque
é a forma mais rápida de ouvir o material.

**Teste independente**: `Ui.ComputerKeyboardPlaysNotes` envia `A`, `W`, `S` e
confirma as notas 60, 61, 62 a chegar ao motor.

## Requisitos

### Visual

1. `OpcodaLookAndFeel : juce::LookAndFeel_V4`, com paleta, tipografia e métricas
   do `design/DESIGN-SYSTEM.md`, e o LookAndFeel instalado no editor, não global.
2. Ícones em SVG carregados por `juce::Drawable::createFromSVG`, com fallback
   para desenho vetorial quando o recurso não abre.
3. Knob: arco de valor, ponteiro, marca de origem, anel de foco visível, e valor
   em texto com fonte monoespaçada.
4. Páginas por módulo: `GRAIN` (2 páginas), `ENVELOPE`, `FILTER` (2), `MOD`,
   `KEYS`, `OUT`. Navegação por `Tab` e por setas dentro da página.
5. Todos os controles mantêm nome acessível descritivo em inglês e foco visível,
   conforme `docs/10-acessibilidade-w3c.md`.

### Leitura

6. **Visualizador do envelope**: desenha os quatro estágios com as curvas da
   F020, atualizado a 30 Hz no máximo, e destaca o estágio ativo.
7. **Resposta do filtro**: magnitude dos dois biquads combinados, em 32 pontos
   log-espaçados de 20 Hz a 20 kHz, com a frequência de corte marcada.
8. **Régua e marcas de seção** acima da waveform da F022.
9. **Indicador de estado** no rodapé: material, região, taxa, e o modo de desenho
   (software ou OpenGL, da F024).

### Interação

10. **Menu de contexto**: `getHostContext()` → `getContextMenuForParameter(id)`
    quando o host oferece `HostProvidedContextMenu`; caso contrário, menu local
    com *Reset*, *Copy*, *Paste* e *MIDI Learn*.
11. **MIDI learn local**: o próximo CC recebido liga-se ao parâmetro escolhido,
    com indicação visível e forma de remover.
12. **Zoom e região por rato** (da F022) ligados aos mesmos gestos em toda a
    waveform.
13. **Teclado do computador**: `MidiKeyboardComponent` + `KeyboardState`,
    mapeamento `A W S E D F T G Y H U J K`, oitava por `Z`/`X`, e o componente
    mostra a tecla premida.

## Critérios de aceite

- [ ] os 41 parâmetros são alcançáveis por teclado em 480×434, sem scroll;
- [ ] a curva do envelope desenhada coincide com a do motor;
- [ ] a resposta do filtro coincide com o biquad em ±0,5 dB;
- [ ] no Ableton, o botão direito num knob mostra o menu do host com automação;
- [ ] no Standalone, o mesmo gesto mostra o menu local, sem item de automação;
- [ ] o teclado do computador toca notas e o componente reflete o estado;
- [ ] contraste ≥ 4,5:1 para texto e ≥ 3:1 para elementos gráficos;
- [ ] nenhum controle depende só de cor;
- [ ] a interface não aloca no thread de áudio (não toca nele).

## Não-objetivos

- **WebView2 ou janela separada.** O editor é embutido no host; uma janela
  separada só faria sentido no Standalone e partiria a paridade entre formatos.
- **Temas alternativos.** Um tema, bem feito.
- **Redimensionamento livre.** Tamanho mínimo e máximo declarados, com escala
  inteira.
- **Visualizador 3D.** É a F024.

## Testes

| Teste | O que prova |
| --- | --- |
| `Ui.EveryParameterReachableAtMinimumSize` | escala e teclado |
| `Ui.EnvelopeCurveMatchesEngine` | visualizador do envelope |
| `Ui.FilterResponseMatchesBiquad` | visualizador do filtro |
| `Ui.ContextMenuDelegatesToHostWhenAvailable` | menu do host |
| `Ui.ContextMenuFallsBackInStandalone` | menu local |
| `Ui.ComputerKeyboardPlaysNotes` | teclado do computador |
| `Ui.SvgIconsLoadOrFallBack` | robustez do SVG |
| `Ui.ContrastMeetsWcag` | contraste |
