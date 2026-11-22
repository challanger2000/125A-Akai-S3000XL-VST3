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

## Open release gates
- Confirm exact P050 Windows VST3/Steinberg/125A tester build.
- On original embedded-ROM Windows VST3 verify the owner's actual ISO shows the same 14 volumes, then load real program/sample with original MAME, trigger audible MIDI PCM and verify project save/close/reopen.
- Verify existing project recalls with firmware SCSI-ID4 remain compatible and HDD+CD routing can be reached via original firmware SCSI menu.
- User has explicitly cancelled external WAV import; do not recreate it.
- All unexecuted original-Windows/Studio Pro gates: NOT RUN / RELEASE NO-GO.
