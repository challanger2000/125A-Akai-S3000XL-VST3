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

