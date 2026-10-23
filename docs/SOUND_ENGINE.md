# Sound engine

## Signal flow

PE file -> byte-to-sample conversion -> normalized audio buffer -> 8-voice
granular engine (Hann/Blackman/Hamming/Gaussian window) -> two series biquad
filters -> DC blocker -> limiter -> output. A global ADSR gate shapes the output.

## Granulation

Grains of 1-100 ms are scheduled by density (grains/second). Each grain reads
from a randomized position (base position plus spray dispersion), with optional
cloud delay, pitch spread and duration jitter. Pitch is converted to a read
step relative to source length.

## Spray vs entropy

Spray is random dispersion of each grain's read position. It does not depend on
entropy. Entropy is a measured property of the loaded byte region (Shannon
entropy in bits/byte, 0-8) and is available as a modulation source in the
modulation matrix; it is not itself a randomness generator.

## Modulation

An 8-route matrix routes sources (LFO, envelope, velocity, keytrack, mod wheel,
CC74, CC71, entropy) to destinations (pitch, density, position, spray, grain
size, filters, pan, grain level, drive). Stored in project state.

## Real-time constraints

The audio callback allocates nothing; UI-to-DSP communication uses atomics and
an SPSC queue. An allocation guard test enforces this.