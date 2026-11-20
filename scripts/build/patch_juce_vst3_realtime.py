#!/usr/bin/env python3
"""Fail-closed per-build JUCE 8.0.12 VST3 realtime callback-lock adjustment.

The original JUCE VST3 wrapper calls CriticalSection::enter() unconditionally
inside processAudio(), which maps to Win32 EnterCriticalSection once per block.
The independent 125A v0.3.2 callback tracer confirmed 32 potentially blocking
calls in 32 host-paced blocks even when this Akai instrument has not booted.

For this plugin build ONLY, use JUCE's native ScopedTryLock and emit silence
if an editor/host thread owns the callback lock. Never process unlocked.
JUCE's normal path stays bit-identical when the lock is uncontended.
"""
from __future__ import annotations
import argparse
from pathlib import Path

ORIGINAL = "            const ScopedLock sl (pluginInstance->getCallbackLock());"
PATCHED = """            // 125A: A VST3 audio callback must not block on UI or host locks.
            // Use the same JUCE CriticalSection, but try once and fail silent.
            // On success it remains held for the original protected scope.
            const ScopedTryLock sl (pluginInstance->getCallbackLock());
            if (! sl.isLocked())
            {
                buffer.clear();
                return;
            }"""

def patch(path: Path) -> None:
    if not path.is_file():
        raise ValueError(f"Pinned JUCE VST3 wrapper not found: {path}")
    source=path.read_text(encoding="utf-8-sig")
    if source.count(ORIGINAL)!=1 or source.count(PATCHED)!=0:
        raise ValueError("JUCE 8.0.12 source mismatch; refusing an unsafe wrapper edit")
    next_source=source.replace(ORIGINAL,PATCHED)
    if next_source.count("const ScopedTryLock sl (pluginInstance->getCallbackLock());")!=1:
        raise AssertionError("Failed to install one realtime try-lock")
    if next_source.count(ORIGINAL)!=0:
        raise AssertionError("Blocking audio callback lock still present")
    path.write_text(next_source,encoding="utf-8",newline="\n")
    print("JUCE_VST3_RT_CALLBACK_TRYLOCK=PATCHED")
    print("CONTENDED_CALLBACK_BEHAVIOUR=silence_not_blocking")

def main():
    a=argparse.ArgumentParser()
    a.add_argument("--juce-root",type=Path,required=True)
    args=a.parse_args()
    wrapper=args.juce_root/"modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp"
    patch(wrapper)

if __name__=="__main__":
    main()
