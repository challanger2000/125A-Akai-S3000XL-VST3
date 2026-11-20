// Self-contained native test for the S3000XL analogue level contract.
// Run in a native Windows C++20 build (or locally with any C++20 compiler).
#include "../Source/AkaiAnalogControls.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

static int checked = 0;
static void check(bool condition, const char* name) {
    ++checked;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(EXIT_FAILURE); }
}
static bool near(float lhs, float rhs, float tol = 1.0e-6f) {
    return std::abs(lhs - rhs) <= tol;
}

int main() {
    using namespace AkaiAnalogControls;
    check(normalized(-0.1f) == 0.0f, "clamp_negative");
    check(normalized(0.0f) == 0.0f, "zero");
    check(normalized(1.0f) == 1.0f, "unity");
    check(normalized(1.1f) == 1.0f, "clamp_above_unity");
    check(normalized(std::numeric_limits<float>::quiet_NaN()) == 1.0f, "nan_fallback");
    check(normalized(std::numeric_limits<float>::infinity()) == 1.0f, "inf_fallback");
    check(recordInput(0.25f, 1.0f) == 0.25f, "original_unity_identity");
    check(recordInput(-0.5f, 1.0f) == -0.5f, "original_polarity_identity");
    check(recordInput(0.5f, 0.0f) == 0.0f, "rec_gain_zero");
    check(recordInput(0.5f, 0.5f) == 0.25f, "rec_gain_half");
    check(recordInput(-0.5f, 0.5f) == -0.25f, "stereo_right_gain");
    check(recordInput(std::numeric_limits<float>::quiet_NaN(), 1.0f) == 0.0f, "invalid_audio");
    const float c = smoothingCoefficient(48000.0f);
    check(c > 0.0f && c < 1.0f, "smoothing_coefficient");
    check(smoothingCoefficient(0.0f) == 1.0f, "invalid_samplerate_fallback");
    float value = 1.0f, previous = 1.0f;
    for (int i = 0; i < 4800; ++i) {
        value = smooth(value, 0.0f, c);
        check(value >= 0.0f && value <= previous, "monotone_ramp_down");
        previous = value;
    }
    check(value == 0.0f, "zero_reached_exactly");
    for (int i = 0; i < 4800; ++i) {
        previous = value;
        value = smooth(value, 1.0f, c);
        check(value <= 1.0f && value >= previous, "monotone_ramp_up");
    }
    check(value == 1.0f, "unity_reached_exactly");
    check(near(smooth(0.0f, 1.0f, 0.5f), 0.5f), "known_interpolation");
    std::printf("ANALOG_LEVEL_CONTRACT=PASS checks=%d\n", checked);
    return EXIT_SUCCESS;
}
