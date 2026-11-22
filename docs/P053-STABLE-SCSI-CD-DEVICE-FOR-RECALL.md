# P053 — Stable original Akai SCSI CD device across project recall

Parent P052 `7c4b70a28be1566dcecffde350e36e1c8ba027ce`; includes earlier CD-before-original-machine-state ordering fix and P051 project identity. The rejected P049 SCSI STATUS patch remains excluded.

## Newly reproduced original-ROM firmware bug

The actual owner-supplied Akai CD volume 3001 G.PF 2 was loaded through original V2.0 firmware using MAME 0.288 with optical device `cdrom_2x` at SCSI target 5. The genuine original MAME compressed full-machine snapshot was saved after loading 4 additional programs and 18 samples. The exact same native saved state was then loaded in two distinct fresh original-firmware MAME processes with NO ISO mounted:

1. **Empty CD device `cdrom_2x`**: original state jumps to saved 52-second emulated time immediately; MAME WAV output 12.36 seconds; original MIDI ProgramChange 1–4 C3 notes produce actual audio, previously proven source-causal and bit-identical on repeated restarts.
2. **Empty CD device `cdrom`** (previous P050/P051 source behavior): state does NOT jump when requested; emulated machine instead runs from 1 to 52 seconds and output WAV is 60 seconds. ProgramChange 1–4 CD-loaded voices do not sound; only factory program 0 plays. No crash is necessary for this silent data-loss failure. Exact native Linux MAME command differed only in `-scsi:5 cdrom` versus `cdrom_2x`.

These are **real original-ROM CPU/PCM experiments**, not simulated parser checks. No owner ROM or commercial ISO data was committed.

A 10-second native-MAME no-media boot performance spot check with otherwise matching hardware yielded 192.12% for `cdrom` and 198.22% for `cdrom_2x` under -nothrottle (two short runs, not a CPU benchmark). No basis to sacrifice native-state compatibility for claimed empty-drive performance.

## Correction and test

- Use `cdrom_2x` unconditionally for the chosen optical SCSI target, whether or not an ISO is inserted; keep P050 target5 CD-only and target4 HDD+CD decisions.
- Project missing ISO: maintain the same CD device with no disc, then load the full Akai machine image, preserving user-loaded samples in RAM.
- The already-present P052 mounted-media-before-state ordering remains.
- Extend P050 test with constant opt-device identity for CD-only and HDD+CD with/without ISO, and reject original plain `cdrom` when no disc. Register the tests in native Windows/Linux CTest core CI as well as existing Windows full-VST3 CI.

## Remaining acceptance

| Gate | Result |
| --- | --- |
| True original V2.0 firmware/no-disc native MAME state: cdrom_2x | PASS |
| Changing device to plain cdrom destroys CD sample recall | FAIL: reproduced, removed by P053 source |
| Project CD identity / C++ unit-tests | P053 CI pending |
| Windows x64 VST3 ROM-free compilation, Steinberg validator, Plugin Tester | Pending exact P053 commit |
| Private bundled-ROM Windows VST3: Akai CD, program MIDI, project close/reopen with missing ISO | NOT RUN |
| Actual Fender Studio Pro, multiple instances and hardened recovery | NOT RUN |

Never call P053 FINAL on original Linux MAME or ROM-free Windows host testing alone.
