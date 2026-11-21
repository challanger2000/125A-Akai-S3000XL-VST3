# P024 — Actual owner V2.0 Akai firmware SAMPLE → NAME → ARM → GO

**Evidence class:** executed private original-MAME 0.288 with user-owned original V2.0 ROM; no artificial Akai sample RAM writes, no fake menu fields. The sample input was SILENT because the standalone headless Linux MAME test had no DAW PCM feeder. **Not Windows VST3 / MIDI playable / portable project approval.**

## Original-firmware timeline and observed LCD
The original patched MAME executable comes from ROM-free [Linux Actions #44915905416](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44915905416). Both original user ROMs were privately SHA-256 verified as documented in P021. Native Lua invokes only the original MAME input-field methods for physical keys and takes screenshots of MAME's real framebuffer. All ROMs, original snapshot bytes and screenshots remain private.

1. Original V2.0 boots with `PROGRAMS IN MEMORY` and `16 Mwords fitted`. After the SCSI initialization stall, the LCD explicitly shows `waiting for hard disk ready.. SKIP`. Until the correct F8 boot skip is received, pressing SAMPLE has NO effect. A genuine **F8** press at emulated 23.5 s changes the firmware to normal `SLCT MIX MIDI LOUD DISK DEL RNUM` footer. This observation motivated P023.
2. Native **SAMPLE** button opens firmware `RECORD SET-UP`, with `sample name: PULSE`, `mode: MONO`, `source: ANALOG`, `monitor: ON`, `bandwidth: 20kHz`, `orig.pitch: C3`, `record tim: 1.00s`. Original F2 enters `RECORD MONO exists! PULSE`.
3. Native **NAME** first opens the real LCD naming screen `LETTERS ..(NAME for numbers ENT to exit)`. A second original NAME switches to `NUMBERS ..(NAME for letters ENT to exit)`.
4. Twelve original hardware numeric key presses `125000000001` followed by **ENTER** yield original LCD `RECORD MONO  - new -  125000000001`. This is a genuine visible firmware confirmation, matching the P018 fail-closed `hasUniqueRecordName` predicate.
5. Native **F8/ARM** yields genuine `WAITING FOR START ... GO EXIT`. Native **F7/GO** completes the programmed 1.00-second recording and the LCD changes to `RECORD MONO exists! 125000000001`. The name is no longer new; the original firmware has registered a sample with that name.

## What this genuinely establishes
Original MAME + original firmware supports the panel navigation, original sample name creation, arming and firmware GO/STOP progression after HDD boot skip. No guest RAM shortcuts or direct synthetic sample header write were involved. The real LCD is the output oracle, not merely CTest assumptions.

## Remaining release blockers — DO NOT CONFLATE WITH THIS PASS
- The standalone MAME audio-input callback did **not** receive a WAV. No nonzero PCM was captured, no sample audio was compared to a source WAV and no MIDI-triggered actual VST3 audio was heard.
- Original firmware **RECORD TIME defaults to just 1.00 s**. WAV longer than this can be truncated. The current controller deliberately rejects such incomplete input. Reliable automatic original-panel record-duration configuration remains outstanding.
- The sample can exist natively without being assigned to a real **program/keygroup**, so immediate MIDI playability has not been proved and must not be assumed.
- The private ROM-inclusive Windows VST3, actual independent multi-instance Windows host, project save/close/reopen without original WAV, and matching audio output remain unverified.
- The P020 ROM-free Windows build [44918146869](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44918146869) passed 125A Tester **55 PASS / 0 WARNING / 0 FAIL**, Steinberg **47 passed / 0 failed**; those results belong to P020 source `98c6d19abf0cbb0153de6ce33e75bd19be7d9c2d`, not this newer code.
- Isolated P023 Windows/Linux tests are separate, and never equivalent to this original-ROM evidence or a final product.
