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

## P006 owner-only private ROM inclusion (source implementation)
- `scripts/build/embed_private_roms.py` locally verifies the two previously
  uploaded ROMs by exact SHA256 and generates a private, ignored C++ data object.
- CMake flag `AKAI_PRIVATE_ROM_ARCHIVE` embeds both original binaries
  directly into the private VST3. The plugin supplies them to MAME from an
  automatically created per-instance temporary sandbox, deleted on shutdown.
- The public CI job deliberately omits this flag and remains ROM-free.
- **No complete private Windows VST3 build or original V2.0 ROM boot is claimed
  until actual locally executed tests demonstrate both.** See
  `docs/P006-PRIVATE-ROM-EMBED.md`.

## P005 full Windows build - source pipeline
- The author-specific `F:\\MPC3000vsti` Projucer path and missing
  Ensoniq decors/MAME ZIP prevented a portable build.
- Replaced them with an independent CMake/JUCE 8.0.12 + MAME 0.288
  Windows VS2022 pipeline and original MAME layout resources.
- The public runner is **ROM-free by design**. The user's final build must
  embed both verified ROMs locally and privately.
- **NOT PASSED until full Windows job succeeds**: see
  [P005 build contract](docs/P005-WINDOWS-BUILD.md).

## P004 firmware-guarded recording controller (source only)
- Original-ROM strings and operator manual confirm SAMPLE/F2/ARM/GO workflow and a `- new -` unique-name gate. A read-only MAME LCD-text snapshot and fail-closed button state controller are now coded.
- MEDIA-selected or dragged WAVs request firmware navigation; recording is allowed only after exact current LCD setup/new-name/WAITING screens and WADCS ARM state, with 120 ms key holds and explicit stop confirmation.
- **Critical limitation:** live firmware may render REC pages in graphics mode; the new text snapshot must then reject automatic actions. No ROM-backed success has been demonstrated. An automatic unique name, record length, program/keygroup mapping and portable DAW recall are NOT implemented.
- See `docs/P004-FIRMWARE-CAPTURE.md`. Do not call this a finished single-click playable WAV import, or a full VST3 build.

## P003 WAV source through original Akai ADC (implemented, not one-click yet)
- WAV selection (MEDIA menu) and editor drag/drop implemented on `develop/v1.0.0`.
- Supports RIFF/WAVE PCM 8/16/24/32, float32, mono/stereo, 8–192 kHz, bounded source sizes, no external conversion program. Transforms the decoded audio **off-thread** into host-rate PCM16 using a 32-tap windowed-sinc converter; no normalization or boosts.
- Dedicated immutable two-slot feeder and original firmware `WADCS` bit-3 **GO/STOP** trigger in MAME's existing microphone source callback, preserving the original 44.1-kHz ADC/DMA sample path and applying the physical REC GAIN to WAV capture. No direct guest sample-RAM writes.
- **Explicit user interaction remains:** original SAMPLE → REC → ARM → GO and native Akai sample/program allocation. A one-click firmware capture/program/keygroup workflow is still **NOT implemented**, and DAW portable sample recall is still **NOT implemented**.
- See `docs/P003-WAV-ADC-IMPORT.md` and `tests/AkaiWavInputTests.cpp` for implementation boundaries and test cases. No original-ROM/Windows VST3/DAW pass is claimed.

## P002 original-firmware MON bridge (candidate, not accepted)
- The original Akai V2.0 firmware physically controls RECORD page **MON/Moff** and auto-enables on ARM and disables after a completed recording. **Do not add an invented GUI monitor toggle.**
- Original project recordings measured WADCS control port `0x00C0` sequence `A2` idle, `82` ARM, `8A` mono GO, `9A` stereo GO, `A2` STOP. Port bit 3 is verified record strobe, bit 4 is verified stereo; bit 5 is only a candidate for the MON inhibit, not yet verified by a MON↔Moff toggle.
- MAME patch now forwards true firmware latch writes to the VST via `s3000xl_wadcs`. The VST uses a per-instance atomic latch and zero-copy pre-record monitor mixing from the existing post-REC-GAIN input ring into Main L/R, with a 5ms crossfade.
- The existing firmware/MAME live monitor DURING recording must not be doubled by the host. Main Volume must apply to monitored audio; OUT 1–8 remain unaffected.
- **NOT complete:** actual MON/Moff on/off mapping must be checked by an original-ROM session; bit5 interpretation is provisional. The isolated native tests check the routing implementation and known register sequence **only**, not physical-device fidelity.
- Technical evidence, acceptance cases and limitations: `docs/P002-FIRMWARE-MONITOR.md`.

## P001 implementation in development (not yet Windows-built)
- **REC GAIN and MAIN VOLUME source wiring implemented** on `develop/v1.0.0`. The original Akai operator's manual identifies the second knob as **MAIN VOLUME**, not "REC VOLUME".
- Native MAME-panel hotspots now carry real host parameter gestures, and values reach pre-A/D input attenuation and the post-DSP Main L/R output attenuation, respectively. OUT 1..8 are intentionally unaffected by MAIN VOLUME.
- The original `volume` MIDI CC7 parameter, hardware emulation core, MIDI and ROM paths are unchanged.
- **Functional level contract unit test** compiled and executed on Linux/g++ using byte-identical Git blobs: 9,617 checks PASS (endpoints, stereo/polarity, nonfinite handling and 5ms smoothing). **This does not certify Windows VST3 compilation, physical-pot calibration, firmware sampling, GUI interaction or DAW recall.**
- The correct analogue potentiometer taper is not documented; current 0..1 curve is a safe, explicitly approximate attenuation with unity matching upstream.
- The **Mon** softkey / meter monitoring remains open and is not a physical analogue knob.
- See `docs/P001-ANALOG-CONTROLS.md` for provenance and remaining verification.

## Development order (owner priority, 2026-11-20)
1. Implement both physically present knobs, **REC GAIN and MAIN VOLUME**, and their actual input/Main L/R audio signal effects. **P001 source written; isolated C++ checks PASS; Windows/host gates OPEN.**
2. Complete the original firmware **Mon** function and outstanding first-stereo-record monitoring anomaly; **P002 gated source bridge implemented, firmware/DAW validation OPEN**. Keep native firmware display and behavior.
3. Complete **one-step direct WAV import** consistent with Emulator II/III: **P003 loader/ADC feeder + P004 fail-closed firmware-button controller coded**. Actual LCD readability, unique-name/length setup, program/keygroup assignment, original-ROM proof and portable DAW sample recall remain OPEN.
4. Reproduce a native Windows x64 VST3 build with JUCE, MAME 0.288 and VS2022; replace upstream absolute F: drive paths and integrate private ROMs outside the public Git repository.
5. Demonstrate firmware V2.0 boot, MIDI and 10 output buses, genuine recording/playback and WAV import. Validate real 32 MiB RAM, true project/sample recall and concurrent instances.
6. Optimize sample load speed and realtime/host reliability without changing the original Akai sound path without evidence.
7. Treat optional EB16 effects hardware as a distinct later extension. Finish automated QA/validator/125A probes and only then the final Fender Studio Pro acceptance.

## Acceptance policy
- No modified playback DSP without A/B tests against the original firmware/emulation reference.
- No ROMs, commercial sample libraries, or proprietary Akai software in the Git repository.
- Avoid unnecessary front-end features until basic hardware/firmware behavior is validated.
- Do not claim any test passed unless an actual test report demonstrates it.
- Perform the final practical Fender Studio Pro / Studio One acceptance only after automated technical checks.

## Known upstream limitations (reported, **not yet firmware/DAW retested here**)
- Upstream `REC GAIN` and `MAIN VOLUME` were ineffective; our development branch now contains unvalidated signal/control wiring (see P001).
- Pre-record `MON` input monitoring is missing.
- Sample loading is slower than physical hardware.
- EB16 hardware/effects emulation is not established.

## Upstream provenance
- Source: https://github.com/Tuth/S3000XL-VST
- Baseline tag: https://github.com/Tuth/S3000XL-VST/releases/tag/v1.2
- Revision: `d4a5042dffa26fd833eafae5c0cc9d26e69ef99e`
- License: GPLv3, see `LICENSE`

**Current development status:** P001 analogue knobs, P002 candidate MON bridge, P003 direct WAV selection/ADC feeder in `develop/v1.0.0`. One-click firmware sample allocation, full Windows VST3 build, integrated ROM-backed audio and DAW acceptance remain OPEN. MON bit 5 unverified. Upstream import preserved on `main`.
