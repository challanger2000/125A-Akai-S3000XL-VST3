# P003 – WAV via original Akai A/D recording pathway

## What is implemented
- `Source/AkaiWavInput.h` decodes bounded RIFF/WAVE PCM 8/16/24/32 and IEEE float32, mono/stereo, including supported WAVE_FORMAT_EXTENSIBLE. It rejects invalid chunks, misalignment, nonfinite/out-of-range floats and truncated data.
- 8–192 kHz WAV files are converted **off the audio thread** into stereo PCM16 at the currently configured MAME host stream rate. At nonmatching rates a 32-tap windowed sinc provides antialiasing; no normalization or unrequested gain is applied. Exact-rate PCM16 passes through the direct sample path.
- Decoder/output memory limits: 64 MiB source file, 32 MiB converted stereo PCM16 (software safety caps, not claims about Akai sample RAM capacity). If source is mono, both A/D channels receive the same stream.
- The editor supports WAV by direct file selection in the existing MEDIA menu and drag-and-drop. No external WAV conversion utility, command-line tool or mounted virtual floppy is needed for **audio input preparation**.
- A GUI-owned double-buffered immutable asset is published to the MAME input thread via atomics. No locks/allocations, I/O or object destruction occur on the audio callback or MAME source callback. The engine acknowledges each pointer switch before the GUI can overwrite the inactive slot.
- Source audio is injected at the existing `pullAudioForMame` callback, therefore it reaches the original MICROPHONE -> A/D -> DMA -> original firmware. **No direct sample RAM writes**, guest header fabrication, private firmware copies or alternate sampler engine.
- Original firmware WADCS record strobe (port A bit3, already measured in upstream) starts WAV delivery on actual firmware GO; the STOP transition suspends it. Pre-REC ARM does **not** silently consume WAV. Source tail is explicit silence. The original physical REC GAIN applies to WAV input; output routing remains unchanged.
- Mono/stereo samples preserve channel order. Source has no autoplay while idle and no hidden DAW MIDI injection.

## Native CI evidence (2026-11-20)
- GitHub Actions [run 44812380200](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44812380200): **SUCCESS on windows-latest and ubuntu-latest**.
- Three CTest executables compiled and passed on each operating system: `akai_analog_control_contract`, `akai_firmware_monitor_contract`, `akai_wav_input_contract`. Total **3/3 on Windows and 3/3 on Ubuntu**.
- The WAV contract exercises the bounded decoder, mono/stereo, PCM8/16/24/32, float32, WAVE_FORMAT_EXTENSIBLE, source-rate conversion, malformed inputs, no playback before explicit trigger, playback on trigger, STOP and trigger restart.
- **This is not** a compilation of the complete Akai/MAME/JUCE VST3, not a V2.0 original-firmware test, and not a demonstration of an automatically mapped or DAW-recallable Akai sample.

## Functional limits – DO NOT LABEL THIS A FINISHED ONE-CLICK IMPORT
This P003 **implements WAV decoding, in-plugin selection and playback into the original Akai recording ADC.** It is not yet the complete Emulator-II/III user-facing workflow. A user must currently configure the original `SAMPLE -> REC` screen (name, time, mode), press **ARM** then **GO**, and use the firmware to finish/assign the resulting sample. A one-click button that automatically creates a sample and suitable program/keygroup **does not yet exist**.

Also open:
1. Automated original-firmware REC/ARM/GO/stop/keygroup workflow with firmware-screen/state confirmation. Avoid blindly sending button sequences.
2. Original ROM V2.0 session: witness WAV -> ADC -> native Akai PCM -> sample name -> playable MIDI; mono/stereo and data size/loop/level checks.
3. Imported sample + Akai program/keygroup **DAW project save/reload**. The upstream plugin still saves legacy Ensoniq NVRAM instead of genuine S3000XL sample RAM. WAV path staging alone is **not** portable recall.
4. Full Visual Studio 2022 MAME/JUCE/VST3 build, Steinberg validator and final Fender Studio Pro test. This task's isolated CI checks do **not** prove the integrated product.
5. Source file preview/progress and better multi-sample handling; the source currently allows one staged WAV at a time, with an immutable two-slot replacement policy.

## Technical tests
- `tests/AkaiWavInputTests.cpp` verifies format bounds, PCM formats, stereo channel separation, conversion length, polarities, nonfinite rejection and the feed's arm/GO/STOP/complete states. It is independent of JUCE, MAME and any user ROM.
- Windows and Ubuntu C++20 CMake/CTest in the existing `core-contracts.yml` workflow. This **only proves the isolated engine**, not integrated MAME sampling. State claims should be updated from actual workflow logs, never from scheduling.
- MAME 0.288 original patch applicability is checked separately by `mame-patch-check.yml`.

## Source references
- `challanger2000/125A-Emulator-II-Windows-VST3` – `src/125a_wav_input.h`, `docs/P001-WAV-BRIDGE.md`: source waits until original firmware starts capture.
- `challanger2000/125A-Emulator-III-Windows-VST3` – `source/upstream/runtime/wav_import.h`, `wav_firmware_import.h`: retained original firmware ownership.
- `CLAUDE.md` from upstream Tuth S3000XL: verified WADCS `A2 -> 82 -> 8A/9A -> A2` and fixed 44.1-kHz device A/D. Must preserve the proven stereo recording and not reintroduce 2-word mono DMA, old probes or A/D phase hacks.

No proprietary ROM is committed to the public source tree or CI artifacts.
