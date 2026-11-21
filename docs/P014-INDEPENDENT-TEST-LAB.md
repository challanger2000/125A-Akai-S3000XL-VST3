# P014 — Independent 125A Plugin Test Lab for exact P013 Windows binary

## Independent testing boundary

This workflow checks the **exact ROM-free Windows VST3 binary** produced by the
already-running P013 full Windows MAME/JUCE build:
[Akai workflow #44901613498](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44901613498),
commit `9e48497674f0ad4f82eef67e411d944e245e65a9`. It downloads the
binary from that GitHub Actions run ONLY after both the exact SHA and the
full-build conclusion were checked, and records its actual SHA-256.

The independent Test Lab is checked out by commit
`challanger2000/125A-Plugin-Test-Lab@9f84737b67588c5a3ab5649664bfe3032e529790`.
It builds native Windows x64 VST3 GUI, audio and state-stress test hosts
from source. A known working VST3 must pass; a deliberately defective
VST3 must fail. Actual Akai bundle then receives separate P74 read-only
VST3 discovery and opt-in native audio/state-stress baseline. All actual
reports and subprocess exit codes are uploaded even on failure.

Never use the previously passed 125A Plugin Tester/Steinberg validator as
a substitute for this separate test. Both are retained in the P013 build
pipeline as additional checks.

## Release and proprietary constraints

The public test environment contains **no original Akai ROMs**, user WAVs,
sample data or personal DAW projects. The test refuses an already sealed
private Akai module, and source-run/commit mismatches fail before test.
Neither original firmware boot, completed original WAV capture, audible
native MIDI playback, complete sample mapping, 32 MiB RAM persistence,
nor real Fender Studio Pro project recall is certified by P74. These remain
independently **NOT RUN / NOT COVERED**, even if generic host-level
technical tests pass. Actual failed P74 results are failures, never
reported as passes. Hardware ASIO is also NOT RUN.

## User-visible outputs

- P013 exact full Windows 125A Plugin Tester v0.3.2 and Steinberg validator:
  [P013 full Windows run](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44901613498)
- P014 Test Lab workflow:
  [independent P014 workflow](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/workflows/windows-testlab-p014.yml)
- Artifact: `125A-Akai-P014-Independent-Test-Lab-Windows-P013-ROM-FREE`
  contains actual original P74 reports, known-good and defective controls,
  product module SHA-256 and explicit feature gaps.

## P014/P100 real multibus QA follow-up (2026-11-21)
- First exact-P013 P014 run had a script exit-code false failure despite correctly rejecting the six-profile defective state fixture. This was repaired with real native P32 fault assertions.
- Second exact-P013 P014 run [44904489252](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44904489252) independently inventoried the Akai module, 2128 parameters and 11 buses overall (1 stereo input, 1 16-channel MIDI event input, 1 stereo main output + 8 mono individual outputs). It correctly refused the native P25 state stress in all six profiles because the original Lab host supported only ONE audio output bus. **This was Test Lab incompatibility, not Akai DSP failure; no actual process callbacks occurred.**
- Independent Test Lab [P100 successful Windows run](https://github.com/challanger2000/125A-Plugin-Test-Lab/actions/runs/44904836039), source SHA `9f84737b67588c5a3ab5649664bfe3032e529790`, adds bounded 1–16 mono/stereo output buses, explicitly supplies every bus to native process(), and verifies all returned output channels, not just MAIN L/R. Independent real VST3 tests PASS for nine-bus good, intentionally nonfinite auxiliary output (correctly rejected), and legacy stereo good.
- Akai P014 now pins precisely the independently verified Test Lab P100 revision. Rerun native Akai P74 against **unchanged P013 module SHA256 `fb8081aa4edce880b438093ebcc6516422ad59e0de18514f55ab59c1aaaa3521`**. A passing generic native test still cannot establish a ROM-backed Sampler.

## Final independent P014 native outcome — PASS (2026-11-21)
- [Exact-source Windows P014 run #44905017152](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44905017152) **completed SUCCESS**, testing precisely P013 source `9e48497674f0ad4f82eef67e411d944e245e65a9`, public VST3 PE SHA256 `fb8081aa4edce880b438093ebcc6516422ad59e0de18514f55ab59c1aaaa3521`, and the independently proven P100 Test Lab host commit `9f84737b67588c5a3ab5649664bfe3032e529790`.
- Actual native P74 universal VST3 discovery: **INVENTORY_COMPLETE**. Akai's main stereo + eight mono output buses, stereo input, 16-channel event input and **2,128 advertised parameters** were enumerated in this ROM-free module. P74 **TECHNICAL_BASELINE_PASS** and `native_technical=PASS` in the original uploaded evidence.
- Native P32 tests: **6/6 PASS**, sample rates 44.1/48/96 kHz × 64/512-sample blocks, **500 accepted state saves per profile** with 499 confirmed during active audio callbacks, no native callback errors; the Lab found no measured callback deadline overruns in its scope. Generic state capture and two fresh-process state roundtrips passed at the VST3 API level.
- Independent adverse control **FAILED correctly in all six state test profiles**: the intentionally defective VST3 refused 100/500 state saves per profile; native P32 evidence was checked, not merely the top-level P74 failure flag. The nine-output P100 corrupted-auxiliary-output fixture also failed correctly while main stereo stayed valid.
- [P014 evidence archive](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44905017152/artifacts/14707107862) contains original `PROVENANCE.json`, `AKAI-TEST-LAB-SCOPE.json`, standalone discovery and six-profile native host results, actual state captures, and positive/negative fixture tests.

**Strict limits:** This is a **ROM-free** machine-host QA. Real original-Akai V2.0 firmware boot, original native PCM, WAV capture/program assignment, MIDI-driven audible sample, 32 MiB sample-RAM project recall, actual Fender Studio Pro session, dual real MAME machines and physical ASIO **remain NOT RUN/NOT COVERED**. Two fresh-process API state byte roundtrips do **NOT** verify persistent firmware sample data, and silence in both ROM-free audio render files is not audible instrument proof. Do not label this a playable sampler, a FULL AUDIT or a final release.
