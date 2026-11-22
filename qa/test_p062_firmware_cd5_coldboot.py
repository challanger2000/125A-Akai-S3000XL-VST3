#!/usr/bin/env python3
"""P062: fail-closed Akai S3000XL factory-SCSI5 original firmware CD startup.

The factory firmware defaults to ID 5. Without an HDD, the sole 2x optical
device must occupy ID 5; with an actual HDD on 5, the CD remains on ID 4.
The existing ISO is mounted via MAME -cdrm before the original firmware boots.
Selecting/ejecting a disc in the GUI cold-restarts the emulation, not hot-mount.
All private ROM, Akai sample CD, Studio Pro and audio acceptance still
require an actual owner-side test, not this static contract.
"""
from pathlib import Path
import sys


def valid(media: str, editor: str, processor: str) -> bool:
    begin=media.find("if (hardDiskImage.empty())")
    split=media.find("} else {",begin)
    end=media.find("    if (hasCD)",split)
    if begin<0 or split<begin or end<split:
        return False
    cd_only=media[begin:split]
    with_hdd=media[split:end]
    start=editor.find("void EnsoniqSD1AudioProcessorEditor::loadMediaButtonClicked()")
    stop=editor.find("void EnsoniqSD1AudioProcessorEditor::toggleSettings()",start)
    if start<0 or stop<=start:
        return False
    ui=editor[start:stop]
    uiinsert=ui[ui.find("e->saveGlobalSettings();"):ui.find("e->repaint();")]
    uieject=ui[ui.find("} else if (result==7)"):ui.find("} else if (result==9)")]
    worker=processor.find("void EnsoniqSD1AudioProcessor::runMameEngine()")
    frontend=processor.find("int mameResult = frontend->execute(args);",worker)
    if worker<0 or frontend<=worker:
        return False
    beforeboot=processor[worker:frontend]
    return all((
        'args.emplace_back("-scsi:4");\n        args.emplace_back("");' in cd_only,
        'args.emplace_back("-scsi:5");\n        args.emplace_back("cdrom_2x");' in cd_only,
        'args.emplace_back("-scsi:4");\n        args.emplace_back("cdrom_2x");' in with_hdd,
        'args.emplace_back("-scsi:5");\n        args.emplace_back("harddisk");' in with_hdd,
        'args.emplace_back("-hard");' in with_hdd,
        'args.push_back(hardDiskImage);' in with_hdd,
        'if (hasCD) {' in media,
        'args.emplace_back("-cdrm");\n        args.push_back(cdImage);' in media,
        'akai::appendScsiMediaArguments(args, pendingCdPath, pendingHddPath);' in beforeboot,
        'CD-ROM  (automatic SCSI ID - restarts sampler)' in ui,
        'p.pendingCdPath=path;' in ui,
        'e->saveGlobalSettings();' in ui,
        'p.shutdownMame();\n                        p.checkRomAndBootMame();' in uiinsert,
        'p.requestCdLoad.store(false,std::memory_order_release);' in uiinsert,
        'processor.pendingCdPath="";' in uieject,
        'processor.shutdownMame();\n                processor.checkRomAndBootMame();' in uieject,
        'processor.requestCdLoad.store(false,std::memory_order_release);' in uieject,
        'p.requestCdLoad.store(true,std::memory_order_release);' not in uiinsert,
        'processor.requestCdLoad.store(true,std::memory_order_release);' not in uieject,
    ))


def main():
    media=Path("Source/AkaiScsiMediaPlan.h").read_text(encoding="utf-8")
    editor=Path("Source/PluginEditor.cpp").read_text(encoding="utf-8")
    processor=Path("Source/PluginProcessor.cpp").read_text(encoding="utf-8")
    assert valid(media,editor,processor),"Akai ID5 CD-only cold-boot contract missing"
    mutants=[
        (media.replace('args.emplace_back("cdrom_2x");  // firmware default SCSI ID5','args.emplace_back("harddisk");  // WRONG',1),editor,processor),
        (media.replace('args.emplace_back("");         // disable the upstream CD4 slot','args.emplace_back("cdrom_2x"); // wrong duplicate CD',1),editor,processor),
        (media.replace('args.emplace_back("-scsi:5");','args.emplace_back("-scsi:4");',1),editor,processor),
        (media.replace('args.emplace_back("harddisk");','args.emplace_back("cdrom_2x");',1),editor,processor),
        (media.replace('args.emplace_back("-cdrm");','args.emplace_back("-hard");',1),editor,processor),
        (media,editor.replace('p.shutdownMame();\n                        p.checkRomAndBootMame();','p.requestCdLoad.store(true,std::memory_order_release);',1),processor),
        (media,editor.replace('processor.shutdownMame();\n                processor.checkRomAndBootMame();','processor.requestCdLoad.store(true,std::memory_order_release);',1),processor),
        (media,editor,processor.replace('akai::appendScsiMediaArguments(args, pendingCdPath, pendingHddPath);','// skipped',1)),
        (media,editor.replace('CD-ROM  (automatic SCSI ID - restarts sampler)','CD-ROM  (manual ID)'),processor),
    ]
    for i,variant in enumerate(mutants,1):
        assert not valid(*variant),f"CD5 cold-boot mutation {i} survived"
    print("P062_DEFAULT_FIRMWARE_ID5_CD_ONLY_COLDBOOT=PASS")
    print("P062_HDD5_CD4_TOPOLOGY=PASS")
    print(f"P062_BROKEN_MUTANTS={len(mutants)}/{len(mutants)}_KILLED")
    print("ORIGINAL_ROM_AKAI_CD_STUDIO_PRO_PLAYBACK=NOT_RUN")


if __name__=="__main__":
    try:
        main()
    except (OSError,ValueError,AssertionError) as e:
        print("P062_FAIL: "+str(e),file=sys.stderr)
        sys.exit(1)
