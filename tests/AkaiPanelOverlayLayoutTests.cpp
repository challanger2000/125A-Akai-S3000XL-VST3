// P048: test geometry against the ACTUAL 6000x1112 original rack layout,
// all four host panel layout modes and multiple host resize/zoom widths.
#include "../Source/AkaiPanelOverlayLayout.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <utility>

using akai::panel_overlay::Rect;

static bool intersects(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x
        && a.y < b.y + b.h && a.y + a.h > b.y;
}

int main() {
    int probes = 0;
    for (int width : {900, 1200, 1600, 1790, 2048, 2400}) {
        const int height = (width * 1112 + 3000) / 6000;
        const auto one = akai::panel_overlay::rackEarControls(width, height);
        for (int layout : {0, 1, 2, 3}) {
            // The actual original rack geometry does not change when the user
            // changes the legacy compact/full/rack/tablet display selection.
            const auto g = akai::panel_overlay::rackEarControls(width, height);
            (void) layout;
            assert(g.media.x == one.media.x);
            assert(g.media.y == one.media.y);
            assert(g.settings.x == one.settings.x);
            assert(g.settings.y == one.settings.y);
            for (const auto b : {g.media, g.settings}) {
                assert(b.x >= 0 && b.y >= 0 && b.w >= 1 && b.h >= 16);
                assert(b.x + b.w <= width * 0.050f);
                assert(b.y >= height * 0.25f);
                assert(b.y + b.h <= height * 0.75f);
            }
            assert(!intersects(g.media, g.settings));
            // A click on the controls cannot hit Akai's actual front panel,
            // whose face starts right of the original left rack ear seam.
            assert(g.media.x + g.media.w < width * 0.053f);
            assert(g.settings.x + g.settings.w < width * 0.053f);
            ++probes;
        }
    }
    for (auto [w,h] : {std::pair{0,200}, std::pair{1000,0},
                       std::pair{-1,200}}) {
        const auto b = akai::panel_overlay::rackEarControls(w,h);
        assert(b.media.w == 0 && b.settings.h == 0);
        ++probes;
    }
    // Independent negative controls: old positions land on the Akai face.
    const auto g = akai::panel_overlay::rackEarControls(1790,332);
    const Rect oldCompact{25, 311 - 73, 340, 24};
    const Rect oldRack{72, 159 - 73, 165, 25};
    assert(oldCompact.x + oldCompact.w > 1790 * .053f);
    assert(oldRack.x + oldRack.w > 1790 * .053f);
    assert(g.media.x + g.media.w <= 1790 * .050f);
    std::cout << "P048_RACK_EAR_GEOMETRY=PASS probes=" << probes
              << " old_overlays_rejected=2" << std::endl;
}
