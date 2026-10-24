# 18. Processo de desenvolvimento orientado a especificação

## Spec-kit

O projeto usa o spec-kit do GitHub. Os comandos são skills, acessados pelo agente
de código no diretório `.claude/skills/`:

| Comando | O que faz |
| --- | --- |
| `/speckit-constitution` | estabelece os princípios do projeto |
| `/speckit-specify` | escreve a especificação de uma feature |
| `/speckit-plan` | planeja a implementação |
| `/speckit-tasks` | quebra em tarefas executáveis |
| `/speckit-implement` | executa as tarefas |
| `/speckit-analyze` | checa consistência entre os artefatos |

Uma feature corresponde a um objetivo específico ou a um ensaio do plano de
testes, não a um agrupamento arbitrário de tarefas.

## Constitution

`.specify/memory/constitution.md` prevalece sobre qualquer prática. Não é
organização de pastas: cada princípio é um critério verificável.

| Princípio | Resumo |
| --- | --- |
| I · Núcleo sem JUCE | `opcoda_core` não inclui cabeçalho do framework, o que permite sanitizers e fuzzing sobre o núcleo |
| II · Tempo real rígido | o callback não aloca, não trava, não faz I/O, não lança exceção |
| III · Leitura defensiva | toda leitura confere `offset + tamanho` em aritmética que não transborda |
| IV · Portão de qualidade | nenhuma tarefa fecha sem o seu portão |
| V · Simplicidade | oito vozes, dois compiladores, zero abstração especulativa |
| VI · Documentação | mudança de comportamento altera a documentação no mesmo commit |

## Portões

| Portão | Critério |
| --- | --- |
| A · Build | compila em release sem warnings sob `/W4 /WX` |
| B · Testes | 344 casos na suíte principal mais 8 do guard de alocação, incluindo o critério de 40 dB do T1. Contagem medida no binário em 22/10/2026 |
| C · Tempo real | zero alocação no callback e na troca de material, com duas threads, e também durante a reprodução do transporte. **Desde 19/10/2026 o T2 mede também a fração dinâmica no Ableton**: p99 abaixo de metade do orçamento e zero xruns em 5 minutos, com buffers de 128, 256 e 512 a 44,1 e 48 kHz |
| D · Robustez | 50 mutações de bit-flip e 10 casos de borda, todas com erro tipado |
| E · Interface | contraste, teclado e nome acessível. **O 2.5.8 fechou em 06/10/2026** com a caret de teclado do `ByteDisplay`, e a exceção declarada foi retirada. A F023 reabre a verificação para os módulos novos |
| F · Documentação | `docs/` coerente com o código |

## Rastreabilidade

Cada objetivo específico de `docs/04-objetivos.md` tem uma feature:

| Objetivo | Feature | Estado |
| --- | --- | --- |
| OE1 · parser PE | F002 | pronto: parser, verificação de limites, 14 casos |
| OE2 · ingestão somente-leitura | F002, F003 | pronto: `parseFile`, `toSamples` e diálogo de arquivo; arrasto na interface |
| OE3 · fila lock-free | F003 | pronto: dupla fila SPSC com devolução, portão C verificado com duas threads |
| OE4 · motor granular | F005 | pronto: 8 grãos simultâneos, instrumento mono, janelas, sobreposição |
| OE5 · entropia de Shannon | F004, F008 | pronto: curva por janela, testada, e **mostrada no display** desde 06/10/2026 |
| OE5 (seleção) | F025 | pronto: mochila 0/1 sobre a região atual, três métodos de busca local e 15 testes; o painel OPT publica o material concatenado |
| OE6 · condicionamento | F006 | pronto: DC-blocker e limiter |
| OE7 · interface | F008 | **fechado em 06/10/2026**: três vistas do material (forma de onda, hex, curva de entropia), transporte de audição, operação completa por teclado, peças vetoriais sem PNG. O display de entropia que esta linha afirmava estar pronto em 09/10 tinha sido apagado por `e99d089`; a linha estava errada e a feature corrigiu o código e a afirmação |
| OE8 · isolamento | F003, F008 | pronto: guard de alocação e troca sem alocação verificados, agora também sobre o caminho do transporte; TSan indisponível no Windows |
| OE9 · MIDI | F009 | pronto: gate por nota, sustain e 2 CCs mapeados; falta o ensaio no host |
| T1–T4 | F010 | T1 e T3 prontos; T2 e T4 agendados, com o host licenciado confirmado em 19/10/2026 |
| OE4 (params) | F011 | pronto: grainlevel, pitchrand e scanspeed no motor, 13 testes com fail-without-fix por mutação; UI liga na F015 |
| OE4 (envelope) | F012 | pronto: ADSR global disparado pela aresta da nota, 10 testes; sem nota presa (all-notes-off e sustain entram pela mesma aresta) |
| OE4 (filtros) | F013 | pronto: 2 biquads RBJ em serie por grão (LP/HP/BP/Notch), coefs 1x/bloco, estado zerado no nascimento; 15 testes com fail-without-fix por mutacao |
| OE4 (LFO) | F014 | pronto: LFO como funcao do tempo absoluto (rate, depth, 4 alvos, 5 formas incl. S&H deterministico), modulacao por grao e por bloco; 16 testes com floors de magnitude e mutacoes por alvo |
| OE7 (UI) | F015 | pronto: 5 modulos com abas (GRAIN/ENVELOPE/FILTER/MOD/OUT), 25 params com nomes em ingles e IDs originais intactos, choices em knobs com degraus, cutoff com skew em 1 kHz, scanspeed no transporte; minimo 480x434. A emenda 1.2.0 acrescenta 16 params, e a F023 reorganiza em páginas |
| OE7 (presets) | F016 | pronto: seletor no header (anterior/nome/seguinte + menu), 6 presets de fabrica com valores presos aos intervalos do layout por teste, .opcoda como o XML do estado em ficheiro, "*" de preset sujo via Listener |
| OE2 (teto) | F017 | **por fazer**: a constitution pede teto de ingestão e análise cancelável (§3/§11) e o `ingest` lê o ficheiro inteiro de forma síncrona (`plugin_processor.cpp:144-165`). A F022 depende desta |
| OE4 e OE9 (teclado) | F018 | **pronto, à espera de ensaio manual**: nota vira taxa de reprodução (mono, prioridade à última nota, pilha de 16), escala de 12 modos com tónica, glide linear, parâmetros `scale`/`root`/`glide` no módulo KEYS, teclado virtual de cinco oitavas com rato, teclas do computador e atalho `K`. O caminho é `MidiKeyboardState` → `SpscRing` → `NoteTracker`, sem alocar no callback. Falta confirmar ao vivo na sessão T4 |
| OE4 (modulação) | F019 | **pronto, à espera de ensaio ao vivo**: `lfosync` (Free/Sync), `lfodiv` (12 divisões, pontuadas e tercinas), `lfosteps` (1–16 degraus), BPM e ppq lidos do host uma vez por bloco e entregues ao núcleo como dois doubles; fase derivada do ppq com offset para a troca de modo não dar degrau. Sem transporte cai para 120 BPM |
| OE4 (analógico) | F020 | **pronto, à espera de ensaio ao vivo**: filtro 1 com slope 12/24 (segundo biquad com estado próprio), drive `tanh` com compensação `1/g` que nunca sobe o pico, keytrack e envelope no corte; curvas por estágio `2^(3·curva)` na convenção do Serum; matriz de 8 rotas × 8 fontes × 12 destinos no estado (nó `MODMATRIX`, fora da tabela de parâmetros), publicada por 24 inteiros atómicos e uma geração. Layout em 38 params, 41 com a F021 |
| OE4 (nuvem) | F021 | **pronto, à espera de ensaio ao vivo**: `cloudsize` (janela de 0–500 ms com atraso uniforme por grão, slot ocupado sem escrever), `pitchspread` (±N semitons, simétrico) e `grainjit` (duração entre 0,5× e 1,5×, 1–100 ms), no módulo CLOUD. Com os três a zero o motor reproduz a saída anterior; a semente continua fixa por omissão (`setSeed` disponível) para T1/T2 |
| OE7 (waveform) | F022 | por fazer: waveform PCM do ficheiro inteiro, multi-resolução, sobre a F017 |
| OE7 (interface v2) | F023 | por fazer: LookAndFeel v2 com SVG, visualizador de envelope e de resposta do filtro, régua, zoom, selecção por arrasto, menu de contexto do host |
| OE7 (visualizadores) | F024 | por fazer: OpenGL com queda para software |

## Agentes de revisão

Cada tarefa é revisada por um agente especializado antes de ser declarada
concluída. Os arquivos ficam em `.opencode/agent/`.

| Agente | O que revisa | Existe |
| --- | --- | --- |
| `pe-parser-reviewer` | verificação de limites, endianness, transborno de inteiro, códigos de erro | sim |
| `rt-dsp-auditor` | alocação, trava, I/O e exceção no caminho de áudio, escala de posição | sim |
| `test-engineer` | portões B e D, o teste falha sem a correção, material real | sim |
| `ui-reviewer` | responsividade, contraste, teclado, nomes acessíveis | **por criar**; a revisão por captura de ecrã já apanhou quatro defeitos de layout, e a F023 é o alvo dele |
| `perf-benchmarker` | p99 do `processBlock` contra o orçamento de 2,9 ms | **por criar**; o alvo existe desde 19/10/2026, porque o T2 ficou medível |
| `doc-synchronizer` | coerência entre `docs/`, `specs/` e o código | **por criar**; o alvo é a sincronização da emenda 1.2.0 |

Dos seis, existem três em `.opencode/agent/`: `pe-parser-reviewer`,
`rt-dsp-auditor` e `test-engineer`. Os outros três têm alvo declarado e ficam por
criar — a revisão de interface continua a ser feita por captura de ecrã até lá.

## Regra de alteração

Alterar a constitution exige registro em `docs/18-processo-sdd.md` com o motivo,
o impacto nos portões e a migração das features afetadas. Exceção é permitida
quando o ensaio que a comprova não existe na plataforma, desde que a lacuna
fique escrita em `docs/07-plano-testes.md` e repetida nos artefatos entregues.
O caso do TSan é o exemplo corrente.

## Emenda 1.1.0 (2026-10-08) — escopo do instrumento

**Motivo.** A Fase 2 pedida (ADSR, dois filtros, LFO) não cabia no teto de seis
parâmetros do Princípio V, e o biquad "fixo" que a constitution citava nem
existia no código. Manter o teto seria documentar uma intenção contra o próprio
repositório; a emenda alinha o documento ao instrumento que se vai construir.

**Impacto nos portões.** B ganha testes por parâmetro novo (unidade + borda);
C passa a medir 16 biquads e o LFO no ensaio de orçamento, com coeficientes
fora do caminho por amostra; E re-verifica os módulos novos de UI; T4 passa a
medir 25 parâmetros. A, D e F não mudam de critério.

**Migração.** IDs dos seis originais não mudam (sessões salvas continuam
válidas); só o nome exibido passa a inglês. Tabela 8 do artigo e ensaio T4
(T4 em `docs/07-plano-testes.md`) reescritos para os 25 — ver Next Actions da
emenda. Features novas: F011–F017 no mapa do fluxo de trabalho.

## Emenda 1.2.0 (2026-10-19) — instrumento mono, 41 parâmetros e validação possível

**Motivo.** Três coisas mudaram de uma vez, e nenhuma delas cabia na 1.1.0.

Primeira: a 1.1.0 introduziu ADSR, dois filtros e LFO "por nota", e a palavra
"por nota" arrastou consigo a ideia de polifonia. O motor nunca foi polifónico —
tem um gate só e o envelope é global na saída (`granular_engine.h:289-291`), e as
oito vozes são slots de grão. O texto dizia "motor granular polifónico de 8
vozes" (`docs/04-objetivos.md`) e isso é falso em relação ao código.

Segunda: o pedido de escopo novo (teclado, escala, LFO rítmico, caráter analógico,
nuvem de grãos, waveform PCM, interface v2, visualizadores) exige mais parâmetros
do que os 25, e o Princípio V proibia entrar por antecipação.

Terceira: a licença do host de áudio, que era o bloqueio declarado do T2 e do T4,
existe nesta máquina. A fração dinâmica do contrato de tempo real — a única que o
guard de alocação não alcança — passa a ser medível antes da entrega.

**Impacto nos portões.**

- **B** sobe de 196 para 289 casos medidos, mais os novos de cada parâmetro:
  unidade e borda por parâmetro novo, e os testes de curva do envelope, de
  escala e de tempo do LFO. A medição deixa de ser estimativa: o número sai do
  binário.
- **C** deixa de ter a lacuna declarada. O T2 passa a medir no Ableton, e a matriz
  de teste do T2 muda de método: o instrumento é mono, logo "1, 4 e 8 vozes" não
  significa nada. Mede-se por tamanho de grão (10 e 100 ms) × densidade, que é o
  que carrega o CPU.
- **E** reabre para os módulos novos da F023 e para o teclado da F018. Os seis
  critérios já aceitos valem para todo o controle novo, sem exceção.
- **F** absorve as correções que a mudança obriga: `docs/04` ("polifónico"), o
  `README` (seis parâmetros, CLAP, contagem de testes), `docs/07` e `docs/08`
  (o host existe), e `docs/16` (o estado no host passa a ser verificável).
- **A** e **D** não mudam de critério. O D cresce em corpus se a F021 trouxer
  caminho novo de leitura.

**Migração.** Os 25 IDs da 1.1.0 **não mudam**: sessões salvas e presets `.opcoda`
continuam válidos, e os 16 novos nascem com ID inglês, acrescentados no fim da
lista. Dois desenhos ficam fora da tabela por decisão escrita, com o desenho
completo em `docs/19-parametrizacao-do-estudo-e-briefing.md`: a matriz de 8 rotas
vive no estado do APVTS (não é automatizável pelo host, e custaria 24 parâmetros)
e o caráter analógico entra só no filtro 1 (a paridade custaria 4 parâmetros e
levaria a tabela a 45).

**Features novas.** F018 a F024 no mapa do fluxo de trabalho. A F017, que a 1.1.0
criou e nunca foi implementada, passa a ser pré-requisito declarado da F022.

**O que esta emenda retira.** A palavra "polifónico" em toda a documentação. O
estado "bloqueado" do T2 e do T4. E as três linhas de `docs/07` e `docs/08` que
declaravam a máquina sem host licenciado, que passaram a estar erradas.

## Emenda 1.3.0 (2026-10-22) — seleção por mochila no painel OPT

**Motivo.** O Princípio V declarava a seleção inteligente de blocos (§8) como
adiada, e a F025 a traz para a entrega. A escolha não vira parâmetro: ela é uma
operação de interface, executada na message thread, que mede a região atual,
resolve a mochila 0/1 e concatena os blocos escolhidos antes de publicar o
material pela fila SPSC existente. A tabela continua fechada em 41.

**Impacto nos portões.**

- **A** não muda: o núcleo novo é C++20 puro e o build de release continua sem
  warnings sob `/W4 /WX`.
- **B** sobe para 344 casos medidos, mais os 8 do guard. A F025 acrescenta 15
  testes em `tests/knapsack_test.cpp`, incluindo capacidade, fórmula de valor,
  ordem da concatenação e erro tipado.
- **C** não muda de critério: a otimização corre fora do callback e publica
  pelo mesmo caminho da ingestão; o guard de alocação continua verde. O
  playhead do material concatenado fica declarado como limitação de UI.
- **D** cresce com os erros tipados de região, capacidade e solução inválida;
  nenhuma exceção cruza a fronteira do plugin.
- **E** reabre para a aba OPT: método e campos têm nome acessível, o estado e
  as métricas são texto, e o erro não depende de cor.
- **F** absorve `README`, `docs/07`, `docs/16`, `docs/19`, a spec
  `025-selecao-por-mochila` e a contagem de testes. O atlas continua no
  instantâneo de `4cac7a5` e não lista `optimization/`; a regeneração fica
  declarada como dívida e não bloqueia a F025.

**Migração.** Nenhum ID de parâmetro muda e nenhum parâmetro novo entra na
APVTS. As configurações do painel OPT vivem na sessão da interface, não no
preset `.opcoda`. O aplicativo C# `src/OpcodaOtimizacao/` permanece como
referência e não é distribuído.

**Features novas.** F025 no mapa do fluxo de trabalho.

**O que esta emenda retira.** A frase "seleção inteligente de blocos continua
adiada" da constitution e de `docs/19`. A entropia continua a modular o spray;
a mochila acrescenta a escolha do material, não a substitui.
