# P046 — Remove all external WAV-file import routes (user decision)

**Decision:** The owner withdrew the external WAV-import feature. It must not be offered, marketed or silently reactivated as a feature of this Akai VST3. The release target is the original S3000XL emulator/instrument with its native panel, DAW MIDI and disk-image media support, **without one-click external WAV import**.

## Removed from this revision

- **MEDIA**: delete the `Import WAV via firmware MIDI SDS...` item and its associated firmware transfer/zone status text. No WAV file chooser or direct WAV import action remains.
- Delete the legacy `Resume automatic WAV recording`, `Cancel automatic WAV recording`, `Unload staged WAV` actions and the entire WAV capture/status/alert UI.
- Remove the editor's WAV drag-and-drop target and `importWavFile` method; dropping WAVs is not a supported editor action.
- Remove both external-file import entry points `stageWavForCapture` (file-to-ADC) and `stageWavForOriginalSds` (file-to-genuine Akai MIDI UART), their declarations and compiled bodies. Remove the unused `clearStagedWav` control.
- Preserve compatibility/fail-closed legacy snapshot guards: an older project with an incomplete SDS transfer must still be rejected rather than silently substituting a different sample. Its warning no longer invites the user to re-import WAV.
- The older internal capture/SDS state holders remain inert for compatibility: the new editor sets neither `originalSdsRequested` nor `autoCaptureRequest`. Do not interpret retained unused code as a publicly supported importer. No new WAV file bytes can be staged through the plugin's interface.

## Functionality intentionally preserved

- Original MAME Akai firmware/HD61830 display, physical sampler panel, native Akai ADC from normal host audio input (not an imported WAV file), normal DAW MIDI/audio, native original program/sample state when valid.
- **MEDIA** floppy insertion/ejection, CD-ROM insertion/ejection, SCSI hard-disk insertion/removal. The six menu commands and their request/boot paths remain unchanged.
- Original Akai hardware/media formats and direct editing on the authentic instrument; no promise that the plugin accepts arbitrary stand-alone `.wav` as Akai media.

## Specific tests and release boundaries

- `qa/test_no_external_wav_import.py`: source-bound control ensures the removed editor/file-import methods/actions/drag target never reappear; asserts all six genuine media controls, original MIDI, native audio input, native full-state restore remain; independently injects five deliberate mutants to ensure the oracle rejects reintroduced import functionality or accidentally deleted media actions.
- GitHub Actions Windows/Linux isolated core C++ tests + P046 removal guard; separately native Windows ROM-free MAME/JUCE VST3, Steinberg Validator and pinned 125A Plugin Tester. Each result applies **only to the exact source SHA** under test. 
- **No owner-ROM Windows VST3 or Studio Pro project-save/reopen acceptance is implied by ROM-free CI.** External WAV import is explicitly **out of scope and must not be required** for this no-import product. Audio/MIDI, firmware boot, native sample editing and DAW state remain acceptance targets.

Historical P030–P045 SDS investigations remain archival evidence, not live features. The removed importer is not to be automatically restored by later projects or merges.
