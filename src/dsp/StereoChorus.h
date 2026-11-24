#pragma once

#include "BiquadFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 MOD Module (All 24 Modulation Models from Daftar_Effect_GP-200.xlsx rows 209..232)
// Supports every authentic per-model control: C-Chorus 4-Mode, M-Chorus Depth L/C/R + Filter,
// Jet Pre Delay + Feedback, Trem Jet dual LFO, S-Phase Warm/Sharp, Pan Phase dual LFO,
// Vibe Chorus/Vibrato, Bias Trem Bias, Detune cents + Dry/Wet, Bit Smash Krush/Bit/Hi/Lo Cut,
// Auto Swell Attack + Line/Exp/Log curve.
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
        bassHpf.reset();
        crushLpf.reset();
        crushHpf.reset();
        phaserPeak1.reset();
        phaserPeak2.reset();
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
                    constexpr float kDimRate[4]  = { 0.42f, 0.72f, 1.15f, 1.85f };
                    constexpr float kDimDepth[4] = { 0.30f, 0.50f, 0.72f, 0.92f };
                    constexpr float kDimMix[4]   = { 0.42f, 0.50f, 0.58f, 0.68f };
                    rateHz = kDimRate[modeChoice];
                    depth  = kDimDepth[modeChoice];
                    wet    = kDimMix[modeChoice];
                    preDelayMs = 8.5f;
                    break;
                }
                case 2: // B-Chorus: Depth, Rate, Mix, Sync
                    depth = sp[0] * 0.01f; rateHz = sp[1]; wet = sp[2] * 0.01f; preDelayMs = 9.0f;
                    break;
                case 3: // M-Chorus: Mix, Rate, Filter, Depth L/C/R, Sync
                    wet = sp[0] * 0.01f; rateHz = sp[1];
                    wetTone.setLowPass(sampleRate, 1800.0 + static_cast<double>(sp[2]) * 950.0, 0.72);
                    depth = sp[3] * 0.01f;
                    preDelayMs = 11.0f;
                    break;
                case 4: case 5: case 6: // Jet, B-Jet, N-Jet: Depth, Rate, Pre Delay, Feedback, Sync
                    depth = sp[0] * 0.01f; rateHz = sp[1]; preDelayMs = sp[2]; fbAmt = sp[3] * 0.01f; wet = 0.55f;
                    break;
                case 7: // Trem Jet: Flg Depth, Flg Rate, Feedback, Trm Depth, Trm Rate, Flg Sync, Trm Sync
                    depth = sp[0] * 0.01f; rateHz = sp[1]; fbAmt = sp[2] * 0.01f;
                    depth2 = sp[3] * 0.01f; rate2Hz = sp[4]; preDelayMs = 3.2f; wet = 0.55f;
                    break;
                case 8: case 12: case 15: case 17: // V-Roto, G-Phase, M-Vibe, O-Trem: Depth, Rate, Sync
                    depth = sp[0] * 0.01f; rateHz = sp[1]; wet = (m == 8) ? 1.0f : 0.75f;
                    break;
                case 11: // O-Phase (Phase 90): Rate, Sync
                    rateHz = sp[0]; depth = 0.75f; wet = 0.72f;
                    break;
                case 13: // S-Phase (Small Stone): Rate, Color (Warm/Sharp), Sync
                    rateHz = sp[0]; modeChoice = static_cast<int>(std::lround(sp[1])); depth = 0.80f; wet = 0.78f;
                    break;
                case 14: // Pan Phase: Phs Depth, Phs Rate, Pan Depth, Pan Rate
                    depth = sp[0] * 0.01f; rateHz = sp[1]; depth2 = sp[2] * 0.01f; rate2Hz = sp[3]; wet = 0.78f;
                    break;
                case 16: // Vibe: Depth, Rate, Volume, Mode (Chorus/Vibrato), Sync
                    depth = sp[0] * 0.01f; rateHz = sp[1];
                    outVol = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 1.85f);
                    modeChoice = static_cast<int>(std::lround(sp[3]));
                    wet = (modeChoice == 1) ? 1.0f : 0.72f;
                    break;
                case 20: // Bias Trem: Depth, Rate, Volume, Bias, Sync
                    depth = sp[0] * 0.01f; rateHz = sp[1];
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
                default: // 0:G-Chorus, 9:G-Roto, 10:Vibrato, 18:Sine Trem, 19:Triangle Trem: Depth, Rate, Volume, Sync
                    depth = sp[0] * 0.01f; rateHz = sp[1];
                    outVol = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 1.85f);
                    wet = (m == 9 || m == 10 || m == 18 || m == 19) ? 1.0f : 0.52f;
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
                    lfo = std::tanh(lfo * 1.8f + 0.25f);
                } else if (m == 20) {
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

        // 2. Phaser & Uni-Vibe Family (11..16)
        if (m >= 11 && m <= 16) {
            const bool isVibe = (m >= 15);
            const bool isPan  = (m == 14);
            const bool vibratoOnly = (m == 16 && p.useSlotParams && modeChoice == 1);
            for (int i = 0; i < numSamples; ++i) {
                float lfo = static_cast<float>(std::sin(lfoPhase));
                if (isVibe) lfo = std::tanh(lfo * 1.6f - 0.35f);
                lfoPhase += phaseInc;
                if (lfoPhase >= 2.0 * std::numbers::pi) lfoPhase -= 2.0 * std::numbers::pi;

                float panLfo = 0.0f;
                if (isPan) {
                    panLfo = static_cast<float>(std::sin(lfoPhase2));
                    lfoPhase2 += phaseInc2;
                    if (lfoPhase2 >= 2.0 * std::numbers::pi) lfoPhase2 -= 2.0 * std::numbers::pi;
                }

                if ((i & 7) == 0) {
                    const double norm = 0.5 + 0.5 * static_cast<double>(lfo) * static_cast<double>(depth);
                    const double f1 = 260.0 * std::pow(5.5, norm);
                    const double f2 = f1 * (isVibe ? 2.15 : 2.65);
                    const double gainDb = (m == 13) ? (modeChoice == 1 ? -16.5 : -11.0) : -10.5;
                    const double qPhs = (m == 13 && modeChoice == 1) ? 2.4 : (isVibe ? 1.1 : 1.6);
                    phaserPeak1.setPeaking(sampleRate, f1, qPhs, gainDb);
                    phaserPeak2.setPeaking(sampleRate, f2, qPhs, -gainDb * 0.85);
                }

                const float inL = chL[i];
                const float inR = (chR != nullptr) ? chR[i] : inL;
                const float pL = phaserPeak2.processSample(0, phaserPeak1.processSample(0, inL));
                const float pR = phaserPeak2.processSample(1, phaserPeak1.processSample(1, inR));

                const float panL = isPan ? (1.0f - 0.45f * depth2 * panLfo) : 1.0f;
                const float panR = isPan ? (1.0f + 0.45f * depth2 * panLfo) : 1.0f;
                const float dryMix = vibratoOnly ? 0.0f : (1.0f - wet * 0.5f);
                chL[i] = (inL * dryMix + pL * wet * 0.90f) * panL * outVol;
                if (chR != nullptr) chR[i] = (inR * dryMix + pR * wet * 0.90f) * panR * outVol;
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
                if (modeChoice == 1)      shapedG = swellGain * swellGain;       // Exp
                else if (modeChoice == 2) shapedG = std::sqrt(std::max(0.0f, swellGain)); // Log
                const float g = (1.0f - wet) + wet * shapedG;
                chL[i] *= g;
                if (chR != nullptr) chR[i] *= g;
            }
            return;
        }

        // 5. Chorus (0..3), Flanger (4..7), Vibrato (8..10), Detune (21)
        const bool isFlanger  = (m >= 4 && m <= 7);
        const bool isVibrato  = (m >= 8 && m <= 10);
        const bool isDetune   = (m == 21);
        const bool isBassMod  = (m == 2 || m == 5);
        const bool isMChorus  = (m == 3 && p.useSlotParams);
        const bool isTremJet  = (m == 7 && p.useSlotParams);

        const float centerMs = isFlanger ? std::clamp(preDelayMs * (p.useSlotParams ? 0.55f : 0.22f), 0.5f, 9.0f)
                                         : (isVibrato ? 4.5f : std::clamp(preDelayMs, 3.0f, 25.0f));
        const float modMsL = isDetune ? (0.3f + depth * 1.4f) : (isFlanger ? depth * 2.8f : (isVibrato ? depth * 2.2f : depth * 4.5f));
        const float modMsC = isMChorus ? (sp[4] * 0.01f * 4.2f) : 0.0f;
        const float modMsR = isMChorus ? (sp[5] * 0.01f * 4.5f) : modMsL;
        const float fb = isFlanger ? ((m == 6) ? -std::clamp(fbAmt, 0.0f, 0.85f) : std::clamp(fbAmt, 0.0f, 0.85f)) : 0.0f;
        const float msToSamples = static_cast<float>(sampleRate * 0.001);

        for (int i = 0; i < numSamples; ++i) {
            const float sinL = static_cast<float>(std::sin(lfoPhase));
            const float sinC = static_cast<float>(std::sin(lfoPhase + 2.0943951));
            const float sinR = static_cast<float>(std::sin(lfoPhase + 1.5707963268));
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

            const float srcL = isBassMod ? bassHpf.processSample(0, inL) : inL;
            const float srcR = isBassMod ? bassHpf.processSample(1, inR) : inR;

            delayL.pushSample(0, srcL + fbL * fb);
            delayR.pushSample(0, srcR + fbR * fb);
            if (isMChorus) delayC.pushSample(0, 0.5f * (srcL + srcR));

            const float dSamplesL = std::clamp((centerMs + sinL * modMsL) * msToSamples, 2.0f, static_cast<float>(maxDelaySamples - 4));
            const float dSamplesR = std::clamp((centerMs + sinR * modMsR) * msToSamples, 2.0f, static_cast<float>(maxDelaySamples - 4));

            float wetL = wetTone.processSample(0, delayL.popSample(0, dSamplesL));
            float wetR = wetTone.processSample(1, delayR.popSample(0, dSamplesR));
            if (isMChorus) {
                const float dSamplesC = std::clamp((centerMs + 2.5f + sinC * modMsC) * msToSamples, 2.0f, static_cast<float>(maxDelaySamples - 4));
                const float wetC = delayC.popSample(0, dSamplesC) * 0.45f;
                wetL = wetL * 0.72f + wetC;
                wetR = wetR * 0.72f + wetC;
            }
            fbL = std::tanh(wetL);
            fbR = std::tanh(wetR);

            const float dryMix = isVibrato ? 0.0f : (isDetune ? dry : (1.0f - wet * 0.45f));
            const float wetMix = isVibrato ? 1.0f : wet;

            chL[i] = (inL * dryMix + wetL * wetMix) * trmGain * outVol;
            if (chR != nullptr) {
                chR[i] = (inR * dryMix + wetR * wetMix) * trmGain * outVol;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    int maxDelaySamples = 4096;
    double lfoPhase = 0.0;
    double lfoPhase2 = 0.0;
    float fbL = 0.0f, fbR = 0.0f;
    float swellGain = 0.0f;
    float crushHoldL = 0.0f, crushHoldR = 0.0f;
    int crushCounter = 0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayL { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayR { 4096 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayC { 4096 };
    BiquadFilter wetTone;
    BiquadFilter bassHpf;
    BiquadFilter crushLpf;
    BiquadFilter crushHpf;
    BiquadFilter phaserPeak1;
    BiquadFilter phaserPeak2;
};

} // namespace pedalboard
