# P011 — Confirmed missing sample-RAM recall, release blocker

## Source finding (2026-11-20)
- The original MAME 0.288 `src/mame/akai/s3000.cpp:362–366` maps the
  Akai L7A1045 DSP's **32 MiB wave/sample memory** at
  `0x00000000..0x01ffffff` with `.ram()`.
- Current VST3 `Source/PluginProcessor.cpp`,
  `getStateInformation()` around lines 2849–2865, copies only
  `root_device().memshare("osram")` and
  `root_device().memshare("seqram")` into XML (RAM 65,536 and
  327,680 bytes respectively). It does **not** copy the L7A1045 DSP's
  wave/sample RAM, or capture the full original MAME device state.
- `setStateInformation()` around lines 2977–2997 restores only those
  OS/sequence-RAM blocks. The path-only media fields additionally depend
  on external files. **Therefore a playable Akai sample created from WAV
  cannot be shown to survive DAW project save/reopen**, even if a generic
  125A/VST3 state-roundtrip API test passes.
- In ROM-free [125A host QA run 44830191063](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44830191063),
  the VST3 component's saved state was just **2,134 bytes** while no
  firmware/samples were running. This is valid as an API test but proves
  nothing about preservation of actual Akai 32 MiB wave RAM.

## Required source and tests BEFORE approving sample recall
1. Drive real Akai firmware V2.0 from its original ROM; import/play a WAV
   through the original ADC/DMA pathway and assign a genuine Akai sample
   to a genuine program/keygroup. Capture the same MIDI-triggered PCM.
2. Snapshot **all necessary original MAME device state** from its
   emulation-owner thread at a defined safe boundary (at least DSP wave
   RAM, program/sample tables, mappings, and controller/runtime state).
   Evaluate `running_machine::save().write_file(util::core_file&)` and
   `read_file()` before adding hand-written guest memory-address copying.
   MAME 0.288's `util::core_file::open_ram()` is READ-ONLY, so an in-memory
   WRITE backend cannot simply be assumed; bound file-backed snapshot I/O
   may be necessary off the audio thread.
3. Serialize a versioned portable bank into the DAW state, with explicit
   limits, hashes/size validation and atomic commit. **No blocking I/O,
   allocations or MAME state locks in the real-time audio callback.**
4. Close DAW, remove/rename original source WAV and external image paths,
   reload a fresh VST3 instance and play the same MIDI. Compare rendered
   PCM with the independent pre-save fixture; check sample count, key range
   and velocity boundaries. A matching file name alone is not PASS.
5. Negative controls: damaged/truncated stored state, missing assets,
   memory overrun, double load, duplicate instance and simultaneous audio/
   save/reopen. Failure must preserve last valid instrument or fail
   predictably without corrupting another instance.
6. Confirm MAME's stock 32 MiB RAM and firmware memory-discovery logic agree
   with the expected S3000XL configuration; do not silently confuse OSRAM
   with wave RAM or count CI API acceptance as audible sample recall.

## Status
**Source inspection: CONFIRMED GAP. Portable Akai sample recall: NOT IMPLEMENTED
/ NOT VERIFIED.** No Windows DAW or original-ROM sample-recall test
has been completed. This blocks FULL/FINAL irrespective of Steinberg or
125A Plugin Tester compliance.
