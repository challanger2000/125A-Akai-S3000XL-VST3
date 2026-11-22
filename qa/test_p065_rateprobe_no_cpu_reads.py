#!/usr/bin/env python3
"""P065 verifies a debug-only MAME timer cannot consume CPU cycles in release.

This guard is source-only; real original-ROM ISO performance and WAV/DAW
comparisons are separate acceptance requirements.
"""
from pathlib import Path


def valid(patch: str) -> bool:
    driver = ("diff --git a/src/mame/akai/s3000.cpp "
              "b/src/mame/akai/s3000.cpp")
    if patch.count(driver) != 1:
        return False
    section = patch.split(driver, 1)[1].split("\ndiff --git ", 1)[0]
    start = "+TIMER_CALLBACK_MEMBER(s3000_state::rateprobe_tick)"
    end = "+TIMER_CALLBACK_MEMBER(s3000_state::pcsample_tick)"
    a = section.find(start)
    b = section.find(end, a)
    if a < 0 or b <= a:
        return False
    callback = section[a:b]
    init = section.split(" void s3000_state::machine_start()", 1)
    if len(init) != 2:
        return False
    machine_start = init[1].split("\n@@ ", 1)[0]
    return all((
        "+#define S3000_SAMPLING_DIAG 0" in section,
        "+\tm_rateprobe = timer_alloc(FUNC(s3000_state::rateprobe_tick), this);" in machine_start,
        "+\tm_rateprobe->adjust(attotime::from_hz(1000), 0, attotime::from_hz(1000));" in machine_start,
        "+\tif (!S3000_SAMPLING_DIAG) return;" in callback,
        # Critical: early guard executes before any CPU memory-space read.
        0 <= callback.find("+\tif (!S3000_SAMPLING_DIAG) return;")
            < callback.find("+\taddress_space &sp = m_maincpu->space(AS_PROGRAM);"),
        # Keep the 88.2 kHz analog sampler and original transfer engine intact.
        "+\tm_adcclk->adjust(attotime::from_hz(88200), 0, attotime::from_hz(88200));" in section,
        "m_dsp->set_dma_words_per_frame(1);" in section,
        "m_dsp->set_dma_words_per_frame(m_adc_stereo ? 2 : 1);" in section,
        "diff --git a/src/devices/bus/nscsi/cd.cpp" not in patch,
    ))


def main():
    patch = Path("mame-0288-s3000xl-vst.patch").read_text(encoding="utf-8")
    assert valid(patch), "P065 rateprobe fast-path / safety boundary missing"
    mutations = [
        patch.replace("if (!S3000_SAMPLING_DIAG) return;", "if (S3000_SAMPLING_DIAG) return;", 1),
        patch.replace("if (!S3000_SAMPLING_DIAG) return;", "if (!S3000_SAMPLING_DIAG) {}", 1),
        patch.replace("+#define S3000_SAMPLING_DIAG 0", "+#define S3000_SAMPLING_DIAG 1", 1),
        patch.replace("m_rateprobe = timer_alloc(", "m_rateprobe = nullptr; // timer_alloc(", 1),
        patch.replace("m_rateprobe->adjust(attotime::from_hz(1000)", "m_rateprobe->adjust(attotime::never", 1),
        patch.replace("m_adcclk->adjust(attotime::from_hz(88200)", "m_adcclk->adjust(attotime::never", 1),
        patch.replace("m_dsp->set_dma_words_per_frame(1);", "m_dsp->set_dma_words_per_frame(12);", 1),
    ]
    for i, variant in enumerate(mutations, 1):
        assert not valid(variant), f"P065 mutant {i} survived"
    print("P065_RELEASE_RATEPROBE_CPU_RAM_READS=DISABLED")
    print("P065_MAME_TIMER_AND_SAVE_IDENTITY=UNCHANGED")
    print("P065_NATIVE_SAMPLE_AUDIO_ADC_SCSI_SOURCE=UNCHANGED")
    print(f"P065_NEGATIVE_CONTROLS={len(mutations)}/{len(mutations)}_KILLED")
    print("P065_OWNER_ISO_WAV_DAW_AB=REQUIRES_RUNTIME")


if __name__ == "__main__":
    main()
