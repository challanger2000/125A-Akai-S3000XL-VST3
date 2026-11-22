#!/usr/bin/env python3
"""P048 fail-closed integration contract: tested rack-ear geometry must
actually be wired to both JUCE buttons, not merely exist as an unused header.
No MAME, no ROM, no audio or DAW host needed.
"""
from pathlib import Path
import re

EDITOR = Path(__file__).resolve().parents[1] / "Source/PluginEditor.cpp"


def check(editor: str) -> None:
    if '#include "AkaiPanelOverlayLayout.h"' not in editor:
        raise AssertionError("Rack layout helper not included by production editor")
    match = re.search(
        r"void\s+EnsoniqSD1AudioProcessorEditor::resized\s*\(\)\s*\{"
        r"(?P<body>.*?)"
        r"\n\}\s*\n",
        editor, re.S,
    )
    if match is None:
        raise AssertionError("Cannot find complete JUCE editor resized function")
    body = match.group("body")
    required = (
        r"rackEarControls\s*\(\s*getWidth\s*\(\s*\)\s*,"
        r"\s*getHeight\s*\(\s*\)\s*\)",
        r"loadMediaButton\.setBounds\s*\(\s*media\.x\s*,\s*media\.y"
        r"\s*,\s*media\.w\s*,\s*media\.h\s*\)",
        r"settingsButton\.setBounds\s*\(\s*setup\.x\s*,\s*setup\.y"
        r"\s*,\s*setup\.w\s*,\s*setup\.h\s*\)",
    )
    for pattern in required:
        if not re.search(pattern, body):
            raise AssertionError("GUI did not bind normalized geometry: " + pattern)
    for old in ("loadBtnX", "setBtnX", "loadBtnY", "setBtnY", "btnWidth", "btnHeight"):
        if old in body:
            raise AssertionError("Retired view-dependent overlay placement remains: " + old)
    for button in ("loadMediaButton", "settingsButton"):
        actual = re.findall(r"\b" + button + r"\.setBounds\s*\(", body)
        if len(actual) != 1:
            raise AssertionError(f"{button}: must have one viewport-anchored setBounds, got {len(actual)}")
    if 'settingsButton.setButtonText("SETUP")' not in editor:
        raise AssertionError("Short setup label not applied to narrow rack ear")
    if 'loadMediaButton.setTooltip(' not in editor or 'settingsButton.setTooltip(' not in editor:
        raise AssertionError("Small rack-ear controls must retain legible tooltips")


def main() -> None:
    source = EDITOR.read_text(encoding="utf-8")
    check(source)
    # Mutants show the checker fails when either real control breaks,
    # when geometry exists but is never called, or when old scales return.
    mutants = (
        source.replace("rackEarControls(getWidth(), getHeight())",
                       "rackEarControls(2048, 379)"),
        source.replace("loadMediaButton.setBounds(media.x, media.y, media.w, media.h);",
                       "loadMediaButton.setBounds(20, 270, 192, 30);"),
        source.replace("settingsButton.setBounds(setup.x, setup.y, setup.w, setup.h);",
                       "settingsButton.setBounds(220, 270, 192, 30);"),
        source.replace("    // Save badge defaults (Compact View)",
                       "    int loadBtnX = 55;\n    // Save badge defaults (Compact View)"),
    )
    for i, mutated in enumerate(mutants, 1):
        if mutated == source:
            raise AssertionError(f"Invalid mutant {i}: no change")
        try:
            check(mutated)
        except AssertionError:
            continue
        raise AssertionError(f"Undetected layout regression mutant {i}")
    print("P048_ORIGINAL_RACK_EAR_EDITOR_WIRING=PASS; negative_mutants=4")


if __name__ == "__main__":
    main()
