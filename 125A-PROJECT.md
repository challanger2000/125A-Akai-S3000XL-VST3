# 125A Akai S3000XL VST3 — Project notes

**Binding scope revision — 2026-11-21, P046:** The owner expressly
**cancelled external WAV import**. Do not recreate a WAV file picker, WAV
drag/drop, auto-capture from a WAV file, WAV-to-SDS converter/transfer or
indirect WAV import, and do not treat the earlier P003–P045 experiments as
a reason to reinstate them. These experiments are historical evidence only.
Keep original Akai firmware/panel, native host audio input for Akai ADC,
host MIDI, floppy/CD/SCSI media-image mounting, native project state/recall
and private self-contained original-ROM Windows VST3 as acceptance targets.
[Source changes and regression contract](docs/P046-EXTERNAL-WAV-IMPORT-REMOVED.md).


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

## Cancelled external WAV import (P046)

The user cancelled the one-step external WAV import after its failure to reach
reliable full Windows VST3/DAW acceptance. It is **out of scope**, not an open
P003/P004/P030/P045 task. Direct WAV file input is intentionally absent from
the editor, public processor API and drag-and-drop targets.

The original Akai must still accept its native disk/media images and the
normal host audio/MIDI signals for authentic original-firmware recording
and playback. Do not introduce new external converters or manual disk-image
workarounds as replacements for the cancelled feature. Historical P003/P004
and the owner-ROM SDS investigations remain archived for provenance, not
product release checkboxes. A project save/reopen must still retain valid
native Akai machine state and reject malformed or incomplete legacy states.

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

## P003/P004 and P030–P045: retired external WAV-import research

The previous WAV input decoder, file-to-ADC staging and original firmware
MIDI Sample Dump controller were investigated, including original-ROM
sample-content and recall counterexamples. **P046 removes both external-file
entry points, WAV media menu options, the editor drag target and the
user-triggered recording automation.** Any vestigial internal state must be
inert and remain only to preserve safe legacy project behavior. These
historical tests do **not** certify the current no-WAV Akai VST3 for release.
See [P046](docs/P046-EXTERNAL-WAV-IMPORT-REMOVED.md) and historical
[P003](docs/P003-WAV-ADC-IMPORT.md), [P004](docs/P004-FIRMWARE-CAPTURE.md).

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

## Development and acceptance order (revised 2026-11-21, owner P046)

1. Preserve original hardware/firmware correctness, original panel, original A/D
   and Main L/R behavior, genuine DAW MIDI, monitor and output routing.
2. Maintain true Akai floppy/CD/SCSI image mounting and native Akai program
   loading; no arbitrary standalone WAV import control is offered.
3. Compile native Windows x64 VST3 with pinned MAME/JUCE/VS2022 and privately
   embed both owner-verified original ROMs without public distribution.
4. Prove in an actual private original-ROM Windows VST3 that boot, GUI, MIDI,
   native sample/program handling, ten outputs, project save/reopen, automation,
   and independent DAW instances are correct. Original-ROM Linux MAME tests
   and ROM-free Windows host validation are necessary but insufficient.
5. Finish automated validator/125A QA, then perform final Studio Pro acceptance.
6. Optional EB16 effects and performance work require separate justification.
   **External WAV file import remains cancelled and shall not block release.**

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

**Current development status (P046):** External WAV import intentionally removed from the active Akai development branch. Windows ROM-free build and isolated tests have passed in earlier candidates; the changed P046 revision requires its own exact-SHA CI acceptance. The native owner-ROM Windows VST3 and final Studio Pro host boot/MIDI/project-recall tests remain unaccepted. Original upstream `main` is unchanged, and historical WAV research is archived. MON bit 5 remains unverified.
