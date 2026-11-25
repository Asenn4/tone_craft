#pragma once

#include "BiquadFilter.h"
#include "TempoSync.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <numbers>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 PRE Module (All 20 Models from Daftar_Effect_GP-200.xlsx rows 1..20)
// Supports every authentic per-model parameter from Daftar_Effect_GP-200.xlsx!
class CompressorStomp {
public:
    struct Params {
        bool bypass = false;
        int modelIndex = 0;         // 0..19 (kPreModels)
        float thresholdDb = -22.0f; // legacy
        float ratio = 4.0f;         // legacy
        float attackMs = 10.0f;     // legacy
        float releaseMs = 120.0f;   // legacy
        float makeupDb = 4.0f;      // legacy

        // Authentic per-model slot parameter vector (from Gp200ParamSpecs.h)
        bool useSlotParams = false;
        std::array<float, 12> slotParams {};
        float tempoBpm = 120.0f;
    };

    void prepare(const juce::dsp::ProcessSpec& spec) noexcept {
        sampleRate = std::max(8000.0, spec.sampleRate);
        lastModel = -1;
        reset();
    }

    void reset() noexcept {
        envDb = -96.0f;
        filterEnv = 0.0f;
        lfoPhase = 0.0;
        subSign = 1.0f;
        prevSample = 0.0f;
        pitchWritePos = 0;
        grainPhaseA = 0.0;
        grainPhaseB = 0.5;
        grain2PhaseA = 0.0;
        grain2PhaseB = 0.5;
        smoothedExpr = 0.0f;
        pitchBufL.fill(0.0f);
        pitchBufR.fill(0.0f);
        subLp.reset();
        toneFilter1.reset();
        toneFilter2.reset();
        toneFilter3.reset();
        gainReductionDb.store(0.0f, std::memory_order_relaxed);
    }

    float getGainReductionDb() const noexcept {
        return gainReductionDb.load(std::memory_order_relaxed);
    }

    void process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept {
        if (p.bypass) {
            gainReductionDb.store(0.0f, std::memory_order_relaxed);
            return;
        }

        const int numSamples = buffer.getNumSamples();
        const int numChannels = std::min(2, buffer.getNumChannels());
        if (numSamples <= 0 || numChannels <= 0) {
            return;
        }

        const int m = std::clamp(p.modelIndex, 0, 19);
        if (m != lastModel) {
            configureStaticFilters(m);
        }

        float* ch0 = buffer.getWritePointer(0);
        float* ch1 = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
        const auto& sp = p.slotParams;

        // ======================================================================
        // 1. Boost Family (3..9) & Tape Saturate (19)
        // ======================================================================
        if ((m >= 3 && m <= 9) || m == 19) {
            gainReductionDb.store(0.0f, std::memory_order_relaxed);
            float driveDb = p.makeupDb + (p.ratio - 1.0f) * 1.4f;
            float outVol = 1.0f;
            float wetMix = 1.0f;

            if (p.useSlotParams) {
                if (m == 3 || m == 7) { // Micro Boost / 14 Boost: Gain (0..20 dB)
                    driveDb = sp[0];
                } else if (m == 4 || m == 5 || m == 6) { // AC/B/P-Boost: Gain, Volume, Bass, Treble
                    driveDb = sp[0] * 2.2f;
                    outVol = juce::Decibels::decibelsToGain(-12.0f + sp[1] * 1.8f);
                    toneFilter1.setLowShelf(sampleRate, 130.0, (static_cast<double>(sp[2]) - 5.0) * 2.4, 0.8);
                    toneFilter2.setHighShelf(sampleRate, 3200.0, (static_cast<double>(sp[3]) - 5.0) * 2.4, 0.8);
                } else if (m == 8) { // FAT BB: Bass, Treble, Volume, Low Cut
                    driveDb = 6.0f;
                    outVol = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 2.0f);
                    toneFilter1.setLowShelf(sampleRate, 130.0, (static_cast<double>(sp[0]) - 5.0) * 2.4, 0.8);
                    toneFilter2.setHighShelf(sampleRate, 3200.0, (static_cast<double>(sp[1]) - 5.0) * 2.4, 0.8);
                    toneFilter3.setHighPass(sampleRate, sp[3] > 0.5f ? 155.0 : 30.0, 0.707);
                } else if (m == 9) { // Boost (EP Booster): Gain, +3dB, Bright
                    driveDb = sp[0] + (sp[1] > 0.5f ? 3.0f : 0.0f);
                    toneFilter2.setHighShelf(sampleRate, 3400.0, sp[2] > 0.5f ? 3.6 : 0.4, 0.75);
                } else if (m == 19) { // Saturate: Sat, Mix, Volume, High Cut
                    driveDb = sp[0] * 2.4f;
                    wetMix = std::clamp(sp[1] * 0.01f, 0.0f, 1.0f);
                    outVol = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 1.8f);
                    toneFilter1.setLowPass(sampleRate, std::clamp(static_cast<double>(sp[3]), 1500.0, 12000.0), 0.72);
                }
            }

            const float driveGain = juce::Decibels::decibelsToGain(std::clamp(driveDb, 0.0f, 26.0f));
            const float ceiling = (m == 19) ? 0.78f : 0.94f;
            for (int i = 0; i < numSamples; ++i) {
                for (int c = 0; c < numChannels; ++c) {
                    float* ptr = (c == 0) ? ch0 : ch1;
                    const float dry = ptr[i];
                    float s = toneFilter1.processSample(c, dry);
                    s = toneFilter2.processSample(c, s);
                    if (m == 8 && p.useSlotParams) s = toneFilter3.processSample(c, s);
                    const float wet = std::tanh(s * driveGain * (m == 19 ? 1.45f : 0.72f)) * ceiling;
                    ptr[i] = (dry * (1.0f - wetMix) + wet * wetMix) * outVol;
                }
            }
            return;
        }

        // ======================================================================
        // 2. Acoustic Simulator Family (10 AC Refiner: Shape; 11 AC Sim: Body, Top, Volume, Mode)
        // ======================================================================
        if (m == 10 || m == 11) {
            gainReductionDb.store(0.0f, std::memory_order_relaxed);
            float outGain = juce::Decibels::decibelsToGain(p.makeupDb * 0.5f);
            if (p.useSlotParams) {
                if (m == 10) {
                    const double sh = static_cast<double>(std::clamp(sp[0], 0.0f, 10.0f));
                    toneFilter1.setPeaking(sampleRate, 165.0 + sh * 12.0, 1.2, 2.0 + sh * 0.65);
                    toneFilter2.setHighShelf(sampleRate, 3800.0, 1.5 + sh * 0.55, 0.8);
                    outGain = 1.0f;
                } else {
                    const double body = static_cast<double>(std::clamp(sp[0], 0.0f, 10.0f));
                    const double top  = static_cast<double>(std::clamp(sp[1], 0.0f, 10.0f));
                    const int mode = std::clamp(static_cast<int>(std::lround(sp[3])), 0, 3);
                    constexpr double kBodyHz[4] = { 195.0, 145.0, 225.0, 265.0 };
                    constexpr double kMidCut[4] = { -4.0,  -5.5,  -6.5,  -2.5  };
                    toneFilter1.setPeaking(sampleRate, kBodyHz[mode], 1.25, (body - 3.0) * 1.15);
                    toneFilter2.setPeaking(sampleRate, 780.0, 0.9, kMidCut[mode]);
                    toneFilter3.setHighShelf(sampleRate, mode == 3 ? 5200.0 : 4200.0, (top - 3.0) * 1.1, 0.85);
                    outGain = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 1.9f);
                }
            }
            for (int i = 0; i < numSamples; ++i) {
                for (int c = 0; c < numChannels; ++c) {
                    float* ptr = (c == 0) ? ch0 : ch1;
                    float s = toneFilter1.processSample(c, ptr[i]);
                    s = toneFilter2.processSample(c, s);
                    if (m == 11 && p.useSlotParams) s = toneFilter3.processSample(c, s);
                    ptr[i] = s * outGain;
                }
            }
            return;
        }

        // ======================================================================
        // 3. Filter Family (12 T-Wah, 13 A-WAH, 14 Step Filter)
        // ======================================================================
        if (m >= 12 && m <= 14) {
            gainReductionDb.store(0.0f, std::memory_order_relaxed);
            double rateHz = 2.0;
            double qVal = 4.5;
            double lowHz = 280.0;
            double highHz = 2400.0;
            float sensScale = 8.0f;
            float wetMix = 1.0f;
            float outGain = 1.0f;
            double stepVals[4] = { 0.2, 0.75, 0.4, 0.92 };
            double depthNorm = 0.8;

            if (p.useSlotParams) {
                if (m == 12) { // T-Wah: Sens, Range, Q, Mix, Mode (Guitar/Bass)
                    sensScale = 2.5f + sp[0] * 1.8f;
                    const bool isBass = (sp[4] > 0.5f);
                    lowHz  = isBass ? 110.0 : 300.0;
                    highHz = lowHz * (2.5 + static_cast<double>(sp[1]) * 0.65);
                    qVal   = std::clamp(static_cast<double>(sp[2]), 1.5, 10.0);
                    wetMix = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                } else if (m == 13) { // A-WAH: Depth, Rate, Volume, Low, Q, High, Sync
                    depthNorm = std::clamp(static_cast<double>(sp[0]) * 0.01, 0.0, 1.0);
                    rateHz    = (sp[6] > 0.5f)
                                    ? static_cast<double>(tempo_sync::syncedModRateHz(sp[1], 0.2f, 8.0f, p.tempoBpm))
                                    : std::clamp(static_cast<double>(sp[1]), 0.15, 10.0);
                    outGain   = juce::Decibels::decibelsToGain(-12.0f + sp[2] * 1.8f);
                    lowHz     = std::clamp(static_cast<double>(sp[3]), 80.0, 1200.0);
                    qVal      = std::clamp(static_cast<double>(sp[4]), 1.5, 10.0);
                    highHz    = std::max(lowHz + 150.0, std::clamp(static_cast<double>(sp[5]), 600.0, 5000.0));
                } else { // Step Filter: Step 1-4, Rate, Sync
                    for (int k = 0; k < 4; ++k) stepVals[k] = std::clamp(static_cast<double>(sp[static_cast<size_t>(k)]) * 0.1, 0.0, 1.0);
                    rateHz = (sp[5] > 0.5f)
                                 ? static_cast<double>(tempo_sync::syncedModRateHz(sp[4], 0.5f, 12.0f, p.tempoBpm)) * 0.25
                                 : std::clamp(static_cast<double>(sp[4]), 0.3, 14.0);
                    qVal   = 5.5;
                    lowHz  = 240.0; highHz = 2800.0;
                }
            }

            const double phaseInc = 2.0 * std::numbers::pi * rateHz / sampleRate;
            for (int i = 0; i < numSamples; ++i) {
                float peak = std::abs(ch0[i]);
                if (ch1 != nullptr) peak = std::max(peak, std::abs(ch1[i]));
                filterEnv = 0.992f * filterEnv + 0.008f * peak;

                lfoPhase += phaseInc;
                if (lfoPhase >= 2.0 * std::numbers::pi) lfoPhase -= 2.0 * std::numbers::pi;

                if ((i & 7) == 0) {
                    double sweepNorm = 0.5;
                    if (m == 12) {
                        sweepNorm = std::clamp(static_cast<double>(filterEnv * sensScale), 0.0, 1.0);
                    } else if (m == 13) {
                        sweepNorm = 0.5 + 0.5 * depthNorm * std::sin(lfoPhase);
                    } else {
                        const int step = static_cast<int>((lfoPhase / (2.0 * std::numbers::pi)) * 4.0) & 3;
                        sweepNorm = stepVals[step];
                    }
                    const double cutoff = lowHz * std::pow(highHz / std::max(40.0, lowHz), sweepNorm);
                    toneFilter1.setPeaking(sampleRate, cutoff, qVal, 11.5);
                }

                const float d0 = ch0[i];
                ch0[i] = (d0 * (1.0f - wetMix) + toneFilter1.processSample(0, d0) * wetMix) * outGain;
                if (ch1 != nullptr) {
                    const float d1 = ch1[i];
                    ch1[i] = (d1 * (1.0f - wetMix) + toneFilter1.processSample(1, d1) * wetMix) * outGain;
                }
            }
            return;
        }

        // ======================================================================
        // 4. Pitch / Octave / Whammy (P-Bend) / Ring Mod (15..18)
        // ======================================================================
        if (m >= 15 && m <= 18) {
            gainReductionDb.store(0.0f, std::memory_order_relaxed);

            if (m == 18) { // Ring Mod: Mix, Freq, Fine, Tone
                const float wetMix = p.useSlotParams ? std::clamp(sp[0] * 0.01f, 0.0f, 1.0f) : 0.65f;
                const double freq  = p.useSlotParams ? std::clamp(static_cast<double>(sp[1] + sp[2]), 10.0, 2200.0) : 320.0;
                const double toneHz = p.useSlotParams ? (1200.0 + static_cast<double>(sp[3]) * 850.0) : 6000.0;
                toneFilter1.setLowPass(sampleRate, toneHz, 0.72);
                const double ringInc = 2.0 * std::numbers::pi * freq / sampleRate;
                for (int i = 0; i < numSamples; ++i) {
                    const float carrier = static_cast<float>(std::sin(lfoPhase));
                    lfoPhase += ringInc;
                    if (lfoPhase >= 2.0 * std::numbers::pi) lfoPhase -= 2.0 * std::numbers::pi;
                    const float in0 = ch0[i];
                    const float rm0 = toneFilter1.processSample(0, in0 * carrier);
                    ch0[i] = in0 * (1.0f - wetMix) + rm0 * wetMix;
                    if (ch1 != nullptr) {
                        const float in1 = ch1[i];
                        const float rm1 = toneFilter1.processSample(1, in1 * carrier);
                        ch1[i] = in1 * (1.0f - wetMix) + rm1 * wetMix;
                    }
                }
                return;
            }

            // Expression Pedal Pitch Shifter (15 OCTA, 16 Pitch, 17 P-Bend)
            const float exprTarget = p.useSlotParams ? std::clamp(sp[0], 0.0f, 1.0f) : 0.5f;
            const bool autoSweep = p.useSlotParams && ((m == 15) ? (sp[4] > 0.5f) : (sp[6] > 0.5f));
            const double autoRateInc = 2.0 * std::numbers::pi * 0.65 / sampleRate;
            constexpr double kGrainWindowSamples = 1600.0;

            for (int i = 0; i < numSamples; ++i) {
                float targetExpr = exprTarget;
                if (autoSweep) {
                    lfoPhase += autoRateInc;
                    if (lfoPhase >= 2.0 * std::numbers::pi) lfoPhase -= 2.0 * std::numbers::pi;
                    targetExpr = static_cast<float>(0.5 + 0.5 * std::sin(lfoPhase));
                }
                smoothedExpr = 0.995f * smoothedExpr + 0.005f * targetExpr;

                const float inL = ch0[i];
                const float inR = (ch1 != nullptr) ? ch1[i] : inL;
                pitchBufL[static_cast<size_t>(pitchWritePos)] = inL;
                pitchBufR[static_cast<size_t>(pitchWritePos)] = inR;

                if (m == 15) { // OCTA: Pedal, Low Oct, High Oct, Dry
                    if (prevSample < 0.0f && inL >= 0.0f) subSign = -subSign;
                    prevSample = inL;
                    const float subOct = subLp.processSample(0, inL * subSign);
                    const float upOct  = readDualGrainShifted(0, grainPhaseA, grainPhaseB, 2.0);
                    advanceDualGrain(grainPhaseA, grainPhaseB, 2.0, kGrainWindowSamples);
                    pitchWritePos = (pitchWritePos + 1) & (kPitchBufSize - 1);

                    const float subLvl = std::clamp(sp[1] * 0.01f, 0.0f, 1.0f) * (1.15f - 0.55f * smoothedExpr);
                    const float upLvl  = std::clamp(sp[2] * 0.01f, 0.0f, 1.0f) * (0.35f + 0.85f * smoothedExpr);
                    const float dryLvl = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    ch0[i] = inL * dryLvl + subOct * subLvl + upOct * upLvl;
                    if (ch1 != nullptr) ch1[i] = inR * dryLvl + subOct * subLvl + upOct * upLvl;
                } else if (m == 16) { // Pitch: Pedal, Low Pitch, Hi Pitch, Dry, Low Vol, Hi Vol
                    const double rLow = std::pow(2.0, std::clamp(static_cast<double>(sp[1]), -24.0, 24.0) / 12.0);
                    const double rHi  = std::pow(2.0, std::clamp(static_cast<double>(sp[2]), -24.0, 24.0) / 12.0);
                    const float lowL = readDualGrainShifted(0, grainPhaseA, grainPhaseB, rLow);
                    const float hiL  = readDualGrainShifted(0, grain2PhaseA, grain2PhaseB, rHi);
                    const float lowR = (ch1 != nullptr) ? readDualGrainShifted(1, grainPhaseA, grainPhaseB, rLow) : lowL;
                    const float hiR  = (ch1 != nullptr) ? readDualGrainShifted(1, grain2PhaseA, grain2PhaseB, rHi) : hiL;
                    advanceDualGrain(grainPhaseA, grainPhaseB, rLow, kGrainWindowSamples);
                    advanceDualGrain(grain2PhaseA, grain2PhaseB, rHi, kGrainWindowSamples);
                    pitchWritePos = (pitchWritePos + 1) & (kPitchBufSize - 1);

                    const float dryLvl = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    const float lowLvl = std::clamp(sp[4] * 0.01f, 0.0f, 1.0f) * smoothedExpr;
                    const float hiLvl  = std::clamp(sp[5] * 0.01f, 0.0f, 1.0f) * smoothedExpr;
                    ch0[i] = inL * dryLvl + lowL * lowLvl + hiL * hiLvl;
                    if (ch1 != nullptr) ch1[i] = inR * dryLvl + lowR * lowLvl + hiR * hiLvl;
                } else { // 17: P-Bend (Whammy): Pedal, Hi Pitch, Low Pitch, Wet, Dry, Range
                    const double rangeScale = std::clamp(static_cast<double>(sp[5]) * 0.01, 0.1, 1.0);
                    const double st = static_cast<double>(sp[2]) + static_cast<double>(smoothedExpr) * rangeScale * static_cast<double>(sp[1] - sp[2]);
                    const double ratio = std::pow(2.0, std::clamp(st, -24.0, 24.0) / 12.0);
                    const float wL = readDualGrainShifted(0, grainPhaseA, grainPhaseB, ratio);
                    const float wR = (ch1 != nullptr) ? readDualGrainShifted(1, grainPhaseA, grainPhaseB, ratio) : wL;
                    advanceDualGrain(grainPhaseA, grainPhaseB, ratio, kGrainWindowSamples);
                    pitchWritePos = (pitchWritePos + 1) & (kPitchBufSize - 1);

                    const float wetLvl = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
                    const float dryLvl = std::clamp(sp[4] * 0.01f, 0.0f, 1.0f);
                    ch0[i] = inL * dryLvl + wL * wetLvl;
                    if (ch1 != nullptr) ch1[i] = inR * dryLvl + wR * wetLvl;
                }
            }
            return;
        }

        // ======================================================================
        // 5. Compressor Family (0 COMP Ross, 1 COMP4 Keeley, 2 S-Comp Studio)
        // ======================================================================
        float threshDb = p.thresholdDb;
        float ratio    = p.ratio;
        float atkMs    = p.attackMs;
        float relMs    = p.releaseMs;
        float makeupDb = p.makeupDb;
        float clipAmt  = 0.0f;
        float blendWet = 1.0f;

        if (p.useSlotParams) {
            if (m == 0) { // COMP: Sustain (0..100%), Volume (0..100%)
                threshDb = -8.0f - sp[0] * 0.36f;
                ratio    = 3.0f + sp[0] * 0.06f;
                atkMs    = 5.5f;
                relMs    = 95.0f;
                makeupDb = -6.0f + sp[1] * 0.22f;
            } else if (m == 1) { // COMP4: Sustain, Attack, Volume, Clipping
                threshDb = -8.0f - sp[0] * 0.36f;
                ratio    = 3.2f + sp[0] * 0.06f;
                atkMs    = 1.5f + sp[1] * 0.65f;
                relMs    = 110.0f;
                makeupDb = -6.0f + sp[2] * 0.22f;
                clipAmt  = std::clamp(sp[3] * 0.01f, 0.0f, 1.0f);
            } else { // 2: S-Comp: Threshold, Ratio, Volume, Attack, Release, Tone, Blend
                threshDb = sp[0];
                ratio    = sp[1];
                makeupDb = sp[2];
                atkMs    = sp[3];
                relMs    = sp[4];
                toneFilter1.setHighShelf(sampleRate, 2800.0, (static_cast<double>(sp[5]) - 5.0) * 1.8, 0.8);
                blendWet = std::clamp(sp[6] * 0.01f, 0.0f, 1.0f);
            }
        }

        const float atkSec = std::clamp(atkMs * 0.001f, 0.001f, 0.150f);
        const float relSec = std::clamp(relMs * 0.001f, 0.015f, 0.800f);
        const float atkCoeff = std::exp(-1.0f / static_cast<float>(atkSec * static_cast<float>(sampleRate)));
        const float relCoeff = std::exp(-1.0f / static_cast<float>(relSec * static_cast<float>(sampleRate)));
        const float effRatio = std::clamp(ratio, 1.0f, 20.0f);
        const float makeupGain = juce::Decibels::decibelsToGain(makeupDb);
        constexpr float kneeWidth = 4.0f;
        constexpr float halfKnee = kneeWidth * 0.5f;

        float maxGrDb = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float peak = std::abs(ch0[i]);
            if (ch1 != nullptr) peak = std::max(peak, std::abs(ch1[i]));

            const float inDb = juce::Decibels::gainToDecibels(peak, -96.0f);
            const float coeff = (inDb > envDb) ? atkCoeff : relCoeff;
            envDb = coeff * envDb + (1.0f - coeff) * inDb;

            const float over = envDb - threshDb;
            float grDb = 0.0f;
            if (over > halfKnee) {
                grDb = over * (1.0f - 1.0f / effRatio);
            } else if (over > -halfKnee) {
                const float x = over + halfKnee;
                grDb = (1.0f - 1.0f / effRatio) * (x * x) / (2.0f * kneeWidth);
            }

            maxGrDb = std::max(maxGrDb, grDb);
            const float gain = juce::Decibels::decibelsToGain(-grDb) * makeupGain;

            const float dry0 = ch0[i];
            float s0 = dry0 * gain;
            if (m == 1 && clipAmt > 0.01f) {
                const float cDrive = 1.0f + clipAmt * 1.4f;
                s0 = std::tanh(s0 * cDrive) / std::tanh(cDrive);
            }
            if (m == 0 || m == 2) s0 = toneFilter1.processSample(0, s0);
            ch0[i] = dry0 * (1.0f - blendWet) + s0 * blendWet;

            if (ch1 != nullptr) {
                const float dry1 = ch1[i];
                float s1 = dry1 * gain;
                if (m == 1 && clipAmt > 0.01f) {
                    const float cDrive = 1.0f + clipAmt * 1.4f;
                    s1 = std::tanh(s1 * cDrive) / std::tanh(cDrive);
                }
                if (m == 0 || m == 2) s1 = toneFilter1.processSample(1, s1);
                ch1[i] = dry1 * (1.0f - blendWet) + s1 * blendWet;
            }
        }

        gainReductionDb.store(maxGrDb, std::memory_order_relaxed);
    }

private:
    void configureStaticFilters(int m) noexcept {
        lastModel = m;
        subLp.setLowPass(sampleRate, 260.0, 0.75);
        if (m == 0) {
            toneFilter1.setLowPass(sampleRate, 6200.0, 0.707);
        } else if (m == 7) {
            toneFilter1.setHighPass(sampleRate, 175.0, 0.80);
            toneFilter2.setPeaking(sampleRate, 1850.0, 1.1, 6.5);
        } else if (m == 8) {
            toneFilter1.setLowShelf(sampleRate, 130.0, 0.0, 0.8);
            toneFilter2.setHighShelf(sampleRate, 3200.0, 0.0, 0.8);
            toneFilter3.setHighPass(sampleRate, 30.0, 0.707);
        } else if (m == 9) {
            toneFilter1.setLowShelf(sampleRate, 220.0, 2.8, 0.75);
            toneFilter2.setHighShelf(sampleRate, 3400.0, 2.2, 0.75);
        } else if (m >= 3 && m <= 6) {
            toneFilter1.setHighPass(sampleRate, 85.0, 0.707);
            toneFilter2.setPeaking(sampleRate, 820.0, 0.8, (m == 4 || m == 5) ? 3.5 : 1.5);
        } else if (m == 10 || m == 11) {
            toneFilter1.setPeaking(sampleRate, 195.0, 1.3, 5.2);
            toneFilter2.setHighShelf(sampleRate, 4400.0, 6.0, 0.85);
            toneFilter3.setHighShelf(sampleRate, 4200.0, 2.0, 0.85);
        } else if (m == 19) {
            toneFilter1.setLowPass(sampleRate, 6500.0, 0.707);
            toneFilter2.setPeaking(sampleRate, 260.0, 0.8, 2.4);
        }
    }

    float readPitchBufLinear(int ch, double delaySamples) const noexcept {
        const auto& buf = (ch == 0) ? pitchBufL : pitchBufR;
        double readPos = static_cast<double>(pitchWritePos) - delaySamples;
        while (readPos < 0.0) readPos += static_cast<double>(kPitchBufSize);
        const int idx0 = static_cast<int>(readPos) & (kPitchBufSize - 1);
        const int idx1 = (idx0 + 1) & (kPitchBufSize - 1);
        const float frac = static_cast<float>(readPos - std::floor(readPos));
        return buf[static_cast<size_t>(idx0)] * (1.0f - frac) + buf[static_cast<size_t>(idx1)] * frac;
    }

    float readDualGrainShifted(int ch, double phA, double phB, double /*pitchRatio*/) const noexcept {
        constexpr double kWin = 1600.0;
        const double dA = 32.0 + phA * kWin;
        const double dB = 32.0 + phB * kWin;
        const float wA = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * phA));
        const float wB = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * phB));
        return readPitchBufLinear(ch, dA) * wA + readPitchBufLinear(ch, dB) * wB;
    }

    static void advanceDualGrain(double& phA, double& phB, double pitchRatio, double grainWinSamples) noexcept {
        const double delta = (1.0 - pitchRatio) / grainWinSamples;
        phA += delta;
        phB += delta;
        while (phA < 0.0) phA += 1.0;
        while (phA >= 1.0) phA -= 1.0;
        while (phB < 0.0) phB += 1.0;
        while (phB >= 1.0) phB -= 1.0;
    }

    static constexpr int kPitchBufSize = 8192;

    double sampleRate = 44100.0;
    int lastModel = -1;
    float envDb = -96.0f;
    float filterEnv = 0.0f;
    double lfoPhase = 0.0;
    float subSign = 1.0f;
    float prevSample = 0.0f;
    float smoothedExpr = 0.0f;
    int pitchWritePos = 0;
    double grainPhaseA = 0.0;
    double grainPhaseB = 0.5;
    double grain2PhaseA = 0.0;
    double grain2PhaseB = 0.5;
    std::array<float, kPitchBufSize> pitchBufL {};
    std::array<float, kPitchBufSize> pitchBufR {};
    std::atomic<float> gainReductionDb { 0.0f };
    BiquadFilter subLp;
    BiquadFilter toneFilter1;
    BiquadFilter toneFilter2;
    BiquadFilter toneFilter3;
};

} // namespace pedalboard
