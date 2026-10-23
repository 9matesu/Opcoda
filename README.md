# Opcoda

Granular synthesizer (VST3 and Standalone, Windows x64) that turns Portable
Executable files into sound in real time. Source audio is the byte stream of a
`.exe`, `.dll`, `.bin` or `.sys` file.

Version 0.1.0. Research/graduation project (TCC); no commercial release yet.

## What it does

Opcoda reads a PE file as raw bytes, converts them to a normalized audio
buffer, and plays it through an 8-voice granular engine. Density, grain size,
position, pitch, filtering, LFO modulation and an ADSR gate are all available
as automatable parameters. Three views (waveform, byte grid, entropy curve) show
the region being played.

## Requirements

- Windows x64.
- Visual Studio 2022/2026 Build Tools (MSVC x64).
- CMake 3.28+.
- JUCE 8.0.14 cloned at `I:\deps\JUCE` (override with `-DJUCE_DIR=`).

## Fluxo de sinal

```mermaid
flowchart LR
  A[Arquivo PE] --> B[Bytes para amostra]
  B --> C[Buffer de audio]
  C --> D[Motor granular 8 vozes]
  D --> E[Filtros x2]
  E --> F[DC blocker]
  F --> G[Limiter]
  G --> H[Saida]
```

## Capturas de tela

Capture a interface do Standalone com `tools/capture-ui.ps1` e coloque as
imagens em `docs/assets/screenshots/`. Ainda nao ha capturas versionadas.

## Build

```powershell
.\tools\build.ps1          # Debug + AddressSanitizer + tests
.\tools\build.ps1 -Release # Release, builds VST3 and Standalone
```

The script loads `vcvars64.bat`; do not invoke CMake directly.

After a release configure, single formats can be built with:

```powershell
cmake --build --preset vst3
cmake --build --preset standalone
```

Tests:

```powershell
ctest --preset dev -C Debug
```

## Install (VST3)

```powershell
.\tools\install-vst3.ps1   # elevated, installs to Common Files\VST3
```

The per-user VST3 path is not supported by Ableton.

## First sound

1. Build and run the Standalone (or load the VST3 in a host).
2. Drag an `.exe`/`.dll`/`.bin`/`.sys` file into the window, or use the folder
   icon. Audio starts within about one second.
3. If the file is rejected, a typed error code (e.g. `E_BAD_MZ`) is shown and
   the previous material keeps playing.
4. Press Space to play the whole region without MIDI; use the keyboard or MIDI
   to trigger grains.

## Parameters

41 automatable parameters, grouped by module: grain (size, density, position,
spray, pitch, volume, window, pan, grain level, pitch random, scan rate),
global envelope (attack, decay, sustain, release and per-stage curves), two
series filters, LFO (rate, depth, target, wave, sync, division, steps),
keyboard (scale, root, glide) and grain cloud (size, pitch spread, duration
jitter). An 8-route modulation matrix is stored in project state and is not
host-automatable.

## Known limitations

- Standalone may open with wrong parameter values (e.g. 100 ms instead of 40 ms)
  due to stale JUCE session state in `%APPDATA%\Opcoda\Opcoda.settings`; delete
  that file to restore defaults.
- Mono output, last-note priority. The 8 voices are grain slots, not note voices.
- CLAP format is not provided.
- Windows only.

## Documentation

- `docs/USER_GUIDE.md` — installation, first steps, workflow.
- `docs/SOUND_ENGINE.md` — audio architecture and signal flow.
- `docs/PARAMETERS.md` — parameter reference.
- `docs/DEVELOPMENT.md` — development environment, build, tests.
- `docs/ROADMAP.md` — planned work.
- `CHANGELOG.md` — change history.

## License

No `LICENSE` file is present in the repository; legal status is undecided. The
project depends on JUCE 8.0.14 (dual GPL/commercial) and GoogleTest 1.15.2
(BSD-3-Clause). Do not redistribute without resolving licensing.

## Credits

- Granular synthesis references: Granulator II (Robert Henke), Quanta 2.
- Parser and audio architecture documented in `docs/16-arquitetura-implementacao.md`
  and `docs/atlas/`.