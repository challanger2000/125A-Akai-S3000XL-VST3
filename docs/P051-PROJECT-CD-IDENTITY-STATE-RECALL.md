# P051 — Project-specific CD identity and restore safety (candidate)

Source parent: P050 f08d70b5e38e11900eaca43080ef4872304cd3ef. Single original S3000XL Windows VST3, MAME 0.288, private owner ROMs. The P049 STATUS modification stays EXCLUDED.

## Confirmed source defect before P051

The P050 getStateInformation() serialized floppy/cart file paths and the entire native machine image but DID NOT serialize scsi_cd_path or is_scsi_cd_loaded. Those values lived only in global instance settings. setStateInformation() likewise restored floppy/cart only. Thus a DAW song that loaded a CD could reopen with another song's globally selected CD, or with no CD, even though its original sample-RAM state was present. This violates project-local media identity. Not a proof that user data had already been lost.

## Bounded correction

- Save the CD path and inserted/ejected flag together in each project's XML; preserve byte-identical existing MAME state envelope.
- Reject a new-format project missing one half of the CD identity, a NUL byte or oversized media path BEFORE any APVTS or original MAME state mutation.
- Legacy DAW XML without either field leaves the former global-CD fallback unchanged.
- When the saved CD is missing, clear the live CD slot but retain the native original MAME snapshot: user-loaded sample RAM need not depend on a still-present source ISO.
- Stage the new project CD path before publishing the native machine restore. Existing active MAME sees CD hot-insertion on its owner thread only; boot-time assignment still uses P050 target5.
- No changes to HDD target5 switching, original audio DSP, MAME firmware or cancelled external WAV import.

## Executed private original-ROM audio evidence (upstream of P051)

From the P050 original-native MAME session with authentic user CD and V2.0 firmware, first note C3/E3/C4 original UART MIDI was played via native MAME 48-kHz 11-channel PCM16. Live CD, factory/no-disc and independent restored-machine/no-disc WAVs recorded.

Independent owner-private PCM contrast oracle on both Main L/R channels (32,000 samples from first note):
- Live CD vs recovered original machine, no CD media: L=0.9997558107; R=0.9997762947 Pearson PCM correlation.
- Live CD vs factory/no-disc: L=0.9317282940; R=0.9397156278.
- Three intentional negative controls (replace live/restore/factory input with wrong reference) rejected.
- Two independent original-ROM MAME fresh-process full state replays after removing the ISO are bit identical (full 48k/11-channel WAV SHA256 910a1ad856876b2ef5a1a64fa7320709b2e99501a7197a0d9ea2212c067fc845 for these 12.36-s fixtures).

This is strong native-MAME original sample+MIDI+recall causal evidence. Do not mislabel it as an actual Windows VST3 project re-open.

## Automated gates and limitations

| Gate | Scope | Status |
| --- | --- | --- |
| Project CD identity C++ helper | Nine explicit valid/missing/legacy/corrupt path cases | Candidate, Windows CI NOT RUN |
| Old CD identity negative | Source P050 lacks both CD XML attributes | Proven source absence |
| Original Akai firmware CD program import and PCM recall | Original Linux MAME with privately held ISO and firmware | PASS for tested sample set |
| P050 ROM-free Windows VST3 | 47/47 Steinberg; 55 PASS / 0 WARN / 0 FAIL 125A Tester v0.3.2 | PASS (ROM-free only) |
| P051 owner-ROM Win64 VST3 XML-save/reopen | Actual packaged candidate with ISO/missing ISO and original MIDI | NOT RUN |
| Native Studio Pro/ASIO; concurrent instances and active-media replacement | Independent host verification | NOT RUN |

IMPORTANT: HDD project-local identity and fixed-harddisk runtime reconfiguration are a separate unresolved contract. Do not claim entire SCSI media persistence solved by the CD-only correction. Current P050 media plan sets an empty CD drive to plain cdrom; hot-insertion timing against cdrom_2x remains to be tested. No final release.
