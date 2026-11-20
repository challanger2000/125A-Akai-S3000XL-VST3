#!/usr/bin/env python3
"""Generate privately embedded firmware arrays from an already-owned ROM ZIP.

NEVER commit output, never run on a public CI runner, never upload source.
This script contains *no* Akai ROM bytes. All output is in the ignored
build/Generated/PrivateRoms directory on the owner's own computer.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
from zipfile import ZipFile

EXPECTED = {
    "s3000xl_v2_0.bin": (524288,
        "67917482d2a26163d8aee4de676ecbce4fd42859a3f6223c5442426998a086c5"),
    "hd61830.bin": (1472,
        "8738511c6a2f02b0ef1be2cde90898458b4517151cb473ff28ae212dbd1c1737")
}

def format_bytes(data: bytes) -> str:
    lines = []
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join(f"0x{x:02x}" for x in data[i:i+24]) + ",")
    return "\n".join(lines)

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom-archive", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    results = {}
    with ZipFile(args.rom_archive) as zf:
        members = zf.namelist()
        for name, (length, digest) in EXPECTED.items():
            matches = [path for path in members
                       if path.replace("\\", "/").split("/")[-1].lower() == name]
            if len(matches) != 1:
                raise SystemExit(f"PRIVATE_ROM_INVALID: expected exactly one {name}")
            data = zf.read(matches[0])
            if len(data) != length or hashlib.sha256(data).hexdigest() != digest:
                raise SystemExit(f"PRIVATE_ROM_INVALID: size/SHA256 mismatch: {name}")
            results[name] = data
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    header = """// GENERATED PRIVATE FILE - NEVER COMMIT OR PUBLISH
#pragma once
#include <cstddef>
namespace akai::private_rom {
extern const unsigned char firmware[];
extern const std::size_t firmware_size;
extern const unsigned char display[];
extern const std::size_t display_size;
}
"""
    (out / "AkaiPrivateRomData.h").write_text(header, encoding="utf-8")
    firmware = results["s3000xl_v2_0.bin"]
    display = results["hd61830.bin"]
    cpp = ("// GENERATED PRIVATE FILE - CONTAINS COPYRIGHTED ROM BYTES.\n"
           "// ONLY FOR THE OWNER'S PRIVATE VST3 BUILD, NEVER PUBLIC CI.\n"
           '#include "AkaiPrivateRomData.h"\n'
           "namespace akai::private_rom {\n"
           "alignas(16) const unsigned char firmware[] = {\n"
           + format_bytes(firmware)
           + "\n};\n"
           "const std::size_t firmware_size = sizeof(firmware);\n"
           "alignas(16) const unsigned char display[] = {\n"
           + format_bytes(display)
           + "\n};\n"
           "const std::size_t display_size = sizeof(display);\n"
           "static_assert(sizeof(firmware) == 524288);\n"
           "static_assert(sizeof(display) == 1472);\n"
           "}\n")
    (out / "AkaiPrivateRomData.cpp").write_text(cpp, encoding="utf-8")
    print("PRIVATE_ROM_EMBED_GENERATED=YES sizes=524288,1472")
    print("WARNING: generated .cpp contains protected ROM bytes; never upload.")

if __name__ == "__main__":
    main()
