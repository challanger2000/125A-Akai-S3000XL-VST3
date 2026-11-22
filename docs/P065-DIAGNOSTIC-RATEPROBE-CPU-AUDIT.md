# P065 — Remove obsolete rateprobe CPU work while preserving original Akai hardware

## Evidence, not a theory
The P062 firmware/SCSI5 path loads the owner's untouched Akai Volume 1 ISO through the original Akai V2.0 LOAD/F5-SCSI/F2-VOLS/F1-LOAD/F8-GO controls. The original ISO exposes 14 volumes; Volume 3001 G.PF 2 loads 4 programs / 18 samples.

A controlled comparison with identical private original ROMs, ISO, MAME Linux binary, `-nothrottle`, and 24 emulated seconds of ROM boot was made. This lab binary differs by **one byte only**: an early RET immediately following the ELF CET `endbr64` prologue of the diagnostic-only `s3000_state::rateprobe_tick`. The callback was previously scheduled at **1000 Hz** and executed multiple emulator CPU-memory reads even though logging was compiled out by `S3000_TRACE=0`.

Initial single samples:
- P062 original: average 132.25% emulation speed; elapsed 22.73 s for a 24-emulated-second benchmark.
- P062 with `rateprobe_tick` doing no work: average 162.44%; elapsed 18.82 s.

These are **not** claims of a repeatable 23% plugin gain: shared-runner CPU scheduling and cache/cold-start differences require multiple runs. A repeatable source-built P065 Linux benchmark with identical original ROM and ISO is needed.

A separate isolated lab control disabled `adc_clock_tick` and increased performance, but it also disables essential A/D recording behavior and is **explicitly rejected**. P065 does not change the A/D timer, rate, sample memory/DMA, acoustic path, MAME CPU, SCSI/CD read speed, original Akai firmware, program parser or output audio.

## Narrow implementation
Add to the already existing `s3000_state::rateprobe_tick`:
```cpp
// P065: Diagnostic only. Avoid 1000 CPU-RAM reads/s in release.
// Keep the allocated timer and callback identity for native state recall.
if (!S3000_SAMPLING_DIAG) return;
```
The compile-time diagnostic setting is already 0 in release. This makes the callback return before any RAM reads. The allocated timer's identity and scheduling are unchanged, avoiding avoidable MAME native-state serialization differences. When explicitly compiled with diagnostics enabled, the original observer logic remains reachable.

## Rejected apparent optimizations
- P063 2x→4x optical transport: owner-original ISO cold boot + sample-import LCD progress was **17/17 screenshot SHA-256-identical at equal emulated times**. Increasing CD byte pacing did not show faster loading and was not inherited here.
- P064 proposed 12× non-recording DSP sample DMA: a private two-byte controlled native MAME lab experiment (constructor default and post-record strobe 12) likewise yielded 17/17 identical original-ISO LCD screenshots at matching emulated timestamps. No faster loading was demonstrated. It is not inherited here.

## QA required for final
- P065 static source guard with 7 broken mutants, native C++ contracts Windows/Linux, pinned MAME patch applicability, Linux native complete MAME build and Windows full VST3/Steinberg/125A QA.
- Compare full owner ISO 4-program/18-sample load with P062 on original-ROM, **wall time** and **emulated machine time** separately, 3 repetitions per version if practical. Confirm program names, RAM percentage and selected program.
- Verify the exact original MIDI notes produce the same sample-source audio (including stereo, attack, velocity); compare PCM hashes if deterministic.
- Original input A/D recording mono and stereo behavior and DAW project complete-machine snapshot/restoration must remain valid. The original-ROM owner Windows VST3 in Studio Pro has to be tested, not inferred from ROM-free Linux CI.

No private ROM or commercial ISO is contained in public code or workflow artifacts. **Release NO-GO until acceptance.**
