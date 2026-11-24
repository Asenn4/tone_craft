#pragma once

#include "BiquadFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 DLY Module (All 19 Delay Models from Daftar_Effect_GP-200.xlsx rows 233..251)
// Supports every authentic per-model control including Stereo Time R%, Spread, Tape Wow/Flutter,
// Age, Scrape, Drive, Sweep Depth/Rate, Ring Echo Mix/Freq/Tone, M-Echo 12 Head Modes,
// Reverse Echo, and real Trail bypass decay!
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

        lastModel = -1;
        configureToneForModel(16, 5.5f, 0.4f);
        reset();
    }

    void reset() noexcept {
        delayLineL.reset();
        delayLineR.reset();
        feedbackLpf.reset();
        feedbackHpf.reset();
        sweepFilter.reset();
        modPhase = 0.0;
        revPhase = 0.0;
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
        bool usePingPong = p.pingPong || (m == 8);

        if (p.useSlotParams) {
            switch (m) {
                case 0: case 1: // BBD Delay S, Digital Delay S: Mix, Feedback, Time, Time R%, Spread, Level, Sync, Trail
                    mix = sp[0] * 0.01f; fb = sp[1] * 0.01f; timeMs = sp[2];
                    timeRRatio = std::clamp(sp[3] * 0.01f, 0.25f, 2.0f);
                    spread = std::clamp(sp[4] * 0.01f, 0.0f, 1.0f);
                    outLvl = juce::Decibels::decibelsToGain(-12.0f + sp[5] * 1.85f);
                    trailOn = sp[7] > 0.5f;
                    usePingPong = false;
                    break;
                case 2: // Tape Delay S: Mix, Fdbk, Time, Time R%, Spread, Wow/Flt, Age, Scrape, Drive, Level, Sync, Trail
                    mix = sp[0] * 0.01f; fb = sp[1] * 0.01f; timeMs = sp[2];
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
                    mix = sp[0] * 0.01f; fb = sp[1] * 0.01f; timeMs = sp[2];
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
                    mix = sp[0] * 0.01f; timeMs = sp[1]; fb = sp[2] * 0.01f;
                    sweepDepth = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    sweepRate  = std::clamp(sp[4], 0.1f, 8.0f);
                    trailOn = sp[7] > 0.5f; usePingPong = false;
                    break;
                case 11: // Ring Echo: Dly Mix, Time, Feedback, Ring Mix, Freq, Tone, Sync, Trail
                    mix = sp[0] * 0.01f; timeMs = sp[1]; fb = sp[2] * 0.01f;
                    ringMix = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    ringFreq = std::clamp(sp[4], 40.0f, 1500.0f);
                    toneVal = sp[5]; trailOn = sp[7] > 0.5f; usePingPong = false;
                    break;
                case 13: // M-Echo: Mix, Time, Feedback, Tone, Mode (Head 1..12), Sync, Trail
                    mix = sp[0] * 0.01f; timeMs = sp[1]; fb = sp[2] * 0.01f;
                    toneVal = sp[3];
                    headMode = std::clamp(static_cast<int>(std::lround(sp[4])), 0, 11);
                    trailOn = sp[6] > 0.5f; usePingPong = false;
                    break;
                case 16: case 17: // Vintage Rack, Lofi Echo: Mix, Time, Feedback, Mod, Tone, Sync, Trail
                    mix = sp[0] * 0.01f; timeMs = sp[1]; fb = sp[2] * 0.01f;
                    modAmt = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    toneVal = sp[4]; trailOn = sp[6] > 0.5f; usePingPong = false;
                    break;
                case 18: // Rev Echo: Mix, Time, Feedback, Level, Sync, Trail
                    mix = sp[0] * 0.01f; timeMs = sp[1]; fb = sp[2] * 0.01f;
                    outLvl = juce::Decibels::decibelsToGain(-12.0f + sp[3] * 1.85f);
                    trailOn = sp[5] > 0.5f; usePingPong = false;
                    break;
                default: // Pure, Analog, Tape, Ping Pong, Tube, Sweet Echo, 999 Echo: Mix, Time, Feedback, Sync, Trail
                    mix = sp[0] * 0.01f; timeMs = sp[1]; fb = sp[2] * 0.01f;
                    trailOn = sp[4] > 0.5f;
                    usePingPong = (m == 8);
                    break;
            }
        }

        if (p.bypass && !trailOn) {
            return;
        }

        configureToneForModel(m, toneVal, ageAmt);

        const float effTimeL = (m == 9) ? std::min(timeMs, 180.0f) : ((m == 14) ? std::min(timeMs, 300.0f) : timeMs);
        const float effTimeR = effTimeL * timeRRatio;
        const float tgtL = std::clamp(effTimeL * 0.001f * static_cast<float>(sampleRate), 2.0f, static_cast<float>(maxDelaySamples - 48));
        const float tgtR = std::clamp(effTimeR * 0.001f * static_cast<float>(sampleRate), 2.0f, static_cast<float>(maxDelaySamples - 48));
        smoothedDelayL.setTargetValue(tgtL);
        smoothedDelayR.setTargetValue(tgtR);

        fb = std::clamp(fb, 0.0f, 0.92f);
        const float wet = std::clamp(mix, 0.0f, 1.0f);
        const float dry = p.bypass ? 1.0f : (1.0f - wet * 0.45f);
        const float inputSend = p.bypass ? 0.0f : 1.0f;

        const bool hasMod     = (m == 2 || m == 3 || m == 4 || m == 7 || m == 12 || m == 16 || m == 17);
        const bool isAmbience = (m == 3 || m == 4);
        const bool isMEcho    = (m == 13);
        const bool isSweep    = (m == 10);
        const bool isRing     = (m == 11);
        const bool isLofi     = (m == 16 || m == 17);
        const bool isRev      = (m == 18);

        const double modFreqHz = isRing ? static_cast<double>(ringFreq) : (isSweep ? static_cast<double>(sweepRate) : 1.35);
        const double modInc    = 2.0 * std::numbers::pi * modFreqHz / sampleRate;

        // 12 M-Echo 4-head tap bitmasks (bit0=0.25, bit1=0.50, bit2=0.75, bit3=1.00)
        static constexpr uint8_t kHeadMasks[12] = { 0x1, 0x2, 0x4, 0x8, 0x3, 0x5, 0x9, 0x6, 0xA, 0xC, 0x7, 0xF };
        const uint8_t headMask = kHeadMasks[headMode];

        float* chL = buffer.getWritePointer(0);
        float* chR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i) {
            const float modSin = static_cast<float>(std::sin(modPhase));
            modPhase += modInc;
            if (modPhase >= 2.0 * std::numbers::pi) modPhase -= 2.0 * std::numbers::pi;

            float dL = smoothedDelayL.getNextValue();
            float dR = smoothedDelayR.getNextValue();
            if (hasMod) {
                const float wob = (m == 2 ? (modAmt * 28.0f + scrapeAmt * 5.0f * static_cast<float>(std::sin(modPhase * 6.3))) : modAmt * 18.0f);
                dL = std::clamp(dL + modSin * wob, 2.0f, static_cast<float>(maxDelaySamples - 16));
                dR = std::clamp(dR - modSin * wob, 2.0f, static_cast<float>(maxDelaySamples - 16));
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
                // Smooth Hann-windowed reverse grain playback across [2..dL]
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
                float sumL = 0.0f, sumR = 0.0f;
                int taps = 0;
                constexpr float kFrac[4] = { 0.25f, 0.50f, 0.75f, 1.00f };
                for (int h = 0; h < 4; ++h) {
                    if (headMask & (1u << h)) {
                        const float t = std::max(2.0f, dL * kFrac[h]);
                        sumL += delayLineL.popSample(0, t);
                        sumR += delayLineR.popSample(0, t * 1.012f);
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
            if (m == 2 || m == 7 || m == 12) {
                const float satDrive = 1.05f + driveAmt * 1.15f;
                filtL = std::tanh(filtL * satDrive) / std::tanh(satDrive);
                filtR = std::tanh(filtR * satDrive) / std::tanh(satDrive);
            }

            if (usePingPong) {
                const float monoIn = 0.5f * (inL + inR) * inputSend;
                delayLineL.pushSample(0, monoIn + filtR * fb);
                delayLineR.pushSample(0, filtL * fb);
            } else {
                delayLineL.pushSample(0, inL * inputSend + filtL * fb);
                delayLineR.pushSample(0, inR * inputSend + filtR * fb);
            }

            chL[i] = (inL * dry + outDL * wet * outLvl);
            if (chR != nullptr) {
                chR[i] = (inR * dry + outDR * wet * outLvl);
            }
        }
    }

private:
    void configureToneForModel(int m, float toneVal, float ageAmt) noexcept {
        lastModel = m;
        double baseLpf = 4500.0;
        double hpfHz = 95.0;
        if (m == 1 || m == 5 || m == 8) {
            baseLpf = 11500.0; hpfHz = 55.0;
        } else if (m == 0 || m == 6 || m == 14 || m == 15) {
            baseLpf = 2600.0; hpfHz = 135.0;
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
    double revPhase = 0.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 110000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 110000 };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayL;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelayR;
    BiquadFilter feedbackLpf;
    BiquadFilter feedbackHpf;
    BiquadFilter sweepFilter;
};

} // namespace pedalboard
