# P044 — Original Akai DATA ENTRY native-state recall correction (development candidate)

**Owner original-ROM diagnosis:** Genuine Akai S3000XL V2.0 firmware; original patched MAME 0.288. Two actual MIDI-SDS-imported 800-frame 44.1-kHz WAVs: distinct nonzero source A and all-zero source B. Original hardware PANEL SINGLE → EDIT → F2 → F6 and physical DATA ENTRY two clicks selected `MIDI 99` in TEST PROGRAM keygroup 1. In the SAME original-ROM process, MIDI C3/E3/C4 yielded source-dependent main stereo PCM: 197 nonzero A samples per channel (peaks 938/946), zero B. Native owner ROM and PCM did **not** enter the public repo.

## Reproduced failed legacy native reload

The authentic original MAME `.sta` retains byte-exact WAV PCM but also stores the driver's internal `s3000_state::m_last_dial=2`. A fresh emulator's host-owned external `:DATAENTRY` analog input defaults to **0**. The original driver's 60-Hz `dial_timer_tick` computes a real -2 encoder move and consequently the firmware changes the selected sample back from `MIDI 99` to `PULSE`, even before any replay MIDI is sent. Both distinct sample states then played identical original audio. This is a real false-sound project-state fault, **not** a failed original SDS import.

## Independent ORIGINAL ROM counterfactual validates fix mechanism

On two brand-new native MAME processes, set the genuine external original Akai `:DATAENTRY` hardware input to **2 before** loading each **unchanged original** MAMESAVE. With the source WAV/MIDI import absent, the original OS replays source A with 219 nonzero main-channel PCM samples/peak946 (SHA-256 `1aadf929fd4b7b8ba9ee514e5be3cdb5b3d030589fd180b92c45a40e20d720f0`) while source B is **exactly silent** (SHA-256 `0e59a4bf26d7a187b56e9d01e3b58c9f7a66bbefd59e7098ec8760f8e0e09e40`). Both full outputs: original 11-channel PCM16, 48kHz, 384961 frames. No changes to original Akai firmware, DSP, sample RAM or serialized .sta. Complete historical red, test scope and hashes: [P043 investigation](P043-OWNER-ROM-SDS-SOURCE-IDENTITY-COUNTEREXAMPLE.md).

## Candidate Windows VST3 product correction (THIS BRANCH)

- `Source/AkaiDialRecall.h`: 0..255 discrete physical position, finite/clamped float normalized parsing and rounded exact step conversion (not float truncation).
- `Source/PluginProcessor.cpp`: snapshot the true external DATAENTRY position into explicit version-compatible XML attribute `akai_native_dial_position`; synchronize the saved `data_entry` APVTS parameter *copy* without host callbacks. Validate exact new attribute before mutation. Restore APVTS and physical dial **before** publishing the MAME machine image to its worker. Immediately before original `immediate_load()`, set the original MAME input's native `:DATAENTRY` analog field to the published physical position.
- `Source/PluginProcessor.h`: initialize external dial to original MAME's initial 0.
- `Source/PluginEditor.cpp`: real DATA ENTRY click/drag/reset updates the physical dial immediately AND the actual automatable host `data_entry` parameter with a proper edit gesture, rather than only changing the host-external atomic while the VST3 project parameter stays obsolete.
- `tests/AkaiDialRecallTests.cpp`: 256-step roundtrip, invalid/nonfinite values, original negative -2 and positive zero-delta input conditions. These are pure contract tests; a manual encoder algebra assertion is **not** original-ROM VST3 audio.

**Compatibility:** Prior native project images without `akai_native_dial_position` remain parseable. Their last external rotary position cannot be reconstructed reliably from the legacy JUCE state alone; do not claim backward project audio fidelity for those already-corrupted saves.

**Open engineering risks:** Competing host automation during a 6-second snapshot wait might still change the physical dial at a different time than MAME's internal saved counter; project generation and concurrency need direct stress tests. The P044 source code itself must compile in exact Windows MSVC/JUCE/MAME; no claimed original-licensed-ROM packaged Windows test yet. A diagnostic purely synthetic 800-frame owner-original proof does **not** certify longer samples, stereo source preservation, multi-import, MIDI/program automation, active sampling, multiple VST instances, or Studio One.

## Required release acceptance before calling complete

1. Exact SHA native Windows VST3 MSVC build, full Steinberg 47/47, 125A Plugin Tester, original editor lifecycle and physical dial automation, project state and malformed input.
2. The actual ROM-embedded **private Windows VST3** running original S3000XL V2.0: prepare nonzero A and all-zero B WAV via *real plugin UI*, import through original firmware and MIDI UART, original keygroup assign, render actual host MIDI→main PCM, save complete DAW state, remove external source, reopen a fresh VST3 instance and verify original source-dependent PCM; negative 0-vs-2 dial and 2-vs-2 positive controls.
3. Full real native Windows owner DAW/Studio Pro project save/reopen, timing/pacing and multiple instances. No original ROMs or private state uploaded to public CI.

Until these complete: **P044 Windows VST3 implementation is a CANDIDATE; product RELEASE NO-GO**.


## 2026-11-21 — Exact P044 Windows x64 compilation and host QA (ROM-free): PASS

**Compiled P044 source:** `f8f308dd57e44a5a51814376fe035ff33c0d9721`, [full Windows GitHub run #44945354847](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44945354847), **completed/success**. This run built the genuine Windows MSVC x64 MAME/JUCE VST3, validated `moduleinfo.json`, and exercised the exact loaded VST3 through official Steinberg and pinned 125A tester. The current development branch later added documentation only (`0de4efe`), not a new plugin binary.

**Verified published ROM-free logs:**
- Official Steinberg Validator **47 passed / 0 failed** (actual validator text).
- 125A Plugin Tester v0.3.2 **55 PASS / 0 WARNING / 0 FAIL** with 5 editor create/attach/detach cycles, 50 successful supported fresh processing profiles, 5 reload lifecycles, 50 accepted setup rate/block combinations and a fresh instance accepting/saving controller state. 64-bit processing was not advertised. The tester's generic `RELEASE: PASS` footer is **NOT an owner-ROM release approval**.
- The plugin compiled and linked without licensed ROMs. This confirms **source/build API correctness**, not firmware boot, sample-dependent audible WAV import, native project recall or graphical control correctness on the owner's machine.

**Local private owner-only candidate assembly, NOT distributed:** Source diagnostic artifact ID `14723943289` from the exact run was combined OFF GITHUB with the owner's SHA256-verified original V2.0 firmware and HD61830 ROM. Original PE module bytes were preserved unchanged; the established `125AAKAIS3KV2ROM` footer, ROM lengths/digests, strict moduleinfo, ZIP CRC, every bundled content SHA and full roundtrip ZIP unpack were checked. Resulting private candidate SHA256 `a736a5bd385a43ae20f93181e7bb1f0f84f56d66a8729e5cf218e201f0e55a8e`; internal ROM-bearing module SHA256 `e4ca5b960c0f72302ddfe08e68f811dfe05ac24db81ec355a777ed04fc2acbc9`. This is **packaging integrity**, not a Windows owner-ROM functional test. Never upload owner firmware or private binary to public GitHub or public CI.

**Still unverified and blocking FINAL:** real ROM-embedded Windows VST3 boot, one-step A/nonzero and B/zero WAV import through the *actual VST3 GUI*, source-sensitive MIDI PCM, save/close/reopen sample-specific PCM, serial dial state while automation changes during native snapshot, multiple host instances, and final Studio Pro acceptance. The private package must remain an internal **development candidate**, not be delivered as a complete instrument.
