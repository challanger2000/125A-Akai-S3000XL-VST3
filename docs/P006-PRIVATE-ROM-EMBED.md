# P006 — Private original Akai S3000XL V2.0 firmware embedded in one VST3

## Scope and identity
The owner deliberately keeps this **GitHub source PUBLIC for free/cheaper GitHub
Actions**. A **different, private Windows VST3** must contain the original
firmware and LCD ROM directly. A public repository is not permission to publish
proprietary ROMs. Never change public repository visibility as a workaround.

Original owner-supplied file, verified locally:
- `S3000XL-ROMs-V2.0-geprueft(1).zip`
- `s3000xl_v2_0.bin`, 524288 bytes, SHA256
  `67917482d2a26163d8aee4de676ecbce4fd42859a3f6223c5442426998a086c5`
- `hd61830.bin`, 1472 bytes, SHA256
  `8738511c6a2f02b0ef1be2cde90898458b4517151cb473ff28ae212dbd1c1737`

## Implementation (private build only)
- `scripts/build/embed_private_roms.py` reads the owner's ZIP **locally** and
  checks uniqueness, exact length and SHA256 of both inputs. The script contains
  only validation logic and expected digests, **never any firmware bytes**.
- With `-DAKAI_PRIVATE_ROM_ARCHIVE=...` set, CMake invokes the script and generates
  `build/windows/Generated/PrivateRoms/AkaiPrivateRomData.h` and
  `AkaiPrivateRomData.cpp`. The generated `.cpp` embeds the two byte arrays
  into the resulting private VST3 module; it must not be committed, uploaded,
  shared, logged or placed in a public GitHub Actions artifact.
- On a private plugin instance's boot, `prepareEmbeddedRoms()` writes the
  *unmodified* ROMs to a uniquely named temporary instance sandbox. The
  **original MAME ROM loader** continues to read them from the `-rompath`
  supplied by the plugin. There are no user-facing ROM setup steps.
- ROM byte content is verified against the compiled original before trusting
  a cached temporary copy. On shutdown (after the emulation thread joins) the
  entire instance sandbox, including the temporary ROM copies, is deleted.
- The **public ROM-free** Windows build does not define
  `AKAI_EMBED_PRIVATE_ROMS`; it retains its manual ROM checks only for
  diagnostics. No user ROM bytes enter the public workflow.

## Building the private VST3 locally
Requires patched original MAME 0.288 Release/x64 static libraries and
JUCE 8.0.12, both constructed with Visual Studio 2022:

```powershell
cmake -S . -B build/windows-private -G "Visual Studio 17 2022" -A x64 `
  -DAKAI_MAME_ROOT="C:/125A/mame0288" -DAKAI_JUCE_ROOT="C:/125A/JUCE-8.0.12" `
  -DAKAI_PRIVATE_ROM_ARCHIVE="C:/Users/<owner>/Downloads/S3000XL-ROMs-V2.0-geprueft(1).zip"
cmake --build build/windows-private --config Release --target AkaiS3000XL_VST3
```

`build/windows-private/Generated/PrivateRoms` **contains protected ROM source
and must not be uploaded or sent to a public service**.

## Status and outstanding tests
**Source implementation only.** This does not establish that the actual
Windows VST3 compiles, the firmware boots, the LCD functions, the ROM files
load with MAME 0.288, or that MIDI/audible sampling works. Required checks:
- Confirm the two-byte-array script against the original ZIP (strict SHA256).
- Build and link the actual private MSVC VST3.
- Launch it without Documents/S3000XL/ROMs, verify firmware V2.0 and live LCD.
- Verify the embedded ROM bytes were identical to owner-uploaded originals.
- Verify double/multiple instance boot and sandbox cleanup after plugin exit.
- Verify WAV recording, MON, Main/individual output semantics and real project
  recall. Do not accept without direct firmware-generated audio and MIDI tests.

**No private original ROM contents or derived binary data are committed to
this public repository.**
