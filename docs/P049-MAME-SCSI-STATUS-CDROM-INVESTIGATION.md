# P049 – MB89352 SCSI STATUS phase correction for Akai CD-ROM (candidate)

Date: 2026-11-22. Parent source: feature/p017-p048-rack-ear-overlay-20261122, commit 755ef83269bb4b51430acd0c0ea36719722c378d. Final Windows ROM-backed package is NOT approved.

## Reproduction and input
- User's archive Akai CD-ROM Sound Library Volume 1.zip has one 542117888-byte image, exactly 264706 × 2048-byte logical sectors. Verified local ZIP/ISO bytes, without adding copyrighted bytes to GitHub.
- Earlier original-ROM MAME test reported HARD DISK DRIVE NOT READY under LOAD/HARD-A/VOLS; local SCSI ID 6 did not correct it.
- Pinned MAME 0.288 connects Akai MB89352 at initiator ID 6, SCSI CD-ROM at target 4 and optional HDD at target 5. Plugin mounts CD at boot via cdrom_2x and -cdrm. Raw ISO/Mode 1 support is provided by MAME's cdrom_file::parse_iso.
- Actual original-ROM CD-volume load did not succeed in the earlier test. Cause remains unproven.

## Source diagnosis and bounded change
MAME 0.288 src/devices/machine/mb87030.cpp temp_r() returned stale m_temp while the controller was in STATUS phase. MAME upstream later corrected this in commit ac859eae1ecd91e667fa13f4b83c85a96c72a53e (2026-10-16), initially for NetBSD/x68k SCSI host-controller errors. This is a CREDIBLE shared-controller hypothesis, not evidence that it caused this Akai message.

Backport only the upstream STATUS+INP+REQ sampling and preserve old LOG throttling; do NOT add upstream unconditional logerror on this hot path. No original Akai DSP or ROM changes; no GUI, media identity, state, MIDI or WAV-import changes.

## Exact test boundaries
- RED: isolated compilation of original MAME 0.288 temp_r() against six independent fake-bus STATUS cases fails status_ready.
- GREEN: isolated compile of same method after correction passes all six cases, including guard cases and reading the signal BEFORE step(false).
- The new Windows source workflow compiles actual extracted method and re-tests a deliberately unfixed mutant; it builds pinned MAME and ROM-free VST3 after the patch.
- This does NOT establish that the ISO loads or that the owner-ROM Windows plugin boots.

## Required firmware-backed acceptance (NOT RUN)
1. Start original S3000XL V2.0/HD61830 within exact patched MAME 0.288 with user's actual ISO, SCSI target 4, initiator 6.
2. Record actual INQUIRY, TEST UNIT READY, REQUEST SENSE, READ CAPACITY, READ(6)/READ(10) status bytes and firmware access result; distinguish unit attention, no media, bad block, unsupported command and host controller status.
3. On the original front panel list genuine CD volumes, load at least one original program and sample, send notes and verify original nonzero PCM. Reject an empty CD slot and invalid ISO as negatives.
4. Repeat using the real owner-ROM embedded Windows x64 VST3 and Fender Studio Pro, with save/close/reopen, source removed, MIDI audio recall and independent instances.

Verdict: SCSI STATUS source correction is a targeted candidate. CD-load functional QA remains FAIL/NOT VERIFIED. No release or private complete VST3 claimed.
