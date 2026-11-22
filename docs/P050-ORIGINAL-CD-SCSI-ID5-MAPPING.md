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
