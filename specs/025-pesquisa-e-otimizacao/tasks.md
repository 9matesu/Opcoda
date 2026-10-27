# 025 · Tarefas: Pesquisa e Otimização

## 1. Núcleo de otimização (`src/opcoda_core/opt/`)

- [x] 1.1 `tsp_rng.h`, `tsp_types.h`: RNG determinístico e tipos comuns.
- [x] 1.2 `tsp_instance`: instância fixa por blocos (histograma, Manhattan × 100),
      instância aleatória simétrica semeada, validação, identificadores.
- [x] 1.3 `tsp_tour`: permutação, validade, custo fechado, percursos inicial e
      aleatório.
- [x] 1.4 `tsp_neighborhood`: troca e 2-opt, com custo delta conferido contra
      recálculo completo.
- [x] 1.5 `tsp_solvers`: Subida de Encosta, Subida com Tentativas (TMAX), Têmpera
      Simulada; resultados com iterações, avaliações, tentativas e critério de parada.
- [x] 1.6 `tsp_bruteforce`: enumeração exata n ≤ 10.
- [x] 1.7 `experiment_log`: registro e CSV sem JUCE.

## 2. Testes (`tests/opt_test.cpp`)

- [x] 2.1 Instâncias: determinismo, simetria, diagonal, blocos iguais e vazios,
      validação de entradas inválidas.
- [x] 2.2 Percursos: validade, cobertura, custo conferido à mão (instância 4 nós).
- [x] 2.3 Vizinhança: troca e 2-opt preservam permutação; delta igual ao recálculo.
- [x] 2.4 Métodos: determinismo por semente, paradas declaradas, TMAX respeitado,
      têmpera aceita piora sob temperatura alta e rejeita sob fria.
- [x] 2.5 Referência: enumeração n ≤ 10 nunca perde para os métodos.
- [x] 2.6 Registro: CSV com colunas do protocolo.

## 3. Fixtures e executor

- [x] 3.1 `specs/025-pesquisa-e-otimizacao/fixtures/material-fixo.bin` (PE mínimo,
      secção `.text` de 8 KiB em quatro quadrantes estatísticos) + gerador
      `tools/generate_fixture.py`.
- [x] 3.2 Alvo de console `opcoda_experiment` (`tools/experiment_runner/`), protocolo
      do plan, CSV em `docs/otimizacao/resultados.csv`.

## 4. Interface

- [x] 4.1 `research_panel.{h,cpp}`: abas SOBRE/PROBLEMA/MÉTODOS/GENÉTICO/ANÁLISE,
      paleta e fontes existentes, operação por teclado.
- [x] 4.2 Botão "Pesquisa e Otimização" no cabeçalho; troca do corpo do editor.
- [x] 4.3 Configuração: FIXO/ALEATÓRIO, tamanho, Gerar Problema, identificador,
      semente, mapa de calor da matriz.
- [x] 4.4 Métodos: seletor, TMAX, temperatura inicial e final, fator redutor,
      Executar, resultados completos.
- [x] 4.5 Genéticos: "Módulo em Desenvolvimento".
- [x] 4.6 Análise: bateria de 5 repetições, tabela, gráfico de barras, exportação CSV.
- [x] 4.7 Publicação: `publishReorderedBlocks` + `restoreReferenceMaterial` no
      processor; botões Ouvir resultado / Ouvir referência.

## 5. Documentação

- [x] 5.1 `docs/20-otimizacao-academica.md`: formulação, custo, algoritmos, protocolo.
- [x] 5.2 `LeiaMe.txt` na raiz.
- [x] 5.3 README: secção do painel e lista de documentos.
- [x] 5.4 CHANGELOG.
- [x] 5.5 `docs/16-arquitetura-implementacao.md`: módulo opt e painel.

## 6. Relatório e apresentação

- [x] 6.1 `tools/generate_optimization_report_assets.py`: gráficos a partir do CSV.
- [x] 6.2 `docs/relatorio-otimizacao.tex` → PDF (Times 12, pdflatex duas passagens).
- [x] 6.3 `docs/apresentacao/guia_apresentacao.md`.
- [x] 6.4 `docs/apresentacao/roteiro_oral.md`.
- [x] 6.5 `docs/apresentacao/perguntas_e_respostas.md`.
- [x] 6.6 `docs/apresentacao/exemplo_calculado.md`.

## 7. Portões

- [x] 7.1 `tools/build.ps1` verde (dev com ASan).
- [x] 7.2 `ctest --preset dev -C Debug` verde, incluindo `Opt.*`.
- [x] 7.3 `tools/build.ps1 -Release` verde (plugin compila).
- [x] 7.4 Revisão: rt-dsp-auditor e test-engineer.
- [x] 7.5 Commits isolados na branch; nada de merge.
