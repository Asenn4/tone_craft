#pragma once

#include <array>
#include <cstdint>
#include <juce_core/juce_core.h>

namespace pedalboard {

enum class ModuleId : uint8_t {
    NoiseGate    = 0,
    Compressor   = 1,
    Wah          = 2,
    Overdrive    = 3,
    AmpHead      = 4,
    CabinetIR    = 5,
    GraphicEq    = 6,
    StereoChorus = 7,
    StereoDelay  = 8,
    Reverb       = 9,
    Count        = 10
};

enum class RoutingMode : uint8_t {
    Serial          = 0, // [->]  Single Serial Path
    DualParallel    = 1, // [-<]  Dual Parallel Split (Shared Pre -> Path A + Path B -> Shared Post)
    TripleParallel  = 2, // [-<=] Triple Parallel (Shared Pre -> Path A + Path B + Path C -> MIX -> Shared Post)
    DualIndependent = 3  // [==]  Dual Independent Parallel Chains (Pre/Post Chain A || Pre/Post Chain B -> Out)
};

inline constexpr bool isParallelRouting(RoutingMode m) noexcept {
    return m != RoutingMode::Serial;
}

inline const char* routingModeDisplayName(RoutingMode m) noexcept {
    switch (m) {
        case RoutingMode::Serial:          return "SERIAL CHAIN";
        case RoutingMode::DualParallel:    return "DUAL SPLIT A/B";
        case RoutingMode::TripleParallel:  return "TRIPLE PARALLEL (A/B/C)";
        case RoutingMode::DualIndependent: return "DUAL INDEPENDENT (A || B)";
        default:                           return "SERIAL CHAIN";
    }
}

enum class BusLane : uint8_t {
    Pre   = 0, // Shared Pre (Dual/Triple)  | Chain A Pre-Amp (Dual Independent)
    PathA = 1, // Path A (Dual/Triple)      | Chain A Pre-Amp (Dual Independent)
    PathB = 2, // Path B (Dual/Triple)      | Chain B Pre-Amp (Dual Independent)
    Post  = 3, // Shared Post (Dual/Triple) | Chain B Post-Amp (Dual Independent)
    PathC = 4, // Path C (Triple Parallel)  | Fallback Path B (Dual Split) / Post A (Dual Indep)
    PostA = 5  // Chain A Post-Amp (Dual Independent) | Shared Post (Dual/Triple)
};

constexpr int kNumModules = static_cast<int>(ModuleId::Count);
using ChainOrder = std::array<ModuleId, kNumModules>;
using BusAssignments = std::array<BusLane, kNumModules>;

inline constexpr ChainOrder defaultChainOrder() noexcept {
    return {
        ModuleId::NoiseGate,
        ModuleId::Compressor,
        ModuleId::Wah,
        ModuleId::Overdrive,
        ModuleId::AmpHead,
        ModuleId::CabinetIR,
        ModuleId::GraphicEq,
        ModuleId::StereoChorus,
        ModuleId::StereoDelay,
        ModuleId::Reverb
    };
}

inline constexpr BusAssignments defaultBusAssignments() noexcept {
    // Indexed by ModuleId
    return {
        BusLane::Pre,   // NoiseGate
        BusLane::Pre,   // Compressor
        BusLane::Pre,   // Wah
        BusLane::PathA, // Overdrive
        BusLane::PathA, // AmpHead / NAM
        BusLane::PathA, // CabinetIR
        BusLane::PathB, // GraphicEq (parallel shaping / clean blend)
        BusLane::PathB, // StereoChorus (parallel stereo wet blend)
        BusLane::Post,  // StereoDelay
        BusLane::Post   // Reverb
    };
}

inline const char* moduleKey(ModuleId id) noexcept {
    switch (id) {
        case ModuleId::NoiseGate:    return "noise_gate";
        case ModuleId::Compressor:   return "compressor";
        case ModuleId::Wah:          return "wah";
        case ModuleId::Overdrive:    return "overdrive";
        case ModuleId::AmpHead:      return "amp_head";
        case ModuleId::CabinetIR:    return "cabinet_ir";
        case ModuleId::GraphicEq:    return "graphic_eq";
        case ModuleId::StereoChorus: return "stereo_chorus";
        case ModuleId::StereoDelay:  return "stereo_delay";
        case ModuleId::Reverb:       return "reverb";
        default:                     return "unknown";
    }
}

inline const char* moduleDisplayName(ModuleId id) noexcept {
    switch (id) {
        case ModuleId::NoiseGate:    return "NOISE GATE";
        case ModuleId::Compressor:   return "COMPRESSOR";
        case ModuleId::Wah:          return "WAH PEDAL";
        case ModuleId::Overdrive:    return "OVERDRIVE";
        case ModuleId::AmpHead:      return "AMP / NAM";
        case ModuleId::CabinetIR:    return "CABINET IR";
        case ModuleId::GraphicEq:    return "7-BAND EQ";
        case ModuleId::StereoChorus: return "CHORUS";
        case ModuleId::StereoDelay:  return "STEREO DELAY";
        case ModuleId::Reverb:       return "REVERB";
        default:                     return "MODULE";
    }
}

inline const char* moduleShortTag(ModuleId id) noexcept {
    switch (id) {
        case ModuleId::NoiseGate:    return "GATE";
        case ModuleId::Compressor:   return "COMP";
        case ModuleId::Wah:          return "WAH";
        case ModuleId::Overdrive:    return "DRIVE";
        case ModuleId::AmpHead:      return "AMP";
        case ModuleId::CabinetIR:    return "CAB";
        case ModuleId::GraphicEq:    return "EQ7";
        case ModuleId::StereoChorus: return "CHOR";
        case ModuleId::StereoDelay:  return "DELAY";
        case ModuleId::Reverb:       return "VERB";
        default:                     return "?";
    }
}

inline const char* busLaneKey(BusLane lane) noexcept {
    switch (lane) {
        case BusLane::Pre:   return "pre";
        case BusLane::PathA: return "path_a";
        case BusLane::PathB: return "path_b";
        case BusLane::Post:  return "post";
        case BusLane::PathC: return "path_c";
        case BusLane::PostA: return "post_a";
        default:             return "path_a";
    }
}

inline const char* busLaneShortLabel(BusLane lane) noexcept {
    switch (lane) {
        case BusLane::Pre:   return "PRE";
        case BusLane::PathA: return "A";
        case BusLane::PathB: return "B";
        case BusLane::Post:  return "POST";
        case BusLane::PathC: return "C";
        case BusLane::PostA: return "POST A";
        default:             return "A";
    }
}

inline const char* busLaneShortLabelForMode(BusLane lane, RoutingMode mode) noexcept {
    if (mode == RoutingMode::DualIndependent) {
        switch (lane) {
            case BusLane::Pre:
            case BusLane::PathA: return "PRE A";
            case BusLane::PostA:
            case BusLane::PathC: return "POST A";
            case BusLane::PathB: return "PRE B";
            case BusLane::Post:  return "POST B";
            default:             return "PRE A";
        }
    }
    if (mode == RoutingMode::TripleParallel) {
        switch (lane) {
            case BusLane::Pre:   return "PRE";
            case BusLane::PathA: return "A";
            case BusLane::PathB: return "B";
            case BusLane::PathC: return "C";
            case BusLane::Post:
            case BusLane::PostA: return "POST";
            default:             return "A";
        }
    }
    switch (lane) {
        case BusLane::Pre:   return "PRE";
        case BusLane::PathA: return "A";
        case BusLane::PathB:
        case BusLane::PathC: return "B";
        case BusLane::Post:
        case BusLane::PostA: return "POST";
        default:             return "A";
    }
}

inline BusLane busLaneFromKey(const juce::String& key) noexcept {
    if (key == "pre")    return BusLane::Pre;
    if (key == "path_a") return BusLane::PathA;
    if (key == "path_b") return BusLane::PathB;
    if (key == "post")   return BusLane::Post;
    if (key == "path_c") return BusLane::PathC;
    if (key == "post_a") return BusLane::PostA;
    return BusLane::PathA;
}

inline bool moduleFromKey(const juce::String& key, ModuleId& outId) noexcept {
    for (int i = 0; i < kNumModules; ++i) {
        const auto id = static_cast<ModuleId>(i);
        if (key == moduleKey(id)) {
            outId = id;
            return true;
        }
    }
    return false;
}

// 10 modules x 4 bits = 40 bits (fits in uint64_t for lock-free atomic updates)
inline uint64_t packChainOrder(const ChainOrder& order) noexcept {
    uint64_t packed = 0;
    for (int i = 0; i < kNumModules; ++i) {
        packed |= (static_cast<uint64_t>(static_cast<uint8_t>(order[static_cast<size_t>(i)]) & 0x0Fu) << (i * 4));
    }
    return packed;
}

inline ChainOrder unpackChainOrder(uint64_t packed) noexcept {
    ChainOrder result {};
    std::array<bool, kNumModules> seen {};
    for (int i = 0; i < kNumModules; ++i) {
        const uint8_t raw = static_cast<uint8_t>((packed >> (i * 4)) & 0x0Fu);
        if (raw >= kNumModules || seen[raw]) {
            return defaultChainOrder();
        }
        seen[raw] = true;
        result[static_cast<size_t>(i)] = static_cast<ModuleId>(raw);
    }
    return result;
}

// 10 modules x 3 bits = 30 bits (fits in uint32_t for lock-free atomic updates)
inline uint32_t packBusAssignments(const BusAssignments& buses) noexcept {
    uint32_t packed = 0;
    for (int i = 0; i < kNumModules; ++i) {
        packed |= (static_cast<uint32_t>(static_cast<uint8_t>(buses[static_cast<size_t>(i)]) & 0x07u) << (i * 3));
    }
    return packed;
}

inline BusAssignments unpackBusAssignments(uint32_t packed) noexcept {
    BusAssignments result {};
    for (int i = 0; i < kNumModules; ++i) {
        const uint8_t raw = static_cast<uint8_t>((packed >> (i * 3)) & 0x07u);
        result[static_cast<size_t>(i)] = (raw <= static_cast<uint8_t>(BusLane::PostA))
            ? static_cast<BusLane>(raw)
            : BusLane::PathA;
    }
    return result;
}

} // namespace pedalboard
