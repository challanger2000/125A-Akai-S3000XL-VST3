# S3000XL / MPC3000 VST3 — project rules

> **Read this before touching anything here.** Thread-level context is `F:\MIZU\CLAUDE.md`; the living
> state with measurements is `F:\MPC3000vsti\S3000_Port\STATE.json`. This file is the *operational*
> layer: how to build, what is settled, and which mistakes have already been paid for.
>
> Nothing in here is style preference. Every rule below cost at least one wasted round.

---

## What this is

Two VST3 instruments built by embedding **headless MAME 0.288** (Sojus SD-1 host pattern).
0.287 was abandoned — do not reference it.

- **MPC3K-VST** (Akai MPC3000) — public on GitHub, v1.0 with binary.
- **S3000XL-VST** (Akai S3000XL) — the active work.

**This is a VST3 port, not an upstream MAME contribution.** Thor, 2026-08-30: *"ez a project nem MAME
fix, brutálisan is belenyúlhatsz, itt VST3 portolás folyik."* Cross-layer solutions are allowed when
they model what the hardware does. Upstream cleanliness is not a constraint here.

But note which parts are ours and which are shared:

| file | who exercises it |
|---|---|
| `src/mame/akai/s3000.cpp` | ours |
| `s3000_adc_device`, the MICROPHONE wiring, the whole record path | **ours — upstream MAME has NO input ADC at all** |
| `src/devices/sound/l7a1045_l6028_dsp_a.cpp` | mpc2000, mpc3000, s3000, SNK Hyper NeoGeo 64. **The HNG64 loads samples and never records, so this chip's DMA-IN path is exercised by nobody but us** — which is why SAMPLING 87 found a real upstream defect there |
| `src/devices/machine/am9517a.cpp` | fmtowns, apxen, s900, future32, an Acorn podule — **broadly exercised and mature.** Everything we have put in it is logging. A fault of ours is unlikely to live here |

Thor's method for that table, worth reusing: **before suspecting a module, ask who else runs it.**

---

## Build

```
F:\MPC3000vsti\S3000_Port\run_msbuild_mame_s3000xl.bat     MAME
F:\MPC3000vsti\S3000_Port\run_msbuild_vst3_s3000.bat       plugin
```

- **Two separate commands, NEVER on one line** — the second is silently skipped and still returns 0.
- **Check the artefact TIMESTAMP, not the exit code.** Reporting "built" from an exit code while the
  link had failed on a locked DLL cost a whole round, and Thor was testing the previous build.
- **VSTHost must be EXITED before a link, not merely have the plugin window closed.** It holds the
  DLL; the link fails `LNK1104` while MAME reports success. This has misled us at least five times.
- The plugin log is `%TEMP%\s3000xl-vst.log`. `logerror()` reaches it because the plugin installs a
  log sink; `osd_output` does **not** carry `logerror` — that mistake produced a zero-byte file.

Source lives in `S3000_Port\Source`; the build tree copy is `build_plugin_s3000\Source`. **Edit
`S3000_Port` and sync**, or the build silently uses the old file.

---

## Settled — do not re-open

- **The 12x store rate** (SAMPLING 87). `l7a1045`'s DMA-request timer used `from_ticks(64, clock())`
  where the sample period is 768 ticks. The "8x" in its comment is the **output** oversampling — a
  playback property used to pace an **input** transfer. Fixed to 768.
  **Known cost: sample loading is 12x slower.**
- **The memory corruption** (golden v3). The sound chip's DMA write pointer never wrapped — measured
  44100 words a second into a 128-word region, 99.7 % past its end. It now wraps at the region the
  firmware itself programs.
- **The capture level is 1:1.** Generator -3.67 dB in, `SAMPLE-STORE PEAK 21475 = -3.67 dBFS` stored.
  (An earlier entry here claimed the playback was ~12 dB low. Thor struck it on 2026-09-13: that
  defect does not exist. It was my reading, not his measurement.)
- **The input path is good on BOTH channels.** With a stereo signal in: `SAMPLER-IN PEAK-L=100.0%
  PEAK-R=100.0%`, `ADC-PEAK LEFT 32766 RIGHT 32766`. The MICROPHONE wiring and the FL/FR mapping are
  correct. "The right channel never arrives" is falsified.
- **"Two words per frame" is CLOSED.** Seven builds tried it (111, 112, 119, 122, 126, 128, and a
  reapplication) and all seven were reverted. The decisive argument is not a judgement about a sound:
  **the machine allocates a 44112-word region for one second of stereo — the same size as mono — and
  its own display says 1.00 s = 88200 BYTES = 44100 words.** Twice the data into a region sized for
  half is the octave.
- **The A/D transfer is mode-independent.** DMA channel 2 is armed exactly once per recording, mode
  55, addr 0091B0, count 0FFF, autoinit — identically in mono and stereo.
- **The A/D ring is ONE linear buffer.** The firmware's position routine at `0271B0` subtracts 0x91B0
  and takes it **modulo 0x2000**. `0091B0` and `00A1B0` are two block boundaries inside it, not two
  channel areas. `00A1B0` appears only in stereo because more data is pending at the end.
- **THE DMA REQUEST LINE MUST NEVER BE PACED OR WITHHELD.** Six attempts (57, 60, 61, 68, 91, 96),
  every one crashed, froze or blanked the input meter. **A bound belongs on the ADDRESS**, where
  nothing can stall on it.

---

## Known-good reference points

```
golden v2   mame@2bb825d8   the 12x fixed
golden v3   mame@e5ff299f   the wrap fixed
            mame@8570d4cb   mono recording FLAWLESS (probes off) — bundle 07:50:06, 2026-08-30
            mame@cf610a94   current baseline, mono flawless — bundle 20:26:53, 2026-08-30
```

The mono audio path is **functionally identical to golden v3**: everything since is logging plus
`m_dma_words_per_frame`, whose default of 1 gives the same 768-tick timer. If mono misbehaves, suspect
instrumentation cost before suspecting the model.

---

## Firmware facts (measured from RUNNING code — the OS copies itself into RAM, the ROM image does not contain it)

```
$00C0          i8255 port A, the A/D control latch
  bit 3        RECORD STROBE   brackets the A/D activity exactly
  bit 4        STEREO          the only mode difference anywhere
  bit 1        set in EVERY value the firmware writes — selects nothing (SAMPLING 27's reading, retracted)

L7A1045 control word
  bit 3        set only in stereo: MONO writes CTRL 0150, STEREO writes CTRL 0158, same channels
  bit 6        CONTROL_DMA_START
  bit 8        CONTROL_KEY_ON

A/D ring       $0091B0, 4096 words, armed by the firmware itself at GO, autoinit
$0271B0        position poll, 223870x/s, all arithmetic in BYTES
$0270C6        rep movsw from the ring — copies nothing during a recording (DX=0); the waveform display

the block transfer, decoded from live memory
  260DE        push ds / jcxz end / cmp cx,1 / [7776]=SI / [7778]=CX / call 26177 / si+=2*cx / cx=0
               NO LOOP — one block per call, the decision is one level up
  26177        mov bh,4F / mov bl,49 (this is the "bx 4F49" in the arm log) / out 80,82,84,86
               destination taken from [7772] and [7774], packed by ROR 4 and masked F000/0FFF/001F

the firmware's own bookkeeping
  [730C]       mode flag: if zero, the transfer count is HALVED and the source shifted by 0x800
  [7339]       -L sample END      [733D] -L write pointer      [7341] -L remainder
               all three advanced after every block, from the routine at 0129D8
  [7912:7914]  -R sample BASE — written ONCE at pc=013F8C and never advanced by anything
  [7772/7774]  destination handed to the arm routine    [7776] source    [7778] count
```

---

## GOLD — 2026-09-13, `mame@184c1fe6`, tag `S3000XL-GOLD-2026-09-13`, bundle 06:39:16

Thor's verdict: the plugin runs perfectly. Mono recording flawless; **stereo recording works** —
88200 words/s in, 44100 to one destination and 44099 to the other, **both halves measured**; capture
level 1:1 at the storage point, and the machine's **own** input meter (firmware, not ours) tracks the
input, which is independent confirmation; playback voice pair fixed; memory corruption solved; the
12x store rate solved.

**Monitoring — Thor's own framing, 2026-09-13, and it separates two things I had merged into one:**

| | state |
|---|---|
| the **MON button** — monitoring the input BEFORE a recording starts | **not implemented** |
| the input passing through **DURING** a recording | works, but **independently of the MON button** - it passes with the button on or off - and it only appears **after one stereo recording has been made** |

So the old "the monitor needs one stereo recording first" observation is **still open**. I retired it
prematurely on the strength of an ambiguous report; Thor's clarification narrows it instead of closing
it. What he sees meanwhile is the firmware switching the incoming signal on and off on the machine's
own input level meter.

**How stereo was closed.** Control bit 3 tells the chip the transfer is stereo, and the second
destination is the firmware's own `[7914:7912]` taken **unshifted**. The arm routine at `26177` packs
it into the chip's 48-bit register layout with `ROR 4` — reassembled, `a[23:0] = D[23:0]`, so the
firmware's value IS `voice.start`. Cross-checked against data already on the record: for the -L
transfer `BLOCKREQ` measured `[7772]=0400 [7774]=0002` and `DMA-BURST`, which prints
`m_voice[0].start`, reported `dest 020400` for that same transfer.

Three pieces, none of which works without the others: the right word handed over, store headroom
between the strobe edges, and the bit-3 split with the firmware's own second base. Seven earlier
builds failed because one or two were present without the rest, or because the address was guessed.

**Meters off at gold:** `S3000_DSP_METERS`, `S3000_AD_METERS`, `S3000_FW_PROBES`, `S3000_HOT_PROBES`
all 0. The tap on `[7912:7914]` **stays installed** — it is mechanism, not measurement.

### MULTI OUT — DONE, 2026-09-13

**OUT 1-8 → DAW 1-8, and the stuttering is gone.** Eight mono buses plus the main stereo pair, ten in
all: Thor's call, and his reason is the right one — sending drums out separately would be odd with the
kick on the right of one stereo channel and the open hat on the left.

**The jack order was MEASURED end to end, not derived.** Thor stepped `indiv:` through 1..8 and read
where each came out. The machine's OUT n arrives on `L6028_OUT [4,6,5,7,0,2,1,3]`, so those route to
indiv channels 0..7. **If `channel_remap` or the firmware's `dest` numbering ever changes, RE-MEASURE
with the same eight-step test** rather than reasoning about it — the mapping is the composition of two
tables and neither alone predicts it.

**Routing, settled by three independent sources:** anything sent to an individual output is **also**
summed to the stereo pair. The machine's own program page shows `stereo level: 99` and `indiv: 8
lev:50` as two live parameters; the device models exactly that (`l_volume`/`r_volume` for the pair,
`send_level` for the send); and a Sound On Sound post from someone who knows the machine: *"Anything
assigned to the individual outputs are summed and always appear at the stereo outputs as well. It has
ever been thus since the S900/1000."* Removing a sound from the main is the user's choice of stereo
level, not a routing mode. Thor expected the opposite and the evidence corrected us both.

**The mix channel map is derived at runtime**, by walking `speaker_device_enumerator` and matching the
speaker tag, exactly as `emu/sound.cpp` lays the record buffer out. The old hardcoded *"stride 3,
ch0 = floppy"* was a real measurement that **silently expired** the moment a speaker was added.
Measured after: `MIXMAP stride=11 main=1 indiv=3`.

**The floppy speaker cannot be removed from our side.** `enable_sound(false)` only silences the
device; `floppy_image_device::device_add_mconfig` creates the speaker unconditionally, so the stride
is 11 and not the 10 I expected. Removing it would mean editing `floppy.cpp`, which hundreds of
drivers use. It stays as one silent channel — and deriving the map is what caught it, where a
hardcoded stride would have shifted every channel.

**THE STUTTER WAS THE PROBES — the FIFTH time.** I had a plausible and wrong theory, that the mix
stride going from 3 to 11 was the cost. Had I acted on it, the next round would have gone on making
the `indiv` speaker optional: solving a problem that did not exist. Removing 64 `logerror` calls
across the three MAME files and the plugin's per-frame peak meter fixed it.

**REC GAIN / REC VOLUME** are unmodelled (Hermes brief, 2026-08-29, no reply). Thor, 2026-09-13:
*"a rec gain az utolso ami erdekes a sorban... gyakorlatilag a rec gain es a tokeletes ADC emulacio az
hab a tortan mivel a rec gain az a hostbol is allithato."* So: real, wanted, and not urgent.

**THE ~12 dB DOES NOT EXIST.** Thor, 2026-09-13: *"a -12db hiba nem letezik, azzal nem kell
foglalkozni."* It was my reading, carried in this file as an open item, and it is struck. Do not
re-open it and do not attach it to REC GAIN.

---

## The problem this replaced, kept for the record

## The open problem, in one sentence

**A stereo recording writes the whole stream into one sample and one 2048-word block into the other.**

What is excluded, by measurement: the word rate · the word order · the store ceiling · the destination
half · a voice split · the ring halves · a second DMA path · the input path · the playback pair · the
key bit · the wrap.

**What is proved and should not be re-tested: the bit-3 split MECHANISM works.** With it in place the
-L region received 44100 words a second out of the 88200 arriving — half really does go elsewhere —
and the stereo waveform drawing came into sync for the first time. **Only the second ADDRESS was
wrong**: `[7912:7914]` is not a plain byte address, and a plain `>>1` was a guess. Derive it through
the same ROR-4 packing the arm routine uses.

**The strongest unexplained clue, and it is Thor's:** the monitor is silent until ONE stereo recording
has been made, and from then on it is audible in MONO recordings too. Something a stereo pass sets up
persists, and the mono path never sets it up alone.

**Fixed and kept:** the stereo playback pair (SAMPLING 132). The `!BIT(m_key, partner)` guard in
`l7a1045` key_on tested a behaviour the S3000XL does not have — nothing here ever clears a key bit, so
it admitted the partner once and refused it for the rest of the session.

---

## Instrument rules — all of these were paid for in this project

- **A budget shared with a bulk writer is spent on the bulk writer.** This happened **nine times**
  here. Symptom: every line in the log is from the boot and the event produced none.
  **Do not ration — TRIGGER.** Arm on the record strobe, and extend past its fall when the event is
  part of the closing flush.
- **AN INSTRUMENT ON AN ALWAYS-RUNNING PATH IS ITSELF AN INTERVENTION.** An `install_write_tap` over
  `0x0000-0x00ff` calling `machine().time()` on every I/O write, and a read tap taking `pc()` across
  8 KB of hot RAM, together slowed the emulation enough that **Thor heard it** — a click at the start
  of a recording, and once a recording chopped into segments. Per-access diagnostics must be
  compile-time gated by default (`S3000_HOT_PROBES`, `S3000_SAMPLING_DIAG`).
- **AN INSTRUMENT WHOSE READING IS FIXED BY ITS OWN CONSTRUCTION REPORTS NOTHING.** "44100 words a
  second, exactly" was the ceiling our own DREQ logic imposed; the same number would have appeared if
  the machine wanted 88200. Before believing a measurement, ask **what reading would have been
  impossible**.
- **The dedup key must match the question.** A control-word probe deduped on the value alone, globally
  — the machine writes the same word at every recording, so the whole run produced one line and the
  stereo recording produced none.
- **The probe must report what the code DOES, not what it used to.** A condition was removed from a
  branch and left in the log line beside it, so the log printed "no" for cases the code took.
- **A filter that cannot reject is not a filter.** When picking a return address off the stack, test
  it mechanically — `byte[-3]==E8` for a near call, `byte[-5]==9A` for a far one — and dump the raw
  stack rather than computing a caller from one assumed frame depth.
- **Verify the probe's numbers against something outside it.** A candidate return address looked
  convincing because the code at it wrote unmapped ports; the log then showed those ports are only
  written at boot, never during the recording that sampled the stack.

---

## Method rules

- **ASK "WHEN DID IT LAST WORK" FIRST.** Thor said for days that stereo had once worked; `git log -S`
  found the commit that removed it in seconds, after hours spent investigating the firmware instead.
  A regression is found in the history, not in the machine.
- **REVERT-GUESS.** Grounded in facts and real hardware → keep. Guess → revert, same round.
- **THE FIRMWARE IS SCRIPTURE.** If a value looks wrong, our reading is wrong. Confirmed here: the
  firmware treats both channels as exact equals — the `-R` block code is byte-for-byte the `-L` code
  with a different pointer and ring half.
- **A P-RAM statikusan nem mérhető, csak futó kódból.** The OS copies itself into RAM and runs there.
  Every routine address in this file came from a live dump, not from the ROM image.
- **The repo is the source of truth.** Commit every round with the measurements in the message. Long
  messages go in a file and use `-F`; PowerShell mangles quotes, parentheses and `->` on the command
  line.
- **No MIDI injection for audio tests.** Thor tests with a real keyboard.
- **Never push ROMs or `fw.bin` to any remote.** The public repo is an orphan `public` branch.

---

## Still open, lower priority

- `REC GAIN` / `REC VOLUME` are unmodelled — handed to Hermes via `F:\MIZU\COMM A2A channel`
  (*Claude to Hermes - S3000XL AK5339 input frontend.md*, 2026-08-29, **no reply yet**). Wanted, not
  urgent; REC GAIN is adjustable from the host in the meantime.
- Pre-record monitoring (the MON button) is not implemented, and the pass-through during a recording
  appears only after one stereo recording — see the GOLD section.
- `$00E0`, `$00E2`, `$00E4` are **unmapped** and written ~200 times per run, with a readback of `$00E4`
  at boot that returns 0 in our model. Per the project rule, an unmapped register is not neutral. May
  be related to the machine occasionally not seeing the HDD on the first load.
- Two **run-dependent** symptoms remain — an occasional click and occasional crackling at the start of
  a recording, with identical code. They may share a cause.
- Sample loading is 12x slower (the known cost of the 768-tick timer).
- Gate or remove all diagnostics before a public release, especially those in shared MAME code.
