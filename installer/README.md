# Opcoda Installer

Inno Setup 6 wizard for Opcoda 0.1.0. It installs the VST3 plugin, the standalone
application and the PDF manual, and offers the Visual C++ runtime on clean
machines.

## Build

    powershell -File tools\build-installer.ps1
    powershell -File tools\build-installer.ps1 -SkipBuild

The first command builds the release and the installer. The second repackages the
release that already exists. The script stages the payload, computes the component
sizes, generates the wizard artwork from `resources/brand` and calls ISCC. Output:
`installer/Output/Opcoda-Setup-<version>.exe`.

Requirements: Inno Setup 6 (`winget install --id JRSoftware.InnoSetup -e`) and a
release build for the payload.

## Components

| Component | Content | Destination |
| --- | --- | --- |
| VST3 (64-bit) | `Opcoda.vst3` | `Common Files\VST3` |
| Standalone | `Opcoda.exe` | `Program Files\Opcoda` |
| Manual | `Opcoda-Manual.pdf` | `Program Files\Opcoda\Docs` |

Only the formats that exist are offered. VST2, AU and AAX are not built.

## Runtime

The binaries are statically linked and do not require the Visual C++ runtime. The
installer still offers the Microsoft Visual C++ 2015-2022 x64 redistributable when
the runtime is missing, detected by the registry key
`HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64` or by
`vcruntime140.dll` in System32. The task is checked by default on the additional
tasks page. It is hidden when the runtime is already present. In silent mode use
`/TASKS=` to control it.

## Silent install

    Opcoda-Setup-0.1.0.exe /SILENT /SUPPRESSMSGBOXES /NORESTART /COMPONENTS=vst3,standalone,docs /TASKS=desktopicon

Destinations: `/DIR=` for the application and `/VST3DIR=` for the VST3 folder.
Logs: `/LOG=install.log`.

## Wizard flow

Welcome, license, components, plugin paths, shortcuts, ready, progress, finish.
The license acceptance is recorded in
`HKLM/HKCU\Software\bloodworm trax\Opcoda` with the version and a timestamp. The
components page shows the size of each group and the total.

## Code signing

The installer is unsigned. Sign `Opcoda-Setup-<version>.exe` with a code signing
certificate before distribution:

    signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a Opcoda-Setup-0.1.0.exe

## Uninstall

Start Menu > Uninstall Opcoda, or `C:\Program Files\Opcoda\unins000.exe`. It
removes the product files and the license registry key. Settings under
`%APPDATA%\Opcoda` are kept.