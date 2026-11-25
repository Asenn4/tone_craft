#pragma once

#include "BiquadFilter.h"
#include "TempoSync.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 DLY Module (All 19 Delay Models from Daftar_Effect_GP-200.xlsx rows 233..251)
// White-Box Analog Circuit DSP Upgrades:
// - 999 Echo (#248 Maxon AD900), Sweet Echo (#247 Boss DM-2), Analog Delay (#239), BBD Delay S (#233):
//   Authentic MN3005/MN3008 BBD circuit with NE570 compander (pre-compress -> BBD -> post-expand),
//   dynamic clock-rate anti-aliasing LPF (darker at longer delay times), and musical BBD feedback saturation!
// - Tape Delay S (#235), Tape Delay (#240), Tube Delay (#245), M-Echo (#246 RE-201 4-Head):
//   Dual-component capstan Wow + pinch-roller Flutter + Scrape jitter, asymmetric magnetic tape hysteresis
//   saturation + 115 Hz Tape Head Bump resonance + age-dependent oxide rolloff!
class StereoDelay {
public:
    struct Params {
        bool bypass = false;
        int modelIndex = 16;    // 0..18
        float timeMs = 360.0f;  // legacy
        float feedback = 0.35f; // legacy
        float mix = 0.25f;      // legacy
        bool pingPong = true;   // legacy

        bool useSlotParams = false;
        std::array<float, 12> slotParams {};
        float tempoBpm = 120.0f;
    };

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = std::max(8000.0, spec.sampleRate);
        maxDelaySamples = static_cast<int>(sampleRate * 2.4) + 32;

        delayLineL.setMaximumDelayInSamples(maxDelaySamples);
        delayLineR.setMaximumDelayInSamples(maxDelaySamples);
        delayLineL.prepare(spec);
        delayLineR.prepare(spec);

        smoothedDelayL.reset(sampleRate, 0.04);
        smoothedDelayR.reset(sampleRate, 0.04);
        smoothedDelayL.setCurrentAndTargetValue(static_cast<float>(0.36 * sampleRate));
        smoothedDelayR.setCurrentAndTargetValue(static_cast<float>(0.36 * sampleRate));

        tapeHeadBump.setPeaking(sampleRate, 115.0, 1.15, 2.0);
        lastModel = -1;
        configureToneForModel(16, 5.5f, 0.4f, 360.0f);
        reset();
    }

    void reset() noexcept {
        delayLineL.reset();
        delayLineR.reset();
        feedbackLpf.reset();
        feedbackHpf.reset();
        sweepFilter.reset();
        tapeHeadBump.reset();
        modPhase = 0.0;
        flutterPhase = 0.0;
        revPhase = 0.0;
        bbdCompEnvL = bbdCompEnvR = 0.05f;
        bbdExpEnvL  = bbdExpEnvR  = 0.05f;
        tapeDcL = tapeDcR = 0.0f;
    }

    void process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0) return;

        const int m = std::clamp(p.modelIndex, 0, 18);
        const auto& sp = p.slotParams;

        // Parse per-model parameters
        float mix = p.mix;
        float fb = p.feedback;
        float timeMs = p.timeMs;
        float timeRRatio = 1.0f;
        float spread = 0.75f;
        float outLvl = 1.0f;
        float modAmt = 0.25f;
        float toneVal = 5.5f;
        float ageAmt = 0.4f;
        float scrapeAmt = 0.25f;
        float driveAmt = 0.25f;
        float sweepDepth = 0.6f;
        float sweepRate = 1.2f;
        float ringMix = 0.55f;
        float ringFreq = 260.0f;
        int headMode = 5;
        bool trailOn = true;
        bool timeSynced = false;
        bool usePingPong = p.pingPong || (m == 8);

        if (p.useSlotParams) {
            switch (m) {
                case 0: case 1: // BBD Delay S, Digital Delay S: Mix, Feedback, Time, Time R%, Spread, Level, Sync, Trail
                    mix = sp[0] * 0.01f; fb = sp[1] * 0.01f;
                    timeSynced = sp[6] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[2], 30.0f, 1500.0f, p.tempoBpm) : sp[2];
                    timeRRatio = std::clamp(sp[3] * 0.01f, 0.25f, 2.0f);
                    spread = std::clamp(sp[4] * 0.01f, 0.0f, 1.0f);
                    outLvl = juce::Decibels::decibelsToGain(-12.0f + sp[5] * 1.85f);
                    trailOn = sp[7] > 0.5f;
                    usePingPong = false;
                    break;
                case 2: // Tape Delay S: Mix, Fdbk, Time, Time R%, Spread, Wow/Flt, Age, Scrape, Drive, Level, Sync, Trail
                    mix = sp[0] * 0.01f; fb = sp[1] * 0.01f;
                    timeSynced = sp[10] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[2], 30.0f, 1500.0f, p.tempoBpm) : sp[2];
                    timeRRatio = std::clamp(sp[3] * 0.01f, 0.25f, 2.0f);
                    spread = std::clamp(sp[4] * 0.01f, 0.0f, 1.0f);
                    modAmt = std::clamp(sp[5] * 0.01f, 0.0f, 1.0f);
                    ageAmt = std::clamp(sp[6] * 0.01f, 0.0f, 1.0f);
                    scrapeAmt = std::clamp(sp[7] * 0.01f, 0.0f, 1.0f);
                    driveAmt = std::clamp(sp[8] * 0.01f, 0.0f, 1.0f);
                    outLvl = juce::Decibels::decibelsToGain(-12.0f + sp[9] * 1.85f);
                    trailOn = sp[11] > 0.5f;
                    usePingPong = false;
                    break;
                case 3: case 4: // Ambience 1/2: Mix, Feedback, Time, Level, Mod, Tone, Sync, Trail
                    mix = sp[0] * 0.01f; fb = sp[1] * 0.01f;
                    timeSynced = sp[6] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[2], 30.0f, 1500.0f, p.tempoBpm) : sp[2];
                    outLvl = juce::Decibels::decibelsToGain(-12.0f + sp[3] * 1.85f);
                    modAmt = std::clamp(sp[4] * 0.01f, 0.0f, 1.0f);
                    toneVal = sp[5];
                    trailOn = sp[7] > 0.5f;
                    usePingPong = false;
                    break;
                case 9: // Slapback: Mix, Time, Feedback, Trail
                    mix = sp[0] * 0.01f; timeMs = sp[1]; fb = sp[2] * 0.01f; trailOn = sp[3] > 0.5f; usePingPong = false;
                    break;
                case 10: // Sweep Echo: Mix, Time, Feedback, Sweep Depth, Sweep Rate, Sweep Sync, Time Sync, Trail
                    mix = sp[0] * 0.01f;
                    timeSynced = sp[6] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[1], 30.0f, 1500.0f, p.tempoBpm) : sp[1];
                    fb = sp[2] * 0.01f;
                    sweepDepth = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    sweepRate  = (sp[5] > 0.5f)
                                     ? tempo_sync::syncedModRateHz(sp[4], 0.1f, 6.0f, p.tempoBpm)
                                     : std::clamp(sp[4], 0.1f, 8.0f);
                    trailOn = sp[7] > 0.5f; usePingPong = false;
                    break;
                case 11: // Ring Echo: Dly Mix, Time, Feedback, Ring Mix, Freq, Tone, Sync, Trail
                    mix = sp[0] * 0.01f;
                    timeSynced = sp[6] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[1], 30.0f, 1500.0f, p.tempoBpm) : sp[1];
                    fb = sp[2] * 0.01f;
                    ringMix = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    ringFreq = std::clamp(sp[4], 40.0f, 1500.0f);
                    toneVal = sp[5]; trailOn = sp[7] > 0.5f; usePingPong = false;
                    break;
                case 13: // M-Echo (RE-201 4-Head): Mix, Time, Feedback, Tone, Mode (Head 1..12), Sync, Trail
                    mix = sp[0] * 0.01f;
                    timeSynced = sp[5] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[1], 30.0f, 1500.0f, p.tempoBpm) : sp[1];
                    fb = sp[2] * 0.01f;
                    toneVal = sp[3];
                    headMode = std::clamp(static_cast<int>(std::lround(sp[4])), 0, 11);
                    trailOn = sp[6] > 0.5f; usePingPong = false;
                    modAmt = 0.36f; driveAmt = 0.35f;
                    break;
                case 14: // Sweet Echo (Boss DM-2 20-300ms): Mix, Time, Feedback, Sync, Trail
                    mix = sp[0] * 0.01f;
                    timeSynced = sp[3] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[1], 20.0f, 300.0f, p.tempoBpm) : sp[1];
                    fb = sp[2] * 0.01f;
                    trailOn = sp[4] > 0.5f; usePingPong = false;
                    break;
                case 16: case 17: // Vintage Rack, Lofi Echo: Mix, Time, Feedback, Mod, Tone, Sync, Trail
                    mix = sp[0] * 0.01f;
                    timeSynced = sp[5] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[1], 30.0f, 1500.0f, p.tempoBpm) : sp[1];
                    fb = sp[2] * 0.01f;
                    modAmt = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    toneVal = sp[4]; trailOn = sp[6] > 0.5f; usePingPong = false;
                    break;
                case 18: // Rev Echo: Mix, Time, Feedback, Level, Sync, Trail
                    mix = sp[0] * 0.01f;
                    timeSynced = sp[4] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[1], 50.0f, 1500.0f, p.tempoBpm) : sp[1];
                    fb = sp[2] * 0.01f;
                    outLvl = juce::Decibels::decibelsToGain(-12.0f + sp[3] * 1.85f);
                    trailOn = sp[5] > 0.5f; usePingPong = false;
                    break;
                default: // Pure, Analog, Tape, Ping Pong, Tube, 999 Echo (Maxon AD900): Mix, Time, Feedback, Sync, Trail
                    mix = sp[0] * 0.01f;
                    timeSynced = sp[3] > 0.5f;
                    timeMs = timeSynced ? tempo_sync::syncedDelayMs(sp[1], 30.0f, 1500.0f, p.tempoBpm) : sp[1];
                    fb = sp[2] * 0.01f;
                    trailOn = sp[4] > 0.5f;
                    usePingPong = (m == 8);
                    break;
            }
        }

        if (p.bypass && !trailOn) {
            return;
        }

        const float effTimeL = (m == 9) ? std::min(timeMs, 180.0f) : ((m == 14 && !timeSynced) ? std::min(timeMs, 300.0f) : timeMs);
        const float effTimeR = effTimeL * timeRRatio;
        configureToneForModel(m, toneVal, ageAmt, effTimeL);

        const float tgtL = std::clamp(effTimeL * 0.001f * static_cast<float>(sampleRate), 2.0f, static_cast<float>(maxDelaySamples - 48));
        const float tgtR = std::clamp(effTimeR * 0.001f * static_cast<float>(sampleRate), 2.0f, static_cast<float>(maxDelaySamples - 48));
        smoothedDelayL.setTargetValue(tgtL);
        smoothedDelayR.setTargetValue(tgtR);

        fb = std::clamp(fb, 0.0f, 0.94f);
        const float wet = std::clamp(mix, 0.0f, 1.0f);
        const float dry = p.bypass ? 1.0f : (1.0f - wet * 0.45f);
        const float inputSend = p.bypass ? 0.0f : 1.0f;

        const bool isBbd      = (m == 0 || m == 6 || m == 14 || m == 15); // MN3005/MN3008 BBD (incl. Maxon AD900 & DM-2)
        const bool isTape     = (m == 2 || m == 7 || m == 12 || m == 13); // RE-201 Space Echo / Tape / Tube Tape
        const bool hasMod     = (isTape || m == 3 || m == 4 || m == 16 || m == 17);
        const bool isAmbience = (m == 3 || m == 4);
        const bool isMEcho    = (m == 13);
        const bool isSweep    = (m == 10);
        const bool isRing     = (m == 11);
        const bool isLofi     = (m == 16 || m == 17);
        const bool isRev      = (m == 18);

        const double modFreqHz  = isRing ? static_cast<double>(ringFreq)
                                         : (isSweep ? static_cast<double>(sweepRate) : (isTape ? 0.68 : 1.35));
        const double modInc     = 2.0 * std::numbers::pi * modFreqHz / sampleRate;
        const double flutterInc = 2.0 * std::numbers::pi * 5.85 / sampleRate; // Authentic tape pinch-roller flutter

        // NE570 BBD compander smoothing coefficients (8 ms attack, 65 ms release)
        const float compAtk = std::exp(-1.0f / static_cast<float>(0.008 * sampleRate));
        const float compRel = std::exp(-1.0f / static_cast<float>(0.065 * sampleRate));

        // 12 M-Echo 4-head tap bitmasks (bit0=0.25, bit1=0.50, bit2=0.75, bit3=1.00)
        static constexpr uint8_t kHeadMasks[12] = { 0x1, 0x2, 0x4, 0x8, 0x3, 0x5, 0x9, 0x6, 0xA, 0xC, 0x7, 0xF };
        const uint8_t headMask = kHeadMasks[headMode];

        float* chL = buffer.getWritePointer(0);
        float* chR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i) {
            const float modSin = static_cast<float>(std::sin(modPhase));
            modPhase += modInc;
            if (modPhase >= 2.0 * std::numbers::pi) modPhase -= 2.0 * std::numbers::pi;

            const float fltSin = static_cast<float>(std::sin(flutterPhase));
            const float scrSin = static_cast<float>(std::sin(flutterPhase * 2.53));
            flutterPhase += flutterInc;
            if (flutterPhase >= 2.0 * std::numbers::pi) flutterPhase -= 2.0 * std::numbers::pi;

            float dL = smoothedDelayL.getNextValue();
            float dR = smoothedDelayR.getNextValue();
            if (hasMod) {
                float wobL = modSin * modAmt * 18.0f;
                float wobR = -wobL;
                if (isTape) {
                    // Authentic capstan Wow + pinch-roller Flutter + Tape Scrape micro-jitter
                    const float wowAmt = (m == 2) ? modAmt : (m == 13 ? 0.38f : 0.30f);
                    const float scrAmt = (m == 2) ? scrapeAmt : 0.22f;
                    wobL = modSin * wowAmt * 26.0f + fltSin * wowAmt * 7.5f + scrSin * scrAmt * 3.8f;
                    wobR = -modSin * wowAmt * 22.0f + fltSin * wowAmt * 6.0f - scrSin * scrAmt * 3.2f;
                }
                dL = std::clamp(dL + wobL, 2.0f, static_cast<float>(maxDelaySamples - 16));
                dR = std::clamp(dR + wobR, 2.0f, static_cast<float>(maxDelaySamples - 16));
            }

            if (isSweep && (i & 15) == 0) {
                const double sweepHz = 360.0 * std::pow(6.0, 0.5 + 0.5 * static_cast<double>(sweepDepth * modSin));
                sweepFilter.setPeaking(sampleRate, sweepHz, 2.3, 9.0);
            }

            const float inL = chL[i];
            const float inR = (chR != nullptr) ? chR[i] : inL;

            float delayedL = 0.0f;
            float delayedR = 0.0f;

            if (isRev) {
                const double step = 2.0 / std::max(64.0, static_cast<double>(dL));
                revPhase += step;
                if (revPhase >= 1.0) revPhase -= 1.0;
                const double revPhaseB = (revPhase + 0.5 >= 1.0) ? (revPhase - 0.5) : (revPhase + 0.5);
                const float wA = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * revPhase));
                const float wB = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * revPhaseB));
                const float tapA = std::clamp(static_cast<float>((1.0 - revPhase) * dL), 2.0f, static_cast<float>(maxDelaySamples - 16));
                const float tapB = std::clamp(static_cast<float>((1.0 - revPhaseB) * dL), 2.0f, static_cast<float>(maxDelaySamples - 16));
                delayedL = delayLineL.popSample(0, tapA) * wA + delayLineL.popSample(0, tapB) * wB;
                delayedR = delayLineR.popSample(0, tapA) * wA + delayLineR.popSample(0, tapB) * wB;
            } else if (isMEcho) {
                // RE-201 4-head physical tape spacing with slight head-to-head tone/level spread
                float sumL = 0.0f, sumR = 0.0f;
                int taps = 0;
                constexpr float kFrac[4] = { 0.25f, 0.50f, 0.75f, 1.00f };
                for (int h = 0; h < 4; ++h) {
                    if (headMask & (1u << h)) {
                        const float t = std::max(2.0f, dL * kFrac[h]);
                        const float headGain = 1.0f - static_cast<float>(h) * 0.04f;
                        sumL += delayLineL.popSample(0, t) * headGain;
                        sumR += delayLineR.popSample(0, t * 1.012f) * headGain;
                        ++taps;
                    }
                }
                const float normT = 1.0f / std::max(1.0f, std::sqrt(static_cast<float>(taps)));
                delayedL = sumL * normT;
                delayedR = sumR * normT;
            } else {
                delayedL = delayLineL.popSample(0, dL);
                delayedR = delayLineR.popSample(0, usePingPong ? dL : dR);
                if (isAmbience) {
                    delayedL = 0.65f * delayedL + 0.35f * delayLineL.popSample(0, std::max(2.0f, dL * 0.66f));
                    delayedR = 0.65f * delayedR + 0.35f * delayLineR.popSample(0, std::max(2.0f, dR * 0.75f));
                }
            }

            // Apply NE570 expander on BBD output (complements 2:1 compressor before delay line!)
            if (isBbd) {
                const float absL = std::abs(delayedL);
                const float absR = std::abs(delayedR);
                bbdExpEnvL = (absL > bbdExpEnvL ? compAtk : compRel) * bbdExpEnvL + (1.0f - (absL > bbdExpEnvL ? compAtk : compRel)) * absL;
                bbdExpEnvR = (absR > bbdExpEnvR ? compAtk : compRel) * bbdExpEnvR + (1.0f - (absR > bbdExpEnvR ? compAtk : compRel)) * absR;
                const float expGainL = std::clamp(std::sqrt(std::max(0.02f, bbdExpEnvL)) * 1.35f, 0.45f, 1.35f);
                const float expGainR = std::clamp(std::sqrt(std::max(0.02f, bbdExpEnvR)) * 1.35f, 0.45f, 1.35f);
                delayedL *= expGainL;
                delayedR *= expGainR;
            }

            // Apply stereo spread
            const float monoD = 0.5f * (delayedL + delayedR);
            const float outDL = monoD * (1.0f - spread) + delayedL * spread;
            const float outDR = monoD * (1.0f - spread) + delayedR * spread;

            float filtL = feedbackHpf.processSample(0, feedbackLpf.processSample(0, outDL));
            float filtR = feedbackHpf.processSample(1, feedbackLpf.processSample(1, outDR));

            if (isSweep) {
                filtL = sweepFilter.processSample(0, filtL);
                filtR = sweepFilter.processSample(1, filtR);
            } else if (isRing) {
                filtL = filtL * (1.0f - ringMix) + (filtL * modSin) * ringMix;
                filtR = filtR * (1.0f - ringMix) + (filtR * modSin) * ringMix;
            } else if (isLofi) {
                const float qLevels = (m == 17) ? 28.0f : 128.0f;
                filtL = std::round(filtL * qLevels) / qLevels;
                filtR = std::round(filtR * qLevels) / qLevels;
            }

            if (isTape) {
                // Magnetic tape head bump (~115 Hz) + asymmetric hysteresis soft saturation + DC block
                filtL = tapeHeadBump.processSample(0, filtL);
                filtR = tapeHeadBump.processSample(1, filtR);
                const float satDrive = (m == 12 ? 1.42f : 1.16f) + driveAmt * 1.25f;
                const float bias = (m == 12) ? 0.08f : 0.045f;
                const float satL = (std::tanh((filtL + bias) * satDrive) - std::tanh(bias * satDrive)) / std::tanh(satDrive);
                const float satR = (std::tanh((filtR + bias) * satDrive) - std::tanh(bias * satDrive)) / std::tanh(satDrive);
                tapeDcL = 0.996f * tapeDcL + 0.004f * satL;
                tapeDcR = 0.996f * tapeDcR + 0.004f * satR;
                filtL = satL - tapeDcL;
                filtR = satR - tapeDcR;
            } else if (isBbd) {
                // Warm MN3005 BBD soft-clipping inside feedback loop (organic analog self-oscillation!)
                const float bbdDrive = (m == 15) ? 1.26f : 1.18f;
                filtL = std::tanh(filtL * bbdDrive) / std::tanh(bbdDrive);
                filtR = std::tanh(filtR * bbdDrive) / std::tanh(bbdDrive);
            }

            float sendL = inL * inputSend;
            float sendR = inR * inputSend;
            if (isBbd) {
                // NE570 2:1 compressor feeding into the BBD delay line
                const float absL = std::abs(sendL);
                const float absR = std::abs(sendR);
                bbdCompEnvL = (absL > bbdCompEnvL ? compAtk : compRel) * bbdCompEnvL + (1.0f - (absL > bbdCompEnvL ? compAtk : compRel)) * absL;
                bbdCompEnvR = (absR > bbdCompEnvR ? compAtk : compRel) * bbdCompEnvR + (1.0f - (absR > bbdCompEnvR ? compAtk : compRel)) * absR;
                sendL /= std::clamp(std::sqrt(std::max(0.04f, bbdCompEnvL)) * 1.35f, 0.55f, 1.8f);
                sendR /= std::clamp(std::sqrt(std::max(0.04f, bbdCompEnvR)) * 1.35f, 0.55f, 1.8f);
            }

            if (usePingPong) {
                const float monoIn = 0.5f * (sendL + sendR);
                delayLineL.pushSample(0, monoIn + filtR * fb);
                delayLineR.pushSample(0, filtL * fb);
            } else {
                delayLineL.pushSample(0, sendL + filtL * fb);
                delayLineR.pushSample(0, sendR + filtR * fb);
            }

            chL[i] = (inL * dry + outDL * wet * outLvl);
            if (chR != nullptr) {
                chR[i] = (inR * dry + outDR * wet * outLvl);
            }
        }
    }

private:
    void configureToneForModel(int m, float toneVal, float ageAmt, float effTimeMs) noexcept {
        lastModel = m;
        double baseLpf = 4500.0;
        double hpfHz = 95.0;
        if (m == 1 || m == 5 || m == 8) {
            baseLpf = 11500.0; hpfHz = 55.0;
        } else if (m == 0 || m == 6 || m == 14 || m == 15) {
            // Dynamic BBD clock-rate bandwidth: longer delay slows BBD clock -> darker, warmer Maxon AD900 / DM-2 repeats!
            const double timeNorm = std::clamp((static_cast<double>(effTimeMs) - 40.0) / 560.0, 0.0, 1.0);
            baseLpf = (m == 15 ? 4300.0 : 3800.0) - timeNorm * 1850.0;
            hpfHz = (m == 15) ? 120.0 : 140.0;
        } else if (m == 2 || m == 7 || m == 12 || m == 13) {
            baseLpf = (m == 2) ? (5200.0 - static_cast<double>(ageAmt) * 3200.0) : 3400.0;
            hpfHz = 145.0;
        } else if (m == 17) {
            baseLpf = 2100.0; hpfHz = 320.0;
        }
        const double toneScale = 0.45 + (std::clamp(static_cast<double>(toneVal), 0.0, 10.0) / 10.0) * 1.15;
        feedbackLpf.setLowPass(sampleRate, std::clamp(baseLpf * toneScale, 900.0, 14000.0), 0.707);
        feedbackHpf.setHighPass(sampleRate, hpfHz, 0.707);
        sweepFilter.setPeaking(sampleRate, 900.0, 1.5, 6.0);
    }

    double sampleRate = 44100.0;
    int lastModel = -1;
    int maxDelaySamples = 110000;
    double modPhase = 0.0;
    double flutterPhase = 0.0;
    double revPhase = 0.0;
    float bbdCompEnvL = 0.05f, bbdCompEnvR = 0.05f;
    float bbdExpEnvL  = 0.05f, bbdExpEnvR  = 0.05f;
    float tapeDcL = 0.0f, tapeDcR = 0.0f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 110000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 110000 };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayL;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayR;
    BiquadFilter feedbackLpf;
    BiquadFilter feedbackHpf;
    BiquadFilter sweepFilter;
    BiquadFilter tapeHeadBump;
};

} // namespace pedalboard
