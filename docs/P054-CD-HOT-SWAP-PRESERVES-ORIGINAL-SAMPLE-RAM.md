# P054 – Genuine Akai CD hot-swap must not erase loaded sample RAM

Parent P053 `54c7d6a32c8720299c33a2818ce42cf65e5f9c52`. Previously the GUI labeled CD media "reboots on change" and invoked shutdownMame();checkRomAndBootMame() both on CD insertion and ejection. That destroys the live Akai program/sample memory even though original hardware retains samples after removable CD eject. This is a user-visible hardware-fidelity defect, not a cosmetic label issue.

## Original-ROM positive and negative runtime evidence

With the private genuine original V2.0 firmware and owner Akai CD first volume loaded, genuine MAME 0.288 native state was replayed at 52.0 s. At 52.4 s the real MAME CD device :scsi:5:cdrom_2x:image was ejected using image.unload() on the MAME owner thread, then the original MIDI sequence ProgramChange 0–5, Note C3 was sent. Both main channels rendered exactly the same PCM as the original CD-loaded state replay launched with no inserted disc:

- SHA-256 full 12.36 s 11-channel 48-kHz PCM16 WAV in BOTH executions: `910a1ad856876b2ef5a1a64fa7320709b2e99501a7197a0d9ea2212c067fc845`.
- 653638 nonzero main-channel samples in either run and peak 6263.
- Compared PCM sample arrays: byte-for-byte identical.
- Defective P050 no-disc device change to plain `cdrom` remains a separate negative: original-ROM machine image did not restore and only factory program sounded.

The MAME image loader also accepted owner ISO via hot insertion into initially empty cdrom_2x at SCSI5 without restarting original ROM (`image.load` returned success). Firmware drive selection was still FLOPPY immediately after the hot insertion, so this observation is ONLY proof of device-level insert operation, NOT yet proof of original firmware 14-volume listing in that hot-insert sequence.

## Targeted implementation

- Retain original P053 unchanging cdrom_2x device, original P052 MAME owner-thread requestCdLoad and project restore sequencing.
- CD insert and eject in the JUCE MEDIA menu now change the atomic requestCdLoad flag and never call shutdownMame()/checkRomAndBootMame().
- Removable floppy behavior unchanged. Fixed hard disk insertion/removal still requires its deliberate reboot.
- Actual original ROM audio and loaded-program state remain untouched by the CD eject.
- Source-level regression checks exact editor routing and kills two old reboot mutants. A genuine Windows VST3 ROM-backed DAW project save/reopen, factory media selection and multi-instance check are still required.

Final user release: NOT VERIFIED until actual Windows Studio Pro audio+project-recall evidence.

## Separate fresh-boot original firmware hot-insert proof

To challenge the distinct GUI **CD insert** path independently of CD eject:
- Started a fresh standalone MAME 0.288 S3000XL with original owner V2.0 firmware, `-scsi:4 "" -scsi:5 cdrom_2x`, **no ISO on the MAME command line**.
- At emulated 5.0 s, called the genuine target5 `device_image_interface` image.load() with the owner's unchanged 542117888-byte Akai ISO. It returned no error.
- Used genuine physical Akai keys to open LOAD, F5 SCSI, F1 LOAD (select the SCSI device instead of the initial FLOPPY), then F2 VOLS. At emulated 18.28 s the **original Akai screen listed 14 real Volumes**, including 3001 G.PF 2, 3012 E.PF 1 and 3018 STACK P. No fake filesystem parser or custom machine RAM injection.
- Independent matched negative: same original firmware, same constant cdrom_2x device and exact input/timing sequence, but **no image.load() and no CD media**. The original Akai remained on FLOPPY/empty load page, and no Akai volumes appeared.
- These two native firmware traces are an executed positive/negative test of the CD insertion and SCSI media path. They do not cover original Windows JUCE UI MIDI/audio integration.

During the first hot-insert trial we did not call F1 LOAD from the Akai SCSI page; the firmware still selected the floppy, and no CD Volumes appeared. That incomplete procedure was not a valid proof of failed hot-insert; the corrected actual firmware route above succeeded.

## P055 private final hot-insert import, original UART MIDI and no-disc state replay

Executed on 2026-11-22 after P054, without modifying the Windows VST3 production audio or native MAME core. User-owned ROM and ISO remain local; no ROM, sound library, snapshots or MIDI PCM bytes are in the public repository.

**Important challenge of the earlier VOLS-only test:** Merely listing Volume `3001 G.PF 2` and seeing "progs 4 samps 18" DOES NOT prove those samples were imported into Akai memory. A deliberate follow-up tried F1 LOAD/F8 GO before selecting the real LOAD FROM DISK drive and was rejected by the original firmware with "NO DISK!" and produced only the factory test-program MIDI fingerprint. This invalid early interpretation was corrected, not counted as a pass.

**Actual operator-reproducible firmware path:**
1. Start the original patched MAME 0.288 with `-scsi:4 "" -scsi:5 cdrom_2x`, no CD mounted; wait 5 s for original firmware boot.
2. Insert the unchanged original user ISO into the existing target5 cdrom_2x image device using the real MAME CD hot-swap interface. There is NO reset.
3. On the original hardware keys, open LOAD and SCSI (F5) then LOAD (F1). The firmware initially displays `LOAD FROM DISK: FLOPPYH`. Move its original hardware DATA wheel one detent (`DATAENTRY: 0 -> 1`) with the drive field active. The genuine Akai LCD now displays `LOAD FROM DISK: HARD-:A` and Volume 3001 G.PF 2. Using F2 VOLS, F1 LOAD and F8 GO loads the first CD volume.
4. Genuine LCD free memory changed **100% → 95%** and program/free-PK/S counters changed **1000 → 886**. Physical SINGLE screen reported **5 programs in memory**, including the CD programs B-GRAND PF, CHORUS PF, SOFT GRAND and PIANO PAD plus factory TEST PROGRAM. This is actual original-firmware sample import, not just ISO/volume-directory parsing.
5. With original UART MIDI Note test C3/E3/C4, fresh hot-import audio versus cold-boot imported CD Main L/R Pearson PCM correlation **0.9991511 / 0.9992369**, and versus no-CD factory **0.9319368 / 0.9399339**, all in a separately aligned 32000-sample window after first note. This proves source-causal original loaded CD audio.
6. In another hot-insert/original-ROM run, issued the original MIDI ProgramChange sweep 0–5; recorded actual 48-kHz 11-channel PCM16 via original MAME sound manager. Saved genuine full-original-MAME `MAMESAVE` state after importing the disc, not a fake bank or stand-in sample parser. Fresh original S3000XL process started WITHOUT a CD ISO and with the same empty cdrom_2x SCSI device; restored the snapshot. It jumped directly to the saved emulation time and produced CD-loaded-program MIDI audio.
7. The independent no-CD restored program-sweep audio correlated with hot-import original PCM **0.9993178888 left / 0.9993927826 right**, versus factory/no-disc **0.9441967664 left / 0.9505032463 right**. Peaks restored L/R **6296/6274**, factory **5103/5143**.
8. TWO separate fresh processes using the hot-import saved machine state, same no-disc CD drive and original program-sweep MIDI input, rendered full 48k/11ch WAVs with **identical SHA-256 `d61985eb12a870e0f28229f9cde5c614f3dbc33ec6f92e8269cbc0e6f5109e11`**.

**Boundary:** Native original Akai firmware CD hot insertion/import, MIDI sound, full machine sample-state restore with no ISO, and two independent PCM-identical no-ISO replays are PASS for this one fixture. The original-ROM embedded Windows x64 VST3 host CD browser, project XML save/close/reopen, DAW Studio Pro, multiple instances, and installer remain **NOT RUN**. None of these observations authorizes a user release.

