# P015 — Akai native WAV completion and replacement safety

## Confirmed P013 source defects
P013's MAME input callback set `wavState=3` after all WAV frames were
fed, but the original firmware's STOP edge unconditionally reset state to
`wavState=1`. An LCD/controller observation after STOP thus falsely
reported `wavComplete=false`, potentially classifying a complete
recording as truncated.

Additionally the WAV file staging function only rejected a pending
slot change, not an **active recording** after its pending slot was
acknowledged. A second WAV import could replace the input while Akai
was still recording.

## P015 correction and bounded scope
- Preserve a finished source signal across the native WADCS STOP. If
  firmware stops too early, source reverts to armed/retryable, NOT finished.
- Reject WAV replacement while firmware WADCS recording, capture
  controller running, activation pending, or unload pending. Check
  both before decoding and after decoding under the staging mutex.
- If a source disappears during native RECORDING or while waiting for
  STOP, fail closed rather than mark successful sample capture.
- New native standalone C++ regression for STOP reordering and
  replacement lockout, plus existing controller regression for losing
  the source during native recording. No original Akai sample RAM writes.

## Strict QA scope
P013 Windows builder + Tester + Steinberg and P014 independent Test Lab
are already PASS for the older exact module; they do NOT cover this new
P015 binary. CI must validate new Windows/Linux contracts, full Windows
MAME/JUCE VST3, 125A tester, Steinberg validator, then a separately
SHA-pinned independent Test Lab run. Original-firmware boot, audible
MIDI WAV capture and DAW sample project recall remain NOT RUN.
