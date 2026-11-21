// SPDX-License-Identifier: GPL-3.0-only
// P044 source-independent regression for original Akai DATAENTRY input
// coherence. Actual owner-ROM A-vs-zero audibility remains a private gate.
#include "../Source/AkaiDialRecall.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

static int checks=0;
#define REQUIRE(e) do { ++checks; if (!(e)) { \
    std::fprintf(stderr,"P044 dial recall contract FAIL at line %d: %s\n",__LINE__,#e); \
    return EXIT_FAILURE; } } while(false)

// This is the actual pinned MAME 0.288 s3000_state::dial_timer_tick
// input delta contract; the native MAME binary, not this helper, is the
// owner-original firmware/audio oracle.
static int originalMameWheelDelta(int oldInput, int newInput) noexcept {
    int diff=newInput-oldInput;
    if(diff>0x80) diff=0x100-diff;
    if(diff< -0x80) diff=-0x100-diff;
    return diff;
}
int main() {
    using namespace akai::dial_recall;
    int out=-10;
    for(int position=0;position<256;++position) {
        REQUIRE(validPosition(position));
        REQUIRE(decodeNormalized(normalized(position),out));
        REQUIRE(out==position); // catches truncation 2/255 -> 1
    }
    REQUIRE(!validPosition(-1) && !validPosition(256));
    out=33;
    REQUIRE(!decodeNormalized(std::numeric_limits<float>::quiet_NaN(),out) && out==33);
    REQUIRE(!decodeNormalized(std::numeric_limits<float>::infinity(),out) && out==33);
    REQUIRE(!decodeNormalized(-std::numeric_limits<float>::infinity(),out) && out==33);
    REQUIRE(decodeNormalized(-0.9f,out) && out==0);
    REQUIRE(decodeNormalized(1.8f,out) && out==255);

    // Native owner Akai V2.0 original-ROM negative and positive fixtures:
    // the original loaded MAMESAVE contains m_last_dial==2 after selecting
    // MIDI 99. Default external DAW position==0 is an unintended -2 tick,
    // which selected PULSE; restoring actual dial==2 before MAME load did
    // yield source-dependent sound for nonzero A and silent B.
    constexpr int nativeSavedLastDial=2;
    constexpr int brokenExternalDefault=0;
    constexpr int correctExternal=2;
    REQUIRE(originalMameWheelDelta(nativeSavedLastDial,brokenExternalDefault)==-2);
    REQUIRE(originalMameWheelDelta(nativeSavedLastDial,correctExternal)==0);
    int roundTrip=0;
    REQUIRE(decodeNormalized(normalized(nativeSavedLastDial),roundTrip));
    REQUIRE(originalMameWheelDelta(nativeSavedLastDial,roundTrip)==0);
    REQUIRE(originalMameWheelDelta(255,0)==-1); // wrap boundary
    REQUIRE(originalMameWheelDelta(0,1)==1);
    std::printf("P044_AKAI_DATAENTRY_SAVED_EXTERNAL_POSITION_CONTRACT_PASS checks=%d\n",checks);
    return EXIT_SUCCESS;
}
