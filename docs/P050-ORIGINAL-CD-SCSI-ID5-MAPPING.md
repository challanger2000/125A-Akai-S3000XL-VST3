# P050 original Akai S3000XL V2.0 CD-ROM SCSI target alignment

2026-11-22. Parent P048 original code 755ef83269bb4b51430acd0c0ea36719722c378d; P049 STATUS backport deliberately NOT included.

## Private original-firmware evidence (executed in the container)
- Exact original Akai V2.0 ROM and 542117888-byte user ISO, kept outside public GitHub.
- Old patched MAME 0.288 Linux binary ran original firmware and full native Akai graphical panel using MAME Lua input fields and original bitmap snapshot; no replacement Akai filesystem parser.
- Firmware factory drive SCSI ID was 5. When the ISO was mounted on target4 with the default MAME empty harddisk at target5, the original Akai firmware displayed waiting for hard disk ready, then HARD DISK DRIVE NOT READY.
- With CD-ROM at target5 (and target4 absent), the very same untouched ISO and original ROM displayed 14 actual original Akai volumes in the firmware LOAD > VOLS screen, including 3001 G.PF 2, 3012 E.PF 1 and 3018 STACK P, repeated in two independent native emulator processes.
- The P049 upstream MAME MB89352 STATUS backport completed source compilation and isolated STATUS tests but showed NO CD volumes on the same owner-ROM target5 firmware scenario twice. Reject P049 as a real original-firmware CD regression; P050 uses original MAME 0.288 STATUS semantics.

## P050 candidate
- Native MAME startup arguments: when no external harddisk image, disable default ID4 CD-ROM and attach removable CD to SCSI ID5, with cdrom_2x for an inserted ISO and cdrom for an empty slot.
- If a real harddisk image is configured, preserve the previous HDD-ID5, CD-ID4 arrangement for compatibility with saved harddisk projects.
- Media paths are paired under the preexisting owner-thread control mutex before MAME starts. Existing media hot-swap by the unique image brief name remains untouched. No audio-thread changes or WAV importer.
- Independently compiled five-case C++17 test covers no media, CD-only, Unicode path, HDD-only, HDD+CD; checks rejection of the old CD-only ID4 mapping.
- This is not yet an owner-ROM Windows-VST3/DAW sample-loading and recall PASS.

## Independent P050 private original-ROM test of the actual new command-line configuration
- Verified owner ISO length 542117888 bytes; SHA-256 `388ac703b05d0f93dcbba9e0b3b03d209df59d4bf40c976cfb9871195d289613`. Original ROMs and CD data remained outside public GitHub.
- P050 helper's CD-only argument sequence was executed verbatim by native MAME 0.288: `-scsi:4 "" -scsi:5 cdrom_2x -cdrm <owner-iso>` with original V2.0 firmware. MAME process returned exit code 0; no special CD parser was injected.
- IMPORTANT CONTROL: Allow 5.0 seconds of original firmware boot before sending first LOAD button; pressing LOAD at emulated time 0 produced `waiting for hard disk ready` and is an invalid early-boot test, not evidence that an inserted CD is unreadable.
- Firmware's genuine LOAD > F2 VOLS screen at emulated time 17.15 seconds displayed 14 Disk Volumes, including `3001 G.PF 2`, `3012 E.PF 1`, `3018 STACK P`; screen captured from original HD61830 MAME emulated LCD.
- Independent second process, same P050 arguments, chose firmware LOAD > F8 GO on first volume. Firmware subsequently displayed 4 programs and 18 samples on loading; after pressing SINGLE at emulated time 35.4 seconds, original PROGRAMS IN MEMORY showed B-GRAND PF, CHORUS PF, SOFT GRAND and PIANO PAD under Volume 3001 G.PF 2. This confirms actual original-firmware sample/program import from the user ISO, not merely ISO structure or volume-directory parsing.
- Limit: Native patched MAME Linux / genuine original-ROM evidence, not the compiled Windows VST3; the four loaded programs have NOT been independently proven playable through original MIDI→PCM in this P050 session, and project save/reopen has not been proved.

## Open release gates
- Confirm exact P050 Windows VST3/Steinberg/125A tester build.
- On original embedded-ROM Windows VST3 verify the owner's actual ISO shows the same 14 volumes, then load real program/sample with original MAME, trigger audible MIDI PCM and verify project save/close/reopen.
- Verify existing project recalls with firmware SCSI-ID4 remain compatible and HDD+CD routing can be reached via original firmware SCSI menu.
- User has explicitly cancelled external WAV import; do not recreate it.
- All unexecuted original-Windows/Studio Pro gates: NOT RUN / RELEASE NO-GO.


## Original-CD-PCM-causality — executed original-firmware acceptance, 2026-11-22

This section updates the narrower initial 14-volume-only outcome above, without
changing the source candidate or the scope of Windows CI.

- Original owner ROM v2.0 booted in standalone Linux MAME 0.288 with original
  firmware panel and the untouched Akai-CD ISO at SCSI target5.
- Using original physical LOAD, F5 SCSI, F2 VOLS, F1 LOAD and F8 GO, loaded the
  first CD volume (3001 G.PF 2) through the genuine firmware, not an alternative
  Akai-format parser. Its actual original LCD then reported sample RAM free
  100 percent -> 95 percent; programs in memory 1 -> 5; 4 additional programs
  B-GRAND PF, CHORUS PF, SOFT GRAND and PIANO PAD; volume lists 4 programs and
  18 samples.
- Issued original UART MIDI notes C3/E3/C4 from a genuine Standard MIDI File.
  The native 11-channel MAME PCM16 48-kHz output had 42,158/42,065 nonzero
  left/right Main-channel samples in the observed 49–52 s window; before MIDI
  it was exact silence. A no-disc/no-import factory control produced
  33,583/33,583 nonzero samples instead (different PCM fingerprint).
- Saved a genuine compressed original MAME complete-machine state AFTER CD
  load and physical program selection, then started a distinct new original-ROM
  emulator process with CD-ROM hardware present but NO CD MEDIA MOUNTED.
  The original LCD still displayed the 5 programs and their program names,
  and original UART MIDI replay produced real nonzero PCM. Owner ISO was NOT
  mounted in these independent restores.
- A deliberate per-program causal test sent MIDI ProgramChange 0,1,2,3,4
  with C3 NoteOn/NoteOff. The restored CD memory produced nonzero PCM in
  each of the program slots 0–4. The no-CD / no-restore factory control
  sounded only on Program 0; ProgramChange 1–4 gave EXACT SILENCE. Thus the
  additional program response really comes from the CD-loaded assets and
  not from the always-present factory test program.
- TWO distinct fresh original-ROM MAME processes replayed the same saved
  original native machine state without any CD mounted, using the exact
  same original MIDI ProgramChange/Note fixture and input timing. Full
  593281-frame, 11-channel, 48-kHz PCM16 output hashes were BIT-IDENTICAL:
  SHA256 e522e037401eae5ed919f6feb454394f02b2907f76284acb027a4796209d5471.
  This proves reproducible original native-MAME full-machine recall for
  this one CD-loaded program set, not automatic Windows VST3 state fidelity.
- The true original-ROM read path was successfully observed with CD at SCSI5
  on the old original patch. Repeated source-incompatible P049 MAME STATUS
  variant did not list the CD on an otherwise matched target5 firmware test:
  real-media negative evidence overrides the isolated STATUS unit-test PASS.

Tested user assets and genuine native saved machine image are held locally,
never pushed into public GitHub. MAME screenshot PNGs and PCM WAVs likewise
remain private.

Release boundary: CD access, original program+sample load, source-causal
MIDI PCM, and two bit-identical independent restarts PASS in original-ROM
standalone Linux MAME for this fixture. P050 ROM-free Windows VST3 CI,
actual owner-ROM embedded Windows VST3, host media-picker boot routing,
state recall, DAW Studio Pro, and multiple loaded volumes remain separate
required tests (NOT RUN until evidence exists). Do not claim FINAL from
a Linux-original-ROM functional PASS alone.
