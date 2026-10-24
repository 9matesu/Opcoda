# Desenvolvimento

## Ambiente

- Windows x64.
- Visual Studio 2022 ou 2026 Build Tools com MSVC x64.
- CMake 3.28 ou superior.
- JUCE 8.0.14 clonado em `I:\deps\JUCE`.
- GoogleTest 1.15.2 via `FetchContent` na primeira configuração.

O `cl.exe` precisa das variáveis de `vcvars64.bat`. O script
`tools/build.ps1` carrega esse ambiente e chama o CMake. Não rode
`cmake --preset` fora dele.

## Build

```powershell
.\tools\build.ps1          # Debug, AddressSanitizer e testes
.\tools\build.ps1 -Release # Release, VST3 e Standalone
```

Compilação de um formato depois da configuração Release:

```powershell
cmake --build --preset vst3
cmake --build --preset standalone
```

## Testes

```powershell
ctest --preset dev -C Debug
```

Dois binários:

| Binário | Conteúdo |
| --- | --- |
| `opcoda_tests` | 345 casos: parser, entropia, DSP, tempo real, interface, presets. |
| `opcoda_alloc_guard_test` | Guard de alocação no callback de áudio. |

Um filtro específico:

```powershell
build\tests\opcoda_tests.exe --gtest_filter=Cloud.*
```

Rode os binários em um shell com `vcvars64` carregado. Fora desse ambiente, o
AddressSanitizer pode faltar e o processo termina com `STATUS_DLL_NOT_FOUND`.

## Estrutura

```
src/opcoda_core/     núcleo C++20 sem JUCE
  pe/                parser PE, bytes, colunas, entropia
  entropy/           Shannon por janela
  dsp/               motor granular, filtros, LFO, janelas, limiter
  rt/                fila SPSC, note tracker, transporte, alloc guard
  view/              rastro de grãos
src/opcoda_plugin/   AudioProcessor JUCE, editor, presets
tests/               GoogleTest
docs/                documentação e artigo
specs/               especificações por feature
```

## Regras de arquitetura

- `opcoda_core` não inclui JUCE.
- O callback de áudio não aloca, não bloqueia e não faz I/O.
- A interface envia material pela fila SPSC. Não existe caminho direto da
  interface para o motor.
- Leituras PE usam aritmética segura contra overflow e retornam erros tipados.
- O plugin declara `IS_SYNTH` e `NEEDS_MIDI_INPUT`; o Ableton recusa um
  instrumento sem barramento de eventos MIDI.

## Release e instalação

O build Release usa CRT estático. O script verifica o VST3 com `dumpbin` e falha
se encontrar DLL de debug. O instalador remove `.ilk` e `.pdb` do bundle.

```powershell
.\tools\install-vst3.ps1
```

## Defeito conhecido

O Standalone pode restaurar parâmetros de uma sessão antiga em
`%APPDATA%\Opcoda\Opcoda.settings`. Apague o arquivo para voltar aos defaults.
O Dr. Memory não encontrou leitura de memória não inicializada no código do
Opcoda. A origem dos valores errados continua sem identificação.
