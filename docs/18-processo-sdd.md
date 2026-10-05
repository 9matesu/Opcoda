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
| B · Testes | 153 casos verdes, incluindo o critério de 40 dB do T1 |
| C · Tempo real | zero alocação no callback e na troca de material, com duas threads |
| D · Robustez | entradas malformadas rejeitadas com erro tipado |
| E · Interface | contraste, teclado e nome acessível (etapa S4) |
| F · Documentação | `docs/` coerente com o código |

## Rastreabilidade

Cada objetivo específico de `docs/04-objetivos.md` tem uma feature:

| Objetivo | Feature | Estado |
| --- | --- | --- |
| OE1 · parser PE | F002 | pronto: parser, verificação de limites, 14 casos |
| OE2 · ingestão somente-leitura | F002, F003 | pronto: `parseFile`, `toSamples` e diálogo de arquivo; arrasto na interface |
| OE3 · fila lock-free | F003 | pronto: dupla fila SPSC com devolução, portão C verificado com duas threads |
| OE4 · motor granular | F005 | pronto: 8 vozes, janelas, sobreposição |
| OE5 · entropia de Shannon | F004 | pronto: curvo por janela, testado |
| OE6 · condicionamento | F006 | pronto: DC-blocker e limiter |
| OE7 · interface | F008 | interface pronta: chassi, knobs, display de entropia, grelha de bytes, campo de endereço e telemetria; falta a auditoria de 200% de zoom |
| OE8 · isolamento | F003 | pronto: guard de alocação e troca sem alocação verificados; TSan indisponível no Windows |
| OE9 · MIDI | F009 | pronto: gate por nota, sustain e 2 CCs mapeados; falta o ensaio no host |
| T1–T4 | F010 | T1 e T3 prontos; T2 e T4 dependem de DAW instalado |

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
