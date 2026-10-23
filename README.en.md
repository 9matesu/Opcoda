# Opcoda

Real-time granular synthesizer (VST3 and Standalone, Windows x64) that turns
Portable Executable files into sound. Source audio is the byte stream of a
`.exe`, `.dll`, `.bin` or `.sys` file.

Version 0.1.0. Research/graduation project; no commercial release yet.

## Signal flow

```mermaid
flowchart LR
  A[PE file] --> B[Byte to sample]
  B --> C[Audio buffer]
  C --> D[Granular engine 8 voices]
  D --> E[Filters x2]
  E --> F[DC blocker]
  F --> G[Limiter]
  G --> H[Output]
```

## Screenshots

Capture the Standalone UI with `tools/capture-ui.ps1` and place images under
`docs/assets/screenshots/`. No screenshots are committed yet.

## Build

```powershell
.\tools\build.ps1          # Debug + ASan + tests
.\tools\build.ps1 -Release # Release, VST3 + Standalone
```

See `docs/DEVELOPMENT.md`.