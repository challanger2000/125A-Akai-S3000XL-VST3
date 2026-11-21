# P016 feasibility: original Akai S3000XL MIDI Sample Dump Standard (SDS)

## Why examine this route?
The [original S3000XL operator manual, pp. 235–236](https://www.manualslib.com/manual/797868/Akai-S3000xl.html?page=242)
explicitly states the S3000XL automatically accepts incoming MIDI sample dumps
**without pressing a receive key**. This may avoid the present analogue
REC TIME/ARM/GO sequencing entirely, preserve the original sampler's
firmware allocation semantics and simplify initial digital sample import.

The [original MIDI SDS wire format](https://chriswareham.net/software/sample_dump_standard.html)
has one 21-byte dump header followed by up to 120 data bytes in each
127-byte SysEx packet. For 16-bit signed WAV audio, each sample is
transmitted as three left-justified 7-bit bytes with offset-binary coding,
plus a per-packet XOR checksum. ACK/NAK/WAIT/CANCEL are defined.

## Implemented R&D (NOT connected to the Akai VST3)
- Pure, bounded `Source/AkaiMidiSds.h`: valid 44.1kHz SDS header,
  sample number and length, 16-bit PCM to SDS offset-binary words,
  packet sequencing and checksum, strict device/packet ACK checks,
  bounded NAK retries and explicit CANCEL/WAIT states.
- `tests/AkaiMidiSdsTests.cpp` uses original MIDI SDS known vectors,
  exact sample extremes, padding, stale/wrong-device response rejection
  and adversarial handshake sequences. No sample or proprietary ROM
  is included.
- Test scope only: no firmware/MAME or UART integration, no request
  wiring, no SysEx byte-rate pacing and no real device ACK observation.

## Critical limitations
- MIDI SDS carries **one sample, not a complete Akai program/keygroup**.
  Native program/keygroup allocation still needs original firmware
  control and persistent project recall.
- The standard MIDI DIN transfer is **slow** (tens of seconds per second
  of 16-bit 44.1kHz audio). A private in-process emulator may be faster
  only after actual MIDI UART/firmware timing tests: never assume that.
- Mono-only first candidate; stereo requires two native sample dumps
  and Akai's stereo keygroup convention; no current claim of stereo support.
- The interface must not bypass MIDI handshake or inject guest RAM;
  do not switch the production import from ADC to SDS before the
  **actual private original-ROM end-to-end proof** of upload, sample
  allocation, MIDI note playback, and DAW project recall.

The existing P015 analogue ADC path remains the default and unaffected
by this exploratory feature branch.
