# P004 — Firmware-guarded WAV recording automation (development)

## Owner goal
Match the 125A Emulator-II/Emulator-III end-user intent: import WAV inside the
Windows VST3, let the original sampler firmware create the sample, and make it
playable and portable with the DAW project — without manually managing floppy
images or a separate executable.

## Evidence before implementation
- Original Akai S3000XL V2.0 user-supplied firmware archive was checked privately;
  the read-only ROM string table includes `RECORD SET-UP`, `- new -`,
  `WAITING FOR START ......`, `RECORDING analog`, and
  `overwrite existing sample!?`. **No proprietary ROM contents were committed.**
- The original operator's manual (§ RECORD MODE, pp. 117–123) specifies:
  SAMPLE enters RECORD SET-UP; F2 opens REC; NAME chooses a unique sample name;
  ARM prepares input, GO starts recording, and the firmware ends recording
  using the selected record length. Mono/stereo recording creates native samples.
- Upstream `CLAUDE.md` measured WADCS 0xA2 idle -> 0x82 ARM ->
  0x8A mono / 0x9A stereo GO -> 0xA2 STOP. Bit 3 records GO/STOP.
- Our P003 already streams WAV through the native microphone/A/D/DMA path
  on original firmware GO.

## Implementation
- `Source/AkaiCaptureAutomation.h` is a deterministic, fail-closed controller,
  isolated from JUCE and MAME and unit-tested on Windows/Linux. It never sends
  a key based on an unverified timer alone.
- We added only a **read-only LCD text snapshot** to MAME 0.288's HD61830
  emulation. It reads the **currently displayed** 40×8 text region and refuses
  graphic-mode or incompatible geometry; no hardware state is changed.
- The MAME-owner thread alone runs the controller and presses the original
  SAMPLE, REC/F2, ARM/F8 and GO/F7 matrix buttons through existing
  `buttonParams` + `ioport_field::set_value` wiring. 120 ms holds allow
  the CPU's key scan to see each edge. A button already held by the user or
  DAW is never taken over or released.
- After WAV staging and MAME input acknowledgment, the controller checks:
  `RECORD SET-UP` screen, then `- new -` to avoid recording over an existing
  sample, then WADCS ARM and the visible `WAITING FOR START` page before GO.
  Completion requires WAV source exhaustion and subsequent original WADCS
  STOP; it does not send the destructive abort/STOP key.
- GUI MEDIA item starts this automatically after file selection; same
  workflow for dragged WAVs. There is a menu Cancel/Resume and
  unobtrusive status in MEDIA button. A failed state gives one clear reason.
- No guest RAM shortcut, synthetic sample header/program, hardcoded CPU
  address, or proprietary firmware material was added.

## **Limitations — NOT a finished one-click playable instrument**
- This controller currently *requires a verified visible text LCD screen*.
  The S3000XL has both text and graphics display modes. If firmware RECORD
  pages use graphic mode, the text snapshot returns false and **automation
  must stop rather than blindly press buttons**. A real ROM-backed mode
  observation and, if needed, non-OCR graphics character recognition are
  required. There is no positive S3000XL ROM-powered result yet.
- It currently *requires an unused sample name* already confirmed as
  `- new -` by the firmware. Automatic 12-character name entry, mono/stereo
  selection, base note, correct record duration and memory availability
  from imported WAV metadata are still missing.
- Recorded samples are created by the Akai recorder, but **automatic
  program/keygroup mapping and MIDI playability** are not established.
  DAW project save/reload of actual Akai sample memory is also missing.
- Missing assets in the upstream `.jucer` (icons, overlay PNGs, and
  MAME plugin-data ZIP), local `F:\...` paths and old Ensoniq symbols block
  a reproducible native full Windows VST3 build. The tested isolated core
  does not establish a working plugin.
- The HD61830 patch and controller are unvalidated with the private ROM:
  even a successful MAME `git apply --check` is not a MAME build or LCD
  success. No firmware boot/audio/DAW result may be claimed yet.

## Verified isolated CI evidence (2026-11-20)
- Native C++ [core contract run 44814083309](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44814083309): **4/4 CTest PASS on Windows and 4/4 PASS on Ubuntu**. The fourth `akai_capture_controller_contract` checks generated SAMPLE/REC/ARM/GO control pulses, refusal to overwrite, missing/invalid LCD states, 120 ms hold/release, original WADCS GO and STOP, WAV end and cancellation.
- Official MAME 0.288 [patch application check 44814083347](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44814083347): **PASS** including the new HD61830 text snapshot. This proves `git apply --check` against the upstream `mame0288` tree only.
- **NOT tested:** actual MAME compilation/linking, true ROM LCD mode/snapshot readout, any automatically recorded Akai sample, Keygroup/MIDI program mapping, host project recall or full Windows VST3. Do not treat the two green CI workflows as any of these.

## Test contract
- `tests/AkaiCaptureAutomationTests.cpp`: sample/setup navigation, positive
  120 ms key pulses, `- new -` overwrite gate, WADCS ARM, LCD WAITING state,
  GO, WAV completion, original STOP, user-held key cancellation and timeouts,
  plus missing-LCD fail-closed. These are synthetic observations.
- Windows/Linux CMake/CTest (isolated native helpers) and official MAME 0.288
  `git apply --check` are automatic CI gates.
- A genuine firmware session must demonstrate actual HD61830 display mode
  and screen labels, exact first/second button scan, correctly named mono
  and stereo capture in 32 MiB RAM, sample/program MIDI after save/reload
  and 47/47 Steinberg validation before final Fender Studio Pro acceptance.

**Source references:** [S3000XL operator's manual](https://www.firstpr.com.au/rwi/smem/akai-manuals-2006/S3000XL.PDF)
and this repository's `CLAUDE.md`.
