# P052 — Restore project CD BEFORE native Akai machine state

Scope: P051 `b0201b2389d87fba8408d30e4224c78c7ad35c0c` + one targeted update() ordering fix. P049 MAME STATUS backport remains REJECTED by genuine firmware CD tests. No original firmware, audio engine, UI or external WAV-import code changed.

## Source defect

P051's setStateInformation() published the pending MAME full-state restore after selecting a project CD path, but the owner thread's update() called nativeRestore.take()/immediate_load() BEFORE processing requestCdLoad. Therefore the previously mounted CD remained active at the moment another project's MAME snapshot was loaded. Source order is directly proved; actual audible effects in a Windows host have not yet been measured.

## Targeted change

Move optical image hot load/unload to the beginning of MAME owner-thread update() immediately before nativeRestore processing. Do not issue optical operations from host/GUI or audio callback. Keep P050 SCSI target5 (CD only) and target4 (HDD+CD) mapping unchanged. Check MAME image.load()'s actual return value, set loaded flag false on a failed load or missing image interface and log the error. A missing source disc must not drop the original saved sample memory.

## Verification
- P052 source-level ordering and two deliberately broken variants: ROM-free local and Windows/Linux CI checks; status pending at commit time.
- P050 owner-ROM Linux MAME: read real owner's ISO, display 14 volumes, load 4 programs and 18 samples, trigger MIDI PCM; independent full-state recalls bit-identical — evidence from prior original-ROM test.
- P051 project CD identity: nine native C++ tests previously passed on Windows/Linux.
- Real P052 owner-ROM Windows VST3 load/save/reopen/multiple instances: NOT RUN.
- User release: NO-GO until original bundled-ROM VST3 host audio and project recall have been actually verified.
