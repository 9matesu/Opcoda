# Opcoda Constitution

Sintetizador granular em tempo real (VST3/CLAP/Standalone, Windows x64) que transforma binários Portable Executable em material sonoro. Opcoda da Análise e Desenvolvimento de Sistemas, 5º semestre.

Esta constitution prevalece sobre qualquer prática. Ela não é organização de Pastas: cada princípio abaixo é um critério de aceite verificável, ligado a um objetivo específico (OE) e a um ensaio do plano de testes.

## Princípios centrais

### I. Núcleo sem JUCE (NON-NEGOCIABLE)

`src/opcoda_core/` não inclui nenhum cabeçalho do JUCE e não depende de nenhuma biblioteca de plugin. Parser PE, motor granular, entropia e fila lock-free compilam como biblioteca C++20 pura.

Consequência prática: `libFuzzer`, sanitizers e `llvm-cov` rodam diretamente sobre o núcleo, sem o framework no caminho. É isso que torna OE8 e o ensaio T3 verificáveis em vez de declarativos. Se o núcleo precisar de JUCE, a alteração precisa de aprovação explícita neste documento.

### II. Tempo real rígido

O callback de áudio não aloca memória, não adquire trava, não faz I/O, não lança exceção e não chama o sistema operacional. Tudo que o DSP precisa já está pré-alocado antes do primeiro `process()`.

Parâmetros e amostras chegam ao DSP por atômicos e filas lock-free, nunca por acesso direto ao estado da interface. Nenhum caminho do Parser para o DSP existe fora da fila SPSC.

Teste do guard de alocação: um teste marca a entrada no callback de áudio e falha se `operator new` for chamado dentro da janela. Não é revisão manual; é código que quebra o build.

### III. Leitura defensiva obrigatória

Toda leitura do arquivo passa por verificação de limite antes de acontecer: `offset + tamanho` não pode transbordar o tamanho do arquivo, e a soma é conferida em aritmética que não transborda. `NumberOfSections` e `SizeOfRawData` são validados e truncados para a faixa real.

O binário nunca é executado, nunca é mapeado como executável e nunca é desmontado. Nenhum ponteiro do arquivo mapeado sobrevive ao fechamento: o DSP usa apenas a cópia defensiva no heap.

Entradas inválidas retornam erro tipado (`E_BAD_MZ`, `E_BAD_PE`, `E_TRUNCATED`, `E_OOB`, `E_TOO_MANY_SECTIONS`). Exceção não cruza a ABI do plugin.

### IV. Teste antes da entrega, com portão

Nenhuma tarefa é concluída sem que seu portão passe. Os seis portões:

| Portão | Critério de aceite |
| --- | --- |
| A · Build | compila em clang-cl e em MSVC, sem warnings sob `-Wall -Wextra -Werror` |
| B · Testes | testes unitários verdes e cobertura de branch do parser ≥ 70% |
| C · Tempo real | guard de alocação verde, zero xrun no ensaio de 5 min, p99 < 50% do orçamento |
| D · Robustez | libFuzzer por 60 s sem crash, 100% dos casos de borda rejeitados com erro tipado |
| E · Interface | contraste ≥ 4.5:1, operação completa por teclado, nome acessível por controle |
| F · Documentação | `docs/` e `README` coerentes com o código, rastreabilidade OE presente |

O portão C tem uma ressalva registrada em `docs/17-guia-de-build.md`: o pacote LLVM para Windows não distribui runtime de ThreadSanitizer. A verificação de concorrência é feita por guard de alocação, ensaio de xrun e revisão, e a lacuna do TSan fica declarada como limitação honesta em vez de ser omitida.

**Desde 19/10/2026 o portão C deixou de ter a sua maior lacuna.** A estação de trabalho com host licenciado está disponível, e o T2 mede a fração dinâmica do contrato — p99 e xruns com tráfego MIDI real — que o guard de alocação não alcança. O que era "bloqueado, não atrasado" em `docs/07` e `docs/08` passou a trabalho agendado.

### V. Simplicidade e escopo do instrumento

**O instrumento é mono.** Uma tecla de cada vez, com prioridade à última nota. As oito vozes são *slots de grão simultâneos*, não vozes de nota: o motor sempre foi mono — um gate só e envelope global na saída — e a emenda 1.2.0 corrige a documentação, que dizia "polifónico de 8 vozes" (`docs/04-objetivos.md`). O envelope é global pela mesma razão, e não por voz.

Windows x64, e o conjunto fechado de **41 parâmetros automatizáveis** da tabela abaixo — todos com ID e nome em inglês. IDs dos 25 da emenda 1.1.0 **não mudam**, para não quebrar sessões salvas nem os presets `.opcoda`; os 16 novos nascem já com ID inglês. O que não está na tabela não entra por antecipação. YAGNI vale: sem abstração especulativa, sem dependência para uso futuro hipotético.

| Grupo | Parâmetros (ID) |
| --- | --- |
| Grão (originais) | `grain`, `density`, `position`, `spray`, `pitch`, `volume` |
| Grão (1.1.0) | `window` (choice: Hann, Gaussian, Hamming, Blackman — o motor já tem as 4), `pan`, `grainlevel`, `pitchrand`, `scanspeed` (multiplicador do `advance` do transporte) |
| Envelope | `attack`, `decay`, `sustain`, `release` — o gate com rampa vira ADSR; `all-notes-off` e sustain entram na matriz de teste, sem nota presa |
| Envelope, curvas (1.2.0) | `atkcurve`, `deccurve`, `relcurve` — forma de cada estágio (exponencial a linear), na convenção do Serum |
| Filtros | por filtro (×2, em série, por voz): `f1type`/`f2type` (choice: LP, HP, BP, Notch), `f1cutoff`/`f2cutoff` (log 20–20 kHz), `f1q`/`f2q`. Coeficientes recalculados só quando cutoff/Q mudam, nunca por amostra |
| Filtro, caráter (1.2.0) | `f1slope` (12/24 dB), `f1drive`, `f1keytrack` (corte segue a nota), `f1env` (profundidade do envelope no corte) |
| LFO | `lforate` (Hz), `lfodepth`, `lfotarget` (choice: pitch, density, cutoff, position), `lfowave` (choice: sine, tri, saw, square, S&H) |
| LFO rítmico (1.2.0) | `lfosync` (free/sync com o tempo do host), `lfodiv` (1/1 a 1/64, com pontuada e tercina), `lfosteps` (degraus do padrão, na forma `steps`) |
| Teclado e escala (1.2.0) | `scale` (choice), `root` (choice, 12 notas), `glide` (portamento entre notas) |
| Nuvem de grãos (1.2.0) | `cloudsize` (largura temporal da nuvem), `pitchspread` (dispersão de afinação por grão), `grainjit` (jitter de duração) |

Cortes declarados, não escondidos: **FM não entra** — grão é playback amostrado, não oscilador, e FM de verdade exigiria portadora por voz; o LFO no pitch cobre o musicalmente útil. **Seleção inteligente de blocos (§8) entra na F025 como painel OPT**, fora da tabela de parâmetros: a escolha é feita na interface, o resultado é concatenado antes de chegar ao motor, e a tabela continua fechada em 41. **CLAP continua pós-entrega**, como no objetivo.

Dois desenhos ficam **fora da tabela por decisão, não por esquecimento**, e ambos têm o desenho completo escrito em `docs/19-parametrizacao-do-estudo-e-briefing.md`:

- **A matriz de modulação tem 8 rotas e vive no estado, não nos parâmetros.** Cada rota é um terno (fonte, destino, profundidade) guardado na árvore do APVTS como propriedade — o mesmo padrão que o `sourcePath` e a região já usam. A consequência é declarada: **as rotas viajam no preset `.opcoda` e no estado do projeto, mas o host não as automatiza.** Torná-las automatizáveis custaria 24 parâmetros (3 por rota) e levaria a tabela a 65.
- **O caráter analógico do filtro entra só no filtro 1.** O filtro 2 mantém tipo, corte e Q. Restaurar a paridade custa 4 parâmetros (`f2slope`, `f2drive`, `f2keytrack`, `f2env`) e a tabela passaria a 45; fica no desenho completo, para a próxima emenda decidir com número à frente.

Consequência sobre FFT: o caminho de áudio não usa FFT. A entropia é frequência de bytes, não análise espectral. A FFT existe apenas na ferramenta offline de medição do ensaio T1, como dependência exclusiva daquela ferramenta. Não é preciso modelo demachine learning, framework de áudio alternativo nem camada de plugin de terceiros.

A Tabela 8 do artigo e o ensaio T4 foram reescritos duas vezes, e a segunda na emenda 1.2.0: a Tabela 8 lista os **41 parâmetros** com faixas, e o T4 mede os 41. O ensaio T2 deixou de contar vozes — o instrumento é mono — e passa a medir por tamanho de grão e densidade, que é o que carrega o CPU.

### VI. Documentação como parte do código

Cada mudança de comportamento altera a documentação no mesmo commit: `docs/` reflete o estado real, `specs/` guarda a decisão e o porquê, e o `README` continua sendo a porta de entrada. Documentação desatualizada é defeito, não dívida acceptable.

A rastreabilidade é explícita e bidirecional: cada spec declara quais OE atende, e cada OE de `docs/04-objetivos.md` aponta para sua spec.

## Restrições de tecnologia

- C++20. `clang-cl` para desenvolvimento, MSVC para build de release.
- CMake com Ninja Multi-Config. Presets versionados em `CMakePresets.json`.
- JUCE 8.0.14 restrito a `src/opcoda_plugin/` (o editor vive lá; não existe `src/opcoda_ui/`). O VST3 SDK vem embutido no JUCE; não há SDK externo no repositório.
- `juce_opengl` entra pelo OpenGL dos visualizadores, e **só** no alvo do plugin: o núcleo continua sem framework nenhum. O contexto é criado na message thread, e se não ativar o componente volta a pintar por software — a queda é declarada na interface, não silenciosa.
- GoogleTest para unidade, GoogleTest com `gtest_filter` para integração, `libFuzzer` para o corpus do parser, `llvm-cov` para cobertura.
- ASan e UBSan em presets separados de TSan, porque não combinam entre si.
- Nenhuma chave, token ou credencial versionada. Segredos vêm de variável de ambiente referenciada por `{env:...}`.

## Acessibilidade como requisito, não como improvement

A interface segue WCAG 2.2 nível AA, avaliado pelo método WCAG-EM, conforme `docs/10-acessibilidade-w3c.md`. Os critérios aceitos para o Opcoda são: 2.1.1 Teclado, 2.4.7 Foco visível, 4.1.2 Nome, função e valor, 1.4.10 Refluxo, 3.3.1 Identificação de erro e 1.4.1 Uso da cor.

Erro de parser sempre aparece como texto com código tipado, nunca só por cor. Mapa de seções e curva de entropia sempre carregam rótulo ou padrão além da cor.

## Fluxo de trabalho

O desenvolvimento segue spec-kit: `/speckit-constitution`, depois `/speckit-specify`, `/speckit-plan`, `/speckit-tasks`, `/speckit-implement` para cada feature. Uma feature corresponde a um objetivo ou a um ensaio, não a um agrupamento arbitrário de tarefas.

Mapas de features: F001 fundação e build · F002 parser PE e ingestão (OE1, OE2) · F003 fila lock-free (OE3, OE8) · F004 entropia de Shannon (OE5) · F005 motor granular (OE4) · F006 condicionamento de saída (OE6) · F007 telemetria · F008 interface e acessibilidade (OE7) · F009 MIDI (OE9) · F010 bateria de validação T1–T4 · F011 parâmetros estendidos do motor (window, pan, grainlevel, pitchrand, scanspeed) · F012 ADSR por nota · F013 dois biquads em série por voz · F014 LFO com roteamento · F015 módulos de UI e nomes em inglês (§9) · F016 presets (fábrica + `.opcoda`) · F017 teto de ingestão e análise cancelável (§3/§11) · F018 teclado e afinação (nota vira taxa, escala, glide) · F019 LFO rítmico (tempo do host, nota/pontuada/tercina, degraus) · F020 caráter analógico do filtro e curvas do envelope · F021 nuvem de grãos (§8) · F022 waveform PCM do ficheiro inteiro, sobre a F017 · F023 interface v2 (§9: LookAndFeel, SVG, visualizadores, menu de contexto) · F024 visualizadores OpenGL com queda para software · F025 seleção por mochila (painel OPT, três métodos de busca local, resultado concatenado antes do motor). A ordem real é F002 antes de F001, porque o núcleo é que se constrói primeiro; a numeração registra dependência, não cronologia.

Revisão por agente especializado é obrigatória antes de declarar uma tarefa concluída: `pe-parser-reviewer` para o parser, `rt-dsp-auditor` para o DSP, `test-engineer` para testes. Os agentes de interface, desempenho e documentação entram junto com as etapas F008 e F010, que são as que dão objeto a eles.

O cronograma de entrega é S1 a S5, terminando em 03/11, conforme `docs/08-cronograma.md`. A sigla de marco é sempre `S<n>`.

## Governança

Esta constitution prevalece sobre README, comentários de código e preferência individual. Alteração exige: registro em `docs/18-processo-sdd.md` com o motivo, o impacto nos portões e o plano de migração das features afetadas.

Exceções são permitidas quando o ensaio que a comprova não existe na plataforma, desde que a lacuna seja declarada por escrito em `docs/07-plano-testes.md` e repetida nos artefatos entregues. O caso do TSan é o exemplo corrente: a limitação é documentada, não escondida.

**Versão**: 1.3.0 | **Ratificada**: 2026-10-01 | **Última alteração**: 2026-10-22
