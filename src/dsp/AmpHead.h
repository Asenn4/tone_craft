#pragma once

#include "BiquadFilter.h"
#include "NamModelRunner.h"
#include <algorithm>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 AMP Module (All 71 GP-200 Amp Models from Daftar_Effect_GP-200.xlsx rows 59..129 + Neural NAM = 72 total!):
// - 0..7:   American Tweed / Bassman / Twin / Deluxe / Vibro / Silver Twin / Supro
// - 8..13:  VOX AC-100 / AC30HW / Roland J-120 CL / Matchless Chieftain
// - 14..29: Mesa Lone Star / Bogner Shiva & XTC / Dr. Z Maz 38 / Pendragon / Bad Cat
// - 30..32: Soldano SLO100 (CL / OD / LD)
// - 33..45: British Marshall JTM45 / JMP50 / 1959SLP / JCM800 / JCM900 + Friedman Brown Eye (Flagman)
// - 46..53: Mesa/Boogie Mark IIC+ / Mark IV (Mess4 LD 1-3) / Dual Rectifier (Mess DualV / DualM)
// - 54..63: Orange AD30 & Rockerverb 100 / Peavey 5150 (EV 51) / ENGL Savage & Powerball / Diezel VH4
// - 64..68: Bass Amps (Ampeg SVT, VOX AC-100 Bass, Mesa Bass 400, Ampeg B-15, Alembic F-2B)
// - 69..70: Acoustic Preamps (AER Colourizer AC Pre 1 & 2)
// - 71:     Neural NAM Capture (.nam deep-learning model runner)
class AmpHead {
public:
    struct Params {
        bool bypass = false;
        int channel = 1;       // Legacy 0..3 category (0=Clean, 1=Crunch, 2=High-Gain, 3=NAM)
        int modelIndex = 39;   // 0..71 into gp200::kAmpModels (default 39 = UK SLP / Plexi)
        float gain = 5.5f;     // 0..10 (or Tone on AC Pre)
        float bass = 5.5f;     // 0..10 (or Tone 1 on SUPDual OD, EQ Freq on AC Pre)
        float mid = 5.0f;      // 0..10 (or EQ Q on AC Pre)
        float treble = 5.5f;   // 0..10 (or Tone on Tweedy, EQ Gain on AC Pre)
        float presence = 5.0f; // 0..10 (or Tone Cut on VOX/Dr.Z)
        float master = 6.0f;   // 0..10 (Volume / Master)
        float extra = 5.0f;    // 0..10 (Gain 2 on UK 45JP/50JP/SUPDual OD, Input on L-Star OD, Edge on Bad-KT OD, Balance on AC Pre)
        int switchVal = 0;     // 0..4  (Bright 0/1, Char Cool/Hot 0/1, Ampeg SVT Midrange 220/450/800/1.6k/3kHz)
    };

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = std::max(8000.0, spec.sampleRate);
        oversampling.reset();
        oversampling.initProcessing(static_cast<size_t>(spec.maximumBlockSize));

        inputHpf.setHighPass(sampleRate, 78.0, 0.707);
        interStageHpf.setHighPass(sampleRate * 4.0, 95.0, 0.707);
        powerLpf.setLowPass(sampleRate, 9500.0, 0.707);

        namRunner.prepare(sampleRate, static_cast<int>(spec.maximumBlockSize));

        invalidateCache();
        reset();
    }

    void reset() noexcept {
        oversampling.reset();
        inputHpf.reset();
        preVoicingFilter.reset();
        interStageHpf.reset();
        bassFilter.reset();
        midFilter.reset();
        trebleFilter.reset();
        presenceFilter.reset();
        powerLpf.reset();
        namRunner.reset();
    }

    NamModelRunner& getNamRunner() noexcept { return namRunner; }
    const NamModelRunner& getNamRunner() const noexcept { return namRunner; }

    // Map legacy 4-channel tab index (0..3) to representative GP-200 model index
    static int defaultModelForChannel(int ch) noexcept {
        switch (ch) {
            case 0:  return 6;  // Silver Twin (Clean)
            case 1:  return 39; // UK SLP (BritPlexi Crunch)
            case 2:  return 51; // Mess4 LD 3 (Mesa Mark IV / Hi-Gain)
            case 3:  return 71; // Neural NAM Capture
            default: return 39;
        }
    }

    // Derive category tab index (0=Clean, 1=Drive, 2=Hi-Gain, 3=NAM) from any of the 72 GP-200 amp models
    static int categoryForModel(int modelIdx) noexcept {
        if (modelIdx >= 71) return 3; // NAM Capture
        // Hi-Gain models in GP-200 list:
        // 21..22 (Bog Red), 32 (Solo100 LD), 41 (UK 900), 44..45 (Flagman+), 49..53 (Mess4/Dual), 55..63 (Juice R100, EV 51, Eagle, Power LD, Dizz VH)
        if ((modelIdx >= 21 && modelIdx <= 22) || modelIdx == 32 || modelIdx == 41
            || (modelIdx >= 44 && modelIdx <= 45) || (modelIdx >= 49 && modelIdx <= 53)
            || (modelIdx >= 55 && modelIdx <= 63)) {
            return 2;
        }
        // Clean / Bass / Acoustic models:
        if (modelIdx <= 1 || (modelIdx >= 3 && modelIdx <= 7) || modelIdx == 9 || modelIdx == 10
            || modelIdx == 12 || modelIdx == 13 || modelIdx == 15 || modelIdx == 17 || modelIdx == 23
            || modelIdx == 25 || modelIdx == 26 || modelIdx == 28 || modelIdx == 30 || modelIdx >= 64) {
            return 0;
        }
        return 1; // Drive / Crunch
    }

    void process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept {
        if (p.bypass) {
            return;
        }

        const int numSamples = buffer.getNumSamples();
        const int numChannels = std::min(2, buffer.getNumChannels());
        if (numSamples <= 0 || numChannels <= 0) {
            return;
        }

        const int m = std::clamp(p.modelIndex, 0, 71);
        updateToneStackIfNeeded(p, m);

        if (m == 71 || p.channel == 3) {
            const float driveDb = (std::clamp(p.gain, 0.0f, 10.0f) - 5.0f) * 3.0f;
            const float inputDrive = juce::Decibels::decibelsToGain(driveDb);
            namRunner.process(buffer, inputDrive);

            const float masterGain = juce::Decibels::decibelsToGain(-18.0f + (std::clamp(p.master, 0.0f, 10.0f) / 10.0f) * 24.0f);
            for (int ch = 0; ch < numChannels; ++ch) {
                float* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i) {
                    float s = bassFilter.processSample(ch, data[i]);
                    s = midFilter.processSample(ch, s);
                    s = trebleFilter.processSample(ch, s);
                    s = presenceFilter.processSample(ch, s);
                    data[i] = std::tanh(s * masterGain * 0.95f);
                }
            }
            return;
        }

        // Special handling for AER Colourizer Acoustic Preamps (69: AC Pre, 70: AC Pre 2)
        if (m == 69 || m == 70) {
            const float balanceWet = std::clamp(p.extra / 10.0f, 0.0f, 1.0f);
            const float masterGain = juce::Decibels::decibelsToGain(-18.0f + (std::clamp(p.master, 0.0f, 10.0f) / 10.0f) * 24.0f);
            for (int ch = 0; ch < numChannels; ++ch) {
                float* data = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i) {
                    const float dry = data[i];
                    float s = inputHpf.processSample(ch, dry);
                    s = midFilter.processSample(ch, s);    // Parametric EQ (Freq / Q / Gain)
                    s = trebleFilter.processSample(ch, s); // Tone tilt
                    data[i] = (dry * (1.0f - balanceWet) + s * balanceWet) * masterGain;
                }
            }
            return;
        }

        // Pre-EQ & tight input HPF tailored to the active GP-200 amp model
        for (int ch = 0; ch < numChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                float s = inputHpf.processSample(ch, data[i]);
                data[i] = preVoicingFilter.processSample(ch, s);
            }
        }

        // Combine dual-gain / input / hot-char extras cleanly for authentic amps (UK JP, SUPDual OD, L-Star OD, Foxy 30TB, Bad-KT OD)
        float effGain = std::clamp(p.gain, 0.0f, 10.0f);
        if (m == 8 || m == 35 || m == 38) { // SUPDual OD / UK 45JP / UK 50JP: Gain 1 + Gain 2
            effGain = std::clamp(effGain * 0.62f + std::clamp(p.extra, 0.0f, 10.0f) * 0.55f, 0.0f, 10.0f);
        } else if (m == 16) { // L-Star OD: Input + Gain
            effGain = std::clamp(effGain * 0.72f + std::clamp(p.extra, 0.0f, 10.0f) * 0.42f, 0.0f, 10.0f);
        } else if (m == 11 && p.switchVal >= 1) { // Foxy 30TB Char: Hot mode
            effGain = std::clamp(effGain * 1.18f + 1.2f, 0.0f, 10.0f);
        } else if (m == 65 || m == 67 || m == 68) { // Foxy Bass, Mini Bass, Bass Pre (volume-only gain structure)
            effGain = 3.8f;
        }

        const float gNorm = effGain / 10.0f;
        const float stage1Gain = vStage1Min + std::pow(gNorm, 1.35f) * vStage1Span;
        const float stage2Gain = vStage2Min + gNorm * vStage2Span;
        const float asymBias = vAsymBias;
        const float stage2Scale = vStage2Scale;

        const float dc1 = std::tanh(asymBias);
        const float dc2 = std::tanh(-asymBias * 0.6f);

        juce::dsp::AudioBlock<float> block(buffer);
        auto subBlock = block.getSubsetChannelBlock(0, static_cast<size_t>(numChannels));
        auto upBlock = oversampling.processSamplesUp(subBlock);

        const size_t upLen = upBlock.getNumSamples();
        for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch) {
            float* up = upBlock.getChannelPointer(ch);
            for (size_t i = 0; i < upLen; ++i) {
                float s = std::tanh(up[i] * stage1Gain + asymBias) - dc1;
                s = interStageHpf.processSample(static_cast<int>(ch), s);
                s = (std::tanh(s * stage2Gain - asymBias * 0.6f) - dc2) * stage2Scale;
                up[i] = s;
            }
        }

        oversampling.processSamplesDown(subBlock);

        const float masterGain = juce::Decibels::decibelsToGain(-20.0f + (std::clamp(p.master, 0.0f, 10.0f) / 10.0f) * 26.0f);
        for (int ch = 0; ch < numChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                float s = bassFilter.processSample(ch, data[i]);
                s = midFilter.processSample(ch, s);
                s = trebleFilter.processSample(ch, s);
                s = presenceFilter.processSample(ch, s);
                s = powerLpf.processSample(ch, s);
                data[i] = std::tanh(s * masterGain * 0.9f) * 0.85f;
            }
        }
    }

private:
    void invalidateCache() noexcept {
        lastBass = lastMid = lastTreble = lastPresence = lastExtra = lastGain = -1.0f;
        lastSwitch = -1;
        lastModel = -1;
    }

    void updateToneStackIfNeeded(const Params& p, int m) noexcept {
        const bool dirty = (m != lastModel)
                        || (std::abs(p.bass - lastBass) > 1e-3f)
                        || (std::abs(p.mid - lastMid) > 1e-3f)
                        || (std::abs(p.treble - lastTreble) > 1e-3f)
                        || (std::abs(p.presence - lastPresence) > 1e-3f)
                        || (std::abs(p.extra - lastExtra) > 1e-3f)
                        || (std::abs(p.gain - lastGain) > 1e-3f)
                        || (p.switchVal != lastSwitch);
        if (!dirty) {
            return;
        }

        lastModel = m;
        lastBass = p.bass;
        lastMid = p.mid;
        lastTreble = p.treble;
        lastPresence = p.presence;
        lastExtra = p.extra;
        lastGain = p.gain;
        lastSwitch = p.switchVal;

        const int cat = categoryForModel(m);

        // Configure per-family preamp gain structure & voicing filters across all 71 GP-200 amp models
        double inHpfHz = 75.0;
        double preMidHz = 750.0;
        double preMidDb = 0.0;
        double toneBassHz = 115.0;
        double toneMidHz = 650.0;
        double toneMidOffsetDb = 0.0;
        double toneTrebHz = 2800.0;
        double presHz = 4600.0;
        double pwrLpfHz = 9500.0;

        if (m == 12) {
            // J-120 CL (Roland Jazz Chorus solid-state ultra-clean)
            inHpfHz = 65.0; preMidHz = 500.0; preMidDb = -1.8;
            vStage1Min = 0.9f; vStage1Span = 1.6f; vStage2Min = 0.9f; vStage2Span = 0.6f;
            vAsymBias = 0.01f; vStage2Scale = 0.98f; toneMidHz = 480.0; pwrLpfHz = 12500.0;
        } else if (m >= 69 && m <= 70) {
            // AC Pre 1 & 2 (AER Acoustic Preamp)
            inHpfHz = 45.0; preMidHz = 380.0; preMidDb = -1.5;
            vStage1Min = 0.85f; vStage1Span = 1.4f; vStage2Min = 0.9f; vStage2Span = 0.5f;
            vAsymBias = 0.02f; vStage2Scale = 0.98f; pwrLpfHz = 15000.0;
        } else if (m >= 64 && m <= 68) {
            // Bass Amps (Classic Bass SVT, Foxy Bass, Mess Bass, Mini Bass B-15, Bass Pre)
            inHpfHz = 32.0; preMidHz = 280.0; preMidDb = 2.8;
            vStage1Min = 1.2f; vStage1Span = 6.5f; vStage2Min = 1.1f; vStage2Span = 2.2f;
            vAsymBias = 0.08f; vStage2Scale = 0.92f;
            toneBassHz = 72.0; toneMidHz = 380.0; toneTrebHz = 2200.0; pwrLpfHz = 7800.0;
        } else if (m >= 9 && m <= 11) {
            // Foxy 15TB / 30N / 30TB (VOX AC15/AC30 Class-A Top Boost chime)
            inHpfHz = 105.0; preMidHz = 1100.0; preMidDb = 3.6;
            vStage1Min = 1.4f; vStage1Span = (m == 11) ? 14.0f : 6.0f;
            vStage2Min = 1.2f; vStage2Span = (m == 11) ? 3.8f : 1.8f;
            vAsymBias = 0.16f; vStage2Scale = 0.88f;
            toneMidHz = 920.0; toneTrebHz = 3400.0;
        } else if (m >= 33 && m <= 45) {
            // UK 45 / 50 / SLP / 800 / 900 + Flagman (Marshall & Friedman British roar)
            inHpfHz = 112.0; preMidHz = 840.0; preMidDb = 3.8;
            const bool isHbe = (m == 41 || m >= 44);
            const bool is800 = (m >= 39);
            vStage1Min = isHbe ? 5.5f : (is800 ? 3.2f : 2.2f);
            vStage1Span = isHbe ? 38.0f : (is800 ? 22.0f : 14.5f);
            vStage2Min = isHbe ? 2.8f : 1.7f;
            vStage2Span = isHbe ? 8.5f : 4.2f;
            vAsymBias = 0.15f; vStage2Scale = 0.82f;
            toneMidHz = 740.0; toneMidOffsetDb = 1.4;
        } else if (m >= 46 && m <= 53) {
            // Mess2C+ / Mess4 LD 1-3 / Mess DualV / DualM (Mesa Mark & Dual Rectifier)
            const bool isRecto = (m >= 52);
            inHpfHz = isRecto ? 92.0 : 135.0;
            preMidHz = isRecto ? 620.0 : 780.0;
            preMidDb = isRecto ? 1.5 : 4.8; // Mark series has vocal pre-mid focus + post V-scoop
            vStage1Min = 6.2f; vStage1Span = 44.0f;
            vStage2Min = 3.2f; vStage2Span = 10.5f;
            vAsymBias = 0.21f; vStage2Scale = 0.72f;
            toneMidHz = isRecto ? 560.0 : 750.0;
            toneMidOffsetDb = isRecto ? -2.8 : -1.8;
            presHz = 5100.0;
        } else if (m >= 54 && m <= 63) {
            // Juice (Orange) / EV 51 (5150) / Eagle & Power LD (ENGL) / Dizz VH (Diezel)
            const bool isOrange = (m <= 55);
            const bool is5150 = (m == 56);
            inHpfHz = isOrange ? 82.0 : (is5150 ? 140.0 : 120.0);
            preMidHz = isOrange ? 580.0 : (is5150 ? 1150.0 : 920.0);
            preMidDb = 3.5;
            vStage1Min = isOrange ? 4.2f : 6.8f;
            vStage1Span = isOrange ? 28.0f : 48.0f;
            vStage2Min = 3.0f; vStage2Span = 11.0f;
            vAsymBias = 0.23f; vStage2Scale = 0.70f;
            toneMidHz = isOrange ? 540.0 : 680.0;
            toneMidOffsetDb = isOrange ? 1.2 : -1.5;
        } else if (cat == 0) {
            // Fender Tweed / Twin / Deluxe / Lone Star Clean / Shiva Clean / Soldano Clean
            inHpfHz = 72.0; preMidHz = 520.0; preMidDb = -1.2;
            vStage1Min = 1.05f; vStage1Span = 4.2f;
            vStage2Min = 1.0f;  vStage2Span = 1.5f;
            vAsymBias = 0.04f;  vStage2Scale = 0.95f;
            toneMidHz = 490.0;  toneMidOffsetDb = -1.0;
        } else {
            // Boutique Overdrive & Crunch (Bogner Blue, Matchless OD, Lone Star OD, Soldano OD, etc.)
            inHpfHz = 95.0; preMidHz = 760.0; preMidDb = 2.6;
            vStage1Min = 2.6f; vStage1Span = (cat == 2) ? 36.0f : 19.0f;
            vStage2Min = 1.9f; vStage2Span = (cat == 2) ? 8.5f : 4.8f;
            vAsymBias = 0.15f; vStage2Scale = 0.80f;
            toneMidHz = 680.0;
        }

        // Special AER AC Pre 1 / AC Pre 2 parametric EQ configuration
        if (m == 69 || m == 70) {
            inputHpf.setHighPass(sampleRate, 40.0, 0.707);
            const double minHz = (m == 69) ? 90.0 : 680.0;
            const double maxHz = (m == 69) ? 1600.0 : 11000.0;
            const double eqFreq = minHz * std::pow(maxHz / minHz, std::clamp(static_cast<double>(p.bass) / 10.0, 0.0, 1.0));
            const double eqQ    = 0.6 + (std::clamp(static_cast<double>(p.mid) / 10.0, 0.0, 1.0)) * 5.0;
            const double eqGain = (static_cast<double>(p.treble) - 5.0) * 2.4;
            const double toneDb = (static_cast<double>(p.gain) - 5.0) * 1.8;
            midFilter.setPeaking(sampleRate, eqFreq, eqQ, eqGain);
            trebleFilter.setHighShelf(sampleRate, 3400.0, toneDb, 0.8);
            return;
        }

        // Authentic Ampeg SVT 5-way Midrange selector (220 / 450 / 800 / 1.6k / 3k Hz)
        if (m == 64) {
            static constexpr double kSvtMidHz[5] = { 220.0, 450.0, 800.0, 1600.0, 3000.0 };
            toneMidHz = kSvtMidHz[std::clamp(p.switchVal, 0, 4)];
        }

        // Authentic Bright switch (Dark Twin, Dark Vibra, Foxy 30N, J-120 CL, BogSV CL, Bass Pre)
        const bool hasBrightSwitch = (m == 3 || m == 5 || m == 10 || m == 12 || m == 17 || m == 68);
        const double brightDb = (hasBrightSwitch && p.switchVal >= 1) ? 4.2 : 0.0;

        // Authentic Tone Cut (VOX Foxy 15TB/30N/30TB & Dr. Z Z38 CL/OD — higher value cuts highs!)
        const bool isToneCutAmp = (m == 9 || m == 10 || m == 11 || m == 23 || m == 24);
        // Authentic Bad-KT OD Edge control
        const double edgeDb = (m == 29) ? ((static_cast<double>(p.extra) - 5.0) * 1.6) : 0.0;

        // Subtle deterministic per-model nuance so adjacent variations (e.g. LD 1 / LD 2 / LD 3) have authentic tonal shifts
        const double variantShift = static_cast<double>((m % 3) - 1) * 45.0;
        inputHpf.setHighPass(sampleRate, inHpfHz, 0.707);
        preVoicingFilter.setPeaking(sampleRate, preMidHz + variantShift, 0.85, preMidDb + (m == 11 && p.switchVal >= 1 ? 2.5 : 0.0));
        interStageHpf.setHighPass(sampleRate * 4.0, inHpfHz * 1.18, 0.707);
        powerLpf.setLowPass(sampleRate, pwrLpfHz, 0.707);

        const double bassDb = (static_cast<double>(p.bass) - 5.0) * 2.2;
        const double midDb = toneMidOffsetDb + (static_cast<double>(p.mid) - 5.0) * 2.2;
        const double trebleDb = (static_cast<double>(p.treble) - 5.0) * 2.2 + brightDb + edgeDb;
        const double presDb = isToneCutAmp
            ? -(static_cast<double>(std::clamp(p.presence, 0.0f, 10.0f)) * 1.35)
            : (static_cast<double>(p.presence) - 5.0) * 1.8;

        bassFilter.setLowShelf(sampleRate, toneBassHz, bassDb, 0.85);
        midFilter.setPeaking(sampleRate, toneMidHz + variantShift * 0.5, 0.85, midDb);
        trebleFilter.setHighShelf(sampleRate, toneTrebHz, trebleDb, 0.85);
        presenceFilter.setHighShelf(sampleRate, isToneCutAmp ? 3400.0 : presHz, presDb, 0.9);
    }

    double sampleRate = 44100.0;
    int lastModel = -1;
    int lastSwitch = -1;
    float lastBass = -1.0f, lastMid = -1.0f, lastTreble = -1.0f, lastPresence = -1.0f, lastExtra = -1.0f, lastGain = -1.0f;

    float vStage1Min = 2.5f, vStage1Span = 18.0f;
    float vStage2Min = 1.8f, vStage2Span = 4.5f;
    float vAsymBias = 0.14f, vStage2Scale = 0.80f;

    juce::dsp::Oversampling<float> oversampling {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true
    };
    BiquadFilter inputHpf;
    BiquadFilter preVoicingFilter;
    BiquadFilter interStageHpf;
    BiquadFilter bassFilter;
    BiquadFilter midFilter;
    BiquadFilter trebleFilter;
    BiquadFilter presenceFilter;
    BiquadFilter powerLpf;
    NamModelRunner namRunner;
};

} // namespace pedalboard
