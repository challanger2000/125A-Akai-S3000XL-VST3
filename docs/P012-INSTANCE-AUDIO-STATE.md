# P012 — Instance-isolated S3000XL audio engine state

## Source defect
The MAME output-channel map was cached in process-shared function-local
`static` variables. With two VST3 instances the second one reused the
first one's speaker order; concurrent original firmware boots could race.
The original ADC input correction ticker and diagnostic counters also
shared process-wide state between instances. The ROM-free P011 host QA
could not exercise these paths.

## Repair
Each processor now owns `akai::runtime::AudioState`, reset by its
MAME worker before device initialization. The actual MAME speaker map
is determined separately and is checked for bounds before reading PCM.
ADC drift trim uses a per-instance clock with unchanged 8192-frame
target, 2048-frame hysteresis and 64-call cadence. All diagnostics
are likewise per-instance; no extra audio-thread lock, file I/O or
allocation is introduced.

## Regression
The new native `akai_instance_audio_state_contract` tests two
differently ordered machine speaker layouts, channel selection, invalid
geometry, staggered correction counters, per-instance reset and
simultaneous thread updates; a legacy shared-timer counterfactual
demonstrates the class of error. Tests use unconditional Release checks.

## Scope of evidence
Locally compiled and exercised GNU C++20 isolated helper: PASS.
Full Windows VST3, actual ROM-backed boot, original Akai MIDI/PCM,
multiple real MAME instances, WAV recording and DAW recall are NOT
established by this source-only regression. The 32 MiB sample-RAM
recall gap in P011 remains independently open.
