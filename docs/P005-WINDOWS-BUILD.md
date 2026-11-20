# P005 — Reproducible Windows VST3 build

## Goal
Compile a true Windows x64 VST3 from the original MAME 0.288 S3000XL core
and JUCE, using free public-repository GitHub Actions standard runners.
All proprietary ROMs remain **only in the user's private build**.

## Original source baseline findings
- The upstream Projucer file references a private `F:\\MPC3000vsti` tree,
  a private JUCE install, two old Ensoniq panel headers, four missing PNG
  label overlays, and an embedded MAME Lua-plugin ZIP absent from the repository.
- Simply running the .bat scripts or original .jucer in CI cannot build the
  actual VST3. The complete Windows toolchain and source paths must be supplied.
- The four SD-1 overlays are **not** needed for the original Akai MAME layout:
  the original S3000XL artwork is rendered by MAME. We removed the four
  missing optional SD-1 PNG references rather than faking/copying image art.
- MEDIA floppy/cart icons are now small original SVGs, generated at build
  time; the required MAME `plugins/layout` code comes from the official MAME
  0.288 checkout. The build validates ZIP contents and includes no ROMs.

## New source-based build
1. [`windows-full-vst3.yml`](../.github/workflows/windows-full-vst3.yml)
   fetches `mamedev/mame` tag `mame0288` and `juce-framework/JUCE`
   tag `8.0.12`. Both upstreams are public GitHub sources.
2. Apply this project's MAME DSP/ADC/HD61830 patch and fail if it does not
   apply cleanly. No editing inside the main public GitHub repository.
3. Generate MAME `mames3000xl.sln` with MSYS2/GENie, compile Release/x64
   static libraries via VS2022 x64-hosted MSBuild.
4. Configure `CMakeLists.txt` with paths to MAME and JUCE. It generates
   `mame_plugins.zip` from official Lua layout scripts and two lightweight
   SVG icons. BinaryData contains **no proprietary firmware**.
5. Link the actual JUCE Windows VST3 with original MAME libraries. Require
   a `.vst3` binary inside the bundle before uploading a diagnostic
   ROM-free artifact to public GitHub Actions (retention 7 days).
6. If any compile or link stage fails, the job fails; no dummy VST3 or
   partially built placeholder may be considered complete.

## Known outstanding gates
- **No successful full VST3 build is claimed in P005 until an actual
  GitHub Actions run passes.** CMake and workflow source creation alone are
  not proof of compilation. CI errors must be repaired.
- The public build has **no private firmware ROMs**. A separate offline
  private build embeds the two user-supplied verified ROMs; neither their
  bytes nor binary-embedding artifacts may enter public repository, Actions,
  releases or test logs.
- With firmware locally installed, test real S3000XL V2.0 boot, LCD text vs.
  graphics mode, REC/ARM/GO end-to-end, MON, WAV import, native MIDI
  sample/program mapping, 10 output buses, multiple instances and DAW recall.
- The user's Fender Studio Pro practical test is the final acceptance gate.
- Preserve plugin GPLv3 notices and MAME/JUCE applicable licenses and
  attribution. ROM-inclusive private builds must not be redistributed.

## CI constraints and operational notes
- The full MAME solution is sizable. Job timeout is 180 minutes, with
  `/m:2` and original x64-hosted toolchain to avoid MAME precompiled header
  memory failures; source clones are shallow and tag-pinned.
- The main public branch `main` retains the original upstream import;
  implementation occurs on `develop/v1.0.0`.
- Never use an intermediate CI success from isolated CTests as a VST3
  release acceptance.
