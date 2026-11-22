#!/usr/bin/env python3
"""P060 original Akai 2x SCSI CD-ROM hot-mount contracts.

Source guards and deliberately broken mutants; full Windows VST3 compilation,
private 542,117,888-byte Akai ISO load, firmware LOAD/VOLS and actual MIDI PCM
remain independent acceptance gates. No ROM/ISO is stored in public CI.
"""
from pathlib import Path
import sys


def valid(proc, header, editor):
    pos=proc.find("// P060: mount optical media on the native MAME worker")
    end=proc.find("// P017 native full MAME snapshot.",pos)
    if pos < 0 or end < pos:
        return False
    hot=proc[pos:end]
    if not all((
        'std::strcmp(cdTarget->shortname(), "scsi_cdrom2x") == 0' in hot,
        'image.brief_instance_name() != std::string("cdrm")' in hot,
        'const auto result = image.load(processor->pendingCdPath);' in hot,
        'const bool ready = !result.first && image.exists()' in hot,
        '&& processor->pendingCdPath == image.filename();' in hot,
        'cdTarget->shortname()' in hot,
        'cdTarget->reset();' in hot,
        'processor->isCdLoaded.store(true, std::memory_order_release);' in hot,
        'processor->isCdLoaded.store(false, std::memory_order_release);' in hot,
        'processor->reportCdMountError(' in hot,
        'processor->clearCdMountError();' in hot,
        'processor->requestCdLoad.exchange(false, std::memory_order_acquire)' in hot,
        'processor->mediaMutex' in hot,
        'std::atomic<bool> cdMountErrorPending{ false };' in header,
        'std::atomic<bool> isCdLoaded{ false };' in header,
        'void reportCdMountError(' in header,
        'const juce::ScopedLock lock(cdMountErrorLock);' in proc,
        'audioProcessor.cdMountErrorPending.exchange(' in editor,
        'audioProcessor.getCdMountError()' in editor,
        'juce::AlertWindow::showMessageBoxAsync(' in editor,
        # No power cycle of the Akai CPU/RAM or DAW host on ISO swap.
        'shutdownMame' not in hot,
        'schedule_hard_reset' not in hot,
        'checkRomAndBootMame' not in hot,
    )):
        return False
    return True


def main():
    proc=Path("Source/PluginProcessor.cpp").read_text(encoding="utf-8")
    header=Path("Source/PluginProcessor.h").read_text(encoding="utf-8")
    editor=Path("Source/PluginEditor.cpp").read_text(encoding="utf-8")
    assert valid(proc,header,editor), "P060 native media status or target-only reset is missing"
    mutants=[
        (proc.replace('cdTarget->reset();','/* no target reset */',1),header,editor),
        (proc.replace('std::strcmp(cdTarget->shortname(), "scsi_cdrom2x") == 0','true',1),header,editor),
        (proc.replace('const bool ready = !result.first && image.exists()','const bool ready = image.exists()',1),header,editor),
        (proc.replace('processor->pendingCdPath == image.filename();','true;',1),header,editor),
        (proc.replace('processor->reportCdMountError(','processor->discardCdMountError('),header,editor),
        (proc.replace('cdTarget->reset();','mame_machine->schedule_hard_reset();',1),header,editor),
        (proc,header.replace('std::atomic<bool> cdMountErrorPending{ false };','',1),editor),
        (proc,header,editor.replace('audioProcessor.getCdMountError()','juce::String()',1)),
    ]
    for n,variant in enumerate(mutants,1):
        assert not valid(*variant), f"mutation {n} survived"
    print("P060_NATIVE_CD_IMAGE_LOAD_RESULT_AND_TARGET_ONLY_READY=PASS")
    print("P060_NO_AKAI_CPU_OR_RAM_RESET=PASS")
    print(f"P060_FAIL_CLOSED_MUTANTS={len(mutants)}/{len(mutants)}_KILLED")


if __name__=="__main__":
    try:
        main()
    except (OSError,AssertionError,ValueError) as exc:
        print("P060_FAIL: "+str(exc),file=sys.stderr)
        raise SystemExit(1)
