#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""P046: fail-closed source contract for removal of external WAV file import.

ROM-free. Does NOT claim native Akai firmware, Studio Pro, or Windows-VST3
runtime acceptance. Original Akai sampling controls, host MIDI and image
mounting must remain available.
"""
from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EDITOR_CPP = ROOT / "Source/PluginEditor.cpp"
EDITOR_H = ROOT / "Source/PluginEditor.h"
PROCESSOR_CPP = ROOT / "Source/PluginProcessor.cpp"
PROCESSOR_H = ROOT / "Source/PluginProcessor.h"

FORBIDDEN_EDITOR = (
    r"importWavFile\s*\(",
    r"stageWavForOriginalSds\s*\(",
    r"isInterestedInFileDrag\s*\(",
    r"filesDropped\s*\(",
    r"FileDragAndDropTarget",
    r"\*\.wav",
    r"Import\s+WAV",
    r"Resume automatic WAV recording",
    r"Cancel automatic WAV recording",
    r"Unload staged WAV",
    r"\bresult\s*==\s*(?:20|21|22|23)\b",
    r"\bm\.addItem\s*\(\s*(?:20|21|22|23)\b",
    r"\bautoCaptureRequest\.store\s*\(\s*true",
)
FORBIDDEN_PROCESSOR = (
    r"\bstageWavForCapture\s*\(",
    r"\bstageWavForOriginalSds\s*\(",
    r"\boriginalSdsRequested\.store\s*\(\s*true",
    r"\bautoCaptureRequest\.store\s*\(\s*true",
)
REQUIRED_MEDIA = (
    'Insert Floppy Image',
    'Eject Floppy',
    'Insert CD-ROM Image',
    'Eject CD-ROM',
    'Set Hard Disk Image',
    'Remove Hard Disk',
)


def check(editor_cpp: str, editor_h: str, processor_cpp: str,
          processor_h: str) -> None:
    editor = editor_cpp + "\n" + editor_h
    processor = processor_cpp + "\n" + processor_h
    for pattern in FORBIDDEN_EDITOR:
        if re.search(pattern, editor, re.IGNORECASE):
            raise AssertionError("WAV-import editor entry point remains: " + pattern)
    for pattern in FORBIDDEN_PROCESSOR:
        if re.search(pattern, processor):
            raise AssertionError("WAV file import API/producer remains: " + pattern)
    for menu in REQUIRED_MEDIA:
        if menu not in editor_cpp:
            raise AssertionError("Original Akai media access disappeared: " + menu)
    if 'loadMediaButton.setButtonText("MEDIA")' not in editor_cpp:
        raise AssertionError("MEDIA button no longer has neutral text")
    if 'nativeRestore.publish(' not in processor_cpp:
        raise AssertionError("Machine-state restore was deleted")
    if 'pushMidiByte(' not in processor_cpp:
        raise AssertionError("Original MIDI port logic was deleted")
    if 'pushAudioToMame(' not in processor_cpp:
        raise AssertionError("Original native ADC/audio input was deleted")
    # P047: a stray // may comment out the C++ function definition while
    # leaving its name in the source. Require a live line-start declaration.
    if not re.search(
        r"(?m)^[ \\t]*int\\s+EnsoniqSD1AudioProcessor::pullAudioForMame\\s*\\(",
        processor_cpp,
    ):
        raise AssertionError("Native Akai audio pull definition is absent/commented")


def main() -> None:
    sources = [p.read_text(encoding="utf-8") for p in
               (EDITOR_CPP, EDITOR_H, PROCESSOR_CPP, PROCESSOR_H)]
    check(*sources)

    # The oracle must demonstrably catch reintroduced WAV imports.
    editor_cpp, editor_h, proc_cpp, proc_h = sources
    mutants = (
        (editor_cpp + '\nm.addItem(20, "Import WAV");\n',
         editor_h, proc_cpp, proc_h),
        (editor_cpp, editor_h +
         '\nvoid filesDropped(const juce::StringArray&);\n',
         proc_cpp, proc_h),
        (editor_cpp, editor_h, proc_cpp, proc_h +
         '\nbool stageWavForCapture(const juce::File&, juce::String&);\n'),
        (editor_cpp, editor_h, proc_cpp +
         '\nautoCaptureRequest.store(true);\n', proc_h),
        (editor_cpp.replace("Eject CD-ROM", "Eject removed CD"),
         editor_h, proc_cpp, proc_h),
        # The genuine P046 Windows C2447: an accidental comment marker
        # before the otherwise intact pullAudioForMame() implementation.
        (editor_cpp, editor_h, proc_cpp.replace(
            "int EnsoniqSD1AudioProcessor::pullAudioForMame(",
            "// int EnsoniqSD1AudioProcessor::pullAudioForMame("
        ), proc_h),
    )
    for n, variant in enumerate(mutants, 1):
        try:
            check(*variant)
        except AssertionError:
            continue
        raise AssertionError(f"Negative mutation {n} escaped P046 guard")
    print("P046_NO_EXTERNAL_WAV_IMPORT=PASS; mutant_controls=6; "
          "MEDIA_FLOPPY_CD_SCSI=PASS; NATIVE_AUDIO_MIDI_STATE_PRESERVED=PASS")


if __name__ == "__main__":
    main()
