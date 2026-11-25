#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <juce_core/juce_core.h>

namespace pedalboard {

// Musical note subdivision engine shared by PRE Filters (A-WAH / Step Filter),
// MOD (Chorus / Flanger / Phaser / Vibe / Tremolo), DLY (all 18 synced delays),
// and UI value-pill labels (displays "1/8D", "1/4", etc. when SYNC is ON).
namespace tempo_sync {

struct Division {
    const char* label;
    double beats; // Quarter note = 1.0 beat
};

// Delay divisions: strictly monotonic shortest -> longest (maps linearly from Time knob min..max)
inline constexpr std::array<Division, 11> kDelayDivisions {{
    { "1/16",  0.25 },
    { "1/8T",  1.0 / 3.0 },
    { "1/16D", 0.375 },
    { "1/8",   0.50 },
    { "1/4T",  2.0 / 3.0 },
    { "1/8D",  0.75 },
    { "1/4",   1.00 },
    { "1/2T",  4.0 / 3.0 },
    { "1/4D",  1.50 },
    { "1/2",   2.00 },
    { "1/1",   4.00 }
}};

// Modulation / Filter LFO divisions: strictly monotonic slowest -> fastest (maps linearly from Rate knob min..max)
inline constexpr std::array<Division, 13> kModDivisions {{
    { "4/1",   16.0 },
    { "2/1",   8.0 },
    { "1/1",   4.0 },
    { "1/2D",  3.0 },
    { "1/2",   2.0 },
    { "1/4D",  1.5 },
    { "1/4",   1.0 },
    { "1/8D",  0.75 },
    { "1/4T",  2.0 / 3.0 },
    { "1/8",   0.50 },
    { "1/8T",  1.0 / 3.0 },
    { "1/16",  0.25 },
    { "1/16T", 1.0 / 6.0 }
}};

inline int delayDivisionIndex(float timeMs, float minMs, float maxMs) noexcept {
    const float span = std::max(1.0f, maxMs - minMs);
    const float norm = std::clamp((timeMs - minMs) / span, 0.0f, 1.0f);
    return std::clamp(
        static_cast<int>(std::lround(norm * static_cast<float>(kDelayDivisions.size() - 1))),
        0,
        static_cast<int>(kDelayDivisions.size() - 1)
    );
}

inline float syncedDelayMs(float timeMs, float minMs, float maxMs, float bpm) noexcept {
    const double safeBpm = std::clamp(static_cast<double>(bpm), 30.0, 300.0);
    const int idx = delayDivisionIndex(timeMs, minMs, maxMs);
    const double ms = (60000.0 / safeBpm) * kDelayDivisions[static_cast<size_t>(idx)].beats;
    return static_cast<float>(std::clamp(ms, 20.0, 2250.0));
}

inline const char* delayDivisionLabel(float timeMs, float minMs, float maxMs) noexcept {
    return kDelayDivisions[static_cast<size_t>(delayDivisionIndex(timeMs, minMs, maxMs))].label;
}

inline int modDivisionIndex(float rateHz, float minHz, float maxHz) noexcept {
    const float span = std::max(0.01f, maxHz - minHz);
    const float norm = std::clamp((rateHz - minHz) / span, 0.0f, 1.0f);
    return std::clamp(
        static_cast<int>(std::lround(norm * static_cast<float>(kModDivisions.size() - 1))),
        0,
        static_cast<int>(kModDivisions.size() - 1)
    );
}

inline float syncedModRateHz(float rateHz, float minHz, float maxHz, float bpm) noexcept {
    const double safeBpm = std::clamp(static_cast<double>(bpm), 30.0, 300.0);
    const int idx = modDivisionIndex(rateHz, minHz, maxHz);
    const double hz = (safeBpm / 60.0) / kModDivisions[static_cast<size_t>(idx)].beats;
    return static_cast<float>(std::clamp(hz, 0.05, 16.0));
}

inline const char* modDivisionLabel(float rateHz, float minHz, float maxHz) noexcept {
    return kModDivisions[static_cast<size_t>(modDivisionIndex(rateHz, minHz, maxHz))].label;
}

} // namespace tempo_sync
} // namespace pedalboard
