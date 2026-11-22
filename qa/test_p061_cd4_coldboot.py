#!/usr/bin/env python3
"""P061 regression: original Tuth S3000XL CD4 / 2x / cold boot load.

The source-based regressions below do NOT constitute successful original-ROM,
real ISO or Studio Pro host tests. A real native Akai CD-to-MIDI PCM test is
mandatory for release and cannot be faked by a source check.
"""
from pathlib import Path
import sys

def verify(media: str, gui: str, engine: str) -> bool:
    open_marker='void EnsoniqSD1AudioProcessorEditor::loadMediaButtonClicked()'
    close_marker='void EnsoniqSD1AudioProcessorEditor::toggleSettings()'
    begin=gui.find(open_marker)
    end=gui.find(close_marker,begin)
    if begin < 0 or end <= begin:
        return False
    ui=gui[begin:end]
    insert=ui[ui.find('e->saveGlobalSettings();'):ui.find('e->repaint();')]
    eject=ui[ui.find('} else if (result==7)'):ui.find('} else if (result==9)')]
    engine_start=engine.find('void EnsoniqSD1AudioProcessor::runMameEngine()')
    if engine_start < 0:
        return False
    engine_args=engine[engine_start:engine.find('int mameResult = frontend->execute(args);',engine_start)]
    return all((
        'args.emplace_back("-scsi:4");\n    args.emplace_back("cdrom_2x");' in media,
        'args.emplace_back("-scsi:5");' in media,
        'args.emplace_back("");' in media,
        'args.emplace_back("harddisk");' in media,
        'args.emplace_back("-cdrm");\n        args.push_back(cdImage);' in media,
        'akai::appendScsiMediaArguments(args, pendingCdPath, pendingHddPath);' in engine_args,
        'CD-ROM  (original Akai SCSI drive - auto restart)' in ui,
        'if (result==6)' in insert,
        'p.requestCdLoad.store(false,std::memory_order_release);' in insert,
        'p.shutdownMame();\n                        p.checkRomAndBootMame();' in insert,
        'processor.requestCdLoad.store(false,std::memory_order_release);' in eject,
        'processor.shutdownMame();\n                processor.checkRomAndBootMame();' in eject,
        'e->saveGlobalSettings();' in ui,
        'editor->saveGlobalSettings();' in eject,
        'p.pendingCdPath=path;' in ui,
        'processor.pendingCdPath="";' in eject,
        # A cold boot with -cdrm must not also enqueue a hot-mount.
        'p.requestCdLoad.store(true,std::memory_order_release);' not in insert,
        'processor.requestCdLoad.store(true,std::memory_order_release);' not in eject,
    ))

def main():
    media=Path('Source/AkaiScsiMediaPlan.h').read_text(encoding='utf-8')
    gui=Path('Source/PluginEditor.cpp').read_text(encoding='utf-8')
    engine=Path('Source/PluginProcessor.cpp').read_text(encoding='utf-8')
    assert verify(media,gui,engine),"P061 CD cold boot contract absent"
    mutants=[
        (media.replace('args.emplace_back("-scsi:4");','args.emplace_back("-scsi:5");',1),gui,engine),
        (media.replace('args.emplace_back("cdrom_2x");','args.emplace_back("cdrom");',1),gui,engine),
        (media.replace('args.emplace_back("-cdrm");','args.emplace_back("-flop");',1),gui,engine),
        (media.replace('args.emplace_back("");','args.emplace_back("harddisk");',1),gui,engine),
        (media,gui.replace('p.shutdownMame();\n                        p.checkRomAndBootMame();',
                             'p.requestCdLoad.store(true,std::memory_order_release);',1),engine),
        (media,gui.replace('processor.shutdownMame();\n                processor.checkRomAndBootMame();',
                             'processor.requestCdLoad.store(true,std::memory_order_release);',1),engine),
        (media,gui,engine.replace('akai::appendScsiMediaArguments(args, pendingCdPath, pendingHddPath);',
                                  '// no CD image passed to native MAME',1)),
        (media,gui.replace('p.requestCdLoad.store(false,std::memory_order_release);',
                           'p.requestCdLoad.store(true,std::memory_order_release);',1),engine),
    ]
    for n,case in enumerate(mutants,1):
        assert not verify(*case),f"Broken CD routing mutant {n} survived"
    print("P061_UPSTREAM_S3000XL_CD4_2X_COLDBOOT_DISPATCH=PASS")
    print(f"P061_NEGATIVE_CONTROLS={len(mutants)}/{len(mutants)}_KILLED")
    print("P061_REAL_AKAI_CD_STUDIO_PRO_PCM=NOT_RUN")

if __name__=='__main__':
    try:
        main()
    except (AssertionError,ValueError,OSError) as e:
        print("P061_FAIL: "+str(e),file=sys.stderr)
        sys.exit(1)
