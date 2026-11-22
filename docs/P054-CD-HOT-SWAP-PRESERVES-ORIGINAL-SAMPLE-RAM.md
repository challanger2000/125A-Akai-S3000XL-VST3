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
