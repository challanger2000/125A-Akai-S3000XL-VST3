# P045 — original S3000XL V2.0 source-causal one-second WAV recall (private owner ROM)

**Date:** 2026-11-21. **Result:** PASS **for the tested private native Linux MAME fixture only**. **Private Windows VST3 / Studio Pro DAW = NOT RUN; product release remains NO-GO.**

## Genuine original-firmware setup

- The owner's two privately supplied genuine S3000XL V2.0 ROMs were used in pinned original MAME 0.288, locally compiled native ROM-free binary SHA-256 `68e1805947d8adab38f6f92ec21d29c8f6d12a992dfb6e5d9211d81e05841698`.
- Two *different*, actual original MIDI-SDS UART imports, each with **44,100 mono PCM16 samples at 44,100 Hz**: bounded evolving two-tone/multitone source WAV A (SHA-256 `df235102efc93f42d3f4fc8d895e48e3eda6bad71865bf6f631fd99fc21560a3`) and genuine all-zero source WAV B (SHA-256 `3fa20276d8a4131431490ed129d0b05fafa4e9c82d4339b5cf635512103d6615`). Each used 1,103 data packets with 60 SMF ticks per packet (original MIDI image SHA-256 A `2887e8287cca334ff759d00c235a9c0c85c787e64289a681254d403b22296c52`, B `a14d58fbca70f971d429dc8234827609ad5657ad3b65ea0bf9b1d102f7142ab2`). Exact transferred files are **never committed**.
- The original hardware panel was used to map `MIDI 99` into the original TEST PROGRAM keygroup, navigating SINGLE / EDIT / F2 KGRP / F6 SMP1 and taking physical DATA ENTRY from 0 to **2**. Genuine MAME state from each independent import was saved by MAME (compressed owner files SHA-256 A `129f77ce4c224b906a8a7ee21a305b7127cb4f76a0792deac9e5007ca26619f3`, B `81dcf8f70634dd7ad03dcb78d5b0ffd8ee7a34e123d2381a4577e637d054add7`).
- Both original WAV and original SDS SMF sources were physically renamed **off-path** for the whole replay phase, then restored afterwards with a `finally` guard. A separate, newly synthesized MIDI note-only file was issued per run.
- Original native `:DATAENTRY` was set to **2 before loading each native .sta** in a brand-new MAME process, following independently confirmed P044 fault mechanism.

## Source-dependent PCM: 7 independent original-ROM emulator processes

The exact same original MIDI sequence (C3, E3, C4, MIDI velocity 110; NoteOn/NoteOff) was sent to each process. No host-model synthetic audio, ROM substitute, external WAV player, or DSP shortcut was used. Actual MAME sound output: **11-channel PCM16 at 48 kHz, 384,001 frames (8,448,022-byte PCM)**.

| Independent process | Original DATA ENTRY input | Main ch 1 nonzero | Main ch 2 nonzero | Whole original PCM SHA-256 |
| --- | ---: | ---: | ---: | --- |
| A: one-second tone, restart #1 | 2 | 32,190 | 32,195 | `2876816571d5b84faca29dbf478dc6078fbc359fadf7bb28f2fbc43dc2ff88d8` |
| B: one-second all-zero source, restart #1 | 2 | **0** | **0** | `1f237184e9b8f888eb0ba853a02c1813d38a4e54b674398ff412ae33ec3e9bc0` |
| A: restart #2 | 2 | 32,190 | 32,195 | **identical to A #1** |
| B: restart #2 | 2 | **0** | **0** | **identical to B #1** |
| A: no MIDI notes negative control | 2 | **0** | **0** | **identical to B #1** |
| A: deliberately wrong input (negative mutant) | **0** | 33,776 | 33,776 | `b76d23324f028274a3c109f254f0ec8ba1f298ed64a7eb35d3eb5dbd29452553` |
| B: same wrong input (negative mutant) | **0** | 33,776 | 33,776 | **identical to wrong-dial A** |

Main-channel peaks for the correctly restored tonal sample were 1677/1692 PCM16; the zero sample stayed exactly zero in all 11 channels. This is a **content-dependent** original Akai import/replay/recall result: different source bytes have different audio, each source survives independently without the source file, while the deliberate P044 broken dial position reproducibly changes both to the same wrong factory-sound content.

**The test specifically kills the two previous false positives:** (1) nonzero original audio is not necessarily the imported WAV, and (2) digital state file existence is not enough without restoring the externally-owned physical encoder position. When the position is deliberately wrong, the A-vs-B causal oracle detects it.

## Reuse and strict scope

A reusable ROM-free private runner is now added at [`qa/private_original_rom_sds_source_recall.py`](../qa/private_original_rom_sds_source_recall.py), with arguments `--mame`, `--rompath`, `--tone-state`, `--zero-state`, `--dial-position`, `--bad-dial`, repeatable `--source-to-hide`, `--library-dir` (optional original-MAME library shim), and `--outdir`. It checks actual digital PCM, a silent MIDI negative, two fresh restarts per source, and the wrong-encoder fault mutant, and refuses to call this Windows VST3 QA. Original ROMs, snapshots and WAV files remain owner-local only.

**Remaining NO-GO items:** This confirms the original Linux MAME native path for two *one-second mono fixtures*, not the **Windows ROM-embedded JUCE VST3** one-click import, GUI/host MIDI routing, host automation, project serialization/recall, or multinstance DAW workflow. Multiple WAVs in one instance, stereo source fidelity, longer-length content-dependent replay and the original Akai program/keygroup mapping outside this exact fixture are not validated. Previous P042 2-second transport succeeded but without an equally discriminating A-vs-zero content-control. Those gates must be run before release; neither Linux owner-ROM test nor Windows ROM-free 47/47 Validator + 55/55 Plugin Tester can substitute for them.
