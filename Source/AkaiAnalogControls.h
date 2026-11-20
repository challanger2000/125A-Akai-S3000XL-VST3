#pragma once
// Original Akai analogue controls; separate from firmware/DSP semantics.
// Hardware potentiometer law is not yet calibrated: 0..1 attenuation,
// unity at 1 to preserve the verified upstream audio baseline.
#include <algorithm>
#include <cmath>

namespace AkaiAnalogControls {
inline float normalized(float v) noexcept {
    return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
}
inline float smoothingCoefficient(float sampleRate) noexcept {
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0f) return 1.0f;
    return 1.0f - std::exp(-1.0f / (0.005f * sampleRate));
}
inline float smooth(float current, float target, float coefficient) noexcept {
    const float t = normalized(target);
    if (!std::isfinite(current)) return t;
    if (std::abs(current - t) < 1.0e-5f) return t;
    const float next = static_cast<float>(static_cast<double>(current)
        + (static_cast<double>(t) - current) * coefficient);
    return std::abs(next - t) < 1.0e-5f ? t : next;
}
inline float recordInput(float sample, float gain) noexcept {
    return std::isfinite(sample) ? sample * normalized(gain) : 0.0f;
}
}
