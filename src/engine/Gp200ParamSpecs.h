#pragma once

// Authentic Per-Model Parameter Specifications matching Daftar_Effect_GP-200.xlsx
// Covers ALL 265 Valeton GP-200 models across NR, PRE, WAH, DST, AMP (+ Neural NAM), CAB, EQ, MOD, DLY, RVB.

#include "../dsp/TempoSync.h"
#include "Gp200Catalogue.h"
#include "ModuleId.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace pedalboard {

constexpr int kMaxSlotParams = 12;

enum class ControlKind : uint8_t {
    Rotary          = 0,
    VerticalFader   = 1,
    Toggle          = 2,
    Choice          = 3,
    ExpressionPedal = 4
};

struct PedalSlotParamSpec {
    const char* label  = "";
    const char* suffix = "";
    float minVal       = 0.0f;
    float maxVal       = 10.0f;
    float defaultVal   = 5.0f;
    float step         = 0.1f;
    ControlKind kind   = ControlKind::Rotary;
    std::array<const char*, 12> choices {};
    int numChoices     = 0;

    bool isVerticalFader() const noexcept { return kind == ControlKind::VerticalFader; }
    bool isToggle()        const noexcept { return kind == ControlKind::Toggle; }
    bool isChoice()        const noexcept { return kind == ControlKind::Choice; }
    bool isExpression()    const noexcept { return kind == ControlKind::ExpressionPedal; }
};

namespace param_detail {

inline PedalSlotParamSpec makeRotary(const char* label, const char* suffix,
                                     float mn, float mx, float def, float stp = 0.1f) {
    PedalSlotParamSpec s;
    s.label = label; s.suffix = suffix;
    s.minVal = mn; s.maxVal = mx; s.defaultVal = def; s.step = stp;
    s.kind = ControlKind::Rotary;
    return s;
}

inline PedalSlotParamSpec makeFader(const char* label, float mn = -12.0f, float mx = 12.0f, float def = 0.0f) {
    PedalSlotParamSpec s;
    s.label = label; s.suffix = "dB";
    s.minVal = mn; s.maxVal = mx; s.defaultVal = def; s.step = 0.2f;
    s.kind = ControlKind::VerticalFader;
    return s;
}

inline PedalSlotParamSpec makeToggle(const char* label, float def = 0.0f) {
    PedalSlotParamSpec s;
    s.label = label; s.suffix = "";
    s.minVal = 0.0f; s.maxVal = 1.0f; s.defaultVal = def; s.step = 1.0f;
    s.kind = ControlKind::Toggle;
    return s;
}

inline PedalSlotParamSpec makeExpr(const char* label, float def = 0.5f) {
    PedalSlotParamSpec s;
    s.label = label; s.suffix = "";
    s.minVal = 0.0f; s.maxVal = 1.0f; s.defaultVal = def; s.step = 0.01f;
    s.kind = ControlKind::ExpressionPedal;
    return s;
}

inline PedalSlotParamSpec makeChoice(const char* label, std::initializer_list<const char*> items, int defIdx = 0) {
    PedalSlotParamSpec s;
    s.label = label; s.suffix = "";
    s.minVal = 0.0f;
    s.numChoices = std::min(12, static_cast<int>(items.size()));
    s.maxVal = static_cast<float>(std::max(0, s.numChoices - 1));
    s.defaultVal = static_cast<float>(std::clamp(defIdx, 0, std::max(0, s.numChoices - 1)));
    s.step = 1.0f;
    s.kind = ControlKind::Choice;
    int i = 0;
    for (const char* txt : items) {
        if (i < 12) s.choices[static_cast<size_t>(i++)] = txt;
    }
    return s;
}

} // namespace param_detail

inline constexpr int kNamPedalModelIndex = 33;

// True when a slot model runs Neural Amp Modeler (.nam) pedal capture processing.
inline bool isNamPedalSlotModel(ModuleId category, int modelIndex) noexcept {
    return category == ModuleId::Overdrive && modelIndex == kNamPedalModelIndex;
}

// True when a slot model uses the interactive Expression Pedal Rocker UI (all WAH + OCTA, Pitch, P-Bend).
inline bool isExpressionPedalSlotModel(ModuleId category, int modelIndex) noexcept {
    if (category == ModuleId::Wah) return true;
    if (category == ModuleId::Compressor && (modelIndex == 15 || modelIndex == 16 || modelIndex == 17)) return true;
    return false;
}

// Returns authentic per-model parameter specs directly matching Daftar_Effect_GP-200.xlsx.
inline std::vector<PedalSlotParamSpec> getSlotParamSpecs(ModuleId category, int modelIndex = 0) {
    using namespace param_detail;

    switch (category) {
        // ======================================================================
        // NR (2 Models: Gate 1, Gate 2)
        // ======================================================================
        case ModuleId::NoiseGate: {
            if (modelIndex <= 0) {
                // 130: Gate 1 (ISP Decimator): Threshold
                return {
                    makeRotary("THRESH", "dB", -80.0f, -10.0f, -52.0f, 0.5f)
                };
            }
            // 131: Gate 2: Threshold, Attack, Release
            return {
                makeRotary("THRESH",  "dB", -80.0f, -10.0f, -52.0f, 0.5f),
                makeRotary("ATTACK",  "ms",   0.5f,  50.0f,   3.0f, 0.5f),
                makeRotary("RELEASE", "ms",  10.0f, 500.0f,  95.0f, 1.0f)
            };
        }

        // ======================================================================
        // PRE (20 Models: Comp, Boost, Acoustic, Filter, Pitch, Special)
        // ======================================================================
        case ModuleId::Compressor: {
            switch (std::clamp(modelIndex, 0, 19)) {
                case 0: // 1: COMP (Ross Compressor): Sustain, Volume
                    return {
                        makeRotary("SUSTAIN", "%", 0.0f, 100.0f, 55.0f, 1.0f),
                        makeRotary("VOLUME",  "%", 0.0f, 100.0f, 60.0f, 1.0f)
                    };
                case 1: // 2: COMP4 (Keeley C4): Sustain, Attack, Volume, Clipping
                    return {
                        makeRotary("SUSTAIN",  "%", 0.0f, 100.0f, 55.0f, 1.0f),
                        makeRotary("ATTACK",   "%", 0.0f, 100.0f, 35.0f, 1.0f),
                        makeRotary("VOLUME",   "%", 0.0f, 100.0f, 60.0f, 1.0f),
                        makeRotary("CLIPPING", "%", 0.0f, 100.0f, 30.0f, 1.0f)
                    };
                case 2: // 3: S-Comp: Threshold, Ratio, Volume, Attack, Release, Tone, Blend
                    return {
                        makeRotary("THRESH",  "dB", -50.0f,   0.0f, -22.0f, 0.5f),
                        makeRotary("RATIO",   ":1",   1.2f,  20.0f,   4.0f, 0.1f),
                        makeRotary("VOLUME",  "dB",  -6.0f,  24.0f,   4.0f, 0.2f),
                        makeRotary("ATTACK",  "ms",   1.0f, 100.0f,  10.0f, 0.5f),
                        makeRotary("RELEASE", "ms",  20.0f, 800.0f, 120.0f, 1.0f),
                        makeRotary("TONE",    "",     0.0f,  10.0f,   5.5f, 0.1f),
                        makeRotary("BLEND",   "%",    0.0f, 100.0f,  85.0f, 1.0f)
                    };
                case 3: // 4: Micro Boost (MXR M133): Gain
                case 7: // 8: 14 Boost (Fortin Grind): Gain
                    return {
                        makeRotary("GAIN", "dB", 0.0f, 20.0f, 10.0f, 0.2f)
                    };
                case 4: // 5: AC Boost (Xotic AC Booster)
                case 5: // 6: B-Boost (Xotic BB Preamp): Gain, Volume, Bass/Treble
                case 6: // 7: P-Boost (Xotic RC Booster): Gain, Volume, Bass/Treble
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 5.0f, 0.1f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.0f, 0.1f),
                        makeRotary("BASS",   "", 0.0f, 10.0f, 5.0f, 0.1f),
                        makeRotary("TREBLE", "", 0.0f, 10.0f, 5.5f, 0.1f)
                    };
                case 8: // 9: FAT BB: Bass/Treble, Volume, Low Cut
                    return {
                        makeRotary("BASS",    "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("TREBLE",  "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("VOLUME",  "", 0.0f, 10.0f, 6.5f, 0.1f),
                        makeToggle("LOW CUT", 0.0f)
                    };
                case 9: // 10: Boost (Xotic EP Booster): Gain, +3dB, Bright
                    return {
                        makeRotary("GAIN",   "dB", 0.0f, 20.0f, 8.0f, 0.2f),
                        makeToggle("+3dB",   0.0f),
                        makeToggle("BRIGHT", 1.0f)
                    };
                case 10: // 11: AC Refiner: Shape
                    return {
                        makeRotary("SHAPE", "", 0.0f, 10.0f, 5.5f, 0.1f)
                    };
                case 11: // 12: AC Sim: Body, Top, Volume, Mode (Standard/Jumbo/Enhanced/Piezo)
                    return {
                        makeRotary("BODY",   "", 0.0f, 10.0f, 6.0f, 0.1f),
                        makeRotary("TOP",    "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.0f, 0.1f),
                        makeChoice("MODE", { "Standard", "Jumbo", "Enhanced", "Piezo" }, 0)
                    };
                case 12: // 13: T-Wah: Sens, Range, Q, Mix, Mode (Guitar/Bass)
                    return {
                        makeRotary("SENS",  "",  0.0f,  10.0f,   6.0f, 0.1f),
                        makeRotary("RANGE", "",  0.0f,  10.0f,   6.5f, 0.1f),
                        makeRotary("Q",     "",  1.5f,  10.0f,   4.5f, 0.1f),
                        makeRotary("MIX",   "%", 0.0f, 100.0f, 100.0f, 1.0f),
                        makeChoice("MODE", { "Guitar", "Bass" }, 0)
                    };
                case 13: // 14: A-WAH: Depth, Rate, Volume, Low, Q, High, Sync
                    return {
                        makeRotary("DEPTH",  "%",    0.0f,  100.0f,   75.0f, 1.0f),
                        makeRotary("RATE",   "Hz",   0.2f,    8.0f,    1.8f, 0.05f),
                        makeRotary("VOLUME", "",     0.0f,   10.0f,    6.5f, 0.1f),
                        makeRotary("LOW",    "Hz", 100.0f,  900.0f,  280.0f, 5.0f),
                        makeRotary("Q",      "",     1.5f,   10.0f,    4.5f, 0.1f),
                        makeRotary("HIGH",   "Hz",1000.0f, 4500.0f, 2400.0f, 25.0f),
                        makeToggle("SYNC",   0.0f)
                    };
                case 14: // 15: Step Filter: Step 1-4, Rate, Sync
                    return {
                        makeRotary("STEP 1", "",   0.0f, 10.0f, 2.0f, 0.1f),
                        makeRotary("STEP 2", "",   0.0f, 10.0f, 7.5f, 0.1f),
                        makeRotary("STEP 3", "",   0.0f, 10.0f, 4.0f, 0.1f),
                        makeRotary("STEP 4", "",   0.0f, 10.0f, 9.2f, 0.1f),
                        makeRotary("RATE",   "Hz", 0.5f, 12.0f, 3.2f, 0.1f),
                        makeToggle("SYNC",   0.0f)
                    };
                case 15: // 16: OCTA: Expression Pedal + Low Oct, High Oct, Dry + Auto-Sweep
                    return {
                        makeExpr  ("PEDAL",    0.65f),
                        makeRotary("LOW OCT",  "%", 0.0f, 100.0f, 80.0f, 1.0f),
                        makeRotary("HIGH OCT", "%", 0.0f, 100.0f, 65.0f, 1.0f),
                        makeRotary("DRY",      "%", 0.0f, 100.0f, 75.0f, 1.0f),
                        makeToggle("AUTO-SWEEP", 0.0f)
                    };
                case 16: // 17: Pitch: Expression Pedal + Low Pitch, Hi Pitch, Dry, Low Vol, Hi Vol + Auto-Sweep
                    return {
                        makeExpr  ("PEDAL",     1.0f),
                        makeRotary("LOW PITCH", "st", -24.0f,  24.0f, -12.0f, 1.0f),
                        makeRotary("HI PITCH",  "st", -24.0f,  24.0f,   7.0f, 1.0f),
                        makeRotary("DRY",       "%",    0.0f, 100.0f,  75.0f, 1.0f),
                        makeRotary("LOW VOL",   "%",    0.0f, 100.0f,  40.0f, 1.0f),
                        makeRotary("HI VOL",    "%",    0.0f, 100.0f,  65.0f, 1.0f),
                        makeToggle("AUTO-SWEEP", 0.0f)
                    };
                case 17: // 18: P-Bend (DigiTech Whammy): Expression Pedal + Hi Pitch, Low Pitch, Wet, Dry, Range + Auto-Sweep
                    return {
                        makeExpr  ("PEDAL",     0.0f),
                        makeRotary("HI PITCH",  "st", -24.0f,  24.0f,  12.0f, 1.0f),
                        makeRotary("LOW PITCH", "st", -24.0f,  24.0f,   0.0f, 1.0f),
                        makeRotary("WET",       "%",    0.0f, 100.0f, 100.0f, 1.0f),
                        makeRotary("DRY",       "%",    0.0f, 100.0f,   0.0f, 1.0f),
                        makeRotary("RANGE",     "%",   10.0f, 100.0f, 100.0f, 1.0f),
                        makeToggle("AUTO-SWEEP", 0.0f)
                    };
                case 18: // 19: Ring Mod: Mix, Freq, Fine, Tone
                    return {
                        makeRotary("MIX",  "%",   0.0f,  100.0f,  65.0f, 1.0f),
                        makeRotary("FREQ", "Hz", 40.0f, 1800.0f, 320.0f, 2.0f),
                        makeRotary("FINE", "Hz",-50.0f,   50.0f,   0.0f, 0.5f),
                        makeRotary("TONE", "",    0.0f,   10.0f,   6.0f, 0.1f)
                    };
                case 19: // 20: Saturate: Saturation, Mix, Volume, High Cut
                default:
                    return {
                        makeRotary("SAT",      "",      0.0f,    10.0f,    5.5f,  0.1f),
                        makeRotary("MIX",      "%",     0.0f,   100.0f,   85.0f,  1.0f),
                        makeRotary("VOLUME",   "",      0.0f,    10.0f,    6.0f,  0.1f),
                        makeRotary("HIGH CUT", "Hz", 1500.0f, 12000.0f, 6500.0f, 50.0f)
                    };
            }
        }

        // ======================================================================
        // WAH (5 Models: V-Wah, C-Wah, P-Wah, S-Wah, B-Wah)
        // ======================================================================
        case ModuleId::Wah: {
            if (modelIndex == 2) {
                // 23: P-Wah (John Petrucci Rack CryBaby): Pedal + Range, Q, Volume, EQ + Auto-Wah
                return {
                    makeExpr  ("PEDAL",    0.55f),
                    makeRotary("RANGE",    "", 0.0f, 10.0f, 7.0f, 0.1f),
                    makeRotary("Q",        "", 1.5f, 12.0f, 5.2f, 0.1f),
                    makeRotary("VOLUME",   "", 0.0f, 10.0f, 7.0f, 0.1f),
                    makeRotary("EQ",       "", 0.0f, 10.0f, 5.5f, 0.1f),
                    makeToggle("AUTO-WAH", 0.0f)
                };
            }
            // 21: V-Wah, 22: C-Wah, 24: S-Wah, 25: B-Wah: Pedal + Range, Q, Volume + Auto-Wah
            return {
                makeExpr  ("PEDAL",    0.55f),
                makeRotary("RANGE",    "", 0.0f, 10.0f, 6.5f, 0.1f),
                makeRotary("Q",        "", 1.5f, 12.0f, 4.5f, 0.1f),
                makeRotary("VOLUME",   "", 0.0f, 10.0f, 7.0f, 0.1f),
                makeToggle("AUTO-WAH", 0.0f)
            };
        }

        // ======================================================================
        // DST (34 Models: OD, Fuzz, Distortion, Bass Drive, Boost + Neural NAM Pedal)
        // ======================================================================
        case ModuleId::Overdrive: {
            switch (std::clamp(modelIndex, 0, kNamPedalModelIndex)) {
                case 2:  // 28: Yellow OD: Gain, Volume
                case 17: // 43: Plustortion (MXR Dist+): Gain, Volume
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 5.5f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.5f, 0.05f)
                    };
                case 6: // 32: Scream OD: Gain, Tone, Volume, Fat, Air
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 5.2f, 0.05f),
                        makeRotary("TONE",   "", 0.0f, 10.0f, 5.5f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.5f, 0.05f),
                        makeRotary("FAT",    "", 0.0f, 10.0f, 5.0f, 0.1f),
                        makeRotary("AIR",    "", 0.0f, 10.0f, 5.0f, 0.1f)
                    };
                case 8: // 34: Force OD (Fulltone OCD): Gain, Tone, Volume, Mode (HP/LP)
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 5.5f, 0.05f),
                        makeRotary("TONE",   "", 0.0f, 10.0f, 5.5f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.5f, 0.05f),
                        makeChoice("MODE", { "HP", "LP" }, 0)
                    };
                case 10: // 36: OD JCM800: Gain, Master Volume, Bass/Middle/Treble
                    return {
                        makeRotary("GAIN",       "", 0.0f, 10.0f, 6.0f, 0.05f),
                        makeRotary("MASTER VOL", "", 0.0f, 10.0f, 6.5f, 0.05f),
                        makeRotary("BASS",       "", 0.0f, 10.0f, 5.2f, 0.1f),
                        makeRotary("MIDDLE",     "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("TREBLE",     "", 0.0f, 10.0f, 5.8f, 0.1f)
                    };
                case 11: // 37: TaiChi OD (Zendrive): Gain, Tone, Volume, Voice
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 5.2f, 0.05f),
                        makeRotary("TONE",   "", 0.0f, 10.0f, 5.5f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.5f, 0.05f),
                        makeRotary("VOICE",  "", 0.0f, 10.0f, 5.5f, 0.1f)
                    };
                case 12: // 38: Timmy OD: Gain, Volume, Bass/Treble, Mode (I/II/III)
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 4.8f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.5f, 0.05f),
                        makeRotary("BASS",   "", 0.0f, 10.0f, 5.0f, 0.1f),
                        makeRotary("TREBLE", "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeChoice("MODE", { "Mode I", "Mode II", "Mode III" }, 0)
                    };
                case 13: // 39: Precise OD (Precision Drive): Gain, Tone, Volume, Attack (1-6), Gate
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 4.5f, 0.05f),
                        makeRotary("TONE",   "", 0.0f, 10.0f, 6.0f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.8f, 0.05f),
                        makeRotary("GATE",   "", 0.0f, 10.0f, 3.0f, 0.1f),
                        makeChoice("ATTACK", { "Atk 1", "Atk 2", "Atk 3", "Atk 4", "Atk 5", "Atk 6" }, 2)
                    };
                case 14: // 40: Lazaro Fuzz (Big Muff Pi): Sustain, Tone, Volume
                    return {
                        makeRotary("SUSTAIN", "", 0.0f, 10.0f, 6.5f, 0.05f),
                        makeRotary("TONE",    "", 0.0f, 10.0f, 5.2f, 0.05f),
                        makeRotary("VOLUME",  "", 0.0f, 10.0f, 6.2f, 0.05f)
                    };
                case 15: // 41: Red Haze (Fuzz Face): Fuzz, Volume
                case 16: // 42: Sora Fuzz (Tone Bender): Fuzz, Volume
                    return {
                        makeRotary("FUZZ",   "", 0.0f, 10.0f, 7.0f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.2f, 0.05f)
                    };
                case 19: // 45: Darktale (ProCo The Rat): Gain, Filter, Volume
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 6.2f, 0.05f),
                        makeRotary("FILTER", "", 0.0f, 10.0f, 4.2f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.5f, 0.05f)
                    };
                case 20: // 46: Chief (Marshall Guv'nor): Gain, Volume, Bass/Middle/Treble
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 6.2f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.5f, 0.05f),
                        makeRotary("BASS",   "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("MIDDLE", "", 0.0f, 10.0f, 5.2f, 0.1f),
                        makeRotary("TREBLE", "", 0.0f, 10.0f, 5.8f, 0.1f)
                    };
                case 21: // 47: Master Dist (Shredmaster): Gain, Volume, Bass/Contour/Treble
                    return {
                        makeRotary("GAIN",    "", 0.0f, 10.0f, 7.2f, 0.05f),
                        makeRotary("VOLUME",  "", 0.0f, 10.0f, 6.5f, 0.05f),
                        makeRotary("BASS",    "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("CONTOUR", "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("TREBLE",  "", 0.0f, 10.0f, 6.0f, 0.1f)
                    };
                case 23: // 49: Flex OD: Gain, Tone, Volume, Blend, Mode (Normal/Scoop/Edge)
                    return {
                        makeRotary("GAIN",   "",  0.0f,  10.0f,  5.5f, 0.05f),
                        makeRotary("TONE",   "",  0.0f,  10.0f,  5.5f, 0.05f),
                        makeRotary("VOLUME", "",  0.0f,  10.0f,  6.5f, 0.05f),
                        makeRotary("BLEND",  "%", 0.0f, 100.0f, 75.0f, 1.0f),
                        makeChoice("MODE", { "Normal", "Scoop", "Edge" }, 0)
                    };
                case 24: // 50: Bass OD: Gain, Blend, Volume, Bass/Treble
                    return {
                        makeRotary("GAIN",   "",  0.0f,  10.0f,  5.5f, 0.05f),
                        makeRotary("BLEND",  "%", 0.0f, 100.0f, 65.0f, 1.0f),
                        makeRotary("VOLUME", "",  0.0f,  10.0f,  6.5f, 0.05f),
                        makeRotary("BASS",   "",  0.0f,  10.0f,  5.5f, 0.1f),
                        makeRotary("TREBLE", "",  0.0f,  10.0f,  5.5f, 0.1f)
                    };
                case 25: // 51: Black Bass (Darkglass B7K): Gain, Blend, Volume, Low/Lo-Mid/Hi-Mid/Treble, Attack (Cut/Boost/Flat)
                    return {
                        makeRotary("GAIN",   "",  0.0f,  10.0f,  6.2f, 0.05f),
                        makeRotary("BLEND",  "%", 0.0f, 100.0f, 65.0f, 1.0f),
                        makeRotary("VOLUME", "",  0.0f,  10.0f,  6.5f, 0.05f),
                        makeRotary("LOW",    "",  0.0f,  10.0f,  5.5f, 0.1f),
                        makeRotary("LO-MID", "",  0.0f,  10.0f,  5.0f, 0.1f),
                        makeRotary("HI-MID", "",  0.0f,  10.0f,  5.8f, 0.1f),
                        makeRotary("TREBLE", "",  0.0f,  10.0f,  5.8f, 0.1f),
                        makeChoice("ATTACK", { "Cut", "Flat", "Boost" }, 2)
                    };
                case 26: // 52: Micro Boost: Gain
                case 30: // 56: 14 Boost: Gain
                    return {
                        makeRotary("GAIN", "dB", 0.0f, 20.0f, 10.0f, 0.2f)
                    };
                case 27: // 53: AC Boost
                case 28: // 54: B-Boost: Gain, Volume, Bass/Treble
                case 29: // 55: P-Boost: Gain, Volume, Bass/Treble
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 5.0f, 0.1f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.0f, 0.1f),
                        makeRotary("BASS",   "", 0.0f, 10.0f, 5.0f, 0.1f),
                        makeRotary("TREBLE", "", 0.0f, 10.0f, 5.5f, 0.1f)
                    };
                case 31: // 57: FAT BB: Bass/Treble, Volume, Low Cut
                    return {
                        makeRotary("BASS",    "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("TREBLE",  "", 0.0f, 10.0f, 5.5f, 0.1f),
                        makeRotary("VOLUME",  "", 0.0f, 10.0f, 6.5f, 0.1f),
                        makeToggle("LOW CUT", 0.0f)
                    };
                case 32: // 58: Boost (EP Booster): Gain, +3dB, Bright
                    return {
                        makeRotary("GAIN",   "dB", 0.0f, 20.0f, 8.0f, 0.2f),
                        makeToggle("+3dB",   0.0f),
                        makeToggle("BRIGHT", 1.0f)
                    };
                case 33: // 998: Neural NAM Pedal (.nam Overdrive / Distortion / Fuzz / Preamp capture)
                    return {
                        makeRotary("INPUT",  "dB", -18.0f,  18.0f,   0.0f, 0.2f),
                        makeRotary("OUTPUT", "dB", -18.0f,  18.0f,   0.0f, 0.2f),
                        makeRotary("TONE",   "",     0.0f,  10.0f,   5.0f, 0.1f),
                        makeRotary("MIX",    "%",    0.0f, 100.0f, 100.0f, 1.0f)
                    };
                default: // 26:Green OD, 27:OD 9, 29:Penesas, 30:Swarm, 31:Super OD, 33:Blues OD, 35:Blues Master, 44:SM Dist, 48:La Charger
                    return {
                        makeRotary("GAIN",   "", 0.0f, 10.0f, 5.2f, 0.05f),
                        makeRotary("TONE",   "", 0.0f, 10.0f, 5.5f, 0.05f),
                        makeRotary("VOLUME", "", 0.0f, 10.0f, 6.5f, 0.05f)
                    };
            }
        }

        // ======================================================================
        // EQ (6 Models: Exact Excel band counts & frequencies!)
        // ======================================================================
        case ModuleId::GraphicEq: {
            switch (std::clamp(modelIndex, 0, 5)) {
                case 0: // 203: Guitar EQ 1: 125Hz, 400Hz, 800Hz, 1.6kHz, 4kHz; Volume
                    return {
                        makeFader("125"), makeFader("400"), makeFader("800"),
                        makeFader("1.6k"), makeFader("4k"), makeFader("VOL")
                    };
                case 1: // 204: Guitar EQ 2: 100Hz, 500Hz, 1kHz, 3kHz, 6kHz; Volume
                    return {
                        makeFader("100"), makeFader("500"), makeFader("1k"),
                        makeFader("3k"), makeFader("6k"), makeFader("VOL")
                    };
                case 2: // 205: Bass EQ 1: 33Hz, 150Hz, 600Hz, 2kHz, 8kHz; Volume
                    return {
                        makeFader("33"), makeFader("150"), makeFader("600"),
                        makeFader("2k"), makeFader("8k"), makeFader("VOL")
                    };
                case 3: // 206: Bass EQ 2: 50Hz, 120Hz, 400Hz, 800Hz, 4.5kHz; Volume
                    return {
                        makeFader("50"), makeFader("120"), makeFader("400"),
                        makeFader("800"), makeFader("4.5k"), makeFader("VOL")
                    };
                case 4: // 207: Mess EQ (Mesa Mark 5-band V-EQ): 80Hz, 240Hz, 750Hz, 2.2kHz, 6.6kHz
                    return {
                        makeFader("80"), makeFader("240"), makeFader("750"),
                        makeFader("2.2k"), makeFader("6.6k")
                    };
                case 5: // 208: Hyper EQ (10-band graphic EQ + Volume): 31..16k + VOL
                default:
                    return {
                        makeFader("31"),  makeFader("63"),  makeFader("125"), makeFader("250"),
                        makeFader("500"), makeFader("1k"),  makeFader("2k"),  makeFader("4k"),
                        makeFader("8k"),  makeFader("16k"), makeFader("VOL")
                    };
            }
        }

        // ======================================================================
        // MOD (24 Models: Chorus, Flanger, Vibrato, Phaser, Vibe, Tremolo, Detune, Bit Smash, Auto Swell)
        // ======================================================================
        case ModuleId::StereoChorus: {
            switch (std::clamp(modelIndex, 0, 23)) {
                case 1: // 210: C-Chorus (Boss Dimension C 4-button): Mode (4 mode chorus)
                    return {
                        makeChoice("MODE", { "Mode 1", "Mode 2", "Mode 3", "Mode 4" }, 1)
                    };
                case 2: // 211: B-Chorus (Bass Ensemble Chorus)
                    return {
                        makeRotary("DEPTH", "%",  0.0f, 100.0f, 50.0f, 1.0f),
                        makeRotary("RATE",  "Hz", 0.1f,   6.0f,  0.9f, 0.05f),
                        makeRotary("MIX",   "%",  0.0f, 100.0f, 45.0f, 1.0f),
                        makeToggle("SYNC",  0.0f)
                    };
                case 3: // 212: M-Chorus (Tri-Stereo): Mix, Rate, Filter, Depth L/C/R, Sync
                    return {
                        makeRotary("MIX",     "%",  0.0f, 100.0f, 50.0f, 1.0f),
                        makeRotary("RATE",    "Hz", 0.1f,   6.0f,  0.8f, 0.05f),
                        makeRotary("FILTER",  "",   0.0f,  10.0f,  6.0f, 0.1f),
                        makeRotary("DEPTH L", "%",  0.0f, 100.0f, 55.0f, 1.0f),
                        makeRotary("DEPTH C", "%",  0.0f, 100.0f, 45.0f, 1.0f),
                        makeRotary("DEPTH R", "%",  0.0f, 100.0f, 55.0f, 1.0f),
                        makeToggle("SYNC",    0.0f)
                    };
                case 4: // 213: Jet: Depth, Rate, Pre Delay, Feedback, Sync
                case 5: // 214: B-Jet
                case 6: // 215: N-Jet
                    return {
                        makeRotary("DEPTH",     "%",  0.0f, 100.0f, 55.0f, 1.0f),
                        makeRotary("RATE",      "Hz", 0.05f,  5.0f,  0.5f, 0.05f),
                        makeRotary("PRE DELAY", "ms", 0.5f,  15.0f,  3.5f, 0.1f),
                        makeRotary("FEEDBACK",  "%",  0.0f,  90.0f, 55.0f, 1.0f),
                        makeToggle("SYNC",      0.0f)
                    };
                case 7: // 216: Trem Jet: Flg Depth, Flg Rate, Feedback, Trm Depth, Trm Rate, Flg Sync, Trm Sync
                    return {
                        makeRotary("FLG DEPTH", "%",  0.0f, 100.0f, 50.0f, 1.0f),
                        makeRotary("FLG RATE",  "Hz", 0.05f,  5.0f,  0.5f, 0.05f),
                        makeRotary("FEEDBACK",  "%",  0.0f,  90.0f, 45.0f, 1.0f),
                        makeRotary("TRM DEPTH", "%",  0.0f, 100.0f, 45.0f, 1.0f),
                        makeRotary("TRM RATE",  "Hz", 0.5f,  10.0f,  3.5f, 0.1f),
                        makeToggle("FLG SYNC",  0.0f),
                        makeToggle("TRM SYNC",  0.0f)
                    };
                case 8:  // 217: V-Roto: Depth, Rate, Sync
                case 12: // 221: G-Phase: Depth, Rate, Sync
                case 15: // 224: M-Vibe: Depth, Rate, Sync
                case 17: // 226: O-Trem: Depth, Rate, Sync
                    return {
                        makeRotary("DEPTH", "%",  0.0f, 100.0f, 55.0f, 1.0f),
                        makeRotary("RATE",  "Hz", 0.1f,   8.0f,  1.6f, 0.05f),
                        makeToggle("SYNC",  0.0f)
                    };
                case 11: // 220: O-Phase (MXR Phase 90): Rate, Sync
                    return {
                        makeRotary("RATE", "Hz", 0.1f, 8.0f, 1.2f, 0.05f),
                        makeToggle("SYNC", 0.0f)
                    };
                case 13: // 222: S-Phase (Small Stone): Rate, Color (Warm/Sharp), Sync
                    return {
                        makeRotary("RATE",  "Hz", 0.1f, 8.0f, 1.2f, 0.05f),
                        makeChoice("COLOR", { "Warm", "Sharp" }, 0),
                        makeToggle("SYNC",  0.0f)
                    };
                case 14: // 223: Pan Phase: Phs Depth, Phs Rate, Pan Depth, Pan Rate, Phs Sync, Pan Sync
                    return {
                        makeRotary("PHS DEPTH", "%",  0.0f, 100.0f, 55.0f, 1.0f),
                        makeRotary("PHS RATE",  "Hz", 0.1f,   6.0f,  0.8f, 0.05f),
                        makeRotary("PAN DEPTH", "%",  0.0f, 100.0f, 60.0f, 1.0f),
                        makeRotary("PAN RATE",  "Hz", 0.2f,   8.0f,  2.2f, 0.05f),
                        makeToggle("PHS SYNC",  0.0f),
                        makeToggle("PAN SYNC",  0.0f)
                    };
                case 16: // 225: Vibe (Shin-Ei Uni-Vibe): Depth, Rate, Volume, Mode (Chorus/Vibrato), Sync
                    return {
                        makeRotary("DEPTH",  "%",  0.0f, 100.0f, 65.0f, 1.0f),
                        makeRotary("RATE",   "Hz", 0.2f,   8.0f,  1.8f, 0.05f),
                        makeRotary("VOLUME", "",   0.0f,  10.0f,  6.5f, 0.1f),
                        makeChoice("MODE", { "Chorus", "Vibrato" }, 0),
                        makeToggle("SYNC",   0.0f)
                    };
                case 20: // 229: Bias Trem: Depth, Rate, Volume, Bias, Sync
                    return {
                        makeRotary("DEPTH",  "%",  0.0f, 100.0f, 60.0f, 1.0f),
                        makeRotary("RATE",   "Hz", 0.5f,  10.0f,  3.8f, 0.1f),
                        makeRotary("VOLUME", "",   0.0f,  10.0f,  6.5f, 0.1f),
                        makeRotary("BIAS",   "%",  0.0f, 100.0f, 50.0f, 1.0f),
                        makeToggle("SYNC",   0.0f)
                    };
                case 21: // 230: Detune: Detune (-50..+50 cents), Dry, Wet
                    return {
                        makeRotary("DETUNE", "ct", -50.0f,  50.0f, 14.0f, 1.0f),
                        makeRotary("DRY",    "%",    0.0f, 100.0f, 80.0f, 1.0f),
                        makeRotary("WET",    "%",    0.0f, 100.0f, 55.0f, 1.0f)
                    };
                case 22: // 231: Bit Smash: Mix, Krush, Bit, Hi Cut, Lo Cut
                    return {
                        makeRotary("MIX",    "%",    0.0f,   100.0f,   65.0f,  1.0f),
                        makeRotary("KRUSH",  "x",    1.0f,    32.0f,    6.0f,  1.0f),
                        makeRotary("BIT",    "bit",  2.0f,    16.0f,    8.0f,  0.5f),
                        makeRotary("HI CUT", "Hz", 800.0f, 16000.0f, 8500.0f, 50.0f),
                        makeRotary("LO CUT", "Hz",  20.0f,  1200.0f,   80.0f, 10.0f)
                    };
                case 23: // 232: Auto Swell: Attack, Curve (Line/Exp/Log)
                    return {
                        makeRotary("ATTACK", "ms", 20.0f, 1200.0f, 260.0f, 5.0f),
                        makeChoice("CURVE", { "Line", "Exp", "Log" }, 1)
                    };
                default: // 0:G-Chorus, 9:G-Roto, 10:Vibrato, 18:Sine Trem, 19:Triangle Trem: Depth, Rate, Volume, Sync
                    return {
                        makeRotary("DEPTH",  "%",  0.0f, 100.0f, 52.0f, 1.0f),
                        makeRotary("RATE",   "Hz", 0.1f,   8.0f,  1.4f, 0.05f),
                        makeRotary("VOLUME", "",   0.0f,  10.0f,  6.5f, 0.1f),
                        makeToggle("SYNC",   0.0f)
                    };
            }
        }

        // ======================================================================
        // DLY (19 Models: Stereo, Tape, Multi-Tap, Analog, Sweep, Ring, M-Echo, Vintage, Lofi, Reverse)
        // ======================================================================
        case ModuleId::StereoDelay: {
            switch (std::clamp(modelIndex, 0, 18)) {
                case 0: // 233: BBD Delay S: Mix, Feedback, Time, Time R%, Spread, Level, Sync, Trail
                case 1: // 234: Digital Delay S
                    return {
                        makeRotary("MIX",      "%",   0.0f,  100.0f,  30.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,   92.0f,  38.0f, 1.0f),
                        makeRotary("TIME",     "ms", 30.0f, 1500.0f, 380.0f, 1.0f),
                        makeRotary("TIME R",   "%",  25.0f,  200.0f,  75.0f, 1.0f),
                        makeRotary("SPREAD",   "%",   0.0f,  100.0f,  70.0f, 1.0f),
                        makeRotary("LEVEL",    "",    0.0f,   10.0f,   6.5f, 0.1f),
                        makeToggle("SYNC",     0.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
                case 2: // 235: Tape Delay S: Mix, Feedback, Time, Time R%, Spread, Wow&Flutter, Age, Scrape, Drive, Level, Sync, Trail
                    return {
                        makeRotary("MIX",     "%",   0.0f,  100.0f,  32.0f, 1.0f),
                        makeRotary("FDBK",    "%",   0.0f,   92.0f,  40.0f, 1.0f),
                        makeRotary("TIME",    "ms", 30.0f, 1500.0f, 400.0f, 1.0f),
                        makeRotary("TIME R",  "%",  25.0f,  200.0f,  75.0f, 1.0f),
                        makeRotary("SPREAD",  "%",   0.0f,  100.0f,  65.0f, 1.0f),
                        makeRotary("WOW/FLT", "%",   0.0f,  100.0f,  35.0f, 1.0f),
                        makeRotary("AGE",     "%",   0.0f,  100.0f,  40.0f, 1.0f),
                        makeRotary("SCRAPE",  "%",   0.0f,  100.0f,  25.0f, 1.0f),
                        makeRotary("DRIVE",   "%",   0.0f,  100.0f,  35.0f, 1.0f),
                        makeRotary("LEVEL",   "",    0.0f,   10.0f,   6.5f, 0.1f),
                        makeToggle("SYNC",    0.0f),
                        makeToggle("TRAIL",   1.0f)
                    };
                case 3: // 236: Ambience 1: Mix, Feedback, Time, Level, Mod, Tone, Sync, Trail
                case 4: // 237: Ambience 2
                    return {
                        makeRotary("MIX",      "%",   0.0f,  100.0f,  32.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,   92.0f,  42.0f, 1.0f),
                        makeRotary("TIME",     "ms", 30.0f, 1500.0f, 420.0f, 1.0f),
                        makeRotary("LEVEL",    "",    0.0f,   10.0f,   6.5f, 0.1f),
                        makeRotary("MOD",      "%",   0.0f,  100.0f,  35.0f, 1.0f),
                        makeRotary("TONE",     "",    0.0f,   10.0f,   5.5f, 0.1f),
                        makeToggle("SYNC",     0.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
                case 9: // 242: Slapback: Mix, Time, Feedback, Trail
                    return {
                        makeRotary("MIX",      "%",   0.0f, 100.0f,  35.0f, 1.0f),
                        makeRotary("TIME",     "ms", 20.0f, 180.0f, 105.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,  60.0f,  12.0f, 1.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
                case 10: // 243: Sweep Echo: Mix, Time, Feedback, Sweep Depth, Sweep Rate, Sweep Sync, Time Sync, Trail
                    return {
                        makeRotary("MIX",        "%",   0.0f,  100.0f,  32.0f, 1.0f),
                        makeRotary("TIME",       "ms", 30.0f, 1500.0f, 380.0f, 1.0f),
                        makeRotary("FEEDBACK",   "%",   0.0f,   92.0f,  42.0f, 1.0f),
                        makeRotary("SWP DEPTH",  "%",   0.0f,  100.0f,  60.0f, 1.0f),
                        makeRotary("SWP RATE",   "Hz",  0.1f,    6.0f,   1.2f, 0.05f),
                        makeToggle("SWP SYNC",   0.0f),
                        makeToggle("TIME SYNC",  0.0f),
                        makeToggle("TRAIL",      1.0f)
                    };
                case 11: // 244: Ring Echo: Dly Mix, Time, Feedback, Ring Mix, Freq, Tone, Sync, Trail
                    return {
                        makeRotary("DLY MIX",  "%",   0.0f,  100.0f,  32.0f, 1.0f),
                        makeRotary("TIME",     "ms", 30.0f, 1500.0f, 380.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,   92.0f,  40.0f, 1.0f),
                        makeRotary("RING MIX", "%",   0.0f,  100.0f,  55.0f, 1.0f),
                        makeRotary("FREQ",     "Hz", 50.0f, 1200.0f, 260.0f, 2.0f),
                        makeRotary("TONE",     "",    0.0f,   10.0f,   5.5f, 0.1f),
                        makeToggle("SYNC",     0.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
                case 13: // 246: M-Echo (4-head tape): Mix, Time, Feedback, Tone, Mode (Head 1..12), Sync, Trail
                    return {
                        makeRotary("MIX",      "%",   0.0f,  100.0f,  32.0f, 1.0f),
                        makeRotary("TIME",     "ms", 30.0f, 1500.0f, 400.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,   92.0f,  42.0f, 1.0f),
                        makeRotary("TONE",     "",    0.0f,   10.0f,   5.5f, 0.1f),
                        makeChoice("MODE", {
                            "Head 1", "Head 2", "Head 3", "Head 4",
                            "Head 1+2", "Head 1+3", "Head 1+4", "Head 2+3",
                            "Head 2+4", "Head 3+4", "Head 1+2+3", "All 4 Heads"
                        }, 5),
                        makeToggle("SYNC",     0.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
                case 14: // 247: Sweet Echo (20-300ms): Mix, Time, Feedback, Sync, Trail
                    return {
                        makeRotary("MIX",      "%",   0.0f, 100.0f,  30.0f, 1.0f),
                        makeRotary("TIME",     "ms", 20.0f, 300.0f, 220.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,  90.0f,  38.0f, 1.0f),
                        makeToggle("SYNC",     0.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
                case 16: // 249: Vintage Rack: Mix, Time, Feedback, Mod, Tone, Sync, Trail
                case 17: // 250: Lofi Echo
                    return {
                        makeRotary("MIX",      "%",   0.0f,  100.0f,  28.0f, 1.0f),
                        makeRotary("TIME",     "ms", 30.0f, 1500.0f, 360.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,   92.0f,  36.0f, 1.0f),
                        makeRotary("MOD",      "%",   0.0f,  100.0f,  30.0f, 1.0f),
                        makeRotary("TONE",     "",    0.0f,   10.0f,   5.5f, 0.1f),
                        makeToggle("SYNC",     0.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
                case 18: // 251: Rev Echo: Mix, Time, Feedback, Level, Sync, Trail
                    return {
                        makeRotary("MIX",      "%",   0.0f,  100.0f,  36.0f, 1.0f),
                        makeRotary("TIME",     "ms", 50.0f, 1500.0f, 440.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,   90.0f,  38.0f, 1.0f),
                        makeRotary("LEVEL",    "",    0.0f,   10.0f,   6.5f, 0.1f),
                        makeToggle("SYNC",     0.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
                default: // 5:Pure, 6:Analog, 7:Tape, 8:Ping Pong, 12:Tube, 15:999 Echo: Mix, Time, Feedback, Sync, Trail
                    return {
                        makeRotary("MIX",      "%",   0.0f,  100.0f,  28.0f, 1.0f),
                        makeRotary("TIME",     "ms", 30.0f, 1500.0f, 380.0f, 1.0f),
                        makeRotary("FEEDBACK", "%",   0.0f,   92.0f,  36.0f, 1.0f),
                        makeToggle("SYNC",     0.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
            }
        }

        // ======================================================================
        // RVB (13 Models: Room, Hall, Church, Plate, Spring, Studio, Club, Concert, Arena, N-Star, Deepsea, Sweet Space, Shimmer)
        // ======================================================================
        case ModuleId::Reverb:
        default: {
            switch (std::clamp(modelIndex, 0, 12)) {
                case 0: // 252: Room: Mix, Pre Delay, Decay, Trail
                case 1: // 253: Hall
                case 2: // 254: Church
                    return {
                        makeRotary("MIX",       "%",  0.0f, 100.0f, 24.0f, 1.0f),
                        makeRotary("PRE DELAY", "ms", 0.0f, 150.0f, 18.0f, 1.0f),
                        makeRotary("DECAY",     "%",  0.0f, 100.0f, 48.0f, 1.0f),
                        makeToggle("TRAIL",     1.0f)
                    };
                case 3: // 255: Plate: Mix, Decay, High Damp, Trail
                    return {
                        makeRotary("MIX",       "%", 0.0f, 100.0f, 25.0f, 1.0f),
                        makeRotary("DECAY",     "%", 0.0f, 100.0f, 52.0f, 1.0f),
                        makeRotary("HIGH DAMP", "%", 0.0f, 100.0f, 40.0f, 1.0f),
                        makeToggle("TRAIL",     1.0f)
                    };
                case 4: // 256: Spring: Mix, Decay, Tone, Trail
                    return {
                        makeRotary("MIX",   "%", 0.0f, 100.0f, 26.0f, 1.0f),
                        makeRotary("DECAY", "%", 0.0f, 100.0f, 45.0f, 1.0f),
                        makeRotary("TONE",  "",  0.0f,  10.0f,  5.8f, 0.1f),
                        makeToggle("TRAIL", 1.0f)
                    };
                case 5: // 257: Studio: Mix, Pre Delay, Decay, Low Damp, Hi Damp, Mod, Trail
                case 6: // 258: Club
                case 7: // 259: Concert
                case 8: // 260: Arena
                    return {
                        makeRotary("MIX",       "%",  0.0f, 100.0f, 26.0f, 1.0f),
                        makeRotary("PRE DELAY", "ms", 0.0f, 150.0f, 22.0f, 1.0f),
                        makeRotary("DECAY",     "%",  0.0f, 100.0f, 55.0f, 1.0f),
                        makeRotary("LOW DAMP",  "%",  0.0f, 100.0f, 35.0f, 1.0f),
                        makeRotary("HI DAMP",   "%",  0.0f, 100.0f, 42.0f, 1.0f),
                        makeRotary("MOD",       "%",  0.0f, 100.0f, 25.0f, 1.0f),
                        makeToggle("TRAIL",     1.0f)
                    };
                case 9:  // 261: N-Star: Mix, Decay, Trail
                case 10: // 262: Deepsea
                    return {
                        makeRotary("MIX",   "%", 0.0f, 100.0f, 32.0f, 1.0f),
                        makeRotary("DECAY", "%", 0.0f, 100.0f, 68.0f, 1.0f),
                        makeToggle("TRAIL", 1.0f)
                    };
                case 11: // 263: Sweet Space: Mix, Pre Delay, Decay
                    return {
                        makeRotary("MIX",       "%",  0.0f, 100.0f, 30.0f, 1.0f),
                        makeRotary("PRE DELAY", "ms", 0.0f, 150.0f, 25.0f, 1.0f),
                        makeRotary("DECAY",     "%",  0.0f, 100.0f, 62.0f, 1.0f)
                    };
                case 12: // 264: Shimmer: Lo End, High End, Trail
                default:
                    return {
                        makeRotary("LO END",   "%", 0.0f, 100.0f, 45.0f, 1.0f),
                        makeRotary("HIGH END", "%", 0.0f, 100.0f, 68.0f, 1.0f),
                        makeToggle("TRAIL",    1.0f)
                    };
            }
        }
    }
}

inline std::array<float, kMaxSlotParams> defaultParamsForCategory(ModuleId category, int modelIndex = 0) noexcept {
    std::array<float, kMaxSlotParams> out {};
    const auto specs = getSlotParamSpecs(category, modelIndex);
    for (size_t i = 0; i < specs.size() && i < kMaxSlotParams; ++i) {
        out[i] = specs[i].defaultVal;
    }
    return out;
}

// Finds the matching SYNC toggle index for a TIME or RATE parameter in a GP-200 spec list.
// Returns -1 if this parameter has no tempo sync toggle.
inline int findSyncToggleIndexForParam(const std::vector<PedalSlotParamSpec>& specs,
                                       int paramIdx,
                                       bool& outIsDelayTime) noexcept {
    outIsDelayTime = false;
    if (paramIdx < 0 || paramIdx >= static_cast<int>(specs.size())) return -1;
    const juce::String lbl(specs[static_cast<size_t>(paramIdx)].label);

    juce::String targetToggle;
    juce::String fallbackToggle;
    if (lbl.equalsIgnoreCase("TIME")) {
        outIsDelayTime = true;
        targetToggle   = "TIME SYNC";
        fallbackToggle = "SYNC";
    } else if (lbl.equalsIgnoreCase("FLG RATE")) {
        targetToggle = "FLG SYNC";
    } else if (lbl.equalsIgnoreCase("TRM RATE")) {
        targetToggle = "TRM SYNC";
    } else if (lbl.equalsIgnoreCase("PHS RATE")) {
        targetToggle = "PHS SYNC";
    } else if (lbl.equalsIgnoreCase("PAN RATE")) {
        targetToggle = "PAN SYNC";
    } else if (lbl.equalsIgnoreCase("SWP RATE")) {
        targetToggle = "SWP SYNC";
    } else if (lbl.equalsIgnoreCase("RATE")) {
        targetToggle = "SYNC";
    } else {
        return -1;
    }

    int fallbackIdx = -1;
    for (size_t i = 0; i < specs.size(); ++i) {
        if (!specs[i].isToggle()) continue;
        const juce::String tLbl(specs[i].label);
        if (tLbl.equalsIgnoreCase(targetToggle)) return static_cast<int>(i);
        if (fallbackToggle.isNotEmpty() && tLbl.equalsIgnoreCase(fallbackToggle)) fallbackIdx = static_cast<int>(i);
    }
    return fallbackIdx;
}

// ==============================================================================
// Authentic Per-Model AMP Control Specs (All 71 GP-200 Amps + Neural NAM = 72)
// Maps each authentic knob/switch cleanly to the 8 APVTS Amp slots per rig.
// ==============================================================================
enum class AmpSlotBind : uint8_t {
    Gain     = 0, // kAmpGain
    Bass     = 1, // kAmpBass
    Mid      = 2, // kAmpMid
    Treble   = 3, // kAmpTreble
    Presence = 4, // kAmpPresence
    Master   = 5, // kAmpMaster
    Extra    = 6, // kAmpExtra (Gain 2, Input, Edge, Balance)
    Switch   = 7  // kAmpSwitch (Bright, Char Cool/Hot, Midrange 220..3kHz)
};

struct AmpControlSpec {
    AmpSlotBind bind = AmpSlotBind::Gain;
    const char* label = "GAIN";
    bool isSwitch = false;
    std::array<const char*, 5> choices {};
    int numChoices = 0;
};

inline std::vector<AmpControlSpec> getAmpControlSpecs(int modelIndex) {
    auto k = [](AmpSlotBind b, const char* lbl) -> AmpControlSpec {
        return { b, lbl, false, {}, 0 };
    };
    auto sw = [](const char* lbl, std::initializer_list<const char*> opts) -> AmpControlSpec {
        AmpControlSpec s;
        s.bind = AmpSlotBind::Switch;
        s.label = lbl;
        s.isSwitch = true;
        s.numChoices = std::min(5, static_cast<int>(opts.size()));
        int i = 0;
        for (const char* o : opts) {
            if (i < 5) s.choices[static_cast<size_t>(i++)] = o;
        }
        return s;
    };

    const int m = std::clamp(modelIndex, 0, 71);
    switch (m) {
        case 0: // 59: Tweedy: Gain, Tone, Volume
        case 7: // 66: SUPDual CL: Gain, Tone, Volume
            return { k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Treble, "TONE"), k(AmpSlotBind::Master, "VOLUME") };

        case 3: // 62: Dark Twin: Gain, Volume, Bass/Middle/Treble, Bright
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"), k(AmpSlotBind::Mid, "MIDDLE"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Master, "VOLUME"),
                sw("BRIGHT", { "OFF", "BRIGHT" })
            };

        case 4: // 63: Dark DLX: Gain, Volume, Bass/Treble
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Master, "VOLUME")
            };

        case 5: // 64: Dark Vibra: Gain, Volume, Bass/Treble, Bright
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Master, "VOLUME"),
                sw("BRIGHT", { "OFF", "BRIGHT" })
            };

        case 6:  // 65: Silver Twin: Gain, Volume, Bass/Middle/Treble
        case 25: // 84: Knights CL
        case 26: // 85: Knights CL+
        case 27: // 86: Knights OD
        case 54: // 113: Juice30 OD
        case 55: // 114: Juice R100
        case 66: // 125: Mess Bass
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Mid, "MIDDLE"), k(AmpSlotBind::Treble, "TREBLE"),
                k(AmpSlotBind::Master, "VOLUME")
            };

        case 8: // 67: SUPDual OD: Gain 1/2, Tone 1/2, Volume
            return {
                k(AmpSlotBind::Gain, "GAIN 1"), k(AmpSlotBind::Extra, "GAIN 2"),
                k(AmpSlotBind::Bass, "TONE 1"), k(AmpSlotBind::Treble, "TONE 2"),
                k(AmpSlotBind::Master, "VOLUME")
            };

        case 9: // 68: Foxy 15TB: Gain, Tone cut, Volume, Bass/Treble
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Presence, "TONE CUT"),
                k(AmpSlotBind::Master, "VOLUME")
            };

        case 10: // 69: Foxy 30N: Gain, Tone cut, Volume, Bright
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Presence, "TONE CUT"),
                k(AmpSlotBind::Master, "VOLUME"), sw("BRIGHT", { "OFF", "BRIGHT" })
            };

        case 11: // 70: Foxy 30TB: Gain, Tone cut, Volume, Bass/Treble, Char (Cool/Hot)
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Presence, "TONE CUT"),
                k(AmpSlotBind::Master, "VOLUME"), sw("CHAR", { "COOL", "HOT" })
            };

        case 12: // 71: J-120 CL: Gain, Bright, Bass/Middle/Treble
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Mid, "MIDDLE"), k(AmpSlotBind::Treble, "TREBLE"),
                sw("BRIGHT", { "OFF", "BRIGHT" })
            };

        case 16: // 75: L-Star OD: Input, Gain, Presence, Volume, Bass/Middle/Treble
            return {
                k(AmpSlotBind::Extra, "INPUT"), k(AmpSlotBind::Gain, "GAIN"),
                k(AmpSlotBind::Bass, "BASS"), k(AmpSlotBind::Mid, "MIDDLE"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Presence, "PRESENCE"),
                k(AmpSlotBind::Master, "VOLUME")
            };

        case 17: // 76: BogSV CL: Gain, Presence, Volume, Bass/Treble, Bright
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Presence, "PRESENCE"),
                k(AmpSlotBind::Master, "VOLUME"), sw("BRIGHT", { "OFF", "BRIGHT" })
            };

        case 23: // 82: Z38 CL: Gain, Tone cut, Volume, Bass/Middle/Treble
        case 24: // 83: Z38 OD
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Mid, "MIDDLE"), k(AmpSlotBind::Treble, "TREBLE"),
                k(AmpSlotBind::Presence, "TONE CUT"), k(AmpSlotBind::Master, "VOLUME")
            };

        case 28: // 87: Bad-KT CL: Gain, Presence, Volume
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Presence, "PRESENCE"),
                k(AmpSlotBind::Master, "VOLUME")
            };

        case 29: // 88: Bad-KT OD: Gain, Presence, Volume, Edge, Bass/Treble
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Extra, "EDGE"),
                k(AmpSlotBind::Bass, "BASS"), k(AmpSlotBind::Treble, "TREBLE"),
                k(AmpSlotBind::Presence, "PRESENCE"), k(AmpSlotBind::Master, "VOLUME")
            };

        case 35: // 94: UK 45JP: Gain 1/2, Presence, Volume, Bass/Middle/Treble
        case 38: // 97: UK 50JP
            return {
                k(AmpSlotBind::Gain, "GAIN 1"), k(AmpSlotBind::Extra, "GAIN 2"),
                k(AmpSlotBind::Bass, "BASS"), k(AmpSlotBind::Mid, "MIDDLE"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Presence, "PRESENCE"),
                k(AmpSlotBind::Master, "VOLUME")
            };

        case 64: // 123: Classic Bass (Ampeg SVT): Gain, Midrange (5-way), Bass/Middle/Treble, Volume
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Mid, "MIDDLE"), k(AmpSlotBind::Treble, "TREBLE"),
                k(AmpSlotBind::Master, "VOLUME"),
                sw("MIDRANGE", { "220Hz", "450Hz", "800Hz", "1.6kHz", "3kHz" })
            };

        case 65: // 124: Foxy Bass: Volume, Bass/Treble
        case 67: // 126: Mini Bass
            return {
                k(AmpSlotBind::Bass, "BASS"), k(AmpSlotBind::Treble, "TREBLE"),
                k(AmpSlotBind::Master, "VOLUME")
            };

        case 68: // 127: Bass Pre (Alembic F-2B): Volume, Bright, Bass/Middle/Treble
            return {
                k(AmpSlotBind::Bass, "BASS"), k(AmpSlotBind::Mid, "MIDDLE"),
                k(AmpSlotBind::Treble, "TREBLE"), k(AmpSlotBind::Master, "VOLUME"),
                sw("BRIGHT", { "OFF", "BRIGHT" })
            };

        case 69: // 128: AC Pre: Volume, Tone, Balance, EQ Freq (90Hz-1.6kHz), EQ Q, EQ Gain
        case 70: // 129: AC Pre 2: Volume, Tone, Balance, EQ Freq (680Hz-11kHz), EQ Q, EQ Gain
            return {
                k(AmpSlotBind::Gain, "TONE"), k(AmpSlotBind::Extra, "BALANCE"),
                k(AmpSlotBind::Bass, "EQ FREQ"), k(AmpSlotBind::Mid, "EQ Q"),
                k(AmpSlotBind::Treble, "EQ GAIN"), k(AmpSlotBind::Master, "VOLUME")
            };

        case 71: // Neural NAM Capture
            return {
                k(AmpSlotBind::Gain, "NAM DRIVE"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Mid, "MIDDLE"), k(AmpSlotBind::Treble, "TREBLE"),
                k(AmpSlotBind::Presence, "PRESENCE"), k(AmpSlotBind::Master, "MASTER")
            };

        default: // Standard 6-knob amps (Gain, Bass, Middle, Treble, Presence, Volume)
            return {
                k(AmpSlotBind::Gain, "GAIN"), k(AmpSlotBind::Bass, "BASS"),
                k(AmpSlotBind::Mid, "MIDDLE"), k(AmpSlotBind::Treble, "TREBLE"),
                k(AmpSlotBind::Presence, "PRESENCE"), k(AmpSlotBind::Master, "VOLUME")
            };
    }
}

} // namespace pedalboard
