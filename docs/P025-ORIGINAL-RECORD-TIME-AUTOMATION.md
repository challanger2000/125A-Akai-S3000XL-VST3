# P025 — Dynamic original Akai S3000XL V2.0 record time and factory name fix

## Real original-ROM panel evidence (2026-11-21)
The isolated local patched **MAME 0.288** S3000XL booted with the owner's original verified V2.0 and HD61830 ROM images. An original boot MENU was reached by original F8 SKIP and SAMPLE. In **RECORD SET-UP**, the initial highlighted field is `sample name`, and four original DOWN key pulses select `monitor`; one original RIGHT key pulse selects the `record tim` value.

The **original Akai V2.0 firmware**, not a plugin placeholder, accepted numeric keys:
- `2`, `0`, `0` -> LCD `record tim: 2.00s` (88200 samples and 1% RAM consumption shown).
- `1`, `2`, `3`, `4`, `5` -> LCD `record tim:123.45s` (5,444,188 samples and 33% RAM shown).
- F2 after the selected 2.00s value entered the original `RECORD MONO ... tim: 2.00s` screen. These were true original LCD framebuffer screenshots examined privately; no ROM bytes or protected assets were committed.

The original SET-UP screen displays `sample name: PULSE exists!`. The older capture-controller's global `overwriteWarning()` interpreted `EXISTS!` as a destructive operation and failed *before* it could reach the proven original NAME-edit path. This is not an overwrite prompt: it is an observation that the factory waveform is already named PULSE. The new controller rejects actual `OVERWRITE`, `REPLACE EXISTING` and `DELETE` prompts but allows `exists!`, then requires the original REC page to show `- new -` before ARM.

## P025 integration
- PCM WAV decoding and resampling remain on the control thread, before original MAME recording. The sample's prepared length and rate yield a bounded duration in **integer hundredths of seconds**. The firmware duration is rounded up and given **0.25 s safety margin**, rather than leaving the 1.00s factory default.
- Invalid or unrepresentable values above the verified original numeric-input envelope of 999.99s are refused at WAV staging; never silently truncate the source.
- MAME owner-thread controller uses only actual Akai DOWN x4, RIGHT x1 and numeric physical key pulses, waiting the established 120ms key-scan hold each time. It does **not** manipulate guest RAM, samples, program tables or ROM code.
- Before entering native REC through F2, the controller reads the *real original LCD* via `hd61830_device::snapshot_visible_text` and checks the displayed `record tim` equals exactly the requested duration. A wrong cursor, failed key, unreadable LCD, replaced WAV or timeout must abort without ARM/GO.
- Existing normal 1.00s/short WAVs also pass through explicit target-length selection (unless already equal). The live MAME producer retains source completion and early native STOP truncation checks.

## Evidence limits
Original-ROM setup duration operation and static control trace are now demonstrated; **the complete VST3 path is not yet proved**. The native offline MAME experiments had no DAW WAV feeder. Private ROM-inclusive Windows VST3 must audibly capture a WAV, create its original sample/program/keygroup, respond to C3 MIDI and survive a fresh DAW project reload without the WAV file. P025 CTests are logical contracts only, not a substitute for the original ROM Windows DAW and PCM evidence. No user-facing Final or customer package is approved.
