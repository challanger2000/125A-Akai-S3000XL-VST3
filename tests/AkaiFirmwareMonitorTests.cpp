// P002 - monitoring signal routing contract. This tests our bridge,
// not the physical meaning of Akai WADCS bit 5 (ROM-backed test remains due).
#include "../Source/AkaiFirmwareMonitor.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

static int checks = 0;
static void test(bool value, const char* label) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL %s\n", label);
        std::exit(1);
    }
}
static bool near(float a, float b) { return std::fabs(a-b) < 0.0001f; }

int main() {
    using AkaiFirmwareMonitor::Mode;
    using AkaiFirmwareMonitor::decode;
    using AkaiFirmwareMonitor::mix;
    test(decode(0x00, false) == Mode::Off, "startup_requires_real_firmware_value");
    test(decode(0xa2, true) == Mode::Off, "idle_A2_monitor_off");
    test(decode(0x82, true) == Mode::Prerecord, "armed_82_candidate_monitor_on");
    test(decode(0x8a, true) == Mode::Recording, "mono_GO_8A_no_host_bypass");
    test(decode(0x9a, true) == Mode::Recording, "stereo_GO_9A_no_host_bypass");
    test(decode(0xa2, true) == Mode::Off, "post_record_off");
    test(decode(0x80, true) == Mode::Prerecord, "unrelated_low_bits_ignore");
    test(decode(0x08, true) == Mode::Recording, "record_strobe_priority");
    test(decode(0x28, true) == Mode::Recording, "even_if_inhibit_during_record");
    test(near(mix(0.0f, 32767, 0.0f), 0.0f), "off_silent");
    test(near(mix(0.0f, 32767, 1.0f), 1.0f), "full_scale_positive");
    test(near(mix(0.0f, -32767, 1.0f), -1.0f), "full_scale_negative");
    test(near(mix(0.25f, 16384, 0.5f), 0.25f+0.5f*16384/32767), "mix_half");
    test(near(mix(0.3f, 0, 1.0f), 0.3f), "preserve_original_dsp");
    test(near(mix(0.0f, 12345, std::numeric_limits<float>::quiet_NaN()), 0.0f),
         "invalid_gain_fails_closed");
    test(near(mix(std::numeric_limits<float>::infinity(), 0, 0.0f), 0.0f),
         "nonfinite_host_output_sanitized");
    std::printf("MONITOR_ROUTING_CONTRACT=PASS checks=%d\n", checks);
    return 0;
}
