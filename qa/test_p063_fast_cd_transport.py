#!/usr/bin/env python3
"""P063 ROM-free negative-controlled check for the Akai native MAME CD transport.

This validates one surgical upstream MAME 0.288 patch hunk; it does not
pretend that an Akai firmware/ISO/Studio Pro performance test has run.
"""
from pathlib import Path
import re


OLD = "return attotime::from_ticks(1, 307'200);"
NEW = "return attotime::from_ticks(1, 1'228'800);"
MARK = "diff --git a/src/devices/bus/nscsi/cd.cpp b/src/devices/bus/nscsi/cd.cpp"
TYPE = "nscsi_cdrom_2x_device::scsi_data_byte_period()"


def valid(patch: str) -> bool:
    if patch.count(MARK) != 1:
        return False
    chunk = patch.split(MARK, 1)[1]
    if "\ndiff --git " in chunk or len(re.findall(r"(?m)^@@ ", chunk)) != 1:
        return False
    if not chunk.startswith("\n--- a/src/devices/bus/nscsi/cd.cpp\n"
                            "+++ b/src/devices/bus/nscsi/cd.cpp\n"):
        return False
    if TYPE not in chunk:
        return False
    if "-\t" + OLD not in chunk or "+\t" + NEW not in chunk:
        return False
    if chunk.count("+\t" + NEW) != 1 or chunk.count("-\t" + OLD) != 1:
        return False
    # Speed-only delta: no SCSI command, device type, per-sector data path,
    # DMA, DSP, sampler audio, ROM, command delay, or image format change.
    actual = [line for line in chunk.splitlines()
              if line.startswith(("+", "-")) and not line.startswith(("+++", "---"))]
    removes = [line[1:] for line in actual if line.startswith("-")]
    adds = [line[1:] for line in actual if line.startswith("+")]
    if removes != [
        "\t// 150 sectors/second * 2048 bytes/sector = 307,200 bytes/second",
        "\t" + OLD,
    ]:
        return False
    if not adds or adds[-1] != "\t" + NEW:
        return False
    if any(not line.lstrip().startswith("//") for line in adds[:-1]):
        return False
    match = re.fullmatch(r"\treturn attotime::from_ticks\(1, (\d+(?:'\d+)*)\);", adds[-1])
    if not match:
        return False
    return int(match.group(1).replace("'", "")) == 4 * 307200


def main():
    patch = Path("mame-0288-s3000xl-vst.patch").read_text(encoding="utf-8")
    assert valid(patch), "Speed-only 2x->8x optical patch missing or widened"
    mutants = [
        patch.replace(NEW, OLD, 1),
        patch.replace("1'228'800", "614'400", 1),
        patch.replace(TYPE, "nscsi_cdrom_device::scsi_data_byte_period()", 1),
        patch.replace(MARK, "diff --git a/src/devices/sound/audio.cpp b/src/devices/sound/audio.cpp", 1),
        patch.replace("-\t" + OLD, "", 1),
        patch.replace("+\t" + NEW, "+\t" + NEW + "\n+\tscsi_status_complete(SS_GOOD);", 1),
    ]
    for n, broken in enumerate(mutants, 1):
        assert not valid(broken), f"Mutant {n} survived"
    print("P063_ORIGINAL_CDROM_2X_DEVICE_IDENTITY=UNCHANGED")
    print("P063_VIRTUAL_CD_BANDWIDTH_BYTES_PER_SECOND=1228800")
    print("P063_RATE_RATIO=4x_PREVIOUS")
    print(f"P063_NEGATIVE_MUTANTS={len(mutants)}/{len(mutants)}_KILLED")
    print("P063_REAL_ROM_ISO_LOAD_TIME_AND_AUDIO_AB=NOT_RUN")


if __name__ == "__main__":
    main()
