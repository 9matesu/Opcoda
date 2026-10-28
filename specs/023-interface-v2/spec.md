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
janela crescer para além do mínimo de 480×434 que o plugin tinha de suportar. A
revisão de 24/10/2026 substituiu esse mínimo por uma janela fixa de 1180×760
(ver "Revisão de 24/10/2026").

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

### História 1 — Caber 41 parâmetros numa janela fixa (P1)

Como utilizador num portátil, quero ver todos os controles sem scroll horizontal
nem uma janela que mude de forma quando o host a redimensiona.

**Por que P1**: é a razão de a interface ser redesenhada, e não apenas embelezada.

**Teste independente**: a suite automática cobre o núcleo; na interface, o
critério verifica-se pelo build e pela captura de `tools/capture-ui.ps1`, porque
os testes `Ui.*` desta spec ainda não estão implementados (ver "Estado dos
testes de interface").

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
4. Seis abas: `GRAIN`, `ENV`, `FILTER`, `MOD`, `CLOUD` e `KEYS`. Não há páginas
   dentro do módulo, nem `MAIN` (repetia os onze parâmetros de grão), nem `OUT`
   (o volume passou a knob mestre persistente na coluna escura). Navegação por
   `Tab` e por setas dentro da faixa.
5. Todos os controles mantêm nome acessível descritivo em inglês e foco visível,
   conforme `docs/10-acessibilidade-w3c.md`.

### Leitura

6. **Visualizador do envelope**: desenha os quatro estágios com as curvas da
   F020, atualizado a 30 Hz no máximo, e destaca o estágio ativo.
7. **Resposta do filtro**: magnitude dos dois biquads combinados, em 32 pontos
   log-espaçados de 20 Hz a 20 kHz, com a frequência de corte marcada.
8. **Régua e marcas de seção** acima da waveform da F022.
9. **Indicador de estado** no rodapé: material, região e taxa. O modo de desenho
   saiu da interface: a F024 foi recusada na revisão de redesign de 23/10/2026 e
   o renderizador de software é o único caminho.

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

- [ ] os 41 parâmetros são alcançáveis por teclado na janela fixa de 1180×760,
      sem scroll;
- [ ] o display mantém a forma de onda, a grelha hexadecimal e a entropia
      clicáveis, e o topo do hex não segue os grãos;
- [ ] o `HOLD` congela o transporte sem parar o áudio, e o medidor stereo lê o
      pico por canal;
- [ ] a aba ativa, o valor tocado e o foco são distinguíveis sem depender só da
      cor;
- [ ] a curva do envelope desenhada coincide com a do motor;
- [ ] a resposta do filtro coincide com o biquad em ±0,5 dB;
- [ ] no Ableton, o botão direito num knob mostra o menu do host com automação;
- [ ] no Standalone, o mesmo gesto mostra o menu local, sem item de automação;
- [ ] o teclado do computador toca notas e o componente reflete o estado;
- [ ] contraste ≥ 4,5:1 para texto e ≥ 3:1 para elementos gráficos;
- [ ] nenhum controle depende só de cor;
- [ ] a interface não aloca no thread de áudio (não toca nele).

## Revisão de 24/10/2026 — instrumento de janela fixa

A implementação fechou o desenho numa janela fixa de 1180×760, sem
redimensionamento (`setResizable(false, false)`). O requisito de 480×434 da
primeira versão desta spec deixa de valer: com o display como elemento
dominante e dez knobs de grão numa linha, a escala mínima comprimia o texto e
obrigava a páginas que duplicavam parâmetros. A janela fixa é a decisão de
produto; o host não ganha uma asa de redimensionamento e o layout não tem um
segundo caminho.

As abas passam a seis: `GRAIN`, `ENV`, `FILTER`, `MOD`, `CLOUD` e `KEYS`. A aba
`MAIN` saiu porque repetia os onze parâmetros de grão; a aba `OUT` saiu porque o
volume é um knob mestre persistente na coluna escura à direita, ao lado do
medidor stereo. Cada controle aparece uma vez.

O display ganha a grelha hexadecimal contínua com 16 bytes por linha, coluna
ASCII própria, barra de deslocamento e endereços de 8 dígitos; a faixa de
entropia ocupa a base do display e é clicável e arrastável, assim como a
grelha. O topo do hex é do utilizador: não segue os grãos, e a cabeça só
centraliza uma vez, quando o material chega.

O transporte tem play/pause e `HOLD` no mesmo grupo, no pé do display; o `HOLD`
congela o avanço do transporte (`advance(held ? 0 : n, playing)`), não o áudio. O
medidor de saída é stereo, com escala de 0 a -48 dB e segmentos
verde→âmbar→vermelho; o pico por canal vem da telemetria atómica do callback.

O render é vetorial 2-D, sem OpenGL (F024 recusada). Nenhuma animação ambiente:
só o que responde a um gesto ou ao transporte.

A segunda revisão, do mesmo dia, acrescenta o escopo pedido pelo som:

- **Largura estéreo.** `STEREO` na linha de grão, mid/side na saída, 0 a 200 %,
  com destino `Stereo Width` na matriz. O `mid` fica intocado; só o `side`
  escala, e a rampa corre dentro do bloco.
- **A matriz é parâmetro.** As 8 rotas viram 24 parâmetros automatizáveis
  (`m1src`/`m1dst`/`m1amt` ...), editáveis na quarta vista do display (tecla 4)
  por attachment, com o nó legado `MODMATRIX` a migrar no carregamento. A
  tabela passa de 41 para 66.
- **Roda acelerada.** Entalhes consecutivos multiplicam o passo (1,7× no hex,
  até 32×; zoom até 8×); pausa devolve o passo de base.
- **Ícones sem moldura.** O desligado é só o glifo mais fraco. As vistas e o zoom
  ganham nome em rectângulos de 88×34 na rail de 96 px; os 30 px de 24/10 e a
  tira de navegação de 26 px foram substituídos na revisão de 26/10.

## Revisão de 26/10/2026 — abas de pasta, marcadores e correções

- **Abas de pasta.** As seis abas de módulo passam a assentar na borda do painel
  de parâmetros, com glifo em vez de palavra e 44 px de largura; a ativa funde-se
  com o painel e leva o filete âmbar. O nome falado continua no tooltip e na
  árvore de acessibilidade.
- **Teclado C1–C7.** 43 teclas brancas, dó central em C4; a largura da tecla é
  calculada da baía em `resized()`. O `PITCH` acompanha em ±48 semitons.
- **Silhueta.** A onda deixa de ser riscada por coluna: reamostragem por pixel
  mais um máximo corrido de ±2 px, corpo com gradiente quente e rms como faixa
  cheia. O verde sai das camadas da onda.
- **Clipping no drive.** `tanh` por grão e teto duro no barramento, de 1,0 até
  0,40; o pico deixa de depender do nível de entrada a partir de 6 dB.
- **Haas na largura.** Acima de 1,0 o canal direito atrasa até 12 ms; o atraso
  é slew-limitado a 0,25 amostra por amostra.
- **Marcadores.** Alt+clique alterna uma bandeira no endereço apontado, o clique
  sobre a bandeira salta para ele, e a bandeira desenha-se também no mapa. São
  estado de sessão, não parâmetros: não entram no `APVTS`.
- **Roda exponencial.** O ganho por entalhe cresce com o gesto e satura (32× no
  hex, 8× no zoom), em vez do passo fixo anterior.

## Revisão de 26/10/2026 (2) — acabamento: foco, transporte e agulhas

- **O clique não rouba o foco.** Todos os controlos recebem
  `setMouseClickGrabsKeyboardFocus(false)`; o foco pertence ao teclado virtual e
  um `MouseListener` global devolve-lho depois de qualquer clique. As teclas de
  nota continuam a soar ao trocar de vista, e o anel laranja só aparece na
  travessia por `Tab`.
- **Transporte no pé do display.** Play/pause e `HOLD` passam para a linha de POS
  e REG, com glifo e palavra; a tira de navegação de 32 px é apagada, o display
  cresce para 380 px e as páginas do hex ficam no mesmo pé.
- **`PLAY` não pisca.** A condição de activação deixa de consultar o playhead do
  host; a leitura de `VOICES` sai do rodapé.
- **Agulhas brancas.** Densidade, spray e posição em branco recto e opaco; a
  silhueta do grão usa a janela escolhida (Hann, Gaussian, Hamming, Blackman),
  inclina-se com o pitch, duplica-se com o pitch spread, muda de opacidade com o
  level e desloca o ponto com o pan.
- **Fim do material não é silêncio.** O grão que nasce na posição 1,0 é puxado
  para trás pelo comprimento da janela; o clique no canto direito do mapa de
  entropia deixa de calar o instrumento. Numa região curta com pitch alto o grão
  não cabe e a janela encurta para metade do material, para a posição não ficar
  morta.
- **Escala do medidor.** Piso em -48 dB e rótulos de 12 em 12 dB, centrados no
  traço e limitados à pista; o volume cede 18 px ao medidor.

## Revisão de 26/10/2026 (3) — a janela do standalone e o ecrã de ajustes

- **Barra própria.** `StandaloneWindow` desenha a barra com o `OpcodaLookAndFeel`
  (36 px, grafite, marca, minimizar e fechar planos) em vez da barra do Windows, e
  a janela passa a 1182×798, sem redimensionar.
- **Sem menu Save/Load/About.** O estado do standalone vive em
  `%APPDATA%\Opcoda\Opcoda.settings`; o menu herdado do JUCE oferecia operações
  que o projeto não tem.
- **`SettingsScreen` no lugar do `AudioDeviceSelectorComponent`.** Folha de
  820×580 sobre véu de 62 %, páginas `AUDIO` e `MIDI`, aplicação imediata sem
  `APPLY`, erro do pedido em vermelho ao lado do `CLOSE`. O instrumento continua a
  tocar e as teclas de nota continuam a chegar ao editor com o ecrã aberto.

## Revisão de 26/10/2026 (4) — marca na barra, abas com nome, ADSR e filtro

- **A marca saiu do header.** O nome sobe para a barra da janela, ao lado da
  marca, e o preset e a origem passam a um bloco único centrado no header. No
  VST3, onde não há barra da janela, o editor fica sem assinatura visível.
- **O ícone é texto.** A caixa laranja leva "Op" na fonte do nome; a mesma função
  desenha a barra, o cabeçalho dos ajustes e o ícone da barra de tarefas.
- **Abas de módulo retangulares, com nome.** Seis teclas de 165 px que dividem a
  chapa, com glifo e palavra; o requisito 4 passa a nomear as abas como
  `GRAIN`, `ENV`, `FILTER`, `MOD`, `CLOUD` e `KEYS`, visíveis, e não só no
  tooltip.
- **Envelope desenhado nos dois sentidos.** O visor mostra a forma que o motor
  aplica (as três curvas da F020), com pegas de canto para os tempos e o sustain,
  pegas de meio de segmento para as curvas, e o ponto vivo alimentado pelo
  `Telemetry` — nível, fase e progresso do estágio. Os knobs ficam secundários,
  numa fila à direita.
- **Resposta dos dois filtros desenhada.** Magnitude a partir dos mesmos
  coeficientes do áudio, 96 pontos de 20 Hz a 20 kHz, eixo de +18 a −48 dB; o
  slope de 24 dB é a magnitude ao quadrado. As pegas mudam corte e Q; a linha
  fina mostra a resposta com o corte já modulado.
- **`Ctrl+Z` na interface.** Um retrato do estado por gesto (32 passos, 400 ms de
  silêncio entre gestos), com `replaceState` quando o material não mudou e
  `loadStateXml` quando mudou. Sem refazer; no *host*, o desfazer continua a ser
  o do *host*.
- **O MIDI do host acende o teclado.** Bitmask de 128 teclas publicado pelo áudio
  em blocos com MIDI, espelhado no `MidiKeyboardState` com guarda para a nota não
  ser disparada duas vezes. O pedal não acende a tecla.

Requisito 6 emendado: o visor do envelope repinta a 60 Hz, e só quando o valor
visível muda — o anterior dizia 30 Hz como teto de atualização.

## Revisão de 27/10/2026 (5) — o ícone da onda, o hex sem faixa e o zoom da entropia

- **O ícone é a onda, não o texto.** A caixa laranja com "Op" sai; o ícone passa a
  ser a silhueta espelhada que os bytes desenham, laranja sobre preto, sem entropia
  nem cabeça de leitura. Uma tabela de 32 amplitudes alimenta o `tools/make-icon.ps1`
  (PNG) e o `ICON_BIG`/`ICON_SMALL` do CMake, que injectam o `.ico` no executável;
  o mesmo PNG vai por *binary data* para a janela e a barra de tarefas. Isto emenda
  o segundo ponto da revisão (4).
- **A faixa de entropia do hex foi removida.** O mapa do ficheiro inteiro já vive
  no *overview*; a faixa partia o ecrã em duas metades a competir pela largura. O
  hex passa a ocupar o ecrã todo.
- **Os 16 bytes correm num bloco só.** O vão de 12 px a meio da linha saiu: partia
  a linha em dois grupos de oito que não correspondem a nada.
- **Coluna ASM.** Desmontagem linear a partir do topo da janela, só em material de
  64 bits (`formatTag`), com um descodificador próprio no núcleo
  (`src/opcoda_core/disasm`): sem alocação, sem excepção, comprimento entre 1 e 15.
  AVX e os escapes raros caem em `db` e re-sincronizam — é orientação, não
  engenharia reversa.
- **O zoom da entropia foi corrigido.** A entropia de uma coluna de um byte é
  zero, e no zoom fundo a curva desabava toda para a base; a meio zoom o eixo de
  0–8 bits mentia para baixo. As colunas da vista passam a levar um mínimo de 256
  bytes, em cache própria, e a curva desenha-se por centros com as pontas
  esticadas às bordas do gráfico.

Requisito 5 emendado: a vista de bytes tem três colunas fixas — bytes, ASCII e
ASM — e a de entropia só desenha janelas de 256 bytes ou mais.

## Revisão de 27/10/2026 (6) — superfícies de leitura, onda de uma tinta e ASM com cor

- **ASCII e ASM são superfícies próprias.** A grade do endereço e dos bytes deixa
  de atravessar as colunas de leitura: morre na margem do bloco de bytes, e as duas
  colunas passam a caixas opacas com contorno, pintadas uma vez por baixo das
  linhas. Antes eram faixas translúcidas por linha e a grade via-se por dentro.
- **As linhas de bytes perdem a cor.** Saem as listras alternadas, o
  escurecimento fora da região e os filetes laranja da região; a parede de dígitos
  lê-se toda no mesmo marfim e o âmbar fica reservado ao byte da cabeça de leitura,
  que é a selecção. A região continua marcada pelo nome da secção no endereço e
  pelo mapa do pé do display.
- **O ASM lê-se como um desmontador.** Monoespacada do sistema (a largura fixa é a
  informação), com cor por classe de símbolo: mnemónico em âmbar, registos em
  marfim, números e destinos em verde-pálido, pontuação apagada, `db`/`bad` em
  cinza. A fonte embebida continua a ser só a Inter Display.
- **A onda é uma tinta só.** O gradiente quente, a sombra projectada, o contorno
  de marfim e o núcleo de rms eram quatro camadas sobre a mesma silhueta e, em
  ruído, liam-se como uma nuvem. Fica o corpo a 15 % de marfim e um filete a 78 %
  a fechar a forma, com a reamostragem ao dobro da largura em pixéis e a meia
  altura a 47 % do gráfico para o contorno não ser cortado pelo topo. Emenda o
  ponto «Silhueta» da revisão de 26/10.
- **Rail de teclas simples.** 44×38, só o glifo, rail de 62 px; o nome vive no
  tooltip e no leitor de ecrã. Os 34 px ganhos vão para o display. Emenda os
  rectângulos de 88×34 da revisão de 26/10.
- **Sem marca e sem parafusos.** A caixa "Op" sai da barra da janela (que passa a
  dizer só "Opcoda") e do cabeçalho dos ajustes; os quatro parafusos decorativos
  dos cantos saem. Emenda a revisão (4).

## Estado dos testes de interface

Em 24/10/2026 a suite automática cobre `opcoda_core`; os testes `Ui.*` da tabela
abaixo continuam por implementar. O portão da interface é o build (dev com
AddressSanitizer e release com o guard de CRT) e a captura de
`tools/capture-ui.ps1`, comparada a 1:1. Não se declara um teste que não corre.

## Não-objetivos

- **WebView2 ou janela separada.** O editor é embutido no host; uma janela
  separada só faria sentido no Standalone e partiria a paridade entre formatos.
- **Temas alternativos.** Um tema, bem feito.
- **Redimensionamento.** A janela é fixa em 1180×760; o requisito de 480×434
  foi substituído na revisão de 24/10/2026.
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
