# Development

## Toolchain

Visual Studio Build Tools 2022/2026, CMake 3.28+, JUCE 8.0.14 at `I:\deps\JUCE`.

## Build

```powershell
.\tools\build.ps1          # dev
.\tools\build.ps1 -Release # release
```

## Tests

```powershell
ctest --preset dev -C Debug
```

Two binaries: `opcoda_tests` and `opcoda_alloc_guard_test`.

## Notes

- `opcoda_core` is pure C++20 with no JUCE includes.
- `/WX` is enabled for `opcoda_core` only.