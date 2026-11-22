# P063 — Fast virtual Akai CD-ROM transfer, unchanged original sampler

## Why
The original S3000XL firmware boots a virtual `cdrom_2x` at SCSI ID 5 when no HDD exists (P062). In upstream MAME 0.288, `nscsi_cdrom_2x_device::scsi_data_byte_period()` returns one emulated data byte per 1/307200 s, equivalent to 150 sectors/s × 2048 B = **307,200 B/s**.

The ISO is a local file. Mechanical two-speed transfer pacing does not add useful physical fidelity when the purpose is to load authentic Akai samples into the unchanged original firmware.

## Narrow P063 modification
In `mame-0288-s3000xl-vst.patch`, modify **only** the data-byte pacing inside the existing MAME `nscsi_cdrom_2x_device` from 307,200 B/s to **614,400 B/s** (=300 sectors/s, nominal 4x CD rate). This is **2x the old optical transport limit**, not a measured 2x improvement in complete sample loading.

Retain all the following:
- Same original `cdrom_2x` MAME device class, SCSI IDs 5/4, `-cdrm` cold boot, virtual vendor/product identity, SCSI commands, command delays and sector read paths.
- Same S3000XL firmware, MAME CPU, DSP sample-memory handling, ADC recording, audio mixing and MIDI.
- Same native MAME device class for device-state identity. **Device-state and project recall require functional verification** even though the class is unchanged.
- No ROMs, commercial CD ISO, or private binaries in the public source repository.

The audio DSP patch itself is **not changed**. Its recorded implementation sets the L7A1045 sample-DMA pacing to 44,100 words/s when not recording, and explicitly describes sample loading becoming **12x slower** compared with the upstream old 64-tick request. This may be the dominant bottleneck after SCSI transfer is accelerated. Do not silently remove this guard: the 64-tick behavior was found to corrupt recording. Any separate non-recording DMA fast path must first be justified and compared against native original-ROM PCM/recording tests.

## Automated regression
- `qa/test_p063_fast_cd_transport.py`: checks exact CD-only hunk, device class, new rate, speed-only contents, plus six deliberately broken mutations.
- `mame-patch-check.yml`: checks exact patch applies to pinned clean MAME 0.288, now also checking `src/devices/bus/nscsi/cd.cpp`.
- `core-contracts.yml`: Linux/Windows isolated guards.
- `windows-full-vst3.yml`: exact Windows VST3 build + official host QA, ROM-free.

## Required performance/audio comparison before final delivery
On the same native owner-ROM version and **the user's unchanged Volume 1 ISO** (542,117,888 bytes, 14 volumes):
1. Cold boot without HDD, firmware selected CD at target 5, original LOAD page and 14 volumes.
2. Compare P062 vs P063 **wall-clock** time from F8 GO to loaded 4 programs/18 samples under equal machine, host and UI conditions. Observe original Akai sample RAM and program names.
3. Compare MIDI-triggered audio for every imported program, including gain, attack, sample content, and exact expected stereo channels. Include negative no-CD/empty-memory control.
4. Save/close/reopen with no ISO mounted; confirm original RAM state and host project recall. Multiple independent VST3 instances, paused/stopped transport and recording remain necessary checks.
5. Log emulated machine time separately from wall-clock time; the MAME frontend uses `-nothrottle`, but the DAW audio ring throttles the owner thread. Avoid attributing DAW ring stalls to the SCSI device.
6. If optical throughput improvement does not materially reduce the time, isolate the already documented 12x **sample DMA** regression separately; do not claim this speed-only patch fixed it.

**ROM-free source/CI success does not equal original Akai ISO throughput or audible DAW acceptance. No final VST3 until all real original-ROM tests pass.**
