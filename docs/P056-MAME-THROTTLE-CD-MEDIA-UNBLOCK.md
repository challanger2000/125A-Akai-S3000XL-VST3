# P056 — Pending CD request unblocks audio-driven MAME emulation

Scope: project original-ROM S3000XL CD insert/eject in current 125A VST3. Parent source P055 docs-only `e1b88477c565bc3ef29bc5f2338d776b22d610ab`. ROM, original Akai sampler, JUCE host audio engine and sample format remain unchanged.

## Specific verified source bug
`EnsoniqSD1AudioProcessor::pushAudioFromMame()` uses a loop to throttle its MAME owner thread whenever unread DAW audio fills the host ring above the configured threshold. It already explicitly breaks for native snapshot, native restore and anchor sync requests to let the MAME owner process non-audio work. P054's new CD insert/eject request flag was NOT in that wake-up predicate. With host transport stopped or audio callbacks not draining the ring, a valid pending CD insertion could remain blocked inside the throttle loop even though original standalone MAME supports the hot-swap natively. This is a direct source/control-flow defect; no actual owner-ROM Windows DAW reproduction available.

## Minimal fix
Add one atomic load of `requestCdLoad` (memory_order_acquire) to the existing owner-thread throttle breaker. This interrupts an otherwise blocked audio-producer loop when the CD media work is queued. The MAME update() handles the file operation before native machine restore (P052), outside of the realtime host callback. No additional heap operations or external threads.

## Regression and negative
Extend existing P054 hot-swap ROM-free Python source regression to assert the exact cd request in the precise throttle guard, alongside existing native-state guard, and to kill a mutation that removes the flag. CI and actual host runtime evidence must remain separate.

## Assurance status
- Original V2.0 native Linux MAME: authentic user ISO hot-insert, DATAENTRY to HARD-A, CD Volume 3001 import 4 programs 18 samples, CD-only original MIDI, independent hot-import native-MAME state recall without ISO (bit-identical two replays): PASS.
- P054 Windows/Linux isolated contracts: PASS before this modification. P056 exact-source CI pending.
- P054 complete original Windows ROM-free VST3: build previously in progress; not evidence of private host behavior.
- P056 native Windows VST3/Steinberg/125A ROM-free CI: NOT RUN at time of commit.
- Original owner-ROM private Windows VST3 Studio Pro paused-transport CD insertion, MIDI audio, full DAW state save/reopen: NOT RUN. RELEASE NO-GO.
