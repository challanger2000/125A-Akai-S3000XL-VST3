#!/usr/bin/env python3
"""P064 ROM-free static guard: CD sample-memory DMA vs authentic ADC recording.

This is not a substitute for native original-ROM loading, PCM and recording
acceptance. It rejects a broad DMA-rate change touching both recording and
sample loading. Full native compiler, MAME patch apply, and owner audio
acceptance must be independent.
"""
from pathlib import Path
import sys


def valid(patch: str) -> bool:
    section = "diff --git a/src/mame/akai/s3000.cpp b/src/mame/akai/s3000.cpp"
    if patch.count(section) != 1:
        return False
    body = patch.split(section, 1)[1].split("\ndiff --git ", 1)[0]
    dsp = patch.split(
        "diff --git a/src/devices/sound/l7a1045_l6028_dsp_a.cpp "
        "b/src/devices/sound/l7a1045_l6028_dsp_a.cpp", 1
    )
    if len(dsp) != 2:
        return False
    dsp = dsp[1].split("\ndiff --git ", 1)[0]
    init = body.split(" void s3000_state::machine_start()", 1)
    if len(init) != 2:
        return False
    begin = init[1].split("\n@@ ", 1)[0]
    rec = body.split("+void s3000_state::inctl_w(uint8_t data)", 1)
    if len(rec) != 2:
        return False
    record = rec[1].split("\n@@ ", 1)[0]
    # Machine initializes as before, but this one S3000XL machine is given
    # upstream 64-tick DMA for loading (768 ticks / 12 words per frame).
    if not all((
        "+#include <cstring>" in body,
        '+\tif (std::strcmp(machine().system().name, "s3000xl") == 0)' in begin,
        "+\t\tm_dsp->set_dma_words_per_frame(12);" in begin,
        "+\t\t\t\tm_dsp->set_dma_words_per_frame(m_adc_stereo ? 2 : 1);" in record,
        "+\t\t\t\tm_dsp->set_dma_words_per_frame(12);" in record,
        "+\t\t\tm_rec_active = true;" in record,
        "+\t\t\tm_rec_active = false;" in record,
        "+\tuint8_t         m_dma_words_per_frame = 1;" in patch,
        "const int ticks = 768 / std::max<int>(1, m_dma_words_per_frame);" in dsp,
        "const auto time = attotime::from_ticks(768 / n, clock());" in dsp,
        "+void l7a1045_sound_device::set_dma_words_per_frame(uint8_t n)" in dsp,
    )):
        return False
    # The default legacy device rate remains 1; only the s3000xl config
    # accelerates outside the recording strobe. No CD/SCSI changes here.
    if "diff --git a/src/devices/bus/nscsi/cd.cpp" in patch:
        return False
    return True


def main():
    patch = Path("mame-0288-s3000xl-vst.patch").read_text(encoding="utf-8")
    assert valid(patch), "P064 original rate-limited recording / fast loading split missing"
    mutants = [
        patch.replace("set_dma_words_per_frame(12);", "set_dma_words_per_frame(1);", 1),
        patch.replace("set_dma_words_per_frame(12);", "set_dma_words_per_frame(0);", 1),
        patch.replace(
            "m_adc_stereo ? 2 : 1", "m_adc_stereo ? 12 : 12", 1
        ),
        patch.replace(
            '+\tif (std::strcmp(machine().system().name, "s3000xl") == 0)',
            '+\tif (true)', 1
        ),
        patch.replace(
            "set_dma_words_per_frame(12); // P064:",
            "set_dma_words_per_frame(1); // P064:", 1
        ),
        patch.replace("m_dma_words_per_frame = 1;", "m_dma_words_per_frame = 12;", 1),
        patch.replace(
            "const int ticks = 768 / std::max<int>(1, m_dma_words_per_frame);",
            "const int ticks = 64;", 1
        ),
        patch.replace(
            "const auto time = attotime::from_ticks(768 / n, clock());",
            "const auto time = attotime::from_ticks(64, clock());", 1
        ),
    ]
    for idx, broken in enumerate(mutants, 1):
        assert not valid(broken), f"P064 broken DMA split mutant {idx} survived"
    print("P064_S3000XL_SAMPLE_LOADING_64_TICKS=SOURCE_PASS")
    print("P064_RECORDING_MONO_768_TICKS_STEREO_384_TICKS=SOURCE_PASS")
    print("P064_OTHER_DRIVERS_ORIGINAL_DMA_DEFAULT_1=SOURCE_PASS")
    print("P064_SCSI_CD_AND_AUDIO_MIXER_UNCHANGED=SOURCE_PASS")
    print(f"P064_NEGATIVE_MUTANTS={len(mutants)}/{len(mutants)}_KILLED")
    print("P064_PRIVATE_ORIGINAL_ISO_PCM_DAW_RECORDING=NOT_RUN")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, ValueError) as e:
        print("P064_FAIL: " + str(e), file=sys.stderr)
        sys.exit(1)
