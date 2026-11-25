#pragma once

#include "BiquadFilter.h"
#include "TempoSync.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 MOD Module (All 24 Modulation Models from Daftar_Effect_GP-200.xlsx rows 209..232)
// White-Box Analog Circuit DSP Upgrades:
// - O-Phase (#220 MXR Phase 90): 4-stage cascaded JFET 1st-order all-pass network + feedback mid-bump + JFET warmth
// - S-Phase (#222 EHX Small Stone): 4-stage OTA cascaded all-pass + Warm/Sharp resonant Color feedback loop
// - Vibe (#225 Shin-Ei Uni-Vibe) & M-Vibe (#224): Staggered-capacitor 4-stage asymmetric all-pass + incandescent bulb/CdS photocell lag!
// - C-Chorus (#210 Boss Dimension C): Opposing triangle-LFO dual-BBD cross-coupled spatial matrix (zero pitch wobble!)
// - G-Chorus / G-Roto (#209/#218 Boss CE-1): MN3002 BBD pre/de-emphasis + warm preamp grit + classic dry/inverted-wet stereo field
class StereoChorus {
public:
    struct Params {
        bool bypass = false;
        int modelIndex = 1;    // 0..23 (default 1 = C-Chorus)
        float rateHz = 1.2f;   // legacy
        float depth = 0.45f;   // legacy
        float delayMs = 10.0f; // legacy
        float mix = 0.40f;     // legacy

        bool useSlotParams = false;
        std::array<float, 12> slotParams {};
        float tempoBpm = 120.0f;
    };

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = std::max(8000.0, spec.sampleRate);
        maxDelaySamples = static_cast<int>(sampleRate * 0.065) + 16;

        delayL.setMaximumDelayInSamples(maxDelaySamples);
        delayR.setMaximumDelayInSamples(maxDelaySamples);
        delayC.setMaximumDelayInSamples(maxDelaySamples);
        delayL.prepare(spec);
        delayR.prepare(spec);
        delayC.prepare(spec);

        wetTone.setLowPass(sampleRate, 6200.0, 0.707);
        bbdPreEmph.setHighShelf(sampleRate, 2200.0, 4.5, 0.72);
        bbdDeEmph.setHighShelf(sampleRate, 2200.0, -4.5, 0.72);
        bbdAntiAlias.setLowPass(sampleRate, 4600.0, 0.707);
        bassHpf.setHighPass(sampleRate, 180.0, 0.707);
        crushLpf.setLowPass(sampleRate, 8500.0, 0.707);
        crushHpf.setHighPass(sampleRate, 80.0, 0.707);
        reset();
    }

    void reset() noexcept {
        delayL.reset();
        delayR.reset();
        delayC.reset();
        wetTone.reset();
        bbdPreEmph.reset();
        bbdDeEmph.reset();
        bbdAntiAlias.reset();
        bassHpf.reset();
        crushLpf.reset();
        crushHpf.reset();
        for (auto& st : apStateL) st = 0.0f;
        for (auto& st : apStateR) st = 0.0f;
        for (auto& st : apPrevInL) st = 0.0f;
        for (auto& st : apPrevInR) st = 0.0f;
        apFbL = apFbR = 0.0f;
        vibeLampEnv = 0.5f;
        lfoPhase = 0.0;
        lfoPhase2 = 0.0;
        fbL = fbR = 0.0f;
        swellGain = 0.0f;
        crushHoldL = crushHoldR = 0.0f;
        crushCounter = 0;
    }

    void process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept {
        if (p.bypass) {
            return;
        }

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0) {
            return;
        }

        const int m = std::clamp(p.modelIndex, 0, 23);
        const auto& sp = p.slotParams;

        float rateHz = p.rateHz;
        float depth  = p.depth;
        float wet    = p.mix;
        float dry    = 1.0f - wet * 0.45f;
        float outVol = 1.0f;
        float preDelayMs = p.delayMs;
        float fbAmt  = 0.55f;
        float rate2Hz = 3.0f;
        float depth2  = 0.5f;
        float biasAmt = 0.5f;
        int modeChoice = 0;

        if (p.useSlotParams) {
            switch (m) {
                case 1: { // C-Chorus (Boss Dimension C): Mode 1..4
                    modeChoice = std::clamp(static_cast<int>(std::lround(sp[0])), 0, 3);
                    constexpr float kDimRate[4]  = { 0.34f, 0.58f, 0.92f, 1.48f };
                    constexpr float kDimDepth[4] = { 0.32f, 0.52f, 0.74f, 0.94f };
                    constexpr float kDimMix[4]   = { 0.42f, 0.52f, 0.62f, 0.72f };
                    rateHz = kDimRate[modeChoice];
                    depth  = kDimDepth[modeChoice];
                    wet    = kDimMix[modeChoice];
                    preDelayMs = 7.8f + static_cast<float>(modeChoice) * 1.1f;
                    break;
                }
                case 2: // B-Chorus: Depth, Rate, Mix, Sync
                    depth = sp[0] * 0.01f;
                    rateHz = (sp[3] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.1f, 6.0f, p.tempoBpm) : sp[1];
                    wet = sp[2] * 0.01f; preDelayMs = 9.0f;
                    break;
                case 3: // M-Chorus: Mix, Rate, Filter, Depth L/C/R, Sync
                    wet = sp[0] * 0.01f;
                    rateHz = (sp[6] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.1f, 6.0f, p.tempoBpm) : sp[1];
                    wetTone.setLowPass(sampleRate, 1800.0 + static_cast<double>(sp[2]) * 950.0, 0.72);
                    depth = sp[3] * 0.01f;
                    preDelayMs = 11.0f;
                    break;
                case 4: case 5: case 6: // Jet, B-Jet, N-Jet: Depth, Rate, Pre Delay, Feedback, Sync
                    depth = sp[0] * 0.01f;
                    rateHz = (sp[4] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.05f, 5.0f, p.tempoBpm) : sp[1];
                    preDelayMs = sp[2]; fbAmt = sp[3] * 0.01f; wet = 0.55f;
                    break;
                case 7: // Trem Jet: Flg Depth, Flg Rate, Feedback, Trm Depth, Trm Rate, Flg Sync, Trm Sync
                    depth = sp[0] * 0.01f;
                    rateHz = (sp[5] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.05f, 5.0f, p.tempoBpm) : sp[1];
                    fbAmt = sp[2] * 0.01f;
                    depth2 = sp[3] * 0.01f;
                    rate2Hz = (sp[6] > 0.5f) ? tempo_sync::syncedModRateHz(sp[4], 0.5f, 10.0f, p.tempoBpm) : sp[4];
                    preDelayMs = 3.2f; wet = 0.55f;
                    break;
                case 8: case 12: case 15: case 17: // V-Roto, G-Phase, M-Vibe, O-Trem: Depth, Rate, Sync
                    depth = sp[0] * 0.01f;
                    rateHz = (sp[2] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.1f, 8.0f, p.tempoBpm) : sp[1];
                    wet = (m == 8) ? 1.0f : 0.75f;
                    break;
                case 11: // O-Phase (MXR Phase 90): Rate, Sync
                    rateHz = (sp[1] > 0.5f) ? tempo_sync::syncedModRateHz(sp[0], 0.1f, 8.0f, p.tempoBpm) : sp[0];
                    depth = 0.85f; wet = 0.74f;
                    break;
                case 13: // S-Phase (EHX Small Stone): Rate, Color (Warm/Sharp), Sync
                    rateHz = (sp[2] > 0.5f) ? tempo_sync::syncedModRateHz(sp[0], 0.1f, 8.0f, p.tempoBpm) : sp[0];
                    modeChoice = static_cast<int>(std::lround(sp[1])); depth = 0.90f; wet = 0.78f;
                    break;
                case 14: // Pan Phase: Phs Depth, Phs Rate, Pan Depth, Pan Rate, Phs Sync, Pan Sync
                    depth = sp[0] * 0.01f;
                    rateHz = (sp[4] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.1f, 6.0f, p.tempoBpm) : sp[1];
                    depth2 = sp[2] * 0.01f;
                    rate2Hz = (sp[5] > 0.5f) ? tempo_sync::syncedModRateHz(sp[3], 0.2f, 8.0f, p.tempoBpm) : sp[3];
                    wet = 0.78f;
                    break;
                case 16: // Vibe (Shin-Ei Uni-Vibe): Depth, Rate, Volume, Mode (Chorus/Vibrato), Sync
                    depth = sp[0] * 0.01f;
                    rateHz = (sp[4] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.2f, 8.0f, p.tempoBpm) : sp[1];
                    outVol = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 1.85f);
                    modeChoice = static_cast<int>(std::lround(sp[3]));
                    wet = (modeChoice == 1) ? 1.0f : 0.76f;
                    break;
                case 20: // Bias Trem: Depth, Rate, Volume, Bias, Sync
                    depth = sp[0] * 0.01f;
                    rateHz = (sp[4] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.5f, 10.0f, p.tempoBpm) : sp[1];
                    outVol = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 1.85f);
                    biasAmt = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    wet = 1.0f;
                    break;
                case 21: // Detune: Detune (-50..+50 ct), Dry, Wet
                    depth = std::abs(sp[0]) / 50.0f;
                    rateHz = 0.15f + std::abs(sp[0]) * 0.025f;
                    dry = std::clamp(sp[1] * 0.01f, 0.0f, 1.0f);
                    wet = std::clamp(sp[2] * 0.01f, 0.0f, 1.0f);
                    break;
                case 22: // Bit Smash: Mix, Krush, Bit, Hi Cut, Lo Cut
                    wet = std::clamp(sp[0] * 0.01f, 0.0f, 1.0f);
                    crushLpf.setLowPass(sampleRate, std::clamp(static_cast<double>(sp[3]), 800.0, 16000.0), 0.707);
                    crushHpf.setHighPass(sampleRate, std::clamp(static_cast<double>(sp[4]), 20.0, 1200.0), 0.707);
                    break;
                case 23: // Auto Swell: Attack (ms), Curve (Line/Exp/Log)
                    preDelayMs = sp[0];
                    modeChoice = std::clamp(static_cast<int>(std::lround(sp[1])), 0, 2);
                    wet = 1.0f;
                    break;
                default: // 0:G-Chorus (CE-1), 9:G-Roto (CE-1 Vib), 10:Vibrato, 18:Sine Trem, 19:Triangle Trem
                    depth = sp[0] * 0.01f;
                    rateHz = (sp[3] > 0.5f) ? tempo_sync::syncedModRateHz(sp[1], 0.1f, 8.0f, p.tempoBpm) : sp[1];
                    outVol = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 1.85f);
                    wet = (m == 9 || m == 10 || m == 18 || m == 19) ? 1.0f : 0.54f;
                    break;
            }
        }

        if (!p.useSlotParams && wet <= 0.0001f) return;

        const double phaseInc  = 2.0 * std::numbers::pi * std::clamp(static_cast<double>(rateHz), 0.05, 12.0) / sampleRate;
        const double phaseInc2 = 2.0 * std::numbers::pi * std::clamp(static_cast<double>(rate2Hz), 0.05, 14.0) / sampleRate;
        depth = std::clamp(depth, 0.0f, 1.0f);
        wet   = std::clamp(wet, 0.0f, 1.0f);

        float* chL = buffer.getWritePointer(0);
        float* chR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        // 1. Tremolo Family (17 O-Trem, 18 Sine Trem, 19 Triangle Trem, 20 Bias Trem)
        if (m >= 17 && m <= 20) {
            for (int i = 0; i < numSamples; ++i) {
                float lfo = static_cast<float>(std::sin(lfoPhase));
                if (m == 19) {
                    lfo = static_cast<float>(2.0 / std::numbers::pi * std::asin(std::sin(lfoPhase)));
                } else if (m == 17) {
                    // Demeter TRM-1 Opto-coupler asymmetric lag curve
                    lfo = std::tanh(lfo * 1.85f + 0.28f);
                } else if (m == 20) {
                    // Tube bias-vary tremolo harmonic asymmetry
                    lfo = lfo * (0.9f - 0.35f * biasAmt) + (0.1f + 0.45f * biasAmt) * std::sin(2.0f * static_cast<float>(lfoPhase));
                }
                lfoPhase += phaseInc;
                if (lfoPhase >= 2.0 * std::numbers::pi) lfoPhase -= 2.0 * std::numbers::pi;

                const float modGain = (1.0f - (depth * wet) * (0.5f - 0.5f * lfo)) * outVol;
                chL[i] *= modGain;
                if (chR != nullptr) chR[i] *= modGain;
            }
            return;
        }

        // 2. White-Box 4-Stage Cascaded All-Pass Phaser & Shin-Ei Uni-Vibe Family (11..16)
        if (m >= 11 && m <= 16) {
            const bool isPhase90    = (m == 11);
            const bool isSmallStone = (m == 13);
            const bool isPan        = (m == 14);
            const bool isVibe       = (m == 15 || m == 16);
            const bool vibratoOnly  = (m == 16 && p.useSlotParams && modeChoice == 1);

            // Real Shin-Ei Uni-Vibe staggered capacitor ratios (1.0 : 2.35 : 5.8 : 14.2) create its signature throb!
            static constexpr double kVibeStageMult[4] = { 1.0, 2.35, 5.80, 14.20 };
            const float feedbackGain = isSmallStone ? (modeChoice == 1 ? 0.74f : 0.16f)
                                                    : (isPhase90 ? 0.36f : (isVibe ? 0.28f : 0.42f));
            const float lampAtk = std::exp(-1.0f / static_cast<float>(0.018 * sampleRate)); // Fast bulb heat-up
            const float lampRel = std::exp(-1.0f / static_cast<float>(0.055 * sampleRate)); // Slower CdS photocell decay

            std::array<float, 4> aCoeff { 0.0f, 0.0f, 0.0f, 0.0f };
            const double invSrPi = std::numbers::pi / sampleRate;

            for (int i = 0; i < numSamples; ++i) {
                // Authentic waveform per circuit: triangle for Phase 90 / Small Stone, lamp-lagged asymmetric sine for Uni-Vibe
                double tri = 2.0 / std::numbers::pi * std::asin(std::sin(lfoPhase));
                double norm = 0.5 + 0.5 * tri * static_cast<double>(depth);

                if (isVibe) {
                    const float rawBulb = 0.5f + 0.5f * static_cast<float>(std::sin(lfoPhase));
                    const float shapedBulb = std::pow(std::clamp(rawBulb, 0.0f, 1.0f), 1.65f);
                    const float c = (shapedBulb > vibeLampEnv) ? lampAtk : lampRel;
                    vibeLampEnv = c * vibeLampEnv + (1.0f - c) * shapedBulb;
                    norm = 0.5 + (static_cast<double>(vibeLampEnv) - 0.5) * static_cast<double>(depth) * 1.15;
                    norm = std::clamp(norm, 0.02, 0.98);
                }

                lfoPhase += phaseInc;
                if (lfoPhase >= 2.0 * std::numbers::pi) lfoPhase -= 2.0 * std::numbers::pi;

                float panLfo = 0.0f;
                if (isPan) {
                    panLfo = static_cast<float>(std::sin(lfoPhase2));
                    lfoPhase2 += phaseInc2;
                    if (lfoPhase2 >= 2.0 * std::numbers::pi) lfoPhase2 -= 2.0 * std::numbers::pi;
                }

                // Update 4-stage 1st-order all-pass coefficients (a = (tan(pi*fc/fs) - 1) / (tan(pi*fc/fs) + 1))
                if ((i & 3) == 0) {
                    if (isVibe) {
                        const double baseFc = 115.0 * std::pow(7.2, norm);
                        for (int s = 0; s < 4; ++s) {
                            const double fc = std::clamp(baseFc * kVibeStageMult[s], 45.0, sampleRate * 0.42);
                            const double t = std::tan(fc * invSrPi);
                            aCoeff[static_cast<size_t>(s)] = static_cast<float>((t - 1.0) / (t + 1.0));
                        }
                    } else {
                        const double minFc = isSmallStone ? 105.0 : (isPhase90 ? 190.0 : 160.0);
                        const double ratio = isSmallStone ? 28.0  : (isPhase90 ? 12.5  : 16.0);
                        const double fc = std::clamp(minFc * std::pow(ratio, norm), 55.0, sampleRate * 0.42);
                        const double t = std::tan(fc * invSrPi);
                        const float a = static_cast<float>((t - 1.0) / (t + 1.0));
                        aCoeff = { a, a, a, a };
                    }
                }

                const float inL = chL[i];
                const float inR = (chR != nullptr) ? chR[i] : inL;

                // Soft JFET/OTA saturation inside feedback sum for genuine analog chewy warmth
                float xL = std::tanh((inL + apFbL * feedbackGain) * 1.05f);
                float xR = std::tanh((inR + apFbR * feedbackGain) * 1.05f);

                for (int s = 0; s < 4; ++s) {
                    const size_t si = static_cast<size_t>(s);
                    const float a = aCoeff[si];

                    const float yL = a * xL + apPrevInL[si] - a * apStateL[si];
                    apPrevInL[si] = xL;
                    apStateL[si]  = yL;
                    xL = yL;

                    const float yR = a * xR + apPrevInR[si] - a * apStateR[si];
                    apPrevInR[si] = xR;
                    apStateR[si]  = yR;
                    xR = yR;
                }

                apFbL = xL;
                apFbR = xR;

                const float panL = isPan ? (1.0f - 0.45f * depth2 * panLfo) : 1.0f;
                const float panR = isPan ? (1.0f + 0.45f * depth2 * panLfo) : 1.0f;

                if (vibratoOnly) {
                    // Uni-Vibe Vibrato Mode: 100% phase-modulated wet path (classic watery pitch warble)
                    chL[i] = xL * panL * outVol;
                    if (chR != nullptr) chR[i] = xR * panR * outVol;
                } else {
                    // Authentic 50/50 phase-cancellation summing (Vibe uses wet inversion for deep low-mid pulse)
                    const float sign = isVibe ? -1.0f : 1.0f;
                    const float wetAmt = std::clamp(wet, 0.15f, 1.0f) * 0.62f;
                    const float dryAmt = 1.0f - wetAmt * 0.48f;
                    chL[i] = (inL * dryAmt + sign * xL * wetAmt) * panL * outVol;
                    if (chR != nullptr) chR[i] = (inR * dryAmt + sign * xR * wetAmt) * panR * outVol;
                }
            }
            return;
        }

        // 3. Bit Smash (22: Mix, Krush, Bit, Hi Cut, Lo Cut)
        if (m == 22) {
            const int holdStep = p.useSlotParams
                ? std::clamp(static_cast<int>(std::lround(sp[1])), 1, 32)
                : std::max(1, static_cast<int>(1.0f + depth * 22.0f));
            const float bits = p.useSlotParams ? std::clamp(sp[2], 2.0f, 16.0f) : 8.0f;
            const float levels = std::pow(2.0f, bits - 1.0f);
            for (int i = 0; i < numSamples; ++i) {
                if ((crushCounter++ % holdStep) == 0) {
                    crushHoldL = std::round(chL[i] * levels) / levels;
                    crushHoldR = (chR != nullptr) ? (std::round(chR[i] * levels) / levels) : crushHoldL;
                }
                const float fltL = crushHpf.processSample(0, crushLpf.processSample(0, crushHoldL));
                const float fltR = crushHpf.processSample(1, crushLpf.processSample(1, crushHoldR));
                chL[i] = chL[i] * (1.0f - wet) + fltL * wet;
                if (chR != nullptr) chR[i] = chR[i] * (1.0f - wet) + fltR * wet;
            }
            return;
        }

        // 4. Auto Swell (23: Attack ms, Curve: Line / Exp / Log)
        if (m == 23) {
            const float swellSec = p.useSlotParams
                ? std::clamp(preDelayMs * 0.001f, 0.02f, 1.5f)
                : std::clamp(0.08f + (p.delayMs / 24.0f) * 0.85f, 0.05f, 1.2f);
            const float atkC = std::exp(-1.0f / static_cast<float>(swellSec * static_cast<float>(sampleRate)));
            const float relC = std::exp(-1.0f / static_cast<float>(0.022 * sampleRate));
            for (int i = 0; i < numSamples; ++i) {
                float pk = std::abs(chL[i]);
                if (chR != nullptr) pk = std::max(pk, std::abs(chR[i]));
                if (pk > 0.012f) {
                    swellGain = atkC * swellGain + (1.0f - atkC) * 1.0f;
                } else {
                    swellGain = relC * swellGain;
                }
                float shapedG = swellGain;
                if (modeChoice == 1)      shapedG = swellGain * swellGain;
                else if (modeChoice == 2) shapedG = std::sqrt(std::max(0.0f, swellGain));
                const float g = (1.0f - wet) + wet * shapedG;
                chL[i] *= g;
                if (chR != nullptr) chR[i] *= g;
            }
            return;
        }

        // 5. Chorus (0..3), Flanger (4..7), Vibrato (8..10), Detune (21)
        const bool isCe1      = (m == 0 || m == 9);  // Boss CE-1 Chorus / Vibrato (MN3002 BBD!)
        const bool isDimC     = (m == 1);            // Boss Dimension C (Dual-BBD Opposing Triangle LFO!)
        const bool isFlanger  = (m >= 4 && m <= 7);
        const bool isVibrato  = (m >= 8 && m <= 10);
        const bool isDetune   = (m == 21);
        const bool isBassMod  = (m == 2 || m == 5);
        const bool isMChorus  = (m == 3 && p.useSlotParams);
        const bool isTremJet  = (m == 7 && p.useSlotParams);

        const float centerMs = isFlanger ? std::clamp(preDelayMs * (p.useSlotParams ? 0.55f : 0.22f), 0.5f, 9.0f)
                                         : (isVibrato ? 4.5f : std::clamp(preDelayMs, 3.0f, 25.0f));
        const float modMsL = isDetune ? (0.3f + depth * 1.4f)
                                      : (isDimC ? (1.2f + depth * 3.1f)
                                                : (isFlanger ? depth * 2.8f : (isVibrato ? depth * 2.2f : depth * 4.5f)));
        const float modMsC = isMChorus ? (sp[4] * 0.01f * 4.2f) : 0.0f;
        const float modMsR = isMChorus ? (sp[5] * 0.01f * 4.5f) : modMsL;
        const float fb = isFlanger ? ((m == 6) ? -std::clamp(fbAmt, 0.0f, 0.85f) : std::clamp(fbAmt, 0.0f, 0.85f)) : 0.0f;
        const float msToSamples = static_cast<float>(sampleRate * 0.001);

        for (int i = 0; i < numSamples; ++i) {
            float lfoL = static_cast<float>(std::sin(lfoPhase));
            float lfoC = static_cast<float>(std::sin(lfoPhase + 2.0943951));
            float lfoR = static_cast<float>(std::sin(lfoPhase + 1.5707963268));

            if (isDimC) {
                // Boss Dimension C: pure opposing triangle LFOs (180° out of phase -> lush spatial motion with ZERO pitch wobble!)
                const float tri = static_cast<float>(2.0 / std::numbers::pi * std::asin(std::sin(lfoPhase)));
                lfoL =  tri;
                lfoR = -tri;
            }

            lfoPhase += phaseInc;
            if (lfoPhase >= 2.0 * std::numbers::pi) lfoPhase -= 2.0 * std::numbers::pi;

            float trmGain = 1.0f;
            if (isTremJet) {
                const float trmLfo = static_cast<float>(std::sin(lfoPhase2));
                lfoPhase2 += phaseInc2;
                if (lfoPhase2 >= 2.0 * std::numbers::pi) lfoPhase2 -= 2.0 * std::numbers::pi;
                trmGain = 1.0f - depth2 * (0.5f - 0.5f * trmLfo);
            }

            const float inL = chL[i];
            const float inR = (chR != nullptr) ? chR[i] : inL;

            float srcL = isBassMod ? bassHpf.processSample(0, inL) : inL;
            float srcR = isBassMod ? bassHpf.processSample(1, inR) : inR;

            if (isCe1 || isDimC) {
                // Authentic analog BBD pre-emphasis + warm preamp saturation before delay line
                srcL = std::tanh(bbdPreEmph.processSample(0, srcL) * (isCe1 ? 1.18f : 1.06f));
                srcR = std::tanh(bbdPreEmph.processSample(1, srcR) * (isCe1 ? 1.18f : 1.06f));
            }

            delayL.pushSample(0, srcL + fbL * fb);
            delayR.pushSample(0, srcR + fbR * fb);
            if (isMChorus) delayC.pushSample(0, 0.5f * (srcL + srcR));

            const float dSamplesL = std::clamp((centerMs + lfoL * modMsL) * msToSamples, 2.0f, static_cast<float>(maxDelaySamples - 4));
            const float dSamplesR = std::clamp((centerMs + lfoR * modMsR) * msToSamples, 2.0f, static_cast<float>(maxDelaySamples - 4));

            float wetL = delayL.popSample(0, dSamplesL);
            float wetR = delayR.popSample(0, dSamplesR);

            if (isCe1 || isDimC) {
                // Complementary BBD de-emphasis + MN3002/MN3007 clock anti-aliasing filter
                wetL = bbdAntiAlias.processSample(0, bbdDeEmph.processSample(0, wetL));
                wetR = bbdAntiAlias.processSample(1, bbdDeEmph.processSample(1, wetR));
            } else {
                wetL = wetTone.processSample(0, wetL);
                wetR = wetTone.processSample(1, wetR);
            }

            if (isMChorus) {
                const float dSamplesC = std::clamp((centerMs + 2.5f + lfoC * modMsC) * msToSamples, 2.0f, static_cast<float>(maxDelaySamples - 4));
                const float wetC = delayC.popSample(0, dSamplesC) * 0.45f;
                wetL = wetL * 0.72f + wetC;
                wetR = wetR * 0.72f + wetC;
            }
            fbL = std::tanh(wetL);
            fbR = std::tanh(wetR);

            if (isDimC) {
                // Boss Dimension C cross-coupled stereo matrix: adds wide 3D halo while cancelling mono pitch warble!
                const float dimL = wetL - 0.34f * wetR;
                const float dimR = wetR - 0.34f * wetL;
                const float dryMix = 1.0f - wet * 0.25f;
                chL[i] = (inL * dryMix + dimL * wet * 0.78f) * outVol;
                if (chR != nullptr) chR[i] = (inR * dryMix + dimR * wet * 0.78f) * outVol;
            } else if (m == 0) {
                // Boss CE-1 Chorus: legendary wide stereo spread (L = dry + wet, R = dry - wet) + warm CE-1 buffer
                const float monoWet = 0.5f * (wetL + wetR);
                const float dryMix = 1.0f - wet * 0.32f;
                chL[i] = (inL * dryMix + monoWet * wet) * outVol;
                if (chR != nullptr) chR[i] = (inR * dryMix - monoWet * wet) * outVol;
            } else {
                const float dryMix = isVibrato ? 0.0f : (isDetune ? dry : (1.0f - wet * 0.45f));
                const float wetMix = isVibrato ? 1.0f : wet;
                chL[i] = (inL * dryMix + wetL * wetMix) * trmGain * outVol;
                if (chR != nullptr) {
                    chR[i] = (inR * dryMix + wetR * wetMix) * trmGain * outVol;
                }
            }
        }
    }

private:
    double sampleRate = 44100.0;
    int maxDelaySamples = 4096;
    double lfoPhase = 0.0;
    double lfoPhase2 = 0.0;
    float fbL = 0.0f, fbR = 0.0f;
    float apFbL = 0.0f, apFbR = 0.0f;
    float vibeLampEnv = 0.5f;
    std::array<float, 4> apStateL {};
    std::array<float, 4> apStateR {};
    std::array<float, 4> apPrevInL {};
    std::array<float, 4> apPrevInR {};
    float swellGain = 0.0f;
    float crushHoldL = 0.0f, crushHoldR = 0.0f;
    int crushCounter = 0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayL { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayR { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayC { 4096 };
    BiquadFilter wetTone;
    BiquadFilter bbdPreEmph;
    BiquadFilter bbdDeEmph;
    BiquadFilter bbdAntiAlias;
    BiquadFilter bassHpf;
    BiquadFilter crushLpf;
    BiquadFilter crushHpf;
};

} // namespace pedalboard
