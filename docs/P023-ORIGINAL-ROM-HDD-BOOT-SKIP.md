# P023 — Original S3000XL V2.0 firmware cold-boot HDD wait and safe F8 SKIP

## Discovered in a real owner-ROM run, not synthetic conjecture
In the **owner-supplied original Akai V2.0 ROM**, the genuine patched MAME 0.288 boots the LCD and finds 16 Mwords of RAM, but that **does not mean panel operation is ready**. Independent native-LCD screenshots at 9 s, 15 s, and 23 s emulated show:
`PROGRAMS IN MEMORY` plus `waiting for hard disk ready.. SKIP`.
The user has no HDD image mounted in the default self-contained configuration. Pressing SAMPLE before firmware readiness did not navigate. At 23.5 emulated seconds, pressing and releasing the **original F8** panel key caused the firmware itself to leave the waiting message; its normal `SLCT MIX MIDI LOUD DISK DEL RNUM` soft-key footer appeared. Pressing the **original SAMPLE** key after that brought up the genuine `RECORD SET-UP` screen with `sample name: PULSE`, `mode: MONO`, `source: ANALOG`, `monitor: ON`, `record time: 1.00s`, and the original `SET REC` footer.

This is a firmware-backed **control-path** success. The screenshots were generated privately by the native MAME framebuffer, and neither firmware ROM bytes nor screenshots are placed in GitHub. No host WAV was fed and no MIDI-triggered audio is claimed.

## Corrected logic
- `WaitWav` must not treat early boot's `OPERATING SYSTEM` or `PROGRAMS IN MEMORY` without `SLCT` as interactive. No premature SAMPLE press.
- On the exact original `WAITING FOR HARD DISK` + `SKIP` prompt, send the *hardware* F8 key, hold for a bounded 500 ms, release, and verify the original screen changed before sending SAMPLE.
- Retry at a bounded 2.5 s cadence, max eight attempts; no guessed key presses without the verified prompt; all command and retries run on MAME's owner thread.
- Preserve WAV staging lock during this boot procedure, including key release on user cancellation and safe timeout.
- A user-mounted real hard disk does not require an F8 skip; the controller proceeds when the genuine LCD is ready.
- No fake firmware state, direct guest RAM mutation, bypassed sampler engine or additional background program.

## Acceptance
Added synthetic negative/positive LCD/F8 C++ tests. They must pass on Windows/Linux. Original V2.0 boot + F8 + SAMPLE to RECORD SET-UP was observed in a genuine **native Linux MAME** headless instance. Still blocked for FINAL: actual host WAV→Akai ADC/DMA capture, native name + program/keygroup mapping, audible C3 MIDI PCM, source-file-independent DAW save/reopen and the owner-ROM Windows VST3 host acceptance. ROM-free 125A Test Lab/Steinberg alone cannot certify these.
