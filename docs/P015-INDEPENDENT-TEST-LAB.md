# P015 independent 125A Test Lab — same strict P014 methodology

This is a new **independent** Windows CI pipeline to validate exactly the
original P015 C++ fix (`3c7ce9dae4086a918723d78d1ed53eb8196bab0a`)
against 125A Plugin Test Lab P100 SHA
`9f84737b67588c5a3ab5649664bfe3032e529790`.

The test waits only for [P015 complete Windows VST3 build #44906089036](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44906089036),
checks its exact commit and successful production tester/Steinberg QA,
downloads the ROM-free artifact and verifies that it is a Windows PE
VST3 **without private original firmware**.

The native P74 GUI/ABI inventory and P32 audio/state-recall stress
are then independently run on the real Akai P015 module with all nine
audio outputs activated. The true native working and known-broken
state VST3 fixtures must give correct positive/negative results.
Source/code SHA and real output evidence are preserved on failure.

**Scope limit:** P015 native QA is **not** proof the original firmware
boots or that an imported WAV becomes an audible playable sample.
32 MiB native sample RAM DAW-project recall is also NOT COVERED; two
generic state byte roundtrips do not substitute for sample content.
A private P015 ROM bundle must be sealed and checked offline, and
real ROM-backed boot/WAV/MIDI/project reload tested separately.

Workflow: [P015 independent Test Lab](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/workflows/windows-testlab-p015.yml)
