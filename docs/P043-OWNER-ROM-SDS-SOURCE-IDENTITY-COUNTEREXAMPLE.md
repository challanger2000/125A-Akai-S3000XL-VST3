# P043 — Original-firmware source-identity counterexample (NO-GO)

**Date:** 2026-11-21. **Status:** REPRODUCED NEGATIVE / release gate FAIL. This is an independent private **native Linux MAME** original-Akai-V2.0 run, **not** a current Windows VST3 test. It is not a proof of one particular root cause.

## Why the existing positive test is insufficient

A sample named `MIDI 99` appearing in the Akai's original keygroup LCD and nonzero, bit-identical MIDI playback after two restarts do **not** prove that the output is derived from the WAV just imported. A factory/default waveform can satisfy both those checks. The independent test must change the source PCM while holding the original device, MIDI notes, mapping and transport constant.

## Authentic original-ROM adversarial execution

- Original owner-provided Akai S3000XL V2.0 firmware and HD61830 character ROM, verified by local SHA-256; **no proprietary ROM bytes, snapshots, WAV output or sample data added to GitHub**.
- Same native MAME 0.288 patched S3000XL binary for both cases: SHA-256 `68e1805947d8adab38f6f92ec21d29c8f6d12a992dfb6e5d9211d81e05841698`; ROM-free build artifact from Actions [44915905416](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44915905416), earlier source revision `7ae950a`. This is not the current P042 compiled VST3.
- Two 800-frame, 44,100-Hz, mono, PCM16 WAVs: **A** is an independently generated nonzero evolving sum of sines (WAV SHA-256 `7447d51bbd3e45761fb18ae83b899b05c0be0e8851266106c23051b900d6cb35`); **B** consists of 800 exact zero samples (WAV SHA-256 `5042879d510259161612a77b0871fdd747eb0a6adbdb212fd29d82a76f9d3000`).
- Both were encoded as original SDS device ID 0 / sample number 99, 20 127-byte data packets, individual SMF packet spacing **60 ticks** at 480 PPQN / 120 BPM. Source A MIDI SHA-256 `13ac9adfba4320f69eca83fc50613a14e83dc698ead44821cc06b88f0b24b602`; source B `5f69dc032a7b6b12392dbf5032819eb22b5f926a0ed80bf5261cc17c338a3500`.
- Each independently restored the same owner V2.0 ready-state; mounted its own MIDI file into MAME's genuine `:mdin:midiin:midiinimg`; used only original SINGLE, EDIT, F2 KGRP, F6 SMP1 and DATA ENTRY controls. The original visible LCD displayed **TEST PROGRAM, keygroup 1, `MIDI 99`, velocity 0–127**, not a missing-wave marker, in **both** cases.
- Saved the complete authentic native MAME `.sta`; the original source WAV and SDS MIDI files were renamed/hidden. Each state was then restored in a brand-new MAME process, with identical MIDI NoteOn/NoteOff C3/E3/C4 at velocity 110. This excludes reading the original WAV/MIDI at replay time.

### Measured counterexample

| Condition | Nonzero PCM samples (main ch 1 / ch 2, zero-based) | Original 11-channel PCM16, 48 kHz |
| --- | ---: | --- |
| Nonzero waveform A, LCD `MIDI 99` | 33,820 / 33,820 | 384,961 frames; PCM SHA-256 `a8228d2692b4d1c624cc7c7f296ec5439de0bccac4be152ba7275324e6a6dd39` |
| All-zero waveform B, LCD `MIDI 99` | 33,820 / 33,820 | **same** 384,961 frames; **same PCM SHA-256** |

The complete 8,469,142-byte original PCM payloads were compared byte-for-byte: **0 differing bytes**, while the source WAVs and source MIDI files were demonstrably different. A second independent two-restart A repeat was also bit-identical. A further experiment exited the program edit page via the original SINGLE button *before* saving each state; its A and B MIDI audio was again exactly identical to each other (384,001 frames / SHA-256 `1740ad0b9000607ef2745f04cbe1c2527d842dc9bcd681d93af882c3bbfb1d73`).

The previous **real** MAME positive control (original TEST PROGRAM default sine and native MIDI C3/E3/C4) produced distinct coherent audio, and a no-MIDI control produced exact silence. Thus the failing A-vs-B oracle does not arise from an entirely silent/broken emulator.

## Interpretation and mandatory follow-up

**Original firmware display and nonzero audio are real, but the claimed causal WAV→played PCM link is NOT established.** At least this concrete 800-frame original-ROM scenario continues to sound exactly the same with a completely silent WAV, **even after an independent state save/restore and an attempted program-edit exit**. It is therefore a product-facing FAIL for this fixture. Do **not** call the SDS WAV importer playable, the saved sample state sound-identical to its source, or the VST3 complete.

This test does **not** prove whether the actual fault lies in original SDS reception/allocation, loaded sample RAM, program/keygroup selection, MIDI channel/program routing, save-state restoration, or the MAME DSP/audio path. The 800-frame test cannot establish behavior for 1- or 2-second sources: the earlier genuine P042 2-second positive evidence remains a **narrow firmware/LCD/nonzero-PCM result**, without the necessary A-vs-zero source-identity control. A separate 1-second local source import did not attain `MIDI 99` mapping and cannot be called PASS.

**Required next red/green test on actual owner media and Windows VST3:**

1. Independently prepare visibly different valid full-length sources A and B, including **all-zero versus a bounded nonperiodic signal**. Do not reuse a short sample as the only positive test.
2. Confirm the physical Akai LCD assignment and source/program identity; record original device/sample-RAM provenance without modifying the original firmware or bypassing Akai sample allocation.
3. Issue the same original UART notes/program/bank and render original PCM at matched starting device/FX time. Assert a measurable, source-derived audio difference and a valid independent silence/factory negative.
4. Save both complete states, destroy/rename each source, reopen **fresh** instances, and require source-specific (not merely nonzero) PCM after recall. Repeat across more than one WAV length, independent instances and the actual packaged Windows VST3.
5. Preserve deliberate test-only mutants that substitute a factory waveform or suppress the source transfer: the audio/content oracle must reject them. Do not relax the requirement to “nonzero and deterministic”.

Until that evidence passes, this native original-ROM counterexample is **NO-GO**, regardless of ROM-free Windows compiler/Steinberg/125A-Tester results. Never place the owner's ROMs or native snapshots in public CI.


## Follow-up controls: SDS image detached before save; MIDI Program Changes 0–5

To challenge the possibility that the earlier saved `.sta` only sounded like the factory sample because it retained an actively mounted SDS MIDI image, the two original transfers were repeated with the actual MAME MIDI input image **explicitly unloaded after the 60-tick packet transport completed and before original Akai panel assignment and MAME save**. Both genuine LCD snapshots still showed `TEST PROGRAM` / keygroup 1 / `MIDI 99` / velocity 0–127 without a missing-sample marker. The source WAVs and .mid import files remained unavailable on fresh replay.

- Fresh A-original-ROM state with MIDI input image unloaded: 384,961 frames, 48 kHz, 11-channel PCM16; main left/right indices 1/2 each had **33,819** nonzero samples, peak **4,699**; raw PCM SHA-256 `44d639a39a8c664858ffe079f4eabd653d30766d95f80bbe452e3ec0a547f074`.
- Fresh all-zero B-original-ROM state with MIDI input image unloaded: **exactly the same** frames, amplitudes, counts and entire raw PCM SHA-256. Full PCM byte comparison: **zero differences**.
- A further genuinely scheduled original UART MIDI sequence sent **Program Change 0, 1, 2, 3, 4, 5**, each followed by a NoteOn/NoteOff (C3, velocity 110). With the detached-SDS states, A/B emitted **byte-identical 384,961-frame original PCM**, SHA-256 `21ad0ba3d10791bbd87cb7f31d7e766eb091de883fdac5bc1acde7d4d0463cbf`, with main L/R each 21,727 nonzero samples (peak 4,507). This does not exclude that another MIDI program/bank or front-panel mode is needed, but rules out an ordinary ProgramChange 0–5 as sufficient in this fixture.

**Conclusion strengthened:** The A/B source-content counterexample survives deliberate MIDI-image detachment before native state save, separate fresh-device restores, an original-panel EDIT→SINGLE transition and a six-program-change probe. **Actual import-to-note causality is still unproven.** Do not ascribe a specific faulty subsystem without a traced DSP/Sample-RAM/program route; do not delete or weaken the counterexample merely because the LCD shows `MIDI 99`.

## Exact Windows baseline verified independently, but not a source-content oracle

Full **ROM-free** Windows x64 MAME 0.288 + JUCE 8.0.12 VST3 build at exact production source commit `894338ba38a363b6903278b51bac52fd9fb45c87` passed [Windows Actions #44941367658](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44941367658). Installed QA reports show official Steinberg **47 tests passed / 0 failed** and pinned 125A Tester v0.3.2 **55 PASS / 0 WARNING / 0 FAIL**, including its ROM-free host lifecycle and state probes. ROM-free tests had **zero genuine original-ROM sound**; therefore neither test covers an SDS-imported WAV sound nor a fresh Windows owner-VST3 DAW save/reload. Proprietary original-ROM tests above ran only as separate private Linux MAME experiments from an earlier pinned ROM-free emulator binary, **not** the final Windows VST3.


## Follow-up source-byte localization: imported PCM survives in original native state

Further independent byte inspection of the **actual MAMESAVE v2 image** saved *after the SDS image was unloaded* supplies a useful localization clue. Remove MAME's 32-byte save header, decompress the native zlib payload (37,212,493 bytes) from each full owner-ROM state, and inspect the same offset in both images:

- **Native original-state uncompressed offset 298,948:** all **1,600 consecutive bytes** in the nonzero-A state are **byte-identical** to the 800-frame original WAV's little-endian PCM16 data. A raw byte search finds that exact 20-byte leading PCM signature at this offset.
- At **the same state offset**, the all-zero-B state has **1,600 bytes of zero**, exactly matching the independent silent WAV's PCM16 data.
- Across the full two uncompressed states, **1,810 bytes** differ: 1,550 in MiB index 0 and 260 in MiB index 32. **1,548** of the differences lie within the 1,600-byte PCM window; the other two bytes in that window are already equal in both inputs.

This is strong evidence that the original firmware/emulator **retains exact different imported source PCM in its native save state** even after MIDI-image unmount, while identical post-reload MIDI notes still play identical audio. It narrows the suspicion toward **program/keygroup → active voice/DSP sample selection or playback-state routing**, but it does **not** prove the native save-state window is specifically the DSP's live sample RAM without establishing MAME's owner/type for that serialized region. It also does not establish that the original sample was selected by the sounding MIDI part; never patch guest RAM as a workaround before tracing the actual original firmware selection.

**Audit status remains original-ROM WAV source-to-audio FAIL / NO-GO**. A byte-identical copy in a native state snapshot, by itself, is not evidence of sounding audio. No private state or PCM bytes have been uploaded to GitHub.
