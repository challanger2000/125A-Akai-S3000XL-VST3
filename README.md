# S3000XL-VST

**An independent, open-source VST3/AU instrument built around a headless MAME emulation of a classic 1990s 16-bit rack sampler.**

The plugin boots the original firmware inside your DAW: the front panel is rendered live from MAME's own hardware artwork, the buttons and the DATA ENTRY dial drive the real emulated key matrix, MIDI goes in and out through the emulated UART, and all ten of the sampler's outputs — the main stereo pair and the eight individual jacks — are streamed straight into your host at its own sample rate. Audio from the host can be **sampled**: the emulated A/D converter records it, in mono or stereo, through the machine's own firmware.

> **No ROMs are included.** The plugin is non-functional without firmware that you must supply yourself, from hardware you own. See [ROMs](#roms).

---

## Status

Working:

| Area | State |
|---|---|
| Panel + buttons | Rendered from the MAME layout; hit-test → emulated key matrix (C0–C7) |
| DATA ENTRY dial | Analog `IPT_DIAL`; click ±1, drag-to-turn, and a `data_entry` VST parameter |
| Audio out | **Ten outputs**: main stereo pair plus eight individual mono buses, `OUT 1`–`OUT 8` |
| **Sampling** | **Works, mono and stereo.** Audio from the host is recorded through the emulated A/D and the machine's own firmware |
| Recording level | 1:1 — a known level in is the same level in the stored sample, verified at the storage point |
| Input monitoring | The input passes through during a recording |
| MIDI | In/out via the emulated UART, responsive |
| Media | Floppy, CD-ROM and SCSI hard disk images; samples load cleanly |
| Parameters | 45 automatable VST parameters |
| Display | Live refresh (blinking cursor, sample-load progress, live waveform while recording) |

Known limitations and development work (125A branch):

* **125A development branch:** physical `REC GAIN` and `MAIN VOLUME` control paths are now coded
  (pre-A/D and Main L/R output respectively). This source change has **not** yet passed a native
  Windows VST3 build or firmware-backed/DAW test. Analogue potentiometer tapers are not yet calibrated.
  The original second knob is MAIN VOLUME, not REC VOLUME (Akai S3000XL manual, p.18).
* **125A branch P002:** pre-record monitor signal routing is implemented behind an original-firmware
  `0x00C0` WADCS latch; the port-bit interpretation is provisional pending direct V2.0
  MON/Moff firmware tests. This is **not a completed or validated MON implementation**.
  See [P002 evidence and test contract](docs/P002-FIRMWARE-MONITOR.md).
* Sample **loading** is slower than the hardware, a known cost of getting the transfer rate right.

Open items and the measurements behind every decision are tracked in [`STATE.json`](STATE.json).

---

## ROMs

You need to provide the firmware yourself and place it here:

```
Documents/S3000XL/ROMs/s3000xl/
```

Required:

* the sampler's main firmware ROM
* the LCD character-generator device ROM (`hd61830.bin`)

**The LCD character-generator ROM is easy to miss.** Without it MAME silently refuses to start the machine and the plugin shows a black panel rather than an error. If the panel is black, check this first.

A quick way to verify your set is complete is to run a standalone MAME with the same ROM path and confirm `-verifyroms` reports *best available*.

---

## Build

Prerequisites: MSYS2 (MinGW64) for GENie, Visual Studio 2022 Build Tools, and JUCE.

1. **Patch MAME.** Apply the patches in this repository to a stock **MAME 0.288** tree:

   ```
   git apply mame-0288-s3000xl-vst.patch
   ```

   This carries everything the plugin needs from the emulation side: the sampler's A/D input and
   record path (which upstream MAME does not have at all), the individual-output routing, two genuine
   bug fixes in the shared sound-DSP device, and the small changes that let MAME run headless inside a
   plugin. See [Fixes contributed](#fixes-contributed-back-to-the-emulation).

2. **Build the MAME libraries** for the sampler subtarget: `run_genie_s3000xl.bat`, then `run_msbuild_mame_s3000xl.bat`.

3. **Build the plugin**: `run_msbuild_vst3_s3000.bat`.

Build notes, toolset pinning and the traps worth knowing are documented in [`MAME-to-VSTi-RECIPE.md`](MAME-to-VSTi-RECIPE.md) — a generalized recipe for wrapping *any* MAME machine as a VST3/AU instrument, written from two complete ports.

---

## Fixes contributed back to the emulation

Building this plugin turned up three genuine defects in MAME's L7A1045 L6028 sound-DSP emulation. The
same chip is used across a whole family of samplers and drum machines of the era, so none of them is
specific to this model.

**Stereo samples played on one side only.** Register tracing showed that the firmware programs a stereo
sample as a *voice pair* — the hard-left half on voice `ch ^ 2`, the hard-right half on voice `ch` —
and then issues a **single** key-on with `ch` selected. Real hardware starts both halves of the pair;
the emulation started only the selected voice, leaving the left half silent. The fix starts the partner
voice too, but only when the two form a complementary hard-panned pair, so mono playback is untouched.

**The DMA request timer ran twelve times too fast.** The device asked the CPU for a sample-memory word
every 64 clock ticks where the sample period is 768, so during a recording the machine stored twelve
times as much data as arrived — measured at the storage point: 529 200 words written in the second the
converter delivered 44 100. The comment beside it called 64 "8x the sample period"; that eight is the
*output* oversampling of the D/A, a playback property, used to pace an *input* transfer. This path is
only exercised by a machine that records, which is why it had not been noticed.

**The DMA write pointer never wrapped.** Measured: 44 100 words a second written into a 128-word
region, 99.7 % of them past its end, overrunning by 43 973 words — 88 KB written beyond a 256-byte
buffer, which quietly destroyed whatever else was in sample memory. It now wraps at the region the
firmware itself programs.

A fourth change is a judgement call rather than a clear bug, and is marked as such in the patch: the
stereo voice-pair start was guarded by "don't restart a voice that is already sounding", and this
machine never clears a key bit — its voices loop rather than end — so the guard admitted the pair once
and refused it for the rest of the session. The guard is removed here. On a machine that *does* clear
key bits the original guard may well be right, so this one is worth testing elsewhere before it goes
anywhere near upstream.

---

## Licence

**GPLv3.** MAME is licensed GPL-2.0-or-later and JUCE is used under its GPLv3 terms; the combined work is distributed under the GPLv3. See [`LICENSE`](LICENSE).

Modifications to MAME are shipped as patches in this repository, as the GPL requires.

## Trademarks

This project is **not affiliated with, endorsed by, or sponsored by** any hardware manufacturer. Product and company names are the trademarks of their respective owners and are used only to identify the hardware being emulated. The manufacturer name visible on the on-screen panel is part of MAME's own hardware artwork.
