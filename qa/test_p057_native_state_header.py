#!/usr/bin/env python3
"""P057: native MAME owns the authoritative save-header signature check.

A source regression can prove ordering and kill deliberate mutants; real
owner-ROM/Windows-DAW sample playback remains a separate release gate.
"""
from pathlib import Path
import sys

PROC = Path("Source/PluginProcessor.cpp")
HEADER = Path("Source/AkaiMachineState.h")


def valid(code, header):
    anchor = "if (const auto work=processor->nativeRestore.take())"
    if code.count(anchor)!=1:
        return False
    scoped = code[code.index(anchor):]
    verify = "save_manager::check_file("
    load = "mame_machine->immediate_load(statePath);"
    if scoped.count(verify)!=1 or scoped.count(load)!=1:
        return False
    a = scoped.index(verify)
    b = scoped.index(load)
    if a>=b:
        return False
    between = scoped[a:b]
    if "if (!nativeHeaderMatches)" not in between:
        return False
    if "if (opened" in between: # invalid file opening check syntax
        return False
    required = [
        "emu_file verify(OPEN_FLAG_READ);",
        "nativeHeaderMatches=(checked.first==STATERR_NONE);",
        "if (!nativeHeaderMatches)",
        "} else {",
        "issued=true;",
    ]
    if not all(s in scoped for s in required):
        return False
    if "x[8]==2" not in header or '"s3000xl"' not in header:
        return False
    return True


def main():
    source=PROC.read_text(encoding="utf-8")
    header=HEADER.read_text(encoding="utf-8")
    if not valid(source,header):
        raise AssertionError("P057 original MAME state load is not signature-gated")
    early=source.replace("save_manager::check_file(",
                         "save_manager::check_file_DELETED(",1)
    if valid(early,header):
        raise AssertionError("P057 missing native signature negative not detected")
    bad=source.replace("nativeHeaderMatches=(checked.first==STATERR_NONE);",
                       "nativeHeaderMatches=true;",1)
    if valid(bad,header):
        raise AssertionError("P057 ignored signature error negative not detected")
    version=header.replace("x[8]==2","x[8]==0",1)
    if valid(source,version):
        raise AssertionError("P057 original MAME version change not detected")
    print("P057_ORIGINAL_MAME_HEADER_PREFLIGHT_ORDER_PASS")
    print("P057_WRONG_SIGNATURE_AND_VERSION_MUTANTS_KILLED")


if __name__ == "__main__":
    try:
        main()
    except (OSError,AssertionError) as err:
        print("P057_FAIL: "+str(err),file=sys.stderr)
        raise SystemExit(1)
