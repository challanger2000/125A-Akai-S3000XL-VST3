#pragma once
// P048 — original Akai S3000XL panel overlay geometry.
//
// MAME 0.288 renders the entire original 6000x1112 rack face into the
// JUCE editor. Its two unused outer rack ears are part of the artwork.
// On the LEFT ear (0..approximately 5.3 % of view width), between the
// mounting screws, is the ONLY safe surface for auxiliary host controls.
// Avoid legacy layout-specific 2048x1744 / 2048x379 / 2048x671
// coordinates: the view changes but the S3000XL rack artwork does not.
//
// No JUCE, MAME or audio dependencies: tested as an independent contract.
#include <algorithm>
#include <cmath>

namespace akai::panel_overlay {
struct Rect {
    int x, y, w, h;
};
struct Controls {
    Rect media;
    Rect settings;
};

inline Controls rackEarControls(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    const auto round = [](float v) noexcept {
        return static_cast<int>(std::lround(v));
    };

    // Keep the right edge before the panel's vertical rack-to-face seam
    // (~5.3 % width). Width and position track the ACTUAL rendered frame,
    // never any of the four legacy 'Panel Layout' base dimensions.
    const int x = std::max(3, round(width * 0.008f));
    const int right = round(width * 0.047f);
    const int w = std::max(1, right - x);

    // Mounting screws are near the top and bottom of this side rail.
    // A centred vertical 2-button stack remains between them at any
    // supported host zoom / resized editor width (900..2400).
    const int h = std::clamp(round(height * 0.067f), 16, 28);
    const int gap = std::clamp(round(height * 0.020f), 4, 10);
    const int top = (height - (2 * h + gap)) / 2;

    return {{x, top, w, h}, {x, top + h + gap, w, h}};
}
} // namespace akai::panel_overlay
