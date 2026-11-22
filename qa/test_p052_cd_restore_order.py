#!/usr/bin/env python3
"""Fail closed on CD media mount occurring after native MAME machine restore.

Tests actual owner-thread source order plus two adversarial regressions.
This is not a genuine Windows-VST3/ROM-host functional test.
"""
from pathlib import Path
import sys

START = "virtual void update(bool skip_redraw) override {"
MOUNT = "if (processor->requestCdLoad.exchange(false, std::memory_order_acquire))"
RESTORE = "if (const auto work=processor->nativeRestore.take())"
IMAGE = "image.load(processor->pendingCdPath)"
MARKER = "        // P060: mount optical media on the native MAME worker"
END = "        // P017 native full MAME snapshot."


def valid(source):
    if source.count(START) != 1:
        return False
    body = source.split(START,1)[1]
    if body.count(MOUNT)!=1 or body.count(RESTORE)!=1 or body.count(IMAGE)!=1:
        return False
    cd_pos=body.find(MOUNT)
    restore_pos=body.find(RESTORE)
    if cd_pos>=restore_pos or cd_pos<0 or restore_pos<0:
        return False
    block=body[cd_pos:restore_pos]
    return all(x in block for x in (
        "std::lock_guard<std::mutex> lock(processor->mediaMutex)",
        "image.unload();",
        "const auto result = image.load(processor->pendingCdPath);",
        "const bool ready = !result.first && image.exists()",
        "if (!ready)",
        "isCdLoaded.store(false",
        "foundCdImageDevice",
    ))


def main():
    source=Path("Source/PluginProcessor.cpp").read_text(encoding="utf-8")
    if not valid(source):
        raise AssertionError("Live MAME owner-thread source violates CD-before-restore")
    if source.count(MARKER)!=1 or source.count(END)!=1:
        raise AssertionError("P052 source insertion markers are ambiguous")
    a=source.index(MARKER)
    b=source.index(END,a)
    cd=source[a:b]
    old=source[:a]+source[b:]+cd
    if valid(old):
        raise AssertionError("Old restore-before-mount mutant unexpectedly passes")
    unchecked=source.replace("const auto result = image.load(processor->pendingCdPath);",
                             "(void)image.load(processor->pendingCdPath);",1)
    if valid(unchecked):
        raise AssertionError("Unchecked-CD-mount mutant unexpectedly passes")
    print("P052_CD_BEFORE_MACHINE_RESTORE_PASS")
    print("P052_OLD_ORDER_AND_UNCHECKED_MOUNT_NEGATIVES_KILLED")


if __name__=="__main__":
    try:
        main()
    except (AssertionError,OSError) as error:
        print(f"P052_CD_RESTORE_REGRESSION_FAIL: {error}",file=sys.stderr)
        sys.exit(1)
