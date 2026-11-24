#pragma once

#include "../engine/Gp200Catalogue.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pedalboard::params {

// Global I/O & Tuner
inline constexpr const char* kInputGain       = "input_gain";
inline constexpr const char* kOutputGain      = "output_gain";
inline constexpr const char* kTunerMute       = "tuner_mute";

// Looper controls (MIDI-learnable & automatable)
inline constexpr const char* kLooperLevel     = "looper_level";
inline constexpr const char* kLooperPreFx     = "looper_prefx";

// Dual / Triple Parallel Routing & Console Mixer
inline constexpr const char* kRoutingMode     = "routing_mode";
inline constexpr const char* kPathALevel      = "path_a_level";
inline constexpr const char* kPathAPan        = "path_a_pan";
inline constexpr const char* kPathAMute       = "path_a_mute";
inline constexpr const char* kPathBLevel      = "path_b_level";
inline constexpr const char* kPathBPan        = "path_b_pan";
inline constexpr const char* kPathBMute       = "path_b_mute";
inline constexpr const char* kPathBPhase      = "path_b_phase";
inline constexpr const char* kPathCLevel      = "path_c_level";
inline constexpr const char* kPathCPan        = "path_c_pan";
inline constexpr const char* kPathCMute       = "path_c_mute";
inline constexpr const char* kPathCPhase      = "path_c_phase";

// Noise Gate (NR: 2 models)
inline constexpr const char* kGateBypass      = "gate_bypass";
inline constexpr const char* kGateModel       = "gate_model";
inline constexpr const char* kGateThreshold   = "gate_threshold";
inline constexpr const char* kGateRelease     = "gate_release";
inline constexpr const char* kGateDepth       = "gate_depth";

// Pre / Compressor (PRE: 20 models)
inline constexpr const char* kCompBypass      = "comp_bypass";
inline constexpr const char* kCompModel       = "comp_model";
inline constexpr const char* kCompThreshold   = "comp_threshold";
inline constexpr const char* kCompRatio       = "comp_ratio";
inline constexpr const char* kCompAttack      = "comp_attack";
inline constexpr const char* kCompRelease     = "comp_release";
inline constexpr const char* kCompMakeup      = "comp_makeup";

// Wah (WAH: 5 models)
inline constexpr const char* kWahBypass       = "wah_bypass";
inline constexpr const char* kWahModel        = "wah_model";
inline constexpr const char* kWahPedal        = "wah_pedal";
inline constexpr const char* kWahQ            = "wah_q";
inline constexpr const char* kWahSensitivity  = "wah_sensitivity";
inline constexpr const char* kWahMix          = "wah_mix";
inline constexpr const char* kWahAuto         = "wah_auto";

// Overdrive / Distortion / Fuzz (DST: 33 models)
inline constexpr const char* kOdBypass        = "od_bypass";
inline constexpr const char* kOdModel         = "od_model";
inline constexpr const char* kOdDrive         = "od_drive";
inline constexpr const char* kOdTone          = "od_tone";
inline constexpr const char* kOdLevel         = "od_level";

// Amp Head A / NAM A (AMP: 72 models)
inline constexpr const char* kAmpBypass       = "amp_bypass";
inline constexpr const char* kAmpChannel      = "amp_channel";
inline constexpr const char* kAmpModel        = "amp_model";
inline constexpr const char* kAmpGain         = "amp_gain";
inline constexpr const char* kAmpBass         = "amp_bass";
inline constexpr const char* kAmpMid          = "amp_mid";
inline constexpr const char* kAmpTreble       = "amp_treble";
inline constexpr const char* kAmpPresence     = "amp_presence";
inline constexpr const char* kAmpMaster       = "amp_master";
inline constexpr const char* kAmpExtra        = "amp_extra";
inline constexpr const char* kAmpSwitch       = "amp_switch";

// Independent Amp Head B / NAM B (Path B, 72 models)
inline constexpr const char* kAmpBBypass      = "amp_b_bypass";
inline constexpr const char* kAmpBChannel     = "amp_b_channel";
inline constexpr const char* kAmpBModel       = "amp_b_model";
inline constexpr const char* kAmpBGain        = "amp_b_gain";
inline constexpr const char* kAmpBBass        = "amp_b_bass";
inline constexpr const char* kAmpBMid         = "amp_b_mid";
inline constexpr const char* kAmpBTreble      = "amp_b_treble";
inline constexpr const char* kAmpBPresence    = "amp_b_presence";
inline constexpr const char* kAmpBMaster      = "amp_b_master";
inline constexpr const char* kAmpBExtra       = "amp_b_extra";
inline constexpr const char* kAmpBSwitch      = "amp_b_switch";

// Independent Amp Head C / NAM C (Path C - Triple Parallel, 72 models)
inline constexpr const char* kAmpCBypass      = "amp_c_bypass";
inline constexpr const char* kAmpCChannel     = "amp_c_channel";
inline constexpr const char* kAmpCModel       = "amp_c_model";
inline constexpr const char* kAmpCGain        = "amp_c_gain";
inline constexpr const char* kAmpCBass        = "amp_c_bass";
inline constexpr const char* kAmpCMid         = "amp_c_mid";
inline constexpr const char* kAmpCTreble      = "amp_c_treble";
inline constexpr const char* kAmpCPresence    = "amp_c_presence";
inline constexpr const char* kAmpCMaster      = "amp_c_master";
inline constexpr const char* kAmpCExtra       = "amp_c_extra";
inline constexpr const char* kAmpCSwitch      = "amp_c_switch";

// Cabinet IR A (CAB: 71 models)
inline constexpr const char* kCabBypass       = "cab_bypass";
inline constexpr const char* kCabModel        = "cab_model";
inline constexpr const char* kCabLowCut       = "cab_lowcut";
inline constexpr const char* kCabHighCut      = "cab_highcut";
inline constexpr const char* kCabLevel        = "cab_level";

// Independent Cabinet IR B (Path B, 71 models)
inline constexpr const char* kCabBBypass      = "cab_b_bypass";
inline constexpr const char* kCabBModel       = "cab_b_model";
inline constexpr const char* kCabBLowCut      = "cab_b_lowcut";
inline constexpr const char* kCabBHighCut     = "cab_b_highcut";
inline constexpr const char* kCabBLevel       = "cab_b_level";

// Independent Cabinet IR C (Path C - Triple Parallel, 71 models)
inline constexpr const char* kCabCBypass      = "cab_c_bypass";
inline constexpr const char* kCabCModel       = "cab_c_model";
inline constexpr const char* kCabCLowCut      = "cab_c_lowcut";
inline constexpr const char* kCabCHighCut     = "cab_c_highcut";
inline constexpr const char* kCabCLevel       = "cab_c_level";

// Graphic EQ (EQ: 6 models)
inline constexpr const char* kEqBypass        = "eq_bypass";
inline constexpr const char* kEqModel         = "eq_model";
inline constexpr const char* kEq100           = "eq_100";
inline constexpr const char* kEq200           = "eq_200";
inline constexpr const char* kEq400           = "eq_400";
inline constexpr const char* kEq800           = "eq_800";
inline constexpr const char* kEq1600          = "eq_1600";
inline constexpr const char* kEq3200          = "eq_3200";
inline constexpr const char* kEq6400          = "eq_6400";
inline constexpr const char* kEqLevel         = "eq_level";

// Modulation (MOD: 24 models)
inline constexpr const char* kChorusBypass    = "chorus_bypass";
inline constexpr const char* kChorusModel     = "chorus_model";
inline constexpr const char* kChorusRate      = "chorus_rate";
inline constexpr const char* kChorusDepth     = "chorus_depth";
inline constexpr const char* kChorusDelay     = "chorus_delay";
inline constexpr const char* kChorusMix       = "chorus_mix";

// Delay (DLY: 19 models)
inline constexpr const char* kDelayBypass     = "delay_bypass";
inline constexpr const char* kDelayModel      = "delay_model";
inline constexpr const char* kDelayTime       = "delay_time";
inline constexpr const char* kDelayFeedback   = "delay_feedback";
inline constexpr const char* kDelayMix        = "delay_mix";
inline constexpr const char* kDelayPingPong   = "delay_pingpong";

// Reverb (RVB: 13 models)
inline constexpr const char* kReverbBypass    = "reverb_bypass";
inline constexpr const char* kReverbModel     = "reverb_model";
inline constexpr const char* kReverbSize      = "reverb_size";
inline constexpr const char* kReverbDamping   = "reverb_damping";
inline constexpr const char* kReverbMix       = "reverb_mix";

inline const char* modelParamFor(ModuleId id, int rigIndex = 0) noexcept {
    switch (id) {
        case ModuleId::NoiseGate:    return kGateModel;
        case ModuleId::Compressor:   return kCompModel;
        case ModuleId::Wah:          return kWahModel;
        case ModuleId::Overdrive:    return kOdModel;
        case ModuleId::AmpHead:      return (rigIndex == 2) ? kAmpCModel : ((rigIndex == 1) ? kAmpBModel : kAmpModel);
        case ModuleId::CabinetIR:    return (rigIndex == 2) ? kCabCModel : ((rigIndex == 1) ? kCabBModel : kCabModel);
        case ModuleId::GraphicEq:    return kEqModel;
        case ModuleId::StereoChorus: return kChorusModel;
        case ModuleId::StereoDelay:  return kDelayModel;
        case ModuleId::Reverb:       return kReverbModel;
        default:                     return kOdModel;
    }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addFloat = [&](const char* id, const char* name, Range range, float def, const char* suffix = "") {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 },
            name,
            range,
            def,
            juce::AudioParameterFloatAttributes().withLabel(suffix)
        ));
    };

    auto addBool = [&](const char* id, const char* name, bool def) {
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { id, 1 },
            name,
            def
        ));
    };

    auto addModelChoice = [&](const char* id, const char* name, ModuleId mod, int defIndex) {
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { id, 1 },
            name,
            gp200::getModelNameArray(mod),
            defIndex
        ));
    };

    // Global I/O, Tuner & Looper
    addFloat(kInputGain,      "Input Gain",      Range(-24.0f, 24.0f, 0.1f),   0.0f, "dB");
    addFloat(kOutputGain,     "Output Gain",     Range(-24.0f, 18.0f, 0.1f),   0.0f, "dB");
    addBool (kTunerMute,      "Tuner Mute",      false);
    addFloat(kLooperLevel,    "Looper Level",    Range(-24.0f, 6.0f, 0.1f),    0.0f, "dB");
    addBool (kLooperPreFx,    "Looper Pre-FX",   false);

    // Multi-Topology Routing & Mixer (Serial | Dual Split | Triple Parallel | Dual Independent)
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { kRoutingMode, 1 },
        "Routing Mode",
        juce::StringArray { "Serial", "Dual Split A/B", "Triple Parallel", "Dual Independent" },
        0
    ));
    addFloat(kPathALevel,     "Path A Level",    Range(-36.0f, 12.0f, 0.1f),   0.0f, "dB");
    addFloat(kPathAPan,       "Path A Pan",      Range(-1.0f, 1.0f, 0.02f),    0.0f);
    addBool (kPathAMute,      "Path A Mute",     false);
    addFloat(kPathBLevel,     "Path B Level",    Range(-36.0f, 12.0f, 0.1f),  -4.0f, "dB");
    addFloat(kPathBPan,       "Path B Pan",      Range(-1.0f, 1.0f, 0.02f),    0.0f);
    addBool (kPathBMute,      "Path B Mute",     false);
    addBool (kPathBPhase,     "Path B Phase",    false);
    addFloat(kPathCLevel,     "Path C Level",    Range(-36.0f, 12.0f, 0.1f),  -6.0f, "dB");
    addFloat(kPathCPan,       "Path C Pan",      Range(-1.0f, 1.0f, 0.02f),    0.0f);
    addBool (kPathCMute,      "Path C Mute",     false);
    addBool (kPathCPhase,     "Path C Phase",    false);

    // Noise Gate (NR)
    addBool       (kGateBypass,     "Gate Bypass",     true);
    addModelChoice(kGateModel,      "NR Model",        ModuleId::NoiseGate, 0); // Gate 1
    addFloat      (kGateThreshold,  "Gate Threshold",  Range(-80.0f, 0.0f, 0.5f),  -54.0f, "dB");
    addFloat      (kGateRelease,    "Gate Release",    Range(15.0f, 500.0f, 1.0f, 0.5f), 110.0f, "ms");
    addFloat      (kGateDepth,      "Gate Depth",      Range(-80.0f, 0.0f, 0.5f),  -60.0f, "dB");

    // Pre / Compressor (PRE)
    addBool       (kCompBypass,     "Comp Bypass",     true);
    addModelChoice(kCompModel,      "PRE Model",       ModuleId::Compressor, 0); // COMP
    addFloat      (kCompThreshold,  "Comp Threshold",  Range(-50.0f, 0.0f, 0.5f),  -22.0f, "dB");
    addFloat      (kCompRatio,      "Comp Ratio",      Range(1.0f, 12.0f, 0.1f),     4.0f, ":1");
    addFloat      (kCompAttack,     "Comp Attack",     Range(1.0f, 80.0f, 0.5f, 0.5f), 10.0f, "ms");
    addFloat      (kCompRelease,    "Comp Release",    Range(20.0f, 500.0f, 1.0f, 0.5f), 120.0f, "ms");
    addFloat      (kCompMakeup,     "Comp Makeup",     Range(0.0f, 24.0f, 0.1f),     4.0f, "dB");

    // Wah (WAH)
    addBool       (kWahBypass,      "Wah Bypass",      true);
    addModelChoice(kWahModel,       "WAH Model",       ModuleId::Wah, 0); // V-Wah
    addFloat      (kWahPedal,       "Wah Pedal",       Range(0.0f, 1.0f, 0.01f),     0.50f);
    addFloat      (kWahQ,           "Wah Resonance",   Range(1.5f, 8.5f, 0.1f),      4.2f);
    addFloat      (kWahSensitivity, "Wah Auto Sens",   Range(0.0f, 1.0f, 0.01f),     0.60f);
    addFloat      (kWahMix,         "Wah Mix",         Range(0.0f, 1.0f, 0.01f),     1.00f);
    addBool       (kWahAuto,        "Auto-Wah",        false);

    // Overdrive / Distortion (DST)
    addBool       (kOdBypass,       "OD Bypass",       true);
    addModelChoice(kOdModel,        "DST Model",       ModuleId::Overdrive, 0); // Green OD
    addFloat      (kOdDrive,        "OD Drive",        Range(0.0f, 10.0f, 0.05f),    3.5f);
    addFloat      (kOdTone,         "OD Tone",         Range(0.0f, 10.0f, 0.05f),    5.8f);
    addFloat      (kOdLevel,        "OD Level",        Range(0.0f, 10.0f, 0.05f),    5.5f);

    // Amp Head A / NAM A (Default: Pure Clean Fender '65 Twin Reverb)
    addBool (kAmpBypass,      "Amp A Bypass",    false);
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { kAmpChannel, 1 },
        "Amp A Channel",
        juce::StringArray { "Clean", "Crunch", "High-Gain", "NAM Capture" },
        0
    ));
    addModelChoice(kAmpModel,       "AMP A Model",     ModuleId::AmpHead, 3); // Dark Twin (Fender '65 Twin Reverb Clean)
    addFloat      (kAmpGain,        "Amp A Gain",      Range(0.0f, 10.0f, 0.05f),    3.8f);
    addFloat      (kAmpBass,        "Amp A Bass",      Range(0.0f, 10.0f, 0.05f),    5.4f);
    addFloat      (kAmpMid,         "Amp A Mid",       Range(0.0f, 10.0f, 0.05f),    5.2f);
    addFloat      (kAmpTreble,      "Amp A Treble",    Range(0.0f, 10.0f, 0.05f),    6.2f);
    addFloat      (kAmpPresence,    "Amp A Presence",  Range(0.0f, 10.0f, 0.05f),    5.4f);
    addFloat      (kAmpMaster,      "Amp A Master",    Range(0.0f, 10.0f, 0.05f),    6.5f);
    addFloat      (kAmpExtra,       "Amp A Extra",     Range(0.0f, 10.0f, 0.05f),    5.0f);
    addFloat      (kAmpSwitch,      "Amp A Switch",    Range(0.0f,  4.0f, 1.0f),     0.0f);

    // Independent Amp Head B / NAM B (Path B)
    addBool (kAmpBBypass,     "Amp B Bypass",    false);
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { kAmpBChannel, 1 },
        "Amp B Channel",
        juce::StringArray { "Clean", "Crunch", "High-Gain", "NAM Capture" },
        0
    ));
    addModelChoice(kAmpBModel,      "AMP B Model",     ModuleId::AmpHead, 3); // Dark Twin
    addFloat      (kAmpBGain,       "Amp B Gain",      Range(0.0f, 10.0f, 0.05f),    3.4f);
    addFloat      (kAmpBBass,       "Amp B Bass",      Range(0.0f, 10.0f, 0.05f),    5.6f);
    addFloat      (kAmpBMid,        "Amp B Mid",       Range(0.0f, 10.0f, 0.05f),    5.0f);
    addFloat      (kAmpBTreble,     "Amp B Treble",    Range(0.0f, 10.0f, 0.05f),    6.6f);
    addFloat      (kAmpBPresence,   "Amp B Presence",  Range(0.0f, 10.0f, 0.05f),    5.8f);
    addFloat      (kAmpBMaster,     "Amp B Master",    Range(0.0f, 10.0f, 0.05f),    6.5f);
    addFloat      (kAmpBExtra,      "Amp B Extra",     Range(0.0f, 10.0f, 0.05f),    5.0f);
    addFloat      (kAmpBSwitch,     "Amp B Switch",    Range(0.0f,  4.0f, 1.0f),     0.0f);

    // Independent Amp Head C / NAM C (Path C - Triple Parallel)
    addBool (kAmpCBypass,     "Amp C Bypass",    false);
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { kAmpCChannel, 1 },
        "Amp C Channel",
        juce::StringArray { "Clean", "Crunch", "High-Gain", "NAM Capture" },
        0
    ));
    addModelChoice(kAmpCModel,      "AMP C Model",     ModuleId::AmpHead, 3); // Dark Twin
    addFloat      (kAmpCGain,       "Amp C Gain",      Range(0.0f, 10.0f, 0.05f),    3.6f);
    addFloat      (kAmpCBass,       "Amp C Bass",      Range(0.0f, 10.0f, 0.05f),    5.5f);
    addFloat      (kAmpCMid,        "Amp C Mid",       Range(0.0f, 10.0f, 0.05f),    5.0f);
    addFloat      (kAmpCTreble,     "Amp C Treble",    Range(0.0f, 10.0f, 0.05f),    6.2f);
    addFloat      (kAmpCPresence,   "Amp C Presence",  Range(0.0f, 10.0f, 0.05f),    5.5f);
    addFloat      (kAmpCMaster,     "Amp C Master",    Range(0.0f, 10.0f, 0.05f),    6.5f);
    addFloat      (kAmpCExtra,      "Amp C Extra",     Range(0.0f, 10.0f, 0.05f),    5.0f);
    addFloat      (kAmpCSwitch,     "Amp C Switch",    Range(0.0f,  4.0f, 1.0f),     0.0f);

    // Cabinet IR A (Default: Matching Clean Dark Twin 2x12")
    addBool       (kCabBypass,      "Cab A Bypass",    false);
    addModelChoice(kCabModel,       "CAB A Model",     ModuleId::CabinetIR, 7); // Dark Twin 2x12"
    addFloat      (kCabLowCut,      "Cab A Low Cut",   Range(35.0f, 260.0f, 1.0f, 0.5f), 80.0f, "Hz");
    addFloat      (kCabHighCut,     "Cab A High Cut",  Range(2500.0f, 16000.0f, 10.0f, 0.5f), 8800.0f, "Hz");
    addFloat      (kCabLevel,       "Cab A Level",     Range(-18.0f, 12.0f, 0.1f),   0.0f, "dB");

    // Independent Cabinet IR B (Path B)
    addBool       (kCabBBypass,     "Cab B Bypass",    false);
    addModelChoice(kCabBModel,      "CAB B Model",     ModuleId::CabinetIR, 7); // Dark Twin 2x12"
    addFloat      (kCabBLowCut,     "Cab B Low Cut",   Range(35.0f, 260.0f, 1.0f, 0.5f), 95.0f, "Hz");
    addFloat      (kCabBHighCut,    "Cab B High Cut",  Range(2500.0f, 16000.0f, 10.0f, 0.5f), 8400.0f, "Hz");
    addFloat      (kCabBLevel,      "Cab B Level",     Range(-18.0f, 12.0f, 0.1f),   0.0f, "dB");

    // Independent Cabinet IR C (Path C - Triple Parallel)
    addBool       (kCabCBypass,     "Cab C Bypass",    false);
    addModelChoice(kCabCModel,      "CAB C Model",     ModuleId::CabinetIR, 7); // Dark Twin 2x12"
    addFloat      (kCabCLowCut,     "Cab C Low Cut",   Range(35.0f, 260.0f, 1.0f, 0.5f), 90.0f, "Hz");
    addFloat      (kCabCHighCut,    "Cab C High Cut",  Range(2500.0f, 16000.0f, 10.0f, 0.5f), 8500.0f, "Hz");
    addFloat      (kCabCLevel,      "Cab C Level",     Range(-18.0f, 12.0f, 0.1f),   0.0f, "dB");

    // Graphic EQ (EQ)
    addBool       (kEqBypass,       "EQ Bypass",       true);
    addModelChoice(kEqModel,        "EQ Model",        ModuleId::GraphicEq, 4); // Mess EQ
    addFloat      (kEq100,          "EQ Band 1",       Range(-12.0f, 12.0f, 0.1f),   0.0f, "dB");
    addFloat      (kEq200,          "EQ Band 2",       Range(-12.0f, 12.0f, 0.1f),   0.0f, "dB");
    addFloat      (kEq400,          "EQ Band 3",       Range(-12.0f, 12.0f, 0.1f),   0.0f, "dB");
    addFloat      (kEq800,          "EQ Band 4",       Range(-12.0f, 12.0f, 0.1f),   0.0f, "dB");
    addFloat      (kEq1600,         "EQ Band 5",       Range(-12.0f, 12.0f, 0.1f),   0.0f, "dB");
    addFloat      (kEq3200,         "EQ Band 6",       Range(-12.0f, 12.0f, 0.1f),   0.0f, "dB");
    addFloat      (kEq6400,         "EQ Band 7",       Range(-12.0f, 12.0f, 0.1f),   0.0f, "dB");
    addFloat      (kEqLevel,        "EQ Level",        Range(-12.0f, 12.0f, 0.1f),   0.0f, "dB");

    // Modulation (MOD)
    addBool       (kChorusBypass,   "Chorus Bypass",   true);
    addModelChoice(kChorusModel,    "MOD Model",       ModuleId::StereoChorus, 1); // C-Chorus
    addFloat      (kChorusRate,     "Chorus Rate",     Range(0.1f, 6.0f, 0.05f, 0.6f), 1.2f, "Hz");
    addFloat      (kChorusDepth,    "Chorus Depth",    Range(0.0f, 1.0f, 0.01f),     0.45f);
    addFloat      (kChorusDelay,    "Chorus Delay",    Range(4.0f, 24.0f, 0.1f),    10.0f, "ms");
    addFloat      (kChorusMix,      "Chorus Mix",      Range(0.0f, 1.0f, 0.01f),     0.40f);

    // Delay (DLY)
    addBool       (kDelayBypass,    "Delay Bypass",    true);
    addModelChoice(kDelayModel,     "DLY Model",       ModuleId::StereoDelay, 16); // Vintage Rack
    addFloat      (kDelayTime,      "Delay Time",      Range(40.0f, 1200.0f, 1.0f, 0.5f), 360.0f, "ms");
    addFloat      (kDelayFeedback,  "Delay Feedback",  Range(0.0f, 0.90f, 0.01f),    0.35f);
    addFloat      (kDelayMix,       "Delay Mix",       Range(0.0f, 1.0f, 0.01f),     0.25f);
    addBool       (kDelayPingPong,  "Delay PingPong",  true);

    // Reverb (RVB)
    addBool       (kReverbBypass,   "Reverb Bypass",   true);
    addModelChoice(kReverbModel,    "RVB Model",       ModuleId::Reverb, 3); // Plate
    addFloat      (kReverbSize,     "Reverb Size",     Range(0.0f, 1.0f, 0.01f),     0.42f);
    addFloat      (kReverbDamping,  "Reverb Damping",  Range(0.0f, 1.0f, 0.01f),     0.50f);
    addFloat      (kReverbMix,      "Reverb Mix",      Range(0.0f, 1.0f, 0.01f),     0.18f);

    return { p.begin(), p.end() };
}

} // namespace pedalboard::params
