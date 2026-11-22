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

## Additional exact-source CI and private-package evidence recorded after initial commit

- Original standalone 125A Akai MAME 0.288, authentic V2.0 ROM and owner ISO: hot-inserted image, original DATAENTRY wheel selected HARD-A, 14 Volumes visible, first Volume loaded 4 programs / 18 samples, MIDI Main PCM response. No private bytes uploaded. See P054/P055 firmware report.
- Two independent fresh original-ROM native MAME instances restored the 1,598,044-byte P055 hot-import full-machine snapshot with **no ISO mounted**, played original MIDI program sweep, and emitted **identical full WAV SHA-256 `d61985eb12a870e0f28229f9cde5c614f3dbc33ec6f92e8269cbc0e6f5109e11`**. CD program slots 1–4 produced nonzero PCM peaks while the factory-control corresponding slots were exactly silent. Evidence is private local, not a public ROM fixture.
- P054 full Windows ROM-free VST3 Windows 2022 compilation and actual host QA at commit `b8481dd1b304c90ef7b0f2b3cc030b54065e95bb`: GitHub Actions run 45042616057, **SUCCESS**. Steinberg official validator **47 passed, 0 failed**; production 125A Plugin Tester v0.3.2 **54 PASS, 0 WARN, 0 FAIL**, source-run QA artifact inspected.
- P054 private owner-only VST3 ZIP was assembled locally from the verified public Windows artifact and owner's two original firmware binaries. Independent ZIP CRC, strict moduleinfo JSON, original public PE prefix and both ROM SHA-256 checks PASS; private ZIP SHA256 `50c2eab0ae689f1305e33a5ce7521d4682b1cc05d21235bdefd56af41664db9b`. **Do NOT call this the finished product**; no real Windows ROM-backed Studio Pro runtime or DAW project recall occurred.
- P056 new CD-throttle unblocking source SHA `ff38ad45faf80389a08ca3973f12a8329ca9114a`, Windows+Ubuntu core CI `45044519171` **SUCCESS**. Exact original Windows ROM-free full build CI `45044519170` was in progress when checked. The fixed throttle branch is newer than the P054 private candidate.
- Original user expectation: one finished Windows VST3 with internally embedded V2 firmware, no additional programs, reliable original CD/ROM MIDI sound and persistent DAW project recall; no interim artifact or premature final claim.

Current release verdict: **NO-GO**, because the exact P056 owner-private Windows VST3 with original firmware/CD on a real Windows host, stopped transport CD hot-swap, DAW save/close/reopen, missing-media recall, audio comparison and multiple instances is **NOT RUN**. ROM-free CI is only partial assurance.
