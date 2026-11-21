# P038 — original Akai V2.0 SDS timing, one-second audio and real cross-process recall

**Original owner S3000XL V2.0 firmware; patched MAME 0.288 native Linux, 21 Nov 2026.** This is not synthetic RAM injection or a Windows VST3 product acceptance test. All ROM and state assets stay private.

## Adversarial original-ROM SDS length series

The original actual MIDI UART receives standard 16-bit mono MIDI Sample Dump from a known nonzero oscillatory waveform at 44.1 kHz, device ID 0, sample number 99. The original physical SINGLE/EDIT/F2 KGRP/F6 SMP1/DATAENTRY front-panel operations select TEST PROGRAM keygroup 1. Only a valid zone1 MIDI 99 without a missing-sample marker counts as successful firmware allocation; the earlier RECORD SET-UP name MIDI 99 is NOT sufficient.

| Source frames | Data packets | Burst 64 packets/event, program zone1 |
| ---: | ---: | --- |
| 801 | 21 | MIDI 99 PASS |
| 8,000 | 200 | MIDI 99 PASS |
| 20,000 | 500 | MIDI 99 PASS |
| 40,000 | 1,000 | MIDI 99 PASS |
| 40,960 | 1,024 | MIDI 99 PASS |
| 40,961 | 1,025 | MIDI 99 PASS |
| 41,240 | 1,031 | MIDI 99 PASS |
| 41,241 | 1,032 | MIDI 99 PASS |
| **44,100 (1.00 s)** | **1,103** | **FAILED: SINE**, despite firmware RECORD SET-UP reporting MIDI 99 |

These counterfactuals refute false hard limits at 1,024 packets or 131,072 total MIDI bytes.

## Root-cause correction: true paced individual SDS MIDI events

The ORIGINAL 44,100-frame sample DID succeed after changing *only* the Standard MIDI File timing to **42 MIDI ticks per 127-byte SDS packet**, at 480 PPQN/120 BPM: 43.75ms between packet starts versus 40.64ms wire transmission time at 31250 bit/s. This allows approximately 3.11ms firmware processing between packets. The old 64-packet burst pacing falsely completed the transfer without a usable firmware sample.

Using the corrected exact source:
1. Genuine original ROM receives all 1,103 SDS packets and displays MIDI 99; real original front-panel controls put **MIDI 99 in TEST PROGRAM keygroup 1** across velocity 0–127, with no missing-sample marker.
2. Save actual MAMESAVE original state after physical mapping. Hide the original SDS source MIDI file before ANY subsequent process.
3. Independently restart the authorized original ROM in two separate native MAME processes, each reopening the same complete original sampler memory/program/DSP state and receiving C3/E3/C4 MIDI notes from the actual MIDI UART.
4. Both replays: **552,001 audio frames**, 11-channel original MAME PCM16 at 48 kHz. Channel 1 (zero-based) **101,376 nonzero samples, peak 4,673**. Channel 2 (zero-based) **101,375 nonzero samples, peak 4,710**. The remaining channels are silent.
5. **Full 11-channel PCM SHA-256 identically from BOTH independent owner-ROM processes: 57d4d01371198b8c0abe321a0e210227d853d14c48c40682d0343b7a91380c45.** The original import file was unavailable throughout both reloads.

Production correction in Source/AkaiMidiSampleDump.h replaces 64-packet bursts with native owner-original-ROM-verified 42 ticks between EVERY SDS packet. Tests/AkaiMidiSampleDumpTests.cpp and Tests/AkaiWavToSdsE2ETests.cpp independently check per-packet timing, checksum, MIDI event framing, genuine one-second RIFF WAV decode and bit-identical 44,100 PCM sample reconstruction. No original firmware or guest memory is modified to fake sample playback.

**Boundary:** Windows owner-ROM VST3 original firmware boot, WAV drag/drop, audio playback and DAW save/reopen without WAV remain unverified; ROM-free Windows host QA is not a substitute. Source ROMs, temporary SDS assets and native snapshot/PCM stay on the authorized local testing machine, never in GitHub.
