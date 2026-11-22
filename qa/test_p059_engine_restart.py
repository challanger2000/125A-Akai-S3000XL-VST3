#!/usr/bin/env python3
"""P059: native MAME startup failure must be diagnosable and restartable in-place.

Static original-source contracts with deliberate negative controls; not a
substitute for the native original-ROM Windows VST3/DAW startup acceptance.
"""
from pathlib import Path
import sys


def valid(proc, header, editor, editor_header):
    start = proc.index("void EnsoniqSD1AudioProcessor::checkRomAndBootMame()")
    frontend = proc.index("int mameResult = frontend->execute(args);")
    worker = proc[frontend:]
    return all((
        "std::atomic<int> mameLastExitCode{ -1 };" in header,
        "mutable juce::CriticalSection mameFrontendErrorLock;" in header,
        'mameHasStarted.store(true, std::memory_order_release);' in proc[start:frontend],
        'mameLastExitCode.store(-1, std::memory_order_release);' in proc[start:frontend],
        'mameLastExitCode.store(mameResult, std::memory_order_release);' in worker,
        'isMameRunning.store(false, std::memory_order_release);' in worker,
        'mameIsFullyBooted.store(false, std::memory_order_release);' in worker,
        "void output_callback(osd_output_channel channel," in proc,
        "processor->setMameFrontendError(" in proc,
        "const juce::ScopedLock lock(mameFrontendErrorLock);" in proc,
        "juce::TextButton retryEngineButton" in editor_header,
        "retryEngineButton.onClick = [this]" in editor,
        "audioProcessor.shutdownMame();\n        audioProcessor.checkRomAndBootMame();" in editor,
        "retryEngineButton.setVisible(!missing && !invalid && !checkFailed" in editor,
        "audioProcessor.getMameFrontendError()" in editor,
        "MAME reported: " in editor,
        "please reload the plugin!" not in editor,
    ))


def main():
    proc = Path("Source/PluginProcessor.cpp").read_text(encoding="utf-8")
    hdr = Path("Source/PluginProcessor.h").read_text(encoding="utf-8")
    ed = Path("Source/PluginEditor.cpp").read_text(encoding="utf-8")
    eh = Path("Source/PluginEditor.h").read_text(encoding="utf-8")
    assert valid(proc, hdr, ed, eh), "engine restart or native error forwarding contract broken"
    mutants = [
        (proc.replace("mameLastExitCode.store(mameResult, std::memory_order_release);", "/* removed */", 1),hdr,ed,eh),
        (proc.replace("processor->setMameFrontendError(", "processor->discardMameFrontendError(", 1),hdr,ed,eh),
        (proc.replace("mameHasStarted.store(true, std::memory_order_release);", "mameHasStarted.store(false, std::memory_order_release);", 1),hdr,ed,eh),
        (proc,hdr,ed.replace("audioProcessor.shutdownMame();\n        audioProcessor.checkRomAndBootMame();", "audioProcessor.checkRomAndBootMame();",1),eh),
        (proc,hdr,ed.replace("audioProcessor.getMameFrontendError()", "juce::String()",1),eh),
        (proc,hdr,ed,eh.replace("juce::TextButton retryEngineButton", "juce::Label retryEngineButton",1)),
    ]
    for index, case in enumerate(mutants,1):
        assert not valid(*case), f"Mutation {index} survived"
    print("P059_ENGINE_RESTART_AND_EXACT_MAME_ERROR=PASS")
    print("P059_NEGATIVE_CONTROLS=6/6_KILLED")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, ValueError) as exc:
        print("P059_FAIL " + str(exc),file=sys.stderr)
        raise SystemExit(1)
