// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// P036: a DAW project must NEVER mark a partially received original Akai
// MIDI Sample Dump as a complete, self-contained native MAME state.
// A transport deadline is not successful sample RAM / keygroup recall.
namespace akai::sds {
enum class ImportStage: int {
 Idle=0, Prepared=1, Sending=2, SentNotMapped=3,
 TransportFailed=4, Mapping=5, LcdMapped=6, MappingFailed=7
};
constexpr bool safeToSnapshotOriginalFirmware(int stage) noexcept {
 return stage==int(ImportStage::Idle) || stage==int(ImportStage::LcdMapped);
}
constexpr bool requiresOriginalFirmwareWarning(int stage) noexcept {
 return !safeToSnapshotOriginalFirmware(stage);
}
}
