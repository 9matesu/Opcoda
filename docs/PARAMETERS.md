# Parameters

41 automatable parameters. Ranges and defaults are defined in
`plugin_processor.cpp` (`createParameterLayout`). Summary by module:

- Grain: size (ms), density (grains/s), position (0-1), spray (0-1), pitch
  (semitones), volume (dB), window type, pan, grain level, pitch random (st),
  scan rate.
- Envelope: attack, decay, sustain, release (s) and per-stage curves (-1..1).
- Filters: type, cutoff (Hz), Q; filter 1 also slope, drive (dB), keytrack,
  envelope depth.
- LFO: rate (Hz), depth, target, wave, sync, division, steps.
- Keyboard: scale, root, glide (s).
- Cloud: size (0-1, up to 500 ms), pitch spread (0-24 st), duration jitter
  (0-1).

Modulation matrix: 8 routes, stored in project state, not automatable.