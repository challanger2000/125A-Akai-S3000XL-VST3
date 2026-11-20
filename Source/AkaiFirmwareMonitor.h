#pragma once
// 125A P002: Akai S3000XL firmware WADCS->pre-record monitor bridge.
// Candidate hardware interpretation, pending V2.0 ROM-backed MON/Moff trace.
// The physical port write (0x00C0) is authoritative; never infer from GUI clicks.
// This header is independent of JUCE and MAME for isolated regression tests.
#include <cstdint>
#include <cmath>

namespace AkaiFirmwareMonitor {
constexpr std::uint8_t kRecordStrobe = 0x08;
constexpr std::uint8_t kMonitorInhibitCandidate = 0x20;

// WADCS observations in original project's own measurements:
// 0xA2 idle (bit5=1); 0x82 ARM (bit5=0); 0x8A mono GO;
// 0x9A stereo GO; 0xA2 stop. Hardware function of bit5 requires verification.
// Do not route host audio during GO: original MAME recording monitor voice
// remains active, and mixing another copy would double the signal.
enum class Mode { Off, Prerecord, Recording };
inline Mode decode(std::uint8_t firmwareLatch, bool latchKnown) noexcept {
    if (!latchKnown) return Mode::Off;
    if ((firmwareLatch & kRecordStrobe) != 0) return Mode::Recording;
    return ((firmwareLatch & kMonitorInhibitCandidate) == 0) ? Mode::Prerecord : Mode::Off;
}
inline float mix(float original, std::int16_t capture, float monitorAmount) noexcept {
    if (!std::isfinite(original)) original = 0.0f;
    if (!std::isfinite(monitorAmount)) return original;
    return original + static_cast<float>(capture) * (monitorAmount / 32767.0f);
}
}
