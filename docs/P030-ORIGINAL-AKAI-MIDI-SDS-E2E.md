# P030 original S3000XL MIDI sample transfer — private original-ROM E2E

The S3000XL original owner's manual (pages 235-236) confirms the genuine firmware receives Standard MIDI Sample Dump automatically; no external sample RAM writing or analogue recording shortcut is necessary. Source: https://manualzz.com/doc/1044514/akai-s3000xl-user-manual, pages 235-236.

Private owner V2.0 original-ROM MAME 0.288 acceptance:
- A synthesized 800-frame 16-bit 44.1kHz NONZERO waveform was transmitted as authentic 21-message Standard MIDI SDS over the original Akai UART, 2561 bytes total, using sample number 99. SysEx logical device ID **0** was required. The otherwise identical ID 127 broadcast negative control did NOT import the sample.
- The original firmware LCD displayed newly received sample **MIDI 99**; source .syx was 100% converted and received via MIDI input image on the original Akai UART.
- Independent original-MAME process loaded full state, pressed genuine physical Akai SINGLE, EDIT, F2 KGRP and F6 SMP1 and changed genuine DATAENTRY encoder value 001 to 002. Original LCD showed zone 1 with **MIDI 99**, valid sample and full 0–127 velocity, no '?' missing-sample marker.
- Original firmware complete native MAME save was created with MIDI 99 assigned. ORIGINAL .syx SOURCE FILE WAS HIDDEN/UNAVAILABLE for two additional independent owner-ROM MAME restarts. Each restart pressed original SINGLE, sent genuine MIDI note-on/off C3, E3, C4 via the original UART and captured all 11 sound output channels.
- Both independent native PCM16 48kHz replays: 456,961 audio frames; authentic Akai main stereo indices 1 and 2 EACH contain 101,295 nonzero PCM samples. Original stereo peaks 4,427 and 4,463. Full entire 11-channel WAV payload SHA256 **82ff2c514121253a81a204665f5462fe3e08560a7dc392bd97328523485add41** both processes (zero differences).
- Original analog input trial, by contrast, showed sample-name/100% firmware progress but returned silent samples. The standardized MIDI SDS route is empirically verified; a received program name alone would not suffice for acceptance.

P030 adds pure bounded, original 16-bit PCM16 to 7-bit SDS MIDI packets (21-byte header, 127-byte data packet, XOR checksums, 100ms pacing estimate). Actual original firmware device ID 0, sample 99 header validated against captured successful original-ROM run. It DOES NOT yet implement Windows VST3 owner-thread MIDI image import, assign zone nor DAW-project save and reopen. Header-only code cannot be misrepresented as completed plugin.

One original SDS dump is MONO. P030 stereo conversion helper explicitly names downmix and **does not claim stereo preservation**. Real stereo import requires two SDS mono samples and proper original Akai program zone routing, subject to original firmware verification. Never publish owner's ROMs, state snapshots or original PCM files.
