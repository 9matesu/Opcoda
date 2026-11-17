# Opcoda Installer

Professional wizard installer for Opcoda (Inno Setup 6).

## Build

    powershell -File tools\build-installer.ps1            # release + installer
    powershell -File tools\build-installer.ps1 -SkipBuild  # repackage existing release

Output: `installer/Output/Opcoda-Setup-<version>.exe`.

Requirements: Inno Setup 6 (`winget install --id JRSoftware.InnoSetup -e`),
CMake/NMake via `tools/build.ps1` for the release payload.

## What it installs

| Component | Content | Size |
|-----------|---------|------|
| VST3 Plugin (64-bit) | `Opcoda.vst3` bundle -> Common Files\VST3 | computed at build |
| Standalone Application | `Opcoda.exe` -> Program Files\Opcoda | computed at build |
| Documentation | `UX-REFINING.md` + `Opcoda-Manual.pdf` -> Program Files\Opcoda\Docs | computed at build |

Only formats that exist in this project are offered (VST2/AU/AAX builds do
not exist). The component manifest lives in `opcoda.iss` ([Components],
[Files], [Tasks]); the build script injects version and per-component sizes.

## Environment preparation (dependencies)

The plugin binaries are statically linked (no VC++ runtime is strictly
required), but the installer still offers the **Microsoft Visual C++
2015-2022 Redistributable (x64)** on the tasks page, so that a clean PC is
ready for this and future builds:

- The task is checked by default; it is a checkbox on the *Select Additional
Tasks* page (clear consent, can be unchecked).
- Detection: the option only appears when the runtime is missing (registry
`HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64` or
`vcruntime140.dll` in System32). On machines that already have it, the task
is hidden and skipped.
- The redistributable is bundled in the installer (`installer/redist/`,
downloaded once at build time from `aka.ms/vs/17/release/vc_redist.x64.exe`)
and installed silently with `/install /quiet /norestart` during the
installation, with a status message.
- In silent mode, the task follows the same default (checked); use
`/TASKS=` to control it (e.g. `/TASKS=desktopicon` skips the runtime).

## Silent / unattended install

    Opcoda-Setup-0.1.0.exe /SILENT /SUPPRESSMSGBOXES /NORESTART ^
        /COMPONENTS=vst3,standalone,docs /TASKS=desktopicon

Custom destinations: `/DIR=...` (application folder) and `/VST3DIR=...`
(VST3 folder). Logs: add `/LOG=install.log`. Components use the identifiers
from `opcoda.iss` ([Components]): `vst3`, `standalone`, `docs`.

## Wizard flow

Welcome -> License (acceptance recorded in `HKLM/HKCU
\Software\bloodworm trax\Opcoda`: version + timestamp) -> Components (sizes per
component + total package size) -> Plugin paths (VST3 folder with Browse and
Restore defaults; app folder) -> Shortcuts (desktop) -> Ready (space required)
-> Progress -> Finish (launch Opcoda / open documentation).

## Code signing (publisher step)

The installer is unsigned. Sign `Opcoda-Setup-<version>.exe` with a code
signing certificate before distribution:

    signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 ^
        /a Opcoda-Setup-0.1.0.exe

## macOS

The project has no macOS build targets (Windows-only toolchain), so no macOS
installer is produced. When macOS artifacts exist, the same component manifest
maps to the [Mac] destinations in the spec (VST3/AU/AAX/Standalone).

## Uninstall

Start Menu -> Uninstall Opcoda, or `C:\Program Files\Opcoda\unins000.exe`.
Removes only product files and the license registry key; user settings under
`%APPDATA%\Opcoda` are preserved.