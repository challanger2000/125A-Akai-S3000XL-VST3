// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// P044: keep the original Akai hardware's *external* endless DATA ENTRY dial
// position coherent with the internal MAME m_last_dial saved in MAMESAVE.
// Both the actual physical input and its serialized position are 0..255.
// This helper only quantizes/validates host values; no guest memory edits.
#include <cmath>

namespace akai::dial_recall {
inline constexpr bool validPosition(int pos) noexcept {
    return pos >= 0 && pos <= 255;
}
inline constexpr float normalized(int pos) noexcept {
    return static_cast<float>(pos) / 255.0f;
}
inline bool decodeNormalized(float v, int& out) noexcept {
    if (!std::isfinite(v)) return false;
    if (v < 0.0f) v=0.0f;
    if (v > 1.0f) v=1.0f;
    const int decoded=static_cast<int>(v*255.0f+0.5f);
    if (!validPosition(decoded)) return false;
    out=decoded;
    return true;
}
} // namespace akai::dial_recall
