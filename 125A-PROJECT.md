# 125A Akai S3000XL VST3 — Project notes

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

**Current status:** Source import/inspection only. No new build, ROM boot, audio or DAW test is claimed.
