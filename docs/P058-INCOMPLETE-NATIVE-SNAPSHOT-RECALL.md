# P058 – Incomplete DAW machine snapshots must never silently recall factory RAM

Base P057 `66a6c1fe4bb40eb48d63d1d6cb97112b1098fa37` has MAME-native file-signature preflight and ROM-free Windows compilation ongoing at this commit. P058 addresses a separate documented release blocker.

## Confirmed source defect

`getStateInformation()` already writes `akai_native_complete=false` when the native MAME owner-thread save times out or fails, omitting `akai_native_machine_v1`. But `setStateInformation()` ignored that explicit failure marker. It published an empty native restore and applied host automation/media attributes. The host could reopen the song in the factory Akai state and falsely appear successfully recalled. Because the user's samples, programs and SCSI CD data belong to the native Akai RAM state, a silent factory fallback is unacceptable.

## Narrow fix

- Introduce a pure C++ rejection policy: explicit completion marker with failed capture, or claimed complete without native bytes, is rejected before APVTS/ media/machine mutation.
- A legacy project without that newer marker is still accepted by existing compatibility paths.
- An editor warning informs the user that the originally saved Akai state was incomplete; no replacement factory program was fabricated. This fires only from the GUI timer, not the realtime audio callback.
- Six positive and negative C++ tests on complete, incomplete, tampered and legacy states; a separate structural two-mutant check guarantees that the rejection comes before host-state mutation.
- Do not change MAME 0.288, DSP, physical CD/harddisk SCSI mapping, original ROM loader or sampling engine.

## Test limitations

This is a source/host failure-path correction, not a false claim of a completed private Windows Akai MIDI+CD+DAW restore. A real original owner-ROM backed x64 Windows VST3 project save/close/reopen and full multi-instance host evidence is still not performed. Release NO-GO until runtime is proven.
