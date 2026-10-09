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
| B · Testes | 196 casos na suíte principal mais 8 do guard de alocação, incluindo o critério de 40 dB do T1 |
| C · Tempo real | zero alocação no callback e na troca de material, com duas threads, e também durante a reprodução do transporte |
| D · Robustez | 50 mutações de bit-flip e 10 casos de borda, todas com erro tipado |
| E · Interface | contraste, teclado e nome acessível. **O 2.5.8 fechou em 06/10/2026** com a caret de teclado do `ByteDisplay`, e a exceção declarada foi retirada |
| F · Documentação | `docs/` coerente com o código |

## Rastreabilidade

Cada objetivo específico de `docs/04-objetivos.md` tem uma feature:

| Objetivo | Feature | Estado |
| --- | --- | --- |
| OE1 · parser PE | F002 | pronto: parser, verificação de limites, 14 casos |
| OE2 · ingestão somente-leitura | F002, F003 | pronto: `parseFile`, `toSamples` e diálogo de arquivo; arrasto na interface |
| OE3 · fila lock-free | F003 | pronto: dupla fila SPSC com devolução, portão C verificado com duas threads |
| OE4 · motor granular | F005 | pronto: 8 vozes, janelas, sobreposição |
| OE5 · entropia de Shannon | F004, F008 | pronto: curva por janela, testada, e **mostrada no display** desde 06/10/2026 |
| OE6 · condicionamento | F006 | pronto: DC-blocker e limiter |
| OE7 · interface | F008 | **fechado em 06/10/2026**: três vistas do material (forma de onda, hex, curva de entropia), transporte de audição, operação completa por teclado, peças vetoriais sem PNG. O display de entropia que esta linha afirmava estar pronto em 09/10 tinha sido apagado por `e99d089`; a linha estava errada e a feature corrigiu o código e a afirmação |
| OE8 · isolamento | F003, F008 | pronto: guard de alocação e troca sem alocação verificados, agora também sobre o caminho do transporte; TSan indisponível no Windows |
| OE9 · MIDI | F009 | pronto: gate por nota, sustain e 2 CCs mapeados; falta o ensaio no host |
| T1–T4 | F010 | T1 e T3 prontos; T2 e T4 dependem de DAW instalado |
| OE4 (params) | F011 | pronto: grainlevel, pitchrand e scanspeed no motor, 13 testes com fail-without-fix por mutação; UI liga na F015 |
| OE4 (envelope) | F012 | pronto: ADSR por nota com disparo na aresta, 10 testes; sem nota presa (all-notes-off e sustain entram pela mesma aresta) |
| OE4 (filtros) | F013 | pronto: 2 biquads RBJ em serie por voz (LP/HP/BP/Notch), coefs 1x/bloco, estado zerado no nascimento; 15 testes com fail-without-fix por mutacao |
| OE4 (LFO) | F014 | pronto: LFO como funcao do tempo absoluto (rate, depth, 4 alvos, 5 formas incl. S&H deterministico), modulacao por grao e por bloco; 16 testes com floors de magnitude e mutacoes por alvo |
| OE7 (UI) | F015 | pronto: 5 modulos com abas (GRAIN/ENVELOPE/FILTER/MOD/OUT), 25 params com nomes em ingles e IDs originais intactos, choices em knobs com degraus, cutoff com skew em 1 kHz, scanspeed no transporte; minimo 480x434 |
| OE7 (presets) | F016 | pronto: seletor no header (anterior/nome/seguinte + menu), 6 presets de fabrica com valores presos aos intervalos do layout por teste, .opcoda como o XML do estado em ficheiro, "*" de preset sujo via Listener |

## Agentes de revisão

Cada tarefa é revisada por um agente especializado antes de ser declarada
concluída. Os arquivos ficam em `.opencode/agent/`.

| Agente | O que revisa | Existe |
| --- | --- | --- |
| `pe-parser-reviewer` | verificação de limites, endianness, transborno de inteiro, códigos de erro | sim |
| `rt-dsp-auditor` | alocação, trava, I/O e exceção no caminho de áudio, escala de posição | sim |
| `test-engineer` | portões B e D, o teste falha sem a correção, material real | sim |
| `ui-reviewer` | responsividade, contraste, teclado, nomes acessíveis | disponível; a revisão por captura de ecrã já apanhou quatro defeitos de layout |
| `perf-benchmarker` | p99 do `processBlock` contra o orçamento de 2,9 ms | etapa S3b |
| `doc-synchronizer` | coerência entre `docs/`, `specs/` e o código | etapa F010 |

Só existem os três que já têm alvo. Os outros três têm coluna marcada com a
etapa em que passam a ter o que revisar: um agente de acessibilidade antes da
GUI, um de desempenho antes do T2, um de documentação depois que houver o que
documentar além do que o próprio portão F já exige.

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
