# Handoff — feat/academic-optimização

Sessão encerrada com o núcleo da seção de Pesquisa e Otimização (spec 025) implementado em `src/opcoda_core/opt/` e o executor de console em `tools/experiment_runner/`. A branch `feat/academic-optimization` está isolada de `main` e ainda não foi publicada.

## Feitos

O núcleo do PCV sobre blocos de bytes está completo e sem JUCE, respeitando a constituição §I. Foram escritos:

- `tsp_types.h/.cpp`: `Cost`, `StopReason`, `TspResult` (com igualdade estrutural para testes de determinismo).
- `tsp_rng.h`: `TspRng` sobre `std::mt19937_64`, determinístico bit a bit pelo padrão C++; `uniformInt`, `uniformReal`, `shuffle` de Fisher-Yates.
- `tsp_instance.h/.cpp`: `buildInstanceFromBlocks` com validações tipadas (`E_NODATA`, `E_BAD_RANGE`, `E_OOB`, `E_BLOCK_COUNT`, `E_TOO_FEW_BYTES`), histograma normalizado de 256 valores por bloco e `d(i,j) = L1 × 100`, simétrico com diagonal zero; `randomSymmetricInstance` gerando matriz inteira em [1, 200] a partir de `mt19937_64`.
- `tsp_tour.h/.cpp`: `isPermutation`, `tourCost` (ciclo fechado), `identityTour`, `randomTour`.
- `tsp_neighborhood.h/.cpp`: `applyMove` (troca e 2-opt), `moveDelta` com dedução exata de arestas (incluindo o par 0/n−1 e a inversão do ciclo inteiro com delta zero), `allMoves` excluindo a reversão do ciclo completo.
- `tsp_solvers.h/.cpp`: subida de encosta melhor-melhora com limite de passos, subida com TMAX reinícios e tempera simulada com Metropolis `exp(−Δ/T)`, resfriamento geométrico e best-of-run; validação de parâmetros (`E_INVALID_TEMPERATURE`, `E_INVALID_ALPHA`, `E_INVALID_MAX_ITERATIONS`, `E_TOO_FEW_NODES`).
- `tsp_bruteforce.h/.cpp`: otimo exato por enumeração para `n ≤ 10`, fixando o nó 0.
- `experiment_log.h/.cpp`: `ExperimentRecord` de 14 colunas, `experimentCsvHeader`, `experimentCsvRow` e `writeExperimentCsv` sem JUCE.

Foram criados `tools/generate_fixture.py` (PE mínimo de 8 KiB com quatro quadrantes estatísticos: zeros, gradiente, LCG de semente 20260101 e texto ASCII) e `tools/experiment_runner/main.cpp` (console, sem JUCE; `--fixture`, `--random seed:n`, `--methods`, `--reps`, `--tmax`, `--hc_iter`, `--sa_t0/tmin/alpha/iter`, `--out`, `--summary`).

O CMake foi atualizado: as fontes de `opt/` entram em `opcoda_core`; `tests/CMakeLists.txt` ganhou `opt_test.cpp`; `CMakeLists.txt` ganhou o alvo `opcoda_experiment` ligado a `opcoda_core`.

Foram escritos `tests/opt_test.cpp` cobrindo: determinismo de instâncias, simetria e diagonal zero, validação de erros, blocos iguais a custo zero, validade de percursos e custo fechado conferido à mão, preservação de permutação por troca e 2-opt, delta exato conferido contra recálculo integral (inclusive casos de borda), determinismo dos três métodos, critérios de parada (`kNoImprovingMove`, `kMaxIterations`, `kTemperatureFloor`, `kAttemptsExhausted`), TMAX respeitado, métodos nunca pioram a referência exata por enumeração, e formatação do CSV.

O fixture foi gerado: `specs/025-pesquisa-e-otimizacao/fixtures/material-fixo.bin` (8704 bytes, 16 blocos de 512 bytes).

## Em andamento

Nada está parcialmente escrito no núcleo. Os próximos blocos de trabalho ainda não foram iniciados:

- Execução do build dev + ASan e `ctest --preset dev -C Debug` para validar a suíte `opcoda_tests` com os novos casos de `opt_test.cpp`.
- Geração de `docs/otimizacao/resultados.csv` via `opcoda_experiment --fixture` sobre as instâncias fixa e aleatórias R8/R16/R32 (sementes 20260101–03), 10 repetições (sementes 1–10), HC 500 iters, HCR TMAX=10 × 500, SA T0=10, Tmin=0,1, α=0,995, 20000 iters, conforme o protocolo experimental da spec 025.
- UI do painel (`plugin_editor`/`plugin_processor`): botão "Pesquisa e Otimização", `publishReorderedBlocks()`, `restoreReferenceMaterial()` — só compila no preset `release` (JUCE).
- Documentação, LeiaMe.txt, apresentação e relatório PDF (pdflatex + mathptmx) com gráficos gerados por `tools/generate_optimization_report_assets.py`; portões A/B/E/F.

## Próximo passo

Rodar `.\tools\build.ps1` (dev, com ASan) e depois `ctest --preset dev -C Debug` a partir de um shell com `vcvars64.bat` carregado, para confirmar que `opcoda_tests` passa com os 156+ casos existentes mais os novos de `opt_test.cpp`. Só então seguir para a geração do CSV e o restante dos portões.

## Observações sobre o estado da árvore

Existem três entradas não rastreadas que não pertencem a esta feature e não foram comitadas: `assistant-panel.spec.ts`, `commands.rs` e `tikzBabel/`. São materiais pré-existentes do autor; decidir se entram na branch ou ficam de fora antes de publicar.
