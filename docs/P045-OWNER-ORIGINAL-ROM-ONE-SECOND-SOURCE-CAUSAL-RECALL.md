# P045 — original S3000XL V2.0 source-causal one-second WAV recall (private owner ROM)

**Date:** 2026-11-21. **Result:** PASS **for tested one- and two-second private native Linux MAME fixtures only**. **Private Windows VST3 / Studio Pro DAW = NOT RUN; product release remains NO-GO.**

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

**Remaining NO-GO items:** This confirms the original Linux MAME native path for two *one-second mono fixtures*, not the **Windows ROM-embedded JUCE VST3** one-click import, GUI/host MIDI routing, host automation, project serialization/recall, or multinstance DAW workflow. Multiple WAVs in one instance, stereo source fidelity, source lengths other than the tested one and two seconds, and program/keygroup mapping outside these exact fixtures are not validated. Previous P042 2-second transport succeeded but without an equally discriminating A-vs-zero content-control. Those gates must be run before release; neither Linux owner-ROM test nor Windows ROM-free 47/47 Validator + 55/55 Plugin Tester can substitute for them.

## Extended P045 adversarial owner original-ROM test: **TWO-SECOND WAVs**

**Performed after the one-second PASS.** Two newly generated 44,100-Hz mono PCM16 sources were imported separately via authentic original MAME MIDI UART/SDS, **88,200 frames each** (2.000 seconds, 2,205 packets at 60 ticks per packet). The A waveform was an evolving bounded multi-sine, WAV SHA-256 `43538870fcbced5026e3d0807a900aebc2d2c478829c16c40a914eac41cb370a`, original SDS SMF `5f16e61a0945476f6ab787a33048df404c85cd4f35cd1798d1a0700263972e64`. B was **exactly zero**, WAV SHA-256 `cfc6b206e99b298a229480f020e61d2b22a791e08a840fdd40ef62d8bd78b155`, SDS `78e0d98fb193397a1825392d5a5e5f17d4ec9912312b5daeca5ef34f33671a78`. The genuine original machine completed both full SDS feeds.

**Important adversarial finding: cannot assume a fixed number of sample-selection clicks.** A first attempt, copied from the *one-second* fixture, advanced the real original DATA ENTRY encoder from 0 to **2**, saved the genuine MAMESAVE, removed the import sources, reloaded each owner-original image with matching external wheel 2, and rendered original MIDI notes. Both A/B outputs were identical nonzero (384,961 frames, PCM SHA-256 `1a639553ff440d6c01292f4ef5a9ee6d6390c97e813d0b1f4c892e51524ce90c`). The contemporaneous **original HD61830 firmware LCD showed `SINE` instead of `MIDI 99`**. This proved a **mapping test setup error**, not WAV-source playback or SDS reception failure. Preserved these red results; do not mark them PASS.

A second native original-firmware test **restored each unchanged A/B MAMESAVE at wheel 2**, navigated the original keygroup `SMP1` sample field **one real clockwise encoder tick at a time**, and used actual original-LCD snapshots as the only mapping evidence. The LCD stayed `SINE` at wheel 3, then displayed valid **`MIDI 99` in keygroup 1, velocity 0–127 at wheel 4** for **both** independent original imports. Both original programs were saved at the actual physical wheel position **4**: owner-native `.sta` SHA-256 A `d6f26d0eae434104e51abedab65266bc7397cc451a15615e1f86d389fce22c3a`, B `5a37c84e02fd11cb930e8ca9a222b6cbe4261d77eb0005e5a4c22a0d449927ba`. Source WAVs/SDS files were renamed away before every replay.

**7 independent owner-original MAME restarts, corrected physical DATA ENTRY 4**, actual 11-channel PCM16 48 kHz, **384,961 frames** each, same MIDI C3/E3/C4/velocity110:

| Original owner-ROM run | External wheel | Original main L/R nonzero PCM samples | PCM SHA-256 |
| --- | ---: | --- | --- |
| Two-second tone A #1 | **4** | 32,889 / 32,892; peak 1,816 / 1,831 | `7f0dbcaafaccd02174328b8cbc0c3038a81a4600abdd7d00fb8859b8b97ac4e9` |
| All-zero two-second B #1 | **4** | **0 / 0** | `0e59a4bf26d7a187b56e9d01e3b58c9f7a66bbefd59e7098ec8760f8e0e09e40` |
| A restart #2 | **4** | **bit-identical to A #1** | same as A #1 |
| B restart #2 | **4** | **exact zero, bit-identical to B #1** | same as B #1 |
| No-note negative, A state | **4** | **0 / 0** | same as B #1 |
| Deliberately wrong wheel, A | **2** | 33,741 / 33,741; wrong factory sound | `93c7c3f8d9ec18f1cd8fe23ca8d339607083635b69149af1a9ac7765e7d630fd` |
| Same deliberately wrong wheel, B | **2** | 33,741 / 33,741; **same wrong factory sound** | same as wrong-wheel A |

**PASS for this original MAME two-second source-causal original-firmware import + firmware LCD mapping + native machine state recall.** The intentionally wrong fixed-two-tick mapping was caught. The product's existing `Source/AkaiSdsAssignment.h` controller **already follows live original LCD feedback and allows up to four clockwise ticks**, which is *consistent* with the 2s manual navigation. This test does not claim that the actual Windows VST3 automation exercised the controller, nor that four is adequate for every number of previously loaded samples or other files.

**Release status remains NO-GO** until actual Windows owner-ROM VST3 one-click import, GUI-to-MAME SDS transfer, controller verification, DAW save/reopen with source removal, cross-instance recall, multi-WAV import and supported stereo behavior have been validated. Do not convert this Linux emulator test to a Windows/DAW PASS or invent audio for unopened instruments.
