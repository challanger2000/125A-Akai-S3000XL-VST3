# P057 — Reject incompatible Akai machine-state headers before restore

This patch is based on P056 `a10d40fed09d85403f607d90ae4a6dcbcbaea838` and preserves the original MAME 0.288/Creative Akai S3000XL firmware behavior, current P053 unchanging SCSI cdrom_2x configuration, and P054 hot-media insertion/ejection.

## Confirmed high-risk failure mode

MAME 0.288 running_machine::immediate_load() returns void. Its handle_saveload() internally reports invalid savestate system/version/**registry signature** as a message and then clears save_or_load_pending. The VST3 currently uses only the cleared pending flag to report nativeRestore.finish(...,true); therefore **an incompatible save can be falsely marked restored**. Real owner-ROM negative control with the older cdrom_2x→cdrom substitution demonstrated the symptom: original saved Akai sounds failed to return and only the factory program played.

## P057 correction

- Before issuing original MAME immediate_load, open the incoming original .sta on its owning MAME thread and call the exact public MAME 0.288 save_manager::check_file(machine,file,gamename).
- This checks version, system name and current machine's registered-device CRC, and refuses immediately when the current hardware topology is incompatible. Original wrong-HDD/CD device layouts can no longer be silently acknowledged as valid, though automatic conversion remains unsupported.
- Stage the external hardware DATAENTRY value only if the original save header is compatible.
- Original machine-state envelope now validates MAME format version 2, little/big endian flag bits, and the system name s3000xl, instead of accepting any 32-byte payload with the MAMESAVE prefix. CI unit tests intentionally mutate each field.
- Native snapshot completion now checks the full MAME 32-byte header for the source machine.
- NO claim that a syntactically valid, same-signature state cannot later fail due corrupt compressed content; that needs a separate runtime success signal. Native Linux original ROM and private Windows DAW gate remain required.

## Evidence

- Real user-supplied original Akai S3000XL ROM v2.0 MAME state verified locally: `MAMESAVE` 32-byte native header; version=2; machine=`s3000xl`; signature=0e38a949 (file little-endian bytes 49 a9 38 0e). Kept private.
- Original owner-ROM fresh-process PCM after importing 4 programs/18 samples from user's ISO was already bit-identical in independent no-ISO restores on constant cdrom_2x target5, documented P055.
- New exact-source and mutated native header C++ fixtures: CI pending.
- Owner-ROM Windows x64 VST3: neither Studio Pro audible note testing nor project reopen is claimed.

Status: NOT FINAL until exact modified Windows build, private firmware bundle, runtime sample path, missing-media and project recall are independently verified.