# Inventario

Gerado a partir de `assets/inventory.csv`. Contagens a partir do commit
`4cac7a5`, cujo codigo do produto e' o mesmo do `c6409bb`.

## Por papel

| caminho | linguagem | papel | entrada | notas |
| ------- | --------- | ----- | ------- | ----- |
| `CMakeLists.txt` | cmake | config | sim | alvo `opcoda_core` e raiz da compilacao |
| `src/opcoda_core/pe/pe_parser.cpp` | cpp | logica | sim | leitura defensiva do PE |
| `src/opcoda_core/pe/pe_parser.h` | cpp | logica | nao | tipos do PE e o codigo de erro |
| `src/opcoda_core/pe/byte_to_sample.cpp` | cpp | logica | nao | byte para amostra em `[-1, 1]` |
| `src/opcoda_core/pe/byte_range.cpp` | cpp | logica | nao | valida e alinha a regiao lida |
| `src/opcoda_core/pe/byte_to_position.cpp` | cpp | logica | nao | endereco para fracao e volta |
| `src/opcoda_core/pe/column_reduction.cpp` | cpp | logica | nao | reduz a regiao a colunas |
| `src/opcoda_core/entropy/shannon_entropy.cpp` | cpp | logica | nao | entropia de Shannon por janela |
| `src/opcoda_core/dsp/granular_engine.cpp` | cpp | logica | nao | motor granular, agendador e mistura |
| `src/opcoda_core/dsp/granular_engine.h` | cpp | logica | nao | params, graos e telemetria |
| `src/opcoda_core/dsp/window.cpp` | cpp | logica | nao | janelas Hann, Gaussian, Hamming, Blackman |
| `src/opcoda_core/dsp/biquad.cpp` | cpp | logica | nao | filtros biquad por voz |
| `src/opcoda_core/dsp/lfo.cpp` | cpp | logica | nao | LFO amostrado no instante do grao |
| `src/opcoda_core/dsp/dc_blocker.cpp` | cpp | logica | nao | remove a componente continua |
| `src/opcoda_core/dsp/limiter.cpp` | cpp | logica | nao | limitador no fim da cadeia |
| `src/opcoda_core/rt/alloc_guard.cpp` | cpp | logica | nao | substitui o `operator new` global |
| `src/opcoda_core/rt/note_tracker.cpp` | cpp | logica | nao | notas premidas, pedal e escala |
| `src/opcoda_core/rt/transport.cpp` | cpp | logica | nao | transporte de audicao |
| `src/opcoda_core/rt/spsc_ring.h` | cpp | logica | nao | fila circular sem trava, ponteiro cru |
| `src/opcoda_core/rt/atomic_param.h` | cpp | logica | nao | parametro atomico com suavizacao |
| `src/opcoda_core/view/grain_trail.h` | cpp | logica | nao | rastro dos graos, sem JUCE |
| `src/opcoda_plugin/source/plugin_processor.cpp` | cpp | entrada | sim | fronteira com o host, filas e telemetria |
| `src/opcoda_plugin/source/plugin_processor.h` | cpp | entrada | nao | estado do processador e as duas filas |
| `src/opcoda_plugin/source/plugin_editor.cpp` | cpp | entrada | sim | editor e temporizador de 60 Hz |
| `src/opcoda_plugin/source/plugin_editor.h` | cpp | entrada | nao | arranque de ficheiro por arrasto |
| `src/opcoda_plugin/source/byte_display.cpp` | cpp | logica | nao | desenho da forma de onda e do hex |
| `src/opcoda_plugin/source/hex_grid.cpp` | cpp | logica | nao | grelha de bytes, modo e zoom |
| `src/opcoda_plugin/source/byte_address_field.cpp` | cpp | logica | nao | campo de endereco |
| `src/opcoda_plugin/source/look_and_feel.h` | cpp | logica | nao | cores e tipografia |
| `src/opcoda_plugin/source/knob.h` | cpp | logica | nao | knob com resposta a MIDI |
| `src/opcoda_plugin/source/led.h` | cpp | logica | nao | LED de estado |
| `src/opcoda_plugin/source/animator.h` | cpp | logica | nao | animacao com delta medido |
| `src/opcoda_plugin/source/display_panel.h` | cpp | logica | nao | painel do display |
| `src/opcoda_plugin/source/factory_presets.h` | cpp | config | nao | seis presets de fabrica |
| `src/opcoda_plugin/source/fonts.h` | cpp | config | nao | fontes embebidas |
| `src/opcoda_plugin/source/palette.h` | cpp | config | nao | paleta |
| `tools/build.ps1` | powershell | config | sim | unico ponto de entrada do build |
| `tools/build-atlas.ps1` | powershell | config | sim | pipeline do atlas |
| `tools/render-mermaid.ps1` | powershell | config | sim | render SVG e PNG dos diagramas |
| `tools/mmd2tikz.py` | python | config | sim | Mermaid para TikZ |
| `tests/pe_builder.h` | cpp | teste | nao | construtor de PE para os ensaios |
| `tests/pe_parser_test.cpp` | cpp | teste | nao | ensaio do parser |
| `tests/pe_mutation_test.cpp` | cpp | teste | nao | ensaio de mutacao do PE |
| `tests/alloc_guard_test.cpp` | cpp | teste | nao | ensaio do guard de alocacao |
| `tests/sample_exchange_test.cpp` | cpp | teste | nao | ensaio da troca de material |
| `tests/end_to_end_test.cpp` | cpp | teste | nao | ensaio de ponta a ponta |
| `tests/t1_spectral_test.cpp` | cpp | teste | nao | ensaio espectral |
| `tests/rt_test.cpp` | cpp | teste | nao | ensaio dos atomicos e da fila |
| `tests/dsp_test.cpp` | cpp | teste | nao | ensaio do motor |
| `tests/filter_test.cpp` | cpp | teste | nao | ensaio dos filtros |
| `tests/column_reduction_test.cpp` | cpp | teste | nao | ensaio da reducao a colunas |
| `tests/transport_test.cpp` | cpp | teste | nao | ensaio do transporte |
| `tests/note_tracker_test.cpp` | cpp | teste | nao | ensaio do tracker |

## Resto, por pasta

* `src/opcoda_core/pe`: mais 2 ficheiros de cabecalho, `byte_range.h` e
  `byte_to_position.h`.
* `src/opcoda_core/entropy`: 1 ficheiro de cabecalho, `shannon_entropy.h`.
* `src/opcoda_core/dsp`: mais 4 ficheiros de cabecalho, `biquad.h`,
  `dc_blocker.h`, `lfo.h` e `window.h`.
* `src/opcoda_core/rt`: mais 3 ficheiros de cabecalho, `alloc_guard.h`,
  `note_tracker.h` e `transport.h`.
* `src/opcoda_plugin/source`: mais 1 ficheiro de cabecalho, `hex_grid.h` e
  `byte_display.h` e `byte_address_field.h`.
* `tests`: 17 ficheiros de ensaio, todos com gtest. `alloc_guard_test.cpp`
  fica num binario a parte, porque substitui o `operator new` do processo
  inteiro e nao pode conviver com o resto da suite.
* `tools`: mais 4 ficheiros, `capture-ui.ps1`, `install-vst3.ps1`,
  `vendor_fonts.py` e os geradores de apresentacao.

## Notas para quem le

O nucleo (`src/opcoda_core`) nao inclui JUCE em lado nenhum. A regra esta
escrita na constituicao do projecto, e o `AGENTS.md` repete-a, porque e' a
fronteira que o guard de alocacao e a suite de ensaios dependem.

`tests/alloc_guard_test.cpp` compoe um binario separado,
`opcoda_alloc_guard_test`, e nao `opcoda_tests`. O motivo esta em
`tests/CMakeLists.txt`: o guard substitui o `operator new` global do
processo, e um binario so com os ensaios que precisam disso e' o unico
arranjo que nao contaminationa a suite toda.

Os ficheiros de `docs/` nao entram neste inventario. Sao o que este atlas
mapeia.

A arvore de trabalho tem modificacoes por terminar na funcionalidade F018,
que acrescenta escala, tonalidade e glide. Esse codigo ainda nao esta
versionado, e por isso nao entra no inventario.
