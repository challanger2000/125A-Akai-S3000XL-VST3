#!/usr/bin/env python3
"""Produce only open-source runtime resources needed by the VST3 build.

Do NOT package firmware ROMs or copyrighted Akai media here.
MAME layout Lua files are copied from the pinned upstream MAME source tree.
The small floppy/cart icons below are new GPLv3-compatible drawings.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

FLOPPY_SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
<rect x="3" y="2" width="18" height="20" rx="2" fill="#343b45" stroke="#dae0e7" stroke-width="1.5"/>
<path d="M7 2v8h10V2M7 22v-9h10v9" fill="#8b99ab" stroke="#dae0e7" stroke-width="1"/>
<rect x="13" y="3" width="2" height="5" fill="#344052"/>
</svg>
"""
CART_SVG = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
<rect x="3" y="4" width="18" height="15" rx="1.5" fill="#39444f" stroke="#d9e0e5" stroke-width="1.5"/>
<path d="M6 9h12M6 12h12M7 19v3M11 19v3M15 19v3M19 19v3" stroke="#d9e0e5" stroke-width="1.5"/>
</svg>
"""

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mame-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    mame_root = args.mame_root.resolve()
    layout_root = mame_root / "plugins" / "layout"
    if not (layout_root / "init.lua").is_file() or not (layout_root / "plugin.json").is_file():
        raise SystemExit("MAME 0.288 layout plugin is missing (needs init.lua + plugin.json)")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / "floppy.svg").write_text(FLOPPY_SVG, encoding="utf-8", newline="\n")
    (out / "cart.svg").write_text(CART_SVG, encoding="utf-8", newline="\n")
    archive = out / "mame_plugins.zip"
    with ZipFile(archive, "w", ZIP_DEFLATED, compresslevel=9) as zf:
        for path in sorted(layout_root.rglob("*")):
            if path.is_file():
                zf.write(path, "plugins/layout/" + path.relative_to(layout_root).as_posix())
    with ZipFile(archive) as zf:
        members = set(zf.namelist())
        assert "plugins/layout/init.lua" in members and "plugins/layout/plugin.json" in members
    print(f"OPEN_SOURCE_ASSETS=OK zip={archive} layout_files={len(members)}")
    print("ROM_FILES_INCLUDED=0")

if __name__ == "__main__":
    main()
