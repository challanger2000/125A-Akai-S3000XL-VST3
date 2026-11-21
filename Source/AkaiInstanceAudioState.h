#pragma once
// Per-MAME-instance audio routing and input-clock state: no shared static data.
#include <cstdint>
#include <string_view>
namespace akai::runtime {
struct OutputMap {
  int stride = 0, main = -1, indiv = -1;
  void begin(int outputs) noexcept { stride = outputs; main = -1; indiv = -1; }
  void observe(std::string_view tag, int first, int inputs) noexcept {
    if (first < 0 || inputs < 0 || first > stride || inputs > stride - first) return;
    if (tag == ":speaker" && inputs >= 2) main = first;
    if (tag == ":indiv" && inputs >= 8) indiv = first;
  }
  bool hasMain() const noexcept { return stride > 0 && main >= 0 && main < stride - 1; }
  bool hasIndividuals() const noexcept { return stride > 0 && indiv >= 0 && indiv <= stride - 8; }
};
struct InputMeter {
  std::uint64_t calls = 0, asked = 0, got = 0, lastDropped = 0, lastW = 0;
  int peakL = 0, peakR = 0;
  double lastT = 0.0;
};
struct DriftClock {
  std::uint32_t tick = 0;
  int next(std::uint64_t bufferedAfterRead, int delivered) noexcept {
    if (((++tick) & 63u) != 0 || delivered <= 0) return 0;
    if (bufferedAfterRead > 8192u + 2048u) return 1;
    if (bufferedAfterRead + 2048u < 8192u) return -1;
    return 0;
  }
};
struct AudioState {
  OutputMap output;
  InputMeter meter;
  DriftClock drift;
  void reset() noexcept { output = {}; meter = {}; drift = {}; }
};
} // namespace akai::runtime
