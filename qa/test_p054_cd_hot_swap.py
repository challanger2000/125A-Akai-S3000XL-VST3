#!/usr/bin/env python3
"""P054 source-level CD hot-insertion/ejection regression.

MAME original-ROM audio acceptance is a separate actual private fixture.
"""
from pathlib import Path
import sys

SOURCE = Path("Source/PluginEditor.cpp")


def valid(source):
    begin=source.find("void EnsoniqSD1AudioProcessorEditor::loadMediaButtonClicked()")
    end=source.find("void EnsoniqSD1AudioProcessorEditor::toggleSettings()",begin)
    if begin < 0 or end < 0:
        return False
    text=source[begin:end]
    if 'CD-ROM  (removable - hot swap)' not in text:
        return False
    # One branch chooses the ISO file filter; the other dispatches the image
    # mount. Both are valid and must remain distinct.
    if text.count('if (result==6)') != 2:
        return False
    if text.count('else if (result==7)') != 1:
        return False
    if text.count('else if (result==9)') != 1:
        return False
    insert=text[text.index('e->saveGlobalSettings();'):text.index('e->repaint();',text.index('e->saveGlobalSettings();'))]
    eject=text[text.index('else if (result==7)'):text.index('else if (result==9)')]
    hdd=text[text.index('else if (result==9)'):]
    if 'p.requestCdLoad.store(true,std::memory_order_release);' not in insert:
        return False
    if 'processor.requestCdLoad.store(true,std::memory_order_release);' not in eject:
        return False
    if 'shutdownMame' in eject or 'checkRomAndBootMame' in eject:
        return False
    if 'shutdownMame' not in hdd or 'checkRomAndBootMame' not in hdd:
        return False
    return True


def main():
    text=SOURCE.read_text(encoding='utf-8')
    if not valid(text):
        raise AssertionError("Real editor CD insertion/eject must not reset original Akai RAM")
    # Old P053 route: ejection reboots, losing user-imported samples.
    old=text.replace('processor.requestCdLoad.store(true,std::memory_order_release);',
                     'processor.shutdownMame();processor.checkRomAndBootMame();',1)
    if valid(old):
        raise AssertionError("P054 old CD-eject reboot mutant escaped")
    # Old CD insertion path: CD reload forced an emulator reboot.
    old2=text.replace('p.requestCdLoad.store(true,std::memory_order_release);',
                      'p.shutdownMame();p.checkRomAndBootMame();',1)
    if valid(old2):
        raise AssertionError("P054 old CD-insert reboot mutant escaped")
    print("P054_CD_INSERT_EJECT_NO_MACHINE_RESET=PASS")
    print("P054_OLD_REBOOT_MUTANTS_REJECTED=2")


if __name__=='__main__':
    try:
        main()
    except (OSError, AssertionError) as err:
        print(f"P054_CD_HOT_SWAP_FAIL: {err}",file=sys.stderr)
        sys.exit(1)
