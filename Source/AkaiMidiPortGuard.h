// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Only the explicitly exported DAW virtual MIDI input is a real OSD port.
// The original MAME 0.288 MIDI image device tries a port fallback after an
// unreadable SDS .mid image. Refuse it so the WAV import fails visibly.
#include <string_view>
namespace akai::sds {
constexpr bool isLiveVstMidiInputPort(std::string_view name) noexcept {
    return name=="VST MIDI";
}
}
