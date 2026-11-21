# P018 — single-WAV capture requires original-panel unique naming
The original Akai S3000XL V2.0 operator manual, p.120–123, requires a
unique sample name marked `* NEW *` on the REC page. A cold S3000XL
contains four existing factory waveform samples; P017's controller
therefore waits eight seconds without naming and rejects most first
imports.

This P018 branch adds a **bounded, original-hardware-keyboard-only**
fallback. On a readable REC page that does not show `* NEW *`,
the MAME-owner controller presses the physical NAME key, toggles its
numeric input mode, types exactly 12 digits (`125` plus a per-session
nine-digit sequence), and presses ENTER. It must then see the **real
original firmware** report `* NEW *` before any ARM or GO. Duplicate
names are retried (max 16). Overwrite warnings still abort; source
loss and user-held keys remain fail-closed. Pending WAV replacement
is blocked during every newly introduced name-entry stage.

This does **not** assert name-entry works on original ROM until the
private owner-original V2.0 LCD and MIDI/PCM acceptance test proves it.
It does not change MAME's audio engine or bypass its sample allocation.
The P017 firmware REC TIME bound for long WAVs remains a separate
release blocker. Do not merge or release on CTest alone.

## Original ROM v2.0 naming-mode interlocks
For safety, P018 cannot assume that a readable LCD means the naming
editor opened. The private owner-supplied original V2.0 ROM contains
the **exact display mode strings**
`LETTERS ..(NAME for numbers ENT to exit)` and
`NUMBERS ..(NAME for letters ENT to exit)` (visible text only; NO
firmware bytes are present in the public repo).

Pressing NAME to enter numerical mode now requires the original
LETTERS header; typing digits and confirming ENT require the original
NUMBERS header. Any unrelated screen, lost LCD or failure to switch mode
remains fail-closed. New adversarial C++ tests explicitly present
an unrelated RECORD screen during naming and verify that NO numeric
key is sent; the positive exact-header sequence must still pass.
