# 125A Akai S3000XL VST3 — Project notes

## Repository visibility and private build: deliberate separation
**This GitHub repository is intentionally PUBLIC to minimize GitHub Actions CI costs (public-repository Actions runner billing), NOT for collaboration or source-code accessibility. Do not question or suggest changing its visibility merely because the resulting plugin is private.** The **ROM-inclusive, self-contained Windows VST3 is for private personal use only**. The proprietary firmware ROMs are deliberately absent from the repository, Git history, public releases, and public CI artifacts. These two visibility choices are not contradictory. This is a fixed project decision, not an open question.

## Goal
Develop and validate a **single native Windows VST3** reproducing the Akai S3000XL hardware and firmware behavior as faithfully as practical, building on [Tuth/S3000XL-VST](https://github.com/Tuth/S3000XL-VST) v1.2 (upstream commit `d4a5042dffa26fd833eafae5c0cc9d26e69ef99e`).

This repository imports the **original upstream source and patch files unchanged** as its baseline. The original `README.md`, `LICENSE`, copyright notices and upstream build documentation remain in place. MAME, JUCE and other components retain their applicable license obligations. **GPLv3 applies to this combined work.**

## Private firmware integration — user requirement
- User-supplied archive checked locally: `S3000XL-ROMs-V2.0-geprueft(1).zip`.
- Supplied main firmware `s3000xl_v2_0.bin`, 524288 bytes, SHA-256 `67917482d2a26163d8aee4de676ecbce4fd42859a3f6223c5442426998a086c5`.
- Supplied LCD character ROM `hd61830.bin`, 1472 bytes, SHA-256 `8738511c6a2f02b0ef1be2cde90898458b4517151cb473ff28ae212dbd1c1737`.
- **Final private Windows VST3 build must contain both ROMs internally**, needing no external firmware directory, copy operation, or separate installer step.
- Keep ROM bytes and any binary embedding artifacts **out of this public repository**, its Git history, public CI artifacts and public releases. Build the private, ROM-inclusive version only from an approved local/private input location. No user-provided ROM bytes are stored here.
- Verify MAME firmware boot and live LCD output with this specific ROM set before claiming that bundling works.
- Private integration is intended solely for the user's own use; do not release/distribute a ROM-inclusive build without independently confirming rights to do so.

## WAV import requirement — one-step, no external tools
The user wants to import ordinary Windows .wav samples **directly into the running emulated Akai**, using an in-plugin file picker and/or drag-and-drop. Import must not require the user to create, manage or mount floppy/HDD images or run an external converter.

### Preferred implementation to investigate
- Decode WAV inside the plugin (mono and stereo, common PCM and float encodings) with explicit conversion to supported native Akai S3000 sample representation, with no accidental normalization/clipping.
- Make sample data available to the **original Akai firmware** via a safe internal virtual-media/transfer path; preserve its native program/sample management, keygroup semantics, MIDI playback and DSP. Avoid ad-hoc writes into sample RAM that bypass firmware bookkeeping.
- Deliver one-step user UX: choose/drop a WAV -> conversion + transfer + original-OS sample registration/load -> immediately audibly usable/assignable, with clear failure feedback.
- Compare the converted audio and resulting Akai sample metadata with known native S3000 images. Check stereo channels, loop points, sample rate, length, pitch/root note, name handling, memory limits and level accuracy.
- Save and reopen the DAW project: imported sample bytes **and** native Akai program/mapping/selection must survive, independent of the original WAV remaining on disk. Different VST instances must not share mutable import state.
- Prefer a fully integrated conversion module, not a separately installed program. Study `akaiutil` for format-validation reference only pending compatibility/licensing evaluation (its published license is GPLv2, while this project combines GPLv3 components).
- This feature is **not implemented upstream**. The plugin currently accepts DAW input for original-firmware sampling and mounts Akai floppy/CD/SCSI images; those are distinct from direct WAV import.

### UX and implementation precedent: the user's Emulator II and Emulator III projects
This is **not a new UX concept**: follow the existing user-approved direct WAV-import workflow and standards of the user's repositories
- https://github.com/challanger2000/125A-Emulator-II-Windows-VST3 — `docs/P001-WAV-BRIDGE.md`, `src/125a_wav_input.h`, firmware SAMPLE/SAR recording
- https://github.com/challanger2000/125A-Emulator-III-Windows-VST3 — `source/upstream/runtime/wav_import.h`, `source/upstream/runtime/wav_firmware_import.h`, firmware-driven import

The **user-facing workflow should match the Emulator II/III**: directly choose/load a WAV within the plugin and make it playable as a native sample through the original sampler firmware, with no additional externally installed utilities or manual disk image creation. Reuse the **approach and verified methods**, but adapt the implementation to **Akai S3000XL's** own ADC, disk, firmware and sample-memory architecture; do not transplant E-mu-specific device code or assume identical sampling behavior. Protect native sample formatting, stereo, memory, and project recall. Require positive S3000XL ROM-backed proof before declaring parity.

### Repository audit flags (initial source inspection; needs actual tests)
- The upstream code still contains legacy Ensoniq SD-1 names, OS-RAM and sequencer-RAM save/restore code, and a UI message claiming "10 specific .bin files." These are suspect for the S3000XL, which requires the main V2.0 firmware and HD61830 character ROM. Audit/remove only after checking which paths are live.
- MAME sample-memory region is mapped to 32 MiB, but effective S3000XL V2.0 detected size must be measured with firmware.
- The upstream save-state code visibly serializes SD-1 `osram`/`seqram` shares; that is **not a proof** S3000XL sample RAM and programs are restored correctly. True sample/project recall is a priority.
- MAME global process state may prevent safe simultaneous VST instances, as upstream port notes warn. Verify, do not assume.

## P001 implementation in development (not yet Windows-built)
- **REC GAIN and MAIN VOLUME source wiring implemented** on `develop/v1.0.0`. The original Akai operator's manual identifies the second knob as **MAIN VOLUME**, not "REC VOLUME".
- Native MAME-panel hotspots now carry real host parameter gestures, and values reach pre-A/D input attenuation and the post-DSP Main L/R output attenuation, respectively. OUT 1..8 are intentionally unaffected by MAIN VOLUME.
- The original `volume` MIDI CC7 parameter, hardware emulation core, MIDI and ROM paths are unchanged.
- **Functional level contract unit test** compiled and executed on Linux/g++ using byte-identical Git blobs: 9,617 checks PASS (endpoints, stereo/polarity, nonfinite handling and 5ms smoothing). **This does not certify Windows VST3 compilation, physical-pot calibration, firmware sampling, GUI interaction or DAW recall.**
- The correct analogue potentiometer taper is not documented; current 0..1 curve is a safe, explicitly approximate attenuation with unity matching upstream.
- The **Mon** softkey / meter monitoring remains open and is not a physical analogue knob.
- See `docs/P001-ANALOG-CONTROLS.md` for provenance and remaining verification.

## Development order
1. Reproduce a Windows x64 VST3 build from the upstream baseline (JUCE, MAME 0.288, Visual Studio 2022).
2. Verify boot, firmware/ROM requirements, MIDI, 10 audio outputs, sampling, media loading and original sound path.
3. Audit actual project recall, simultaneous instances, and RAM configuration/capacity before claiming support.
4. Investigate and implement missing REC GAIN, REC VOLUME, pre-record MON operation.
5. Evaluate the optional EB16 hardware/effects separately; no implementation is claimed.
6. Validate audio, performance, stability and VST3 lifecycle with automated tests before the final DAW acceptance run.

## Acceptance policy
- No modified playback DSP without A/B tests against the original firmware/emulation reference.
- No ROMs, commercial sample libraries, or proprietary Akai software in the Git repository.
- Avoid unnecessary front-end features until basic hardware/firmware behavior is validated.
- Do not claim any test passed unless an actual test report demonstrates it.
- Perform the final practical Fender Studio Pro / Studio One acceptance only after automated technical checks.

## Known upstream limitations (reported, **not yet retested here**)
- `REC GAIN` and `REC VOLUME` are ineffective.
- Pre-record `MON` input monitoring is missing.
- Sample loading is slower than physical hardware.
- EB16 hardware/effects emulation is not established.

## Upstream provenance
- Source: https://github.com/Tuth/S3000XL-VST
- Baseline tag: https://github.com/Tuth/S3000XL-VST/releases/tag/v1.2
- Revision: `d4a5042dffa26fd833eafae5c0cc9d26e69ef99e`
- License: GPLv3, see `LICENSE`

**Current development status:** P001 analogue controls coded with isolated C++ unit tests; native Windows build, original firmware operation, integrated audio and host/DAW validation remain NOT RUN. Stable upstream import remains on `main`.
