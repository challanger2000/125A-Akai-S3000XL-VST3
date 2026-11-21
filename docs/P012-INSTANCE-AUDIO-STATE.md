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

## Verified evidence — source commit d90306162e252b4d9af26e1fb88ea5c909f84f77
- [Native core contracts Windows + Linux](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44893227289): 5/5 PASS on each OS; includes the independently configured speaker-map and 2-instance clock contract.
- [Full Windows MAME 0.288/JUCE 8.0.12 VST3](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44893227285): build linked and strict moduleinfo PASS; 125A Plugin Tester v0.3.2 **55 PASS / 0 WARNING / 0 FAIL**, official Steinberg Validator PASS. This is explicitly the public **ROM-free** host path.
- The private owner's original Akai V2.0 firmware and display images were rechecked against the previously verified size/SHA256 contract. Locally sealed only into the above exact public PE binary without changing any prefix bytes. Private full VST3 ZIP SHA256: `8008e45bb2b18e078ad5af516e97e6e7f650dc2ce2f482dbdaa69368201a4f25`. The proprietary file is **not uploaded to GitHub**.
- Packaging negative controls actually rejected a truncated display ROM, double sealing, and unexpected VST3 metadata.

## Remaining blockers
Actual private firmware V2.0 boot, audible firmware MIDI/PCM, real two-MAME-instance behavior, WAV capture-to-Akai sample/program mapping and DAW project sample-data persistence are **NOT VERIFIED**. The P011 32 MiB sample-RAM persistence defect remains **OPEN**. A successful sealed ZIP and ROM-free QA do not establish a playable sampler or a release-ready VST3.
