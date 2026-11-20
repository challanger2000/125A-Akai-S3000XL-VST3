#!/usr/bin/env python3
"""Negative-control tests for the fail-closed pinned JUCE VST3 RT patcher."""
from pathlib import Path
from tempfile import TemporaryDirectory
from patch_juce_vst3_realtime import ORIGINAL, PATCHED, patch

def testcase(raw: str, expect_success: bool):
    with TemporaryDirectory() as temp:
        p=Path(temp)/"juce_audio_plugin_client_VST3.cpp"
        p.write_text(raw,encoding="utf-8")
        success=True
        try:
            patch(p)
        except ValueError:
            success=False
        if success!=expect_success:
            raise AssertionError(f"Unexpected patch success={success} for a fixture")
        next_text=p.read_text(encoding="utf-8")
        if success:
            assert next_text.count(PATCHED)==1
            assert ORIGINAL not in next_text
            assert "if (! sl.isLocked())" in next_text
            assert "buffer.clear();" in next_text
            # Idempotence is rejected: cannot silently patch twice.
            try:
                patch(p)
            except ValueError:
                pass
            else:
                raise AssertionError("Double application was not rejected")
        else:
            assert next_text==raw, "On failure the patcher must not modify the input"

testcase("header\n"+ORIGINAL+"\nfooter\n", True)
testcase("header\n\nfooter\n", False)
testcase("header\n"+ORIGINAL+"\n"+ORIGINAL+"\nfooter\n", False)
testcase("header\n"+PATCHED+"\nfooter\n", False)
print("JUCE_VST3_RT_PATCH_NEGATIVE_CONTROLS=PASS tests=4")
