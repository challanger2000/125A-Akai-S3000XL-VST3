#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Private owner-original-ROM native MAME sample-source/recall adversarial test.

Never upload original ROMs, MAME .sta snapshots, WAV or generated PCM to
public GitHub. This is a Linux MAME original-firmware gate, NOT Windows VST3 QA.
Requires genuine original-MAME states recorded from TWO distinct original-SDS
WAV imports (one nonzero signal and one exactly zero), with the intended
original firmware keygroup assigned to the imported SDS sample in both.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import wave

HEADER = bytes.fromhex("4d546864000000060000000101e0")
TEMPO = bytes.fromhex("00ff510307a120")
FOOTER = bytes.fromhex("00ff2f00")


def vlq(n: int) -> bytes:
    b = [n & 127]
    n >>= 7
    while n:
        b.insert(0, (n & 127) | 128)
        n >>= 7
    return bytes(b)


def midi_notes() -> bytes:
    track = bytearray(TEMPO)
    for idx, note in enumerate((60, 64, 72)):
        track.extend(b"\x00" + bytes((0x90, note, 110)))
        track.extend(vlq(480) + bytes((0x80, note, 0)))
        if idx != 2:
            track.extend(vlq(240))
    track.extend(FOOTER)
    return HEADER + b"MTrk" + struct.pack(">I", len(track)) + track


def test_one(args: argparse.Namespace, label: str, source: str,
             with_notes: bool, external_dial: int) -> dict:
    state = args.tone_state if source == "tone" else args.zero_state
    script = args.outdir / (label + ".lua")
    notes = args.outdir / (label + ".mid")
    rendered = args.outdir / (label + ".wav")
    if with_notes:
        notes.write_bytes(midi_notes())
    # json.dumps emits valid Lua double-quoted paths in normal path usage.
    # Each run is a fresh native MAME process, not an in-process state reload.
    command = (
        "local img=manager.machine.images[':mdin:midiin:midiinimg']; "
        "assert(img,'original MIDI input image missing'); "
        "local err=img:load(" + json.dumps(str(notes.resolve())) + "); "
        "if err then error('MIDI_NOTES_IMAGE_LOAD_FAILED '..tostring(err)) end; "
        "emu.print_info('NOTE_MIDI_LOADED')"
        if with_notes else "emu.print_info('NO_MIDI_NEGATIVE')"
    )
    source_lua = """local requested=false;local start=nil;local loaded=false;local done=false
emu.register_periodic(function()
 local t=emu.time()
 if not requested and t>=1 then
  requested=true
  manager.machine.ioport.ports[':DATAENTRY']:field(255):set_value(%d)
  manager.machine:load(%s)
  emu.print_info('P045_PRIVATE_ORIGINAL_RESTORE_REQUESTED')
 end
 if requested and not start and t>20 then
  start=t;emu.print_info('P045_ORIGINAL_FIRMWARE_ACTIVE')
 end
 if start and t>=start+0.35 and not loaded then
  loaded=true
  %s
 end
 if start and t>=start+7 and not done then
  done=true;emu.print_info('P045_ORIGINAL_ROM_TEST_END');manager.machine:exit()
 end
end)
""" % (external_dial, json.dumps(str(state.resolve())), command)
    script.write_text(source_lua, encoding="utf-8")
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    if args.library_dir:
        env["LD_LIBRARY_PATH"] = str(args.library_dir) + ":" + env.get("LD_LIBRARY_PATH", "")
    cmd = [
        str(args.mame.resolve()), "s3000xl", "-rompath",
        str(args.rompath.resolve()), "-bios", "default",
        "-noreadconfig", "-video", "none", "-sound", "none",
        "-nothrottle", "-skip_gameinfo", "-seconds_to_run", "400",
        "-wavwrite", str(rendered.resolve()), "-autoboot_script",
        str(script.resolve()),
    ]
    result = subprocess.run(cmd, cwd=args.outdir, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=160)
    (args.outdir / (label + ".log")).write_text(
        result.stdout, encoding="utf-8")
    if result.returncode != 0 or "P045_ORIGINAL_ROM_TEST_END" not in result.stdout:
        raise RuntimeError(label + " original-ROM emulator failed: " + result.stdout[-1100:])
    with wave.open(str(rendered), "rb") as stream:
        channels, rate, frames = (stream.getnchannels(), stream.getframerate(),
                                  stream.getnframes())
        if stream.getsampwidth() != 2:
            raise RuntimeError("original native MAME was not PCM16")
        pcm = stream.readframes(frames)
    count, peak = [0]*channels, [0]*channels
    for idx, (value,) in enumerate(struct.iter_unpack("<h", pcm)):
        channel = idx % channels
        if value:
            count[channel] += 1
        peak[channel] = max(peak[channel], abs(value))
    return dict(test=label, source=source, with_notes=with_notes,
                external_dial=external_dial, channels=channels, rate=rate,
                frames=frames, nonzero=count, peak=peak,
                pcm_sha256=hashlib.sha256(pcm).hexdigest())


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--mame", required=True, type=Path)
    p.add_argument("--rompath", required=True, type=Path)
    p.add_argument("--tone-state", required=True, type=Path)
    p.add_argument("--zero-state", required=True, type=Path)
    p.add_argument("--dial-position", required=True, type=int,
                   help="physical DATAENTRY wheel position when both states were saved")
    p.add_argument("--bad-dial", type=int,
                   help="optional original-firmware negative control (e.g. 0 vs saved 2)")
    p.add_argument("--source-to-hide", action="append", default=[], type=Path,
                   help="repeat for each original imported WAV and SDS MIDI source")
    p.add_argument("--library-dir", type=Path,
                   help="local directory with an owner native MAME SDL-ttf shim if needed")
    p.add_argument("--outdir", required=True, type=Path)
    args = p.parse_args()
    if not all(x.is_file() for x in (args.mame, args.tone_state, args.zero_state)):
        raise RuntimeError("native MAME or original state is missing")
    if args.tone_state.resolve() == args.zero_state.resolve():
        raise RuntimeError("same fixture for tone and zero is not a valid A/B control")
    if not args.rompath.is_dir():
        raise RuntimeError("original-ROM directory is missing")
    if not 0 <= args.dial_position <= 255:
        raise RuntimeError("invalid saved hardware wheel position")
    if args.bad_dial is not None and not 0 <= args.bad_dial <= 255:
        raise RuntimeError("invalid deliberate negative wheel position")
    args.outdir.mkdir(parents=True, exist_ok=True)
    hidden = []
    try:
        for source in args.source_to_hide:
            if not source.is_file():
                raise RuntimeError("import source to hide not found: " + str(source))
            if source.resolve() in (args.tone_state.resolve(), args.zero_state.resolve()):
                raise RuntimeError("cannot hide original .sta fixture")
            off = source.with_name(source.name + ".p045-offline")
            if off.exists():
                raise RuntimeError("preexisting offline path; refusing overwrite: " + str(off))
            source.rename(off)
            hidden.append((source, off))
        tasks = [
            ("tone_a", "tone", True, args.dial_position),
            ("zero_a", "zero", True, args.dial_position),
            ("tone_b", "tone", True, args.dial_position),
            ("zero_b", "zero", True, args.dial_position),
            ("tone_no_notes", "tone", False, args.dial_position),
        ]
        if args.bad_dial is not None:
            tasks.extend([
                ("tone_bad_dial", "tone", True, args.bad_dial),
                ("zero_bad_dial", "zero", True, args.bad_dial),
            ])
        with ThreadPoolExecutor(max_workers=3) as pool:
            results = list(pool.map(lambda t: test_one(args, *t), tasks))
        for item in results:
            print(json.dumps(item, sort_keys=True), flush=True)
        tone, zero, tone2, zero2, quiet = results[:5]
        if not (tone["channels"] == zero["channels"] == quiet["channels"] == 11 and
                tone["rate"] == zero["rate"] == quiet["rate"] == 48000 and
                tone["frames"] == zero["frames"] == quiet["frames"] and
                tone["frames"] == tone2["frames"] == zero2["frames"]):
            raise AssertionError("mismatched authentic MAME audio format/length")
        if not tone["nonzero"][1] or not tone["nonzero"][2]:
            raise AssertionError("tone did not play on both main Akai channels")
        if any(zero["nonzero"]) or any(quiet["nonzero"]):
            raise AssertionError("all-zero source / no MIDI negative must be silent")
        if tone["pcm_sha256"] == zero["pcm_sha256"]:
            raise AssertionError("imported sources sound identical: false source recall")
        if tone["pcm_sha256"] != tone2["pcm_sha256"] or zero["pcm_sha256"] != zero2["pcm_sha256"]:
            raise AssertionError("independent original-ROM restarts not deterministic")
        if args.bad_dial is not None:
            wrong_tone, wrong_zero = results[5:7]
            if wrong_tone["pcm_sha256"] != wrong_zero["pcm_sha256"] or not any(wrong_tone["nonzero"]):
                raise AssertionError("known original wrong-wheel mutant not reproduced")
        (args.outdir / "results.json").write_text(json.dumps(
            dict(results=results, hidden_sources=len(hidden), status="PASS"),
            indent=2), encoding="utf-8")
        print("P045_OWNER_ORIGINAL_ROM_SOURCE_CAUSAL_RECALL=PASS", flush=True)
        print("SCOPE=NATIVE_LINUX_MAME_ONLY; ORIGINAL_WINDOWS_VST3_DAW=NOT_RUN",
              flush=True)
    finally:
        for original, offline in reversed(hidden):
            offline.rename(original)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("P045_SOURCE_CAUSAL_RECALL=FAIL " + str(exc), file=sys.stderr)
        sys.exit(1)
