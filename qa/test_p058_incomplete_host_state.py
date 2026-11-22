#!/usr/bin/env python3
"""P058: fail closed on DAW save explicitly lacking original Akai state.

These structural checks complement the C++ six-case logic tests, but do not
replace private original-ROM Windows DAW round-trip tests.
"""
from pathlib import Path
import sys


def valid(proc, header, editor):
    save_begin=proc.index("void EnsoniqSD1AudioProcessor::getStateInformation")
    load_begin=proc.index("void EnsoniqSD1AudioProcessor::setStateInformation")
    saved=proc[save_begin:load_begin]
    loaded=proc[load_begin:]
    if 'xml->setAttribute("akai_native_complete",false)' not in saved:
        return False
    if 'xml->setAttribute("akai_native_complete",true)' not in saved:
        return False
    marker="if(akai::machine_state::rejectIncompleteProject("
    if marker not in loaded:
        return False
    reject=loaded.index(marker)
    mutate=loaded.index("apvts.replaceState")
    image=loaded.index("nativeRestore.publish")
    if not reject<mutate<image:
        return False
    if 'nativeRecallIncompleteRejected.store(true,std::memory_order_release)' not in loaded[reject:mutate]:
        return False
    return all(x in header for x in [
        "hasCompletionMarker && (!completionOk || !hasMachineImage)",
        "rejectIncompleteProject(bool hasCompletionMarker",
    ]) and "nativeRecallIncompleteRejected.exchange(" in editor


def main():
    proc=Path("Source/PluginProcessor.cpp").read_text(encoding="utf-8")
    header=Path("Source/AkaiMachineState.h").read_text(encoding="utf-8")
    editor=Path("Source/PluginEditor.cpp").read_text(encoding="utf-8")
    if not valid(proc,header,editor):
        raise AssertionError("Host can substitute factory state for failed native snapshot")
    wrong=proc.replace("if(akai::machine_state::rejectIncompleteProject(",
                       "if(akai::machine_state::ignoreIncompleteProject(",1)
    if valid(wrong,header,editor):
        raise AssertionError("No-rejection mutant survived")
    wrong_header=header.replace(
        "hasCompletionMarker && (!completionOk || !hasMachineImage)",
        "false",1)
    if valid(proc,wrong_header,editor):
        raise AssertionError("Always-accept incomplete state mutant survived")
    wrong_editor=editor.replace("nativeRecallIncompleteRejected.exchange(",
                                "nativeRecallIncompleteRejected_UNUSED.exchange(",1)
    if valid(proc,header,wrong_editor):
        raise AssertionError("Missing user-visible warning mutant survived")
    print("P058_INCOMPLETE_AKAI_NATIVE_STATE_REJECTS_BEFORE_MUTATION=PASS")
    print("P058_LEGACY_STATE_COMPATIBLE_FAILED_SNAPSHOT_MUTANTS=KILLED")


if __name__=="__main__":
    try:
        main()
    except (AssertionError,OSError,ValueError) as err:
        print("P058_FAIL "+str(err),file=sys.stderr)
        raise SystemExit(1)
