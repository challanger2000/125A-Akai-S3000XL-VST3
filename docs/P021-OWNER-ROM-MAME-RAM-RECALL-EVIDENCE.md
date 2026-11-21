# P021 — Owner-original Akai V2.0 ROM: native MAME DSP sample-RAM state experiment

**2026-11-21 — PRIVAT EXECUTED, narrowly verified. This is NOT a VST3/Studio Pro release PASS.**

## Provenance and privacy
- Base: original MAME **0.288** S3000XL machine, compiled with the repository's existing MAME 0.288 hardware patch from public ROM-free Linux Actions [44915905416](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44915905416), source `7ae950a5f639f78a235e29737087c1fb0af01838`.
- Local Linux ELF: 73,431,744 bytes, SHA-256 `68e1805947d8adab38f6f92ec21d29c8f6d12a992dfb6e5d9211d81e05841698`. The Actions artifact contained NO ROM data.
- Original owner-supplied **V2.0 main firmware**: 524,288 bytes, SHA-256 `67917482d2a26163d8aee4de676ecbce4fd42859a3f6223c5442426998a086c5`.
- Original owner-supplied **HD61830 LCD character ROM**: 1,472 bytes, SHA-256 `8738511c6a2f02b0ef1be2cde90898458b4517151cb473ff28ae212dbd1c1737`.
- These original binaries came from the owner's **private** file library and were extracted **only into the isolated local runtime**. They were not uploaded to GitHub, source commits, CI, or this report.
- Host uses SDL dummy video/audio and a temporary, headless-only SDL_ttf symbol shim, solely because the Ubuntu-built binary's SDL2_ttf dependency was absent in the Debian execution container. No GUI/visual/LCD/audio conclusion follows from this setup. Boot used `-bios 0 -video none -sound none -nothrottle`.
- MAME reports `hd61830.bin ROM NEEDS REDUMP` for the upstream BAD_DUMP declaration; it does not prevent execution. `-verifyroms` is NOT fully green because optional older BIOS images are absent and the LCD ROM is BAD_DUMP. The explicitly selected V2.0 ROM executes.

## Positive, actual original-CPU/DSP MAME results
1. **Original-ROM execution:** MAME ran multiple emulated seconds and exited with code 0, without a firmware file missing error. This does not prove the front-panel boot screen has completed.
2. **Real sound-DSP RAM access:** Through the *running original machine* Lua device `:dsp` address space `l6028`, address `0x01fff000` read `0x0000`; after a private test-only `write_u16` of `0x6a35`, the independent read returned `0x6a35`. This directly exercises MAME's mapped 32-MiB DSP/sample RAM, NOT an invented plugin-side RAM buffer. It does not record or create an Akai sample.
3. **Native state snapshot and same-process recovery:** At emulated time 2.0 s write `0x6a35`; at 2.5 s call MAME's own `manager.machine:save(absolute_path)`. Wait for actual saved file. At 4.0 s replace the memory value by `0x3ba9`, confirmed by readback; at 4.5 s load original `.sta` through `manager.machine:load`. Actual post-load memory read was **`0x6a35`**, not `0x3ba9`. The actual `.sta`: 311,457 bytes on disk, SHA-256 `cd9fcc6b6106e512debf299647b48661d4fad42b736e71637f10463b82ac3bd1`; the compressed MAME data expands to 37,212,493 bytes (>32 MiB).
4. **Fresh-process reopen:** Shut down that MAME process, start an independent original-ROM MAME process, read `0x0000` at the same wave-RAM address, load the actual prior `.sta`, then independently read **`0x6a35`**. MAME's native full-state path therefore preserves this original mapped wave-RAM location across fresh process startup. This is a materially stronger result than the legacy 2-KB VST3 API state smoke test.

## Adversarial observation — do not weaken the VST3 envelope gate
A deliberately truncated `.sta` (97 trailing bytes removed), loaded directly through **native MAME without the 125A integrity envelope**, still restored the probed RAM marker from `0x30d1` to `0x6a35`. That shows a partial/truncated MAME file cannot be assumed rejected atomically by this narrow test. It does **not** establish full successful MAME device recovery from corrupted data. The 125A strict length/FNV envelope rejection remains mandatory, and must be exercised on the actual shipped VST3 before touching the running machine.

## Explicitly NOT proved / release blockers
- NO native S3000XL **sample recorded from WAV**, no actual original firmware SAMPLE/REC/ARM/GO automation validated here, and NO MIDI→audible sample PCM measured. Linux MAME was run without audio or input bridge.
- NO direct original Akai PROGRAM/KEYGROUP mapping or one-click MIDI-playable sample established.
- NO exact Windows **ROM-embedded VST3** P020 original-ROM runtime test, no native 125A Test Lab measurement, no Studio Pro session, no GUI click/zoom, no simultaneous instances or ASIO deadlines.
- NO proof that the VST3 P020 snapshot/restore mailbox takes the same load/save path as the above direct MAME Lua runs, nor proof that MAME's void-return `immediate_load` actually succeeded.
- NO source WAV deletion → fresh **VST3** instance → same audible MIDI PCM test. **Project recall remains NOT VERIFIED for the product**, despite native MAME's genuinely positive 32-MiB RAM state proof.

## Next evidence boundary
Build the exact P020 original Windows x64 VST3; privately seal both verified original ROMs into the resulting ROM-free PE via the existing offline overlay packer; use a real Windows VST3 host/DAW to record a WAV with the original firmware, assign a keygroup, trigger C3, save/close/reopen, remove the original WAV path and compare PCM and sample/program memory. Do not call FINAL until that chain, native host QA, and GUI/runtime gates pass.
