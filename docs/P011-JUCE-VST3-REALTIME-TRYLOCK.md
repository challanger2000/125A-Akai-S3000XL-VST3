# P011 – Eliminate one blocking JUCE VST3 wrapper lock per audio block

## Reproducible finding (2026-11-20)
Source: independent [125A Plugin Tester v0.3.2 Windows host
QA run 44834174577](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44834174577),
downloaded full QA report and exact bundled VST3 binary.

- 54 PASS / **1 WARNING** / 0 FAIL. Original Steinberg VST3 SDK validator:
  PASS. This is **not** an overall QA pass and not original-Akai ROM/audio proof.
- Only warning: 5 directly monitored imported Win32 API slots, **0 heap
  allocations**, **32 potentially blocking API calls** during 32 host-paced
  48-kHz 512-sample blocks. The tracer observes `EnterCriticalSection`
  and `WaitForSingleObject[Ex]` from the VST3 DLL import table, but cannot
  prove actual contention. It is nevertheless a realtime-risk signal.
- Independently traced JUCE 8.0.12 official source:
  `modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp`,
  `JuceVST3Component::processAudio()`, original line ~3834:
  `const ScopedLock sl (pluginInstance->getCallbackLock());`
  Runs once for every VST3 audio callback, **including before Akai firmware
  boot**. This precisely explains 32 potential Win32 EnterCriticalSection
  calls in 32 traced blocks; it is not necessarily an Akai DSP lock.
- With ROM-free CI, the plugin returns silence before MAME boot. The
  existing sub-10-µs block timings are therefore evidence for the host
  wrapper's silent path **only**, not actual sampler throughput.

## Chosen minimal corrective design
Preserve original hardware emulation and JUCE's serialization, change
**only** the per-build JUCE VST3 wrapper's audio callback lock acquisition:
- The pinned `8.0.12` JUCE `ScopedLock` is replaced by its own
  `ScopedTryLock` **only in `processAudio`**.
- If the lock is available, the original protected callback code executes
  unchanged, with the lock held until the original scope ends.
- If unavailable, clear the output audio buffer and return from the audio
  processing helper without entering the plugin or blocking the audio
  callback. This preserves race protection and yields silence during
  contention instead of risking a host audio deadline miss. The separate
  GUI/other-thread locks are unaffected.
- Patch the freshly checked-out JUCE source with
  `scripts/build/patch_juce_vst3_realtime.py` before CMake. Fail closed
  if the original pinned anchor is absent, duplicated or already edited;
  do not silently change another JUCE release or copy JUCE source into
  the 125A plugin repository. AGPLv3/JUCE and MAME license notices remain.
- Do **not** weaken tester checks, relabel a warning PASS, remove the
  Windows/Steinberg validation or claim original firmware performance.

## Verification contract
1. Verify the original JUCE 8.0.12 anchor appears exactly once, and the
   patch replaces exactly that one lock. Missing/changed anchors fail.
2. Rebuild full Windows MAME+JUCE VST3 with the patched wrapper. Require
   actual `GetPluginFactory`, controller/editor lifecycle, host state
   and official Steinberg validator.
3. The independent 125A tester's `Callback API tracing (32-bit)` must
   measure **0 heap calls and 0 potentially blocking API calls**, across
   its actual 32-paced-block probe. If any remain, investigate actual
   call origin, do not silence the warning by configuration.
4. Repeat host stress and multi-instance tests. A callback unable to
   get the lock must silence output without crash/deadlock. This
   contention case is still **NOT RUN** until an independent forced-
   contention fixture is executed; the basic tester only proves its
   normal uncontended processing path.
5. **Entire original-ROM loaded path remains NOT VERIFIED** for
   realtime safety. Other code (e.g. the inherited warm-boot/anchor
   loops) may block when MAME is actually running. A true firmware-MIDI
   audio and hostile scheduler run is required before final acceptance.

## State
**ROM-free Windows VST3 and host QA: PASS for actual run 44837428505.**
- MSVC Windows x64 full MAME 0.288 + JUCE 8.0.12 build: PASS.
- Exact package manifest moduleinfo.json strict-JSON verification: PASS.
- Native Windows 125A Plugin Tester v0.3.2: **55 PASS, 0 WARNING, 0 FAIL**.
- VST3 SDK v3.8.1_build_84 official validator: PASS.
- Import-slot callback tracing in 32 host-paced blocks: **0 allocation,
  0 potentially blocking Win32 API calls**; measured max callback 3.5 µs.
- Editor lifecycle, multi-instance probe, 50 host profile setups, 5 module
  reloads, state API and Steinberg validation: PASS.
- Independent [Windows full runner and evidence](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44837428505).
- **Scope remains ROM-FREE:** the runtime original-Akai MIDI/audio signal,
  native sample RAM state and the private packed DLL's original V2.0 firmware
  boot have NOT been executed. The owner's V2.0 ROMs are bundled separately
  into the private P011 test package and both SHA256 hashes match. Packaging
  verification is **NOT a firmware boot proof**.
