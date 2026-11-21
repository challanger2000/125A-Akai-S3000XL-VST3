# P026 — Original Akai SAMPLE → SINGLE program/keygroup → MIDI-assignable waveform

## Owner-ROM controlled experiment (not just a mock)
Using the owner's SHA-verified Akai S3000XL original V2.0 ROMs and the repository's original MAME 0.288 patched sampler machine, a private headless real-firmware run:
1. Boots the original firmware and loads the actual original working panel.
2. Presses original SAMPLE, F2 (REC), NAME twice, enters the original 12-digit sample identifier `125000000001`, presses ENTER, F8 (ARM), and F7 (GO). The genuine RECORD LCD changes from `- new -` to `exists!` after the one-second recording.
3. Presses SINGLE, EDIT, F2 (Keygroups), and F6 (Sample Page). The original V2.0 LCD displays the first keygroup of `TEST PROGRAM` with initial default sample `SINE`.
4. Presses NAME twice in the **keygroup's zone 1 sample field**, types `125000000001`, ENTER. The original LCD displays `1 125000000001   0 127`, without a missing-sample marker.
5. **Negative control:** Starting with the original firmware restored to the same working state **without having recorded that sample**, the exact same zone-1 name entry instead displays `1 125000000001 ? 0 127`. An apparently correct sample name alone therefore does NOT establish playable assignment; the original firmware's per-zone `?` indicator is mandatory negative evidence.

All above steps are genuine original hardware-panel input and original firmware output, with no guest memory patching. The saved screenshots and proprietary ROMs are held **only in the private runtime**. The original MAME standalone audio input was silence; NO audible WAV or DAW recall is claimed.

## Implementation
The capture controller now continues from firmware-verified RECORD/GO/STOP into real SINGLE→EDIT→KGRP→SMPL original key operations. It only operates on the default TEST PROGRAM with one keygroup (fail-closed for other program layouts), invokes NAME/number entry on the original sample-zone field, enters precisely the same generated 12-digit sample name that was recorded, and accepts `RecordedSample` only when the real 8×40 LCD verifies zone 1 contains the exact name **without the original missing-sample question mark**. If wrong screen, missing sample or a competing user-held key: bounded abort `ProgramUnassigned`, with a visible error explaining that MIDI playability has not been demonstrated. No independent third-party sampler, direct sample-memory write or invented program format.

## Remaining acceptance
Positive native original-firmware sample-to-program mapping is a **real UI-state success**, not audible host evidence. The isolated C++ test exercises both the original LCD positive and missing-sample negative cases, press/release order, and exact sample-name identity. Do not call FINAL without owner-ROM **Windows VST3** feeding a nonzero WAV through the original A/D/DMA path, audio sample MIDI playback with a matching PCM oracle, safe DAW save/close/reopen after deleting the original WAV, and multi-instance/host QA. The present one-keygroup TEST PROGRAM mapping is deliberately conservative; future multi-import handling must retain already assigned samples and avoid overwriting the user's existing programs.
