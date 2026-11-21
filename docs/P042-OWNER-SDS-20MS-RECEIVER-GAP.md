# P042 — original S3000XL V2.0 non-handshake MIDI sample dump receiver time

## Negative tests against genuine original firmware, not a fake VST mixer

The real owner Akai S3000XL V2.0 BIOS in original patched MAME 0.288 received standard 16-bit MIDI Sample Dump over original emulated UART at device ID zero. The original MIDI file was replayed without changing firmware registers or sample RAM. The original front-panel SINGLE→EDIT→F2 KGRP→F6 SMP1 and two DATAENTRY ticks were used to read **TEST PROGRAM keygroup 1**. Successful sample-name appearance on RECORD SET-UP is not sufficient; original keygroup must actually select the sample and produce a different-than-factory waveform.

- 1,103 original MIDI SDS data packets / 44,100 samples at **42 ticks**: original program genuinely selects MIDI 99 and later plays source-independent original PCM in two independent emulators.
- 1,600 original data packets / **64,000 frames at 42 ticks**: original record progress misleadingly indicates import, but keygroup still selects factory SINE. The actual original-ROM sample was **NOT** successfully playable.
- 2,205 original data packets / **88,200 frames at 42 ticks**: again SINE after original real DATAENTRY. Original sample was not imported successfully.
- **2,205 data packets / 88,200 frames at 55 ticks**: genuine original firmware selects MIDI 99 after original buttons and physical DATAENTRY. Complete original MAMESAVE state saved.
- The source MIDI dump file was hidden before each of **two brand-new owner-ROM emulator processes** reopened the exact same original saved sample+program+DSP state. Both independently played real MIDI C3/E3/C4 and produced **bit-identical full PCM16 48kHz 11-channel WAV payload SHA256 4af664a1d9218e8177605c8d198e3dcd0b886a6e1004f9b7157598a2f3726d6f**: main channel1 101,321 nonzero samples and peak4673; channel2 101,320 nonzero and peak4710; all nine other channels silent. **The imported sound is truly audible, and fully persists without import source.**

## Why the production host must not rush the Akai UART

The official MIDI Sample Dump Standard has two modes, acknowledged handshake (ACK/NAK/WAIT/CANCEL) or non-handshake. The Akai S3000XL original manual explicitly describes a bidirectional handshake. Standard non-handshake sender waits at least 20ms after each 127-byte data packet for a receiver response/validation. Original physical MIDI UART is 31250 bits/s, 10 bits per byte, hence 127*10/31250=**40.64ms wire** plus **>=20ms receiver time**. 60 SMF MIDI ticks at 480PPQN and 120BPM = **62.50ms**, giving **21.86ms receiver gap**. The 42-tick/43.75ms implementation provides only 3.11ms processing gap and has been falsified for longer samples even though it passed 1s.

Sources: https://chriswareham.net/software/sample_dump_standard.html and original manual https://www.manualslib.com/manual/797868/Akai-S3000xl.html?page=241.

**P042 production:** Conservative non-handshake send pacing set to **60 ticks after every SDS packet**; corresponding offline waveform-to-MIDI conversion and transport deadline assertions updated. This is slower than 55 ticks owner empirical success but complies with the documented 20ms processing interval. It does NOT replace an actual ACK/NAK/WAIT transmitter, which is the future optimal transfer path. Never change the original Akai ROM to fake successful sample import.

## Independent original owner-ROM acceptance at exact **60 MIDI ticks** — PASS

The SAME authorized original Akai S3000XL V2.0 firmware was independently exercised at the new production **60 ticks per SDS data packet** with an original owner-ROM 88,200-frame, 2-second waveform sample. The exact genuine original hardware SINGLE/EDIT/F2/F6/DATAENTRY buttons selected MIDI 99 (not SINE) in original TEST PROGRAM keygroup 1, valid velocity zone without missing-sample marker. A complete original MAME state was saved.

The original 88,200-frame import MIDI file was **hidden and unavailable throughout both** subsequent independent original MAME process launches. Both new genuine original firmware runs restored the exact original saved PCM+program+DSP state and played actual C3/E3/C4 MIDI from the original Akai UART with 48kHz 11-channel PCM16 sound. Each process rendered 552,001 audio frames; **101,317 nonzero values in each of original main stereo channels 1 and 2**, peaks 4,676 / 4,713 respectively, and all other nine channels silent. Both runs are **bit-for-bit identical across the complete PCM payload, SHA-256 a8d323188bfc47d53952a136e8e69798864bd560481e7f0cac5f81d26050e55a**.

Thus the **actual production 60-tick spacing is now owner-original-ROM positively verified** for a two-second waveform, not merely inferred from the MIDI standard. Previous 42-tick original-ROM negative controls remain valid. Local evidence: /tmp/akai-p041-88200-t60-zone-182.png, /tmp/akai-p042-88200-replay-launch.log. No binary assets are published.

**Remaining boundary:** Windows **owner-original-ROM VST3** original firmware + plugin host audio/GUI/DAW project reopen is **NOT YET VERIFIED**. ROM-free GitHub Windows/Linux C++ contracts do not count as Windows owner-ROM acceptance. Proprietary owner ROMs, MIDI source, PCM WAVs and native .sta never enter the public repository.
