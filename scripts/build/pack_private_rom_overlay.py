#!/usr/bin/env python3
"""Private offline packager for the original Akai S3000XL V2.0.

Inputs: the ROM-FREE Windows VST3 build artifact ZIP and owner's verified ROM ZIP.
Output: ONE Windows VST3 bundle with original ROMs appended to its own PE DLL.

The source contains no firmware. The resulting ZIP/BINARY CONTAINS PROPRIETARY
ROMs, MUST NOT be uploaded to public GitHub, CI or distributed to third parties.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from validate_moduleinfo import strip_trailing_commas
from pathlib import Path
import struct
import zipfile

MAGIC = b"125AAKAIS3KV2ROM"  # 16-byte footer identifier checked by VST3
assert len(MAGIC) == 16
ROM = {
    "s3000xl_v2_0.bin": (524288,
       "67917482d2a26163d8aee4de676ecbce4fd42859a3f6223c5442426998a086c5"),
    "hd61830.bin": (1472,
       "8738511c6a2f02b0ef1be2cde90898458b4517151cb473ff28ae212dbd1c1737"),
}

def verified_roms(archive: Path) -> tuple[bytes,bytes]:
    parts = {}
    with zipfile.ZipFile(archive) as z:
        for filename, (size, digest) in ROM.items():
            matches = [i for i in z.infolist()
                       if not i.is_dir() and i.filename.split("/")[-1].lower() == filename]
            if len(matches) != 1:
                raise ValueError(f"Expected one verified owner ROM: {filename}")
            data = z.read(matches[0])
            if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
                raise ValueError(f"Owner ROM SHA256 mismatch: {filename}")
            parts[filename] = data
    return parts["s3000xl_v2_0.bin"], parts["hd61830.bin"]

def seal_module(public_module: bytes, firmware: bytes, display: bytes) -> bytes:
    if len(public_module) < 1024 * 1024 or not public_module.startswith(b"MZ"):
        raise ValueError("Expected a real Windows PE module, not a placeholder")
    # Reject accidentally double-sealing a previously packed module.
    if public_module[-24:-8] == MAGIC:
        raise ValueError("Input already has a private ROM footer")
    footer = MAGIC + struct.pack("<II", len(firmware), len(display))
    sealed = public_module + firmware + display + footer
    if sealed[-24:-8] != MAGIC:
        raise AssertionError("Lost footer")
    flen,dlen = struct.unpack("<II",sealed[-8:])
    original = sealed[-24-flen-dlen:-24]
    if hashlib.sha256(original[:flen]).hexdigest() != ROM["s3000xl_v2_0.bin"][1]:
        raise AssertionError("ROM digest changed after sealing")
    if hashlib.sha256(original[flen:]).hexdigest() != ROM["hd61830.bin"][1]:
        raise AssertionError("Display digest changed after sealing")
    if sealed[:len(public_module)] != public_module:
        raise AssertionError("A PE byte was modified")
    return sealed

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--built-artifact",type=Path,required=True,
                    help="Full Windows ROM-free GitHub Actions build artifact ZIP")
    ap.add_argument("--rom-archive",type=Path,required=True,
                    help="Owner-supplied, verified original S3000XL V2.0 ROM ZIP")
    ap.add_argument("--out",type=Path,required=True)
    args = ap.parse_args()
    firmware,display = verified_roms(args.rom_archive)
    files = {}
    with zipfile.ZipFile(args.built_artifact) as z:
        for info in z.infolist():
            if info.is_dir(): continue
            name = info.filename.replace("\\","/")
            if ".vst3/Contents/" not in name: continue
            bundle_name = name.split("/VST3/")[-1]
            if not bundle_name.startswith("125A Akai S3000XL.vst3/Contents/"):
                raise ValueError("Unexpected plugin bundle: " + name)
            if ".." in bundle_name.split("/"):
                raise ValueError("Unsafe archive entry")
            if bundle_name in files:
                raise ValueError("Duplicate VST3 bundle file")
            files[bundle_name] = z.read(info)
    dlls = [p for p in files if p.endswith("/x86_64-win/125A Akai S3000XL.vst3")]
    modules = [p for p in files if p.endswith(".vst3") and "/Contents/" in p]
    if len(dlls) != 1 or len(modules) != 1 or not any(
            x.endswith("/Resources/moduleinfo.json") for x in files):
        raise ValueError("The source artifact does not contain exactly one complete VST3")
    # The pinned JUCE 8.0.12 build currently includes eight trailing commas
    # in moduleinfo.json. Strict JSON hosts reject this. Canonicalize metadata
    # in the privately assembled bundle without touching the VST3 PE binary.
    module_path = next(p for p in files if p.endswith("/Resources/moduleinfo.json"))
    clean, removed = strip_trailing_commas(files[module_path].decode("utf-8"))
    module_info = json.loads(clean)
    if module_info.get("Name") != "125A Akai S3000XL" or len(module_info.get("Classes", [])) != 2:
        raise ValueError("Unexpected VST3 factory metadata")
    files[module_path] = (json.dumps(module_info, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    print(f"PRIVATE_VST3_MODULEINFO_STRICT_JSON=PASS corrected_commas={removed}")
    target = dlls[0]
    files[target] = seal_module(files[target],firmware,display)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(args.out,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,bytes_ in sorted(files.items()):
            z.writestr(name,bytes_)
        z.writestr("PRIVATE-125A-README.txt",
                   "125A Akai S3000XL - PRIVATE OWNER BUILD\\n"
                   "Both ORIGINAL ROMs are embedded inside the x86_64-win VST3 DLL.\\n"
                   "Do not distribute/upload; contains Akai proprietary firmware.\\n"
                   "No other installed application, ROM folder or standalone process is required.\\n"
                   "The compiled MAME engine uses an automatically managed per-instance ROM cache.\\n"
                   "Windows host, actual ROM boot, MIDI/sample/playback and project recall\\n"
                   "must be separately validated; packaging alone is not acceptance.\\n")
    with zipfile.ZipFile(args.out) as z:
        data=z.read(target)
        if data[-24:-8] != MAGIC or len(data)!=len(files[target]):
            raise AssertionError("Sealed binary was corrupted by ZIP container")
    print(f"PRIVATE_BUNDLE_PACKAGING=PASS pe_bytes={len(files[target])}"
          f" archive_bytes={args.out.stat().st_size}")
    print("PRIVATE_OWNER_ONLY=YES PUBLIC_ROM_BYTES=NONE")

if __name__ == "__main__":
    main()
