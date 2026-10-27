# 025 · Plano: Pesquisa e Otimização

**Branch**: `feat/academic-optimization`
**Data**: 2026-10-26
**Spec**: `specs/025-pesquisa-e-otimizacao/spec.md`

## Arquitetura

### Núcleo (`src/opcoda_core/opt/`, C++20 puro, sem JUCE)

| Unidade | Responsabilidade |
| --- | --- |
| `tsp_instance.{h,cpp}` | Instância: n, matriz de custos, identificador, semente, origem. Construção a partir de blocos (histogramas → distância de Manhattan × 100) e geração aleatória simétrica semead por `std::mt19937_64`. Validação. |
| `tsp_tour.{h,cpp}` | Percurso como permutação; validade; custo fechado `Σ d(πi, π(i+1) mod n)`; percurso inicial e percurso aleatório semeado. |
| `tsp_neighborhood.{h,cpp}` | Movimentos: troca de posições e inversão 2-opt. Ambos preservam permutação. |
| `tsp_rng.h` | `TspRng`: envoltório determinístico sobre `std::mt19937_64` (inteiros e reais em [0,1)). |
| `tsp_solvers.{h,cpp}` | Subida de Encosta (melhor-melhora sobre troca + 2-opt), Subida com Tentativas (TMAX reinícios semeados), Têmpera Simulada (Metropolis, resfriamento geométrico `T ← α·T`). Resultados com custo inicial/final, iterações, avaliações, tentativas e critério de parada. |
| `tsp_bruteforce.{h,cpp}` | Enumeração exata para n ≤ 10 (validação por referência). |
| `experiment_log.{h,cpp}` | Registro de experimento e escrita CSV sem JUCE, reutilizado pela interface e pelo executor de console. |

Decisões de assinatura: nenhum algoritmo aloca fora de vetores locais; nenhum erro
lança exceção — funções devolvem `bool` ou resultado vazio; `TspResult` é estruto
copiável. O tempo de execução é medido com `std::chrono::steady_clock` no chamador
que exibe o resultado.

### Interface (`src/opcoda_plugin/source/research_panel.{h,cpp}`)

- Botão "Pesquisa e Otimização" no cabeçalho, à direita do bloco de ficheiro.
- O painel ocupa todo o corpo abaixo do cabeçalho quando activo; os componentes do
  corpo são escondidos e repostos ao sair. Nada do corpo é destruído.
- Sub-seções trocadas por abas no estilo das abas de módulo: SOBRE, PROBLEMA,
  MÉTODOS, GENÉTICO, ANÁLISE.
- Execução na thread de mensagem. Com os limites da interface (n ≤ 64, TMAX ≤ 100,
  2000 iterações por corrida, 20000 iterações de têmpera) cada execução termina em
  milissegundos; o callback de áudio não é tocado.

### Publicação do material reordenado

`PluginProcessor::publishReorderedBlocks(bytes, label)` junta os blocos na ordem do
percurso, converte com `pe::toSamples`, mede a curva com `pe::reduceToColumns` e
publica pela fila `incoming_` — o mesmo caminho de `publishByteRange`, sem segundo
motor e sem tocar no callback. `restoreReferenceMaterial()` república a região
corrente. Ambos correm na thread de interface.

## Protocolo experimental

- Instâncias: material-fixo (16 blocos de 512 bytes da secção `.text`) e aleatórias
  R8, R16, R32 (sementes 20260101, 20260102, 20260103).
- Métodos e parâmetros: Subida de Encosta (máx. 500 iterações); Subida com Tentativas
  (TMAX = 10, 500 iterações por tentativa); Têmpera Simulada (T0 = 10, Tmin = 0,1,
  α = 0,995, máx. 20000 iterações). Escala do custo: [0, 200].
- 10 repetições por método por instância, sementes 1 a 10.
- Métricas: melhor custo final, custo inicial, tempo, iterações, avaliações,
  tentativas, semente. Estatísticas: média, desvio-padrão, mínimo, máximo por célula
  (instância × método).
- Verificação de referência: enumeração exata nas instâncias com n ≤ 10 (R8 e os
  testes unitários).
- Executor: alvo de console `opcoda_experiment` em `tools/experiment_runner/`,
  sem JUCE, escreve `docs/otimizacao/resultados.csv`.

## Riscos e custos

- **LaTeX na estação**: `pdflatex` do MiKTeX existe e o pipeline do atlas já o usa;
  o relatório usa `mathptmx` (Times) para não depender de fonte do sistema.
- **Não dizer o que não é**: custo não é qualidade musical. Os quatro documentos de
  apresentação e o relatório repetem a limitação no mesmo lugar onde citam números.
- **Portão C**: nenhuma linha do callback muda; a fila `incoming_` continua a ser
  push/pop de ponteiro cru na mesma capacidade.

## Portões

- A · Build: `tools/build.ps1` (dev, com ASan) e `tools/build.ps1 -Release`.
- B · Testes: `ctest --preset dev -C Debug`, suite nova `Opt.*`.
- E · Interface: teclado completo no painel, nomes acessíveis, contraste da paleta
  existente (tokens já ≥ 4.5:1 nos fundos usados).
- F · Documentação: README, `docs/20-otimizacao-academica.md`, `LeiaMe.txt`,
  relatório PDF e material de apresentação coerentes com o código do mesmo commit.
