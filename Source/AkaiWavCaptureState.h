// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// P015: explicit original-Akai WAV lifecycle, no allocation or firmware writes.
namespace akai::wav_capture {
enum class State : int {None=0, Armed=1, Recording=2, SourceComplete=3};
constexpr State afterFirmwareStop(State before, bool feedComplete,
                                  bool sourceLoaded) noexcept {
    if (!sourceLoaded) return State::None;
    return feedComplete || before==State::SourceComplete
        ? State::SourceComplete : State::Armed;
}
constexpr bool canStage(State before, bool firmwareRecording,
                        bool automationActive, bool pendingSwitch,
                        bool pendingClear) noexcept {
    return !firmwareRecording && !automationActive &&
           before!=State::Recording && !pendingSwitch && !pendingClear;
}
} // namespace akai::wav_capture
