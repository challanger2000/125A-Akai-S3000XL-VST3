# P022 — Deferred original-MAME 0.288 native snapshots

## Confirmed source failure
The P017/P020 getState thread waited up to six seconds for a native MAME full-state save. Its MAME worker called `running_machine::immediate_save(path)` and immediately decided success or failure from the existence of a file. **MAME 0.288 does not guarantee immediate completion:** `immediate_save()` calls `handle_saveload()`, which returns without performing I/O while anonymous scheduler timers exist. It leaves `save_or_load_pending()` true for the later scheduler callback. Therefore the old code could mark a valid deferred original-sample-RAM snapshot failed, or race another state operation.

## Fix (P022)
- A bounded per-instance generation-tagged `SnapshotBarrier` remembers exactly which host request is being saved.
- A snapshot starts only if no native restore, no in-flight restore and no MAME save/load is pending. The MAME owner thread is the only code issuing original `.sta` state operations.
- The VST3 host is notified **only after** MAME's own pending state operation resolves. The resulting file must be freshly created, have a bounded length and the `MAMESAVE` native header. A failed or timed-out snapshot is never labelled portable.
- A newer original-MAME project restore cannot interrupt the outstanding save before the initial requester has been completed. A second snapshot cannot overwrite a first deferred snapshot before it is acknowledged.
- Original A/D→DMA capture, native firmware, DSP algorithms and GUI code are unchanged.

## Adversarial regression (isolated contract, 7 CTest executables)
Core negative controls: scheduler already pending, in-flight restore, invalid generation zero, duplicate outstanding generation, *MAME defers the save*, scheduler resolves, re-arm new generation, stale acknowledgement and duplicate completion. The actual machine snapshot and original-ROM audio remain a separate end-to-end gate.

## Important verification boundary
MAME's public `save_or_load_pending()` becoming false is **not itself an error-free MAME state write**. The new worker also checks that a fresh native `.sta` exists with correct header/size. There is still no evidence that this VST3 with the owner firmware reproduces the identical WAV→keygroup→MIDI PCM across DAW project reloading. Windows VST3 + Steinberg Validator + 125A Tester results must be pinned to **this exact P022 source commit**. Tests from the prior P020 commit cannot certify P022. The final owner-private build embeds neither original ROM in public source nor CI.
