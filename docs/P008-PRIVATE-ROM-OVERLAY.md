# P008 — Build one privately ROM-sealed VST3 without exposing ROMs to GitHub Actions

## Reason for the design
A real original-MAME/JUCE Windows x64 VST3 was successfully linked in the
public **ROM-free** diagnostic build
[44823748936](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44823748936).
The artifact contains a 33,862,144-byte Windows VST3 module. Windows further
loaded that actual module and `GetPluginFactory` returned a non-null factory
in the [Windows-host smoke probe](https://github.com/challanger2000/125A-Akai-S3000XL-VST3/actions/runs/44828793299).
Neither check establishes full original-ROM boot or playable MIDI.

The previous private C++ embedding mechanism (`AKAI_PRIVATE_ROM_ARCHIVE`)
requires a **second full MAME+JUCE build on a Windows machine** with the ROM
ZIP. That is possible but unnecessarily costly and user-inconvenient.

## New, simpler private-assembly path
1. Compile the **ROM-free source** with `AKAI_RUNTIME_ROM_OVERLAY=1`. This
   adds read-only runtime support for an appended private ROM trailer, but
   still embeds **no proprietary ROM bytes in any public CI artifact**.
2. On the owner's offline machine, run
   `scripts/build/pack_private_rom_overlay.py` with the publicly built VST3
   artifact ZIP and the original user-provided V2.0 ZIP. The packer
   SHA256-verifies both firmware images BEFORE assembly.
3. It appends the **unmodified** firmware (524288 bytes) and display ROM
   (1472 bytes), then a 24-byte trailer `125AAKAIS3KV2ROM` followed by
   LE32 firmware/display lengths. Windows ignores this PE-file overlay when
   loading the DLL, so **no PE sections or original code are modified**.
4. On plugin boot `AkaiPrivateRomOverlay.h` resolves **its own DLL file**,
   not the DAW executable, validates trailer lengths and exact ROM SHA256
   (using JUCE SHA256), and exports original byte-identical ROMs only to
   this plugin instance's temporary MAME ROM sandbox. The **original MAME ROM
   loader** remains unchanged. The owner has only one `.vst3` bundle.
5. The existing per-instance temporary ROM sandbox is deleted on shutdown.
   The native host does not need external apps, manual ROM folder, separate
   86Box process, Python or a terminal: the local Python script is needed
   only ONCE during the developer's private packaging step.

## Private local packing example
```powershell
python scripts/build/pack_private_rom_overlay.py `
  --built-artifact "125A-Akai-ROMFREE-VST3.zip" `
  --rom-archive "S3000XL-ROMs-V2.0-geprueft(1).zip" `
  --out "125A-Akai-S3000XL-V2.0-PRIVATE-Windows-VST3.zip"
```

**Never commit, upload or publicly distribute the resulting ROM-inclusive
`.vst3` ZIP/DLL.** Public source and CI contain no protected ROM bytes.

## QA gates / no premature success claims
- The earlier ROM-free linked artifact is **not** overlay-aware. Never append
  ROMs to that earlier DLL and falsely label it a playable private build.
- The modified ROM-overlay-aware source requires its **own successful complete
  Windows VS2022 build and host load test**.
- The actual privately sealed DLL needs a Windows plugin host booting the
  original V2.0 firmware with no external ROM folder, a real LCD screen,
  genuine native sample recording, MIDI program/sample playback, multi-
  instance lifecycle and portable DAW project save/reload. These checks
  remain open until the resulting private build is actually tested.
- The isolated core CTests alone do not prove any of the above.
