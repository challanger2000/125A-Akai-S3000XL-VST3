# P002 – firmware-controlled MON signal bridge

## Scope and evidence
- The original S3000XL manual (record chapter, pp. 121–123) describes a **Mon/Moff** softkey: Mon switches input metering/monitoring ON, Moff switches it OFF; ARM enables monitoring, and recording completion disables it. This is a **firmware-owned** function, not a new plugin control.
- Upstream `CLAUDE.md` documents original firmware WADCS port-A values from actual recordings:
  `0xA2` idle; `0x82` armed; `0x8A` mono GO; `0x9A` stereo GO; `0xA2` after stop. Bit 3 is the record strobe, bit 4 the stereo selection.
- **The meaning of bit 5 is not confirmed by a MON/Moff toggle trace.** Its transition `A2 -> 82` when ARM automatically turns the meter on is evidence for a candidate monitor-inhibit gate, but it may also participate in A/D control.

## Source implemented
- Patched MAME `s3000_state::inctl_w` now emits its **actual** `0x00C0` WADCS latch value unchanged using MAME's `output_manager`. No inventing firmware state, no hot-path log.
- The VST3 observes the output with MAME 0.288 `set_notifier` and stores it in a per-instance atomic latch.
- A candidate monitor state decoder maps bit-5 clear and record-strobe clear to **pre-record monitor enabled**.
- When that gate is active, the DAW input – already scaled through the REC GAIN pre-A/D stage – is added to the Main L/R output through a five-millisecond click-free crossfade. There is no extra allocation or lock in the audio callback.
- While firmware recording (strobe high), host direct monitoring is disabled to avoid doubling the original MAME recorder's existing monitor voices.
- MAIN VOLUME is applied after this addition. The eight independent outputs, DSP and firmware are unaffected. No extra MON control appears in the DAW GUI.

## MAME 0.288 patch rebase
The first independent Actions `git apply --check` against the official `mamedev/mame` `mame0288` tag discovered **pre-existing upstream portability errors**:
- The Tuth patch tried to instrument `upd71071_device` methods in `am9517a.cpp`, but the official MAME 0.288 uses `v5x_dmau_device`. That separate patch fragment contained only diagnostic additions and **no deleted or modified original lines**. It was removed rather than transplanting obsolete diagnostic probes. The actual DSP and recording patches remain intact.
- The official `s3000.cpp` contains an additional preexisting `m_floppy_led.resolve();` line that the original patch did not include in its machine-start context. It was added as an unchanged context line and hunk offsets were recomputed, without deleting or changing that original code.
- Both changes are a baseline/compatibility fix, not a behavioral change to Akai audio. Re-run the independent patch applicability check on each subsequent patch revision.
- This does **not** assert a successful full MAME compile or ROM boot.

## Verified CI evidence (2026-11-20)
- [Isolated native core contracts](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44810505993): **PASS on Windows and Ubuntu**, both tests compiled and CTest executed. This verifies only independent analogue and MON state/mixing helpers.
- [MAME 0.288 patch applicability](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44810866097): **PASS**, `git apply --check` using the official upstream `mame0288` checkout, after removing obsolete `upd71071_device` instrumentation and correcting `s3000.cpp` hunk context.
- No complete VST3 binary, native MAME compilation, boot test, or ROM-backed MON/Moff verification has been performed by these jobs. The firmware bit-5 interpretation remains provisional.

## Validation and uncertainty
- `tests/AkaiFirmwareMonitorTests.cpp` tests the known WADCS transition policy and gain-mixing arithmetic; it **does not** prove bit 5 is the original MON state.
- **P002 is an instrumented candidate implementation, not accepted firmware emulation** until the ROM-backed runs demonstrate: RECORD page MON→MOFF toggle, MOFF→MON toggle, ARM (auto-enable), GO and STOP (auto-disable), mono then stereo and vice versa, correct LEFT/RIGHT, no duplicate monitor during recording, and no influence on recorded PCM.
- If the MON toggle does not change bit 5, replace the candidate decoder with the **observed actual firmware state signal** and keep the already separate host mixing path. Do not move the control into the GUI, do not invent another firmware flag.
- ROMs and binary sample material stay private. This public repository contains **no original ROM bytes**.
- The native C++ contracts can be tested in Windows/Linux Actions. Full MAME/JUCE linking and genuine ROM/host verification remain separate gates.

## Source
- Akai S3000XL Operator's Manual, SAMPLE chapter, available at https://www.firstpr.com.au/rwi/smem/akai-manuals/Akai_S3000XL_Manual.pdf
- Original upstream `CLAUDE.md` and `mame-0288-s3000xl-vst.patch` log the firmware WADCS measurements.
