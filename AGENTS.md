# AGENTS.md

## Build (only entrypoint)

```powershell
.\tools\build.ps1          # dev: Debug + ASan, core + tests, NO plugin
.\tools\build.ps1 -Release # release: Standalone + VST3
```

- Never run `cmake --preset` / `ctest` directly: `cl.exe` needs `INCLUDE`/`LIB` from `vcvars64.bat`, loaded only by the script (`cmake/msvc.cmake`, `tools/build.ps1:25-29`). Hardcoded paths: VS18 BuildTools, `C:\Program Files\CMake\bin\`.
- JUCE 8.0.14 must be cloned at `I:\deps\JUCE` (`CMakeLists.txt:14`, override with `-DJUCE_DIR=`). VST3 SDK comes bundled inside JUCE; no external SDK.
- `dev` preset sets `OPCODA_BUILD_PLUGIN=OFF`; `release` uses static CRT (`MultiThreaded`) — required so the VST3 loads on machines without VS.
- Test binaries need the vcvars session too: the ASan runtime is a DLL on the vcvars PATH. `STATUS_DLL_NOT_FOUND` when running `build\tests\*.exe` outside it is an env error, not a code bug (`docs/17-guia-de-build.md`).
- `cmake --build --preset vst3` / `standalone` (after a release configure) builds one format.

## Tests

- `ctest --preset dev -C Debug` runs both suites (invoked by `build.ps1` for `dev`).
- Focused: `build\tests\opcoda_tests.exe --gtest_filter=<Pattern>` (from a vcvars-loaded shell).
- Two binaries, intentionally separate: `opcoda_tests` (156+ cases) and `opcoda_alloc_guard_test` — the alloc guard replaces global `operator new` process-wide, so it cannot live in the main binary (`tests/CMakeLists.txt:35-40`).
- `/WX` is on for `opcoda_core` but off for gtest targets; the `D9025` warning when overriding `/WX` is expected, do not "fix" it. GTest 1.15.2 arrives via `FetchContent` (network needed on first configure).

## Non-negotiable architecture

- `src/opcoda_core/` (pe, entropy, dsp, rt) is pure C++20, **zero JUCE includes** (constitution §I). `note_tracker.h` deliberately reimplements MIDI iteration instead of using JUCE types so ASan/fuzzing run without the framework.
- Audio callback: no alloc, lock, I/O, exceptions, or syscalls. UI→DSP only via atomics + SPSC queue; no parser→DSP path outside the queue. The alloc guard enforces this in code, not review.
- PE reads are bounds-checked with overflow-safe `offset + size` arithmetic; `NumberOfSections`/`SizeOfRawData` truncated to the real range. Invalid input returns typed errors (`E_BAD_MZ`, `E_BAD_PE`, `E_TRUNCATED`, `E_OOB`, `E_TOO_MANY_SECTIONS`); exceptions never cross the plugin ABI.
- Plugin flags: `IS_SYNTH TRUE` + `NEEDS_MIDI_INPUT TRUE` are both required — Ableton rejects an instrument without a MIDI event bus ("plugin has instrument category, but no valid event input bus"). Enforced by a `FATAL_ERROR` guard in `src/opcoda_plugin/CMakeLists.txt:43-51`; do not remove.

## Release / install gotchas

- `build.ps1 -Release` runs a `dumpbin` guard: VST3 must not link `*D.dll`/`ucrtbased.dll` (debug CRT). `dbghelp.dll` is exempt (system DLL). Failure means the build type is wrong, not the code.
- `COPY_PLUGIN_AFTER_BUILD` is OFF. Install via elevated `.\tools\install-vst3.ps1` (target `C:\Program Files\Common Files\VST3` needs UAC). The per-user VST3 path does **not** work — Ableton ignores it. Bundle path is found by glob, never hardcode it.
- `*.ilk`/`*.pdb` inside the bundle are link residue (up to 89 MB); the install script strips them.

## Known defect (do not re-investigate)

- Standalone sometimes starts with wrong param values (e.g. `100 ms` vs default `40 ms`). Cause is stale JUCE session state in `%APPDATA%\Opcoda\Opcoda.settings` — delete that file to restore defaults. Dr. Memory confirmed no uninit-memory frame from our code. The writer of the bad values is still unidentified (`docs/16-arquitetura-implementacao.md`).

## Process

- Spec-kit + constitution (`.specify/memory/constitution.md`) prevails over README/comments. Six quality gates (build, tests, realtime, robustness, UI, docs); behavior change must update `docs/` + `specs/` + `README` in the same commit.
- Mandatory reviewer agents before closing a task: `.opencode/agent/pe-parser-reviewer.md` (parser), `rt-dsp-auditor.md` (DSP), `test-engineer.md` (tests).
- `build/`, `build-release/`, `Opcoda_artefacts/`, `*.vst3/` are gitignored build output — never version them.
