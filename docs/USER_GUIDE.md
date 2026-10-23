# User guide

## Installation

See README and `docs/17-guia-de-build.md`. Build the Release configuration, then
install the VST3 with `tools/install-vst3.ps1` (elevated). The Standalone runs
directly from `build-release`.

## First sound

1. Launch the Standalone or load the VST3.
2. Load a PE file by drag-and-drop or the folder icon.
3. Wait for the material to appear (under one second).
4. Play with Space, or trigger notes via MIDI/keyboard.

## Controls overview

- Views: `1` waveform, `2` byte grid, `3` entropy curve.
- Transport: Space play/stop; Position knob seeks.
- Navigation: arrows move cursor; Enter activates byte; Esc returns cursor to
  playhead; `A` aligns region to nearest PE section.
- File: `L` opens file; `K` focuses the on-screen keyboard.

## Known issue

If parameters open with unexpected values, delete `%APPDATA%\Opcoda\Opcoda.settings`.