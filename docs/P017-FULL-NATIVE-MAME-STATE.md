# P017 — complete original-MAME .sta portable DAW project state (experimental)

This branch replaces the former Ensoniq-only RAM fragments as the source of
truth for project recall. The original S3000XL has 32 MiB DSP/sample RAM and
CPU/DSP registers; copying only legacy OSRAM + SEQRAM cannot restore samples.

## Original MAME API used
Official MAME 0.288 `running_machine::immediate_save(filename)` and
`immediate_load(filename)` execute only in `VstOsdInterface::update()`, on
the owning MAME thread. The per-instance absolute .sta path under the
already-existing unique TEMP sandbox avoids cross-instance collisions.
No alternate sampler engine or fabricated guest RAM headers.

The DAW's getState() requests a numbered MAME snapshot; waits boundedly
for the matching generation (6s maximum) and embeds a strict versioned,
size-bounded, integrity-checked envelope containing original .sta bytes.
A late completion cannot be mistaken for another request. The old OSRAM and
SEQRAM fields are retained only for backward compatibility. A snapshot
failure is tagged explicitly `akai_native_complete=false`: never claim
that a portable Akai bank survived in that case.

setState() verifies envelope length, version, integrity and original
`MAMESAVE` header before changing VST parameters or queuing any MAME
restore. The MAME owner writes the validated original .sta into its sandbox
and applies the native load once its emulation has started. No ROM bytes,
external WAV paths, or unsafe host-thread direct DSP access in the new path.

## Required independent acceptance
P017 envelope tests include 16 positive/adversarial cases and must run on
Windows/Linux Release; full Windows VST3, actual 125A Plugin Tester,
Steinberg Validator, independent Test Lab and intentional corrupted-state
regression must be repeated on the EXACT new binary. Generic state API
roundtrips **do not prove** actual original-firmware sample-RAM recall.
A private ROM-backed test must capture a WAV via the real Akai original
recording, play C3 over MIDI, save the DAW project, destroy source WAV,
reopen a fresh instance, and compare note PCM and original sampler state.

This P017 implementation is NOT a release certificate. Stop on failure and
do not merge without new native Windows and private V2.0 acceptance.

## P020: restore/save race corrected at the IPC boundary (not ROM certified)
The P017 worker previously consumed a single boolean restore request before
issuing original MAME immediate_load. A concurrent second project restore
could be cleared by the first, and an immediate DAW save could capture the
older running machine instead of the newly loaded project's pending image.
A generation-tagged per-instance mailbox now preserves the latest validated
image until the MAME owner has issued its load; an obsolete completion cannot
acknowledge a newer image. A save while restore is unresolved serializes
the pending exact .sta bytes. Failed sandbox writes retain the source image
and explicitly flag failure instead of retrying disk operations every frame.
A new native regression covers both orderings, failed I/O and cancellation.
This is an IPC/source-level correctness improvement only: MAME 0.288
immediate_load has no success return value and successful original-ROM V2.0
load, playable PCM, keygroup mapping and portable project recall remain OPEN.

// MAME 0.288 API review: immediate_load calls handle_saveload, but anonymous
// scheduler timers can leave save_or_load_pending() true. P020 therefore tracks
// each in-flight native load on the MAME owner thread and does not schedule any
// competing native save or legacy state operation while MAME owns a pending
// load. These structural checks need ROM-backed confirmation: the public
// MAME API has no load-success result and an aborted pending load can be
// observed as merely no longer pending. Project/PCM certification remains OPEN.
