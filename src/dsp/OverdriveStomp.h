#pragma once

#include "BiquadFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 DST Module (All 33 Models from Daftar_Effect_GP-200.xlsx rows 26..58)
// Supports authentic per-model controls (1-knob Boost, 2-knob Fuzz Face, 3-knob OD/Rat,
// HP/LP Force OD, Timmy 3-mode, Precision Drive 6-step Attack + Gate, Marshall 3-band EQ,
// Shredmaster Contour, Flex OD 3-mode + Blend, Darkglass B7K 4-band EQ + Attack, EP Boost +3dB/Bright).
class OverdriveStomp {
public:
    struct Params {
        bool bypass = false;
        int modelIndex = 0; // 0..32 (kDstModels)
        float drive = 4.5f; // 0..10 (legacy)
        float tone = 5.5f;  // 0..10 (legacy)
        float level = 5.5f; // 0..10 (legacy)

        bool useSlotParams = false;
        std::array<float, 12> slotParams {};
    };

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = std::max(8000.0, spec.sampleRate);
        oversampling.reset();
        oversampling.initProcessing(static_cast<size_t>(spec.maximumBlockSize));
        lastModel = -1;
        reset();
    }

    void reset() noexcept {
        oversampling.reset();
        preHpf.reset();
        midHump.reset();
        postTone.reset();
        postScoop.reset();
        eqBass.reset();
        eqMid.reset();
        eqHiMid.reset();
        eqTreble.reset();
        gateEnv = 0.0f;
        gateGain = 1.0f;
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

        const int m = std::clamp(p.modelIndex, 0, 32);

        // Resolve authentic per-model parameters into normalized DSP controls
        float driveVal = p.drive;
        float toneVal  = p.tone;
        float levelVal = p.level;
        float bassVal  = 5.0f;
        float midVal   = 5.0f;
        float hiMidVal = 5.0f;
        float trebVal  = 5.0f;
        float extraA   = 5.0f; // Fat / Voice / Contour
        float extraB   = 5.0f; // Air
        float blend    = -1.0f; // >= 0 overrides model default clean blend (0..1)
        int modeChoice = 0;
        bool toggleA   = false; // Low Cut / +3dB
        bool toggleB   = false; // Bright
        float gateKnob = 0.0f;  // Precise OD built-in gate
        float boostDbDirect = -1.0f; // Micro Boost / 14 Boost / EP Boost direct dB

        if (p.useSlotParams) {
            const auto& sp = p.slotParams;
            switch (m) {
                case 2: case 15: case 16: case 17: // Gain/Fuzz, Volume
                    driveVal = sp[0]; levelVal = sp[1]; toneVal = 5.5f;
                    break;
                case 6: // Scream OD: Gain, Tone, Volume, Fat, Air
                    driveVal = sp[0]; toneVal = sp[1]; levelVal = sp[2]; extraA = sp[3]; extraB = sp[4];
                    break;
                case 8: // Force OD: Gain, Tone, Volume, Mode (HP/LP)
                    driveVal = sp[0]; toneVal = sp[1]; levelVal = sp[2]; modeChoice = static_cast<int>(std::lround(sp[3]));
                    break;
                case 10: case 20: // OD JCM800, Chief: Gain, Vol, Bass, Mid, Treble
                    driveVal = sp[0]; levelVal = sp[1]; bassVal = sp[2]; midVal = sp[3]; trebVal = sp[4]; toneVal = trebVal;
                    break;
                case 11: // TaiChi OD: Gain, Tone, Volume, Voice
                    driveVal = sp[0]; toneVal = sp[1]; levelVal = sp[2]; extraA = sp[3];
                    break;
                case 12: // Timmy OD: Gain, Volume, Bass, Treble, Mode (I/II/III)
                    driveVal = sp[0]; levelVal = sp[1]; bassVal = sp[2]; trebVal = sp[3]; modeChoice = static_cast<int>(std::lround(sp[4]));
                    toneVal = trebVal;
                    break;
                case 13: // Precise OD: Gain, Tone, Volume, Gate, Attack (1-6)
                    driveVal = sp[0]; toneVal = sp[1]; levelVal = sp[2]; gateKnob = sp[3]; modeChoice = static_cast<int>(std::lround(sp[4]));
                    break;
                case 19: // Darktale (ProCo Rat): Gain, Filter (reverse!), Volume
                    driveVal = sp[0]; toneVal = 10.0f - std::clamp(sp[1], 0.0f, 10.0f); levelVal = sp[2];
                    break;
                case 21: // Master Dist (Shredmaster): Gain, Volume, Bass, Contour, Treble
                    driveVal = sp[0]; levelVal = sp[1]; bassVal = sp[2]; extraA = sp[3]; trebVal = sp[4]; toneVal = trebVal;
                    break;
                case 23: // Flex OD: Gain, Tone, Volume, Blend, Mode (Normal/Scoop/Edge)
                    driveVal = sp[0]; toneVal = sp[1]; levelVal = sp[2];
                    blend = 1.0f - std::clamp(sp[3] * 0.01f, 0.0f, 1.0f); // Blend=100% -> full wet
                    modeChoice = static_cast<int>(std::lround(sp[4]));
                    break;
                case 24: // Bass OD: Gain, Blend, Volume, Bass, Treble
                    driveVal = sp[0]; blend = 1.0f - std::clamp(sp[1] * 0.01f, 0.0f, 1.0f);
                    levelVal = sp[2]; bassVal = sp[3]; trebVal = sp[4]; toneVal = trebVal;
                    break;
                case 25: // Black Bass (Darkglass B7K): Gain, Blend, Volume, Low, Lo-Mid, Hi-Mid, Treble, Attack
                    driveVal = sp[0]; blend = 1.0f - std::clamp(sp[1] * 0.01f, 0.0f, 1.0f);
                    levelVal = sp[2]; bassVal = sp[3]; midVal = sp[4]; hiMidVal = sp[5]; trebVal = sp[6];
                    modeChoice = static_cast<int>(std::lround(sp[7]));
                    toneVal = trebVal;
                    break;
                case 26: case 30: // Micro Boost, 14 Boost: Gain (0..20 dB)
                    boostDbDirect = sp[0]; driveVal = sp[0] * 0.35f; levelVal = 6.5f;
                    break;
                case 27: case 28: case 29: // AC Boost, B-Boost, P-Boost: Gain, Volume, Bass, Treble
                    driveVal = sp[0]; levelVal = sp[1]; bassVal = sp[2]; trebVal = sp[3]; toneVal = trebVal;
                    break;
                case 31: // FAT BB: Bass, Treble, Volume, Low Cut
                    driveVal = 3.0f; bassVal = sp[0]; trebVal = sp[1]; levelVal = sp[2]; toggleA = sp[3] > 0.5f;
                    toneVal = trebVal;
                    break;
                case 32: // Boost (EP Booster): Gain (0..20 dB), +3dB, Bright
                    boostDbDirect = sp[0] + (sp[1] > 0.5f ? 3.0f : 0.0f);
                    driveVal = sp[0] * 0.3f; levelVal = 6.5f; toggleA = sp[1] > 0.5f; toggleB = sp[2] > 0.5f;
                    break;
                default: // Standard 3-knob: Gain/Sustain, Tone, Volume
                    driveVal = sp[0]; toneVal = sp[1]; levelVal = sp[2];
                    break;
            }
        }

        updateVoicing(m, toneVal, bassVal, midVal, hiMidVal, trebVal, extraA, extraB, modeChoice, toggleA, toggleB);

        // Optional built-in gate on Precise OD (model 13)
        if (m == 13 && gateKnob > 0.2f) {
            const float thresh = juce::Decibels::decibelsToGain(-78.0f + (gateKnob / 10.0f) * 52.0f);
            const float atkC = std::exp(-1.0f / static_cast<float>(0.0008 * sampleRate));
            const float relC = std::exp(-1.0f / static_cast<float>(0.045 * sampleRate));
            for (int i = 0; i < numSamples; ++i) {
                float pk = std::abs(buffer.getReadPointer(0)[i]);
                if (numChannels > 1) pk = std::max(pk, std::abs(buffer.getReadPointer(1)[i]));
                gateEnv = ((pk > gateEnv) ? atkC : relC) * gateEnv + (1.0f - ((pk > gateEnv) ? atkC : relC)) * pk;
                const float tgt = (gateEnv >= thresh) ? 1.0f : 0.002f;
                gateGain = 0.96f * gateGain + 0.04f * tgt;
                for (int c = 0; c < numChannels; ++c) buffer.getWritePointer(c)[i] *= gateGain;
            }
        }

        // Pre-shaping
        for (int ch = 0; ch < numChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                float s = preHpf.processSample(ch, data[i]);
                data[i] = midHump.processSample(ch, s);
            }
        }

        // 4x oversampled nonlinear clipping stage
        juce::dsp::AudioBlock<float> block(buffer);
        auto subBlock = block.getSubsetChannelBlock(0, static_cast<size_t>(numChannels));
        auto upBlock = oversampling.processSamplesUp(subBlock);

        const float dNorm = std::clamp(driveVal, 0.0f, 10.0f) / 10.0f;
        float preGain = 1.2f + std::pow(dNorm, 1.48f) * gainMax;
        if (boostDbDirect >= 0.0f) {
            preGain = juce::Decibels::decibelsToGain(std::clamp(boostDbDirect, 0.0f, 24.0f)) * 0.85f;
        }
        const float effBlend = (blend >= 0.0f) ? blend : cleanBlend;
        const float dcOffset = std::tanh(asymBias);
        const float norm = 1.0f / std::max(0.35f, std::tanh(1.55f));

        const size_t upLen = upBlock.getNumSamples();
        for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch) {
            float* up = upBlock.getChannelPointer(ch);
            for (size_t i = 0; i < upLen; ++i) {
                const float x = up[i];
                const float driven = x * preGain;
                float shaped = 0.0f;

                if (boostDbDirect >= 0.0f) {
                    shaped = std::tanh(driven * 0.65f) * 1.18f;
                } else if (clipMode == 1) {
                    const float hard = std::clamp(driven + asymBias, -0.88f, 0.96f);
                    shaped = std::tanh(hard * 1.65f) * 0.90f;
                } else if (clipMode == 2) {
                    const float s1 = std::tanh(driven * 0.55f + asymBias) - dcOffset;
                    shaped = std::tanh(s1 * 3.4f - asymBias * 0.5f) * 0.88f;
                } else if (clipMode == 3 || effBlend > 0.01f) {
                    const float wet = (std::tanh(driven + asymBias) - dcOffset) * norm;
                    shaped = x * effBlend + wet * (1.0f - effBlend * 0.55f);
                } else {
                    shaped = (std::tanh(driven + asymBias) - dcOffset) * norm;
                }
                up[i] = shaped * 0.85f;
            }
        }

        oversampling.processSamplesDown(subBlock);

        const float outGain = (boostDbDirect >= 0.0f)
            ? 1.0f
            : juce::Decibels::decibelsToGain(-18.0f + (std::clamp(levelVal, 0.0f, 10.0f) / 10.0f) * 24.0f + levelOffsetDb);

        for (int ch = 0; ch < numChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                float s = postTone.processSample(ch, data[i]);
                s = postScoop.processSample(ch, s);
                if (useMultiBandEq) {
                    s = eqBass.processSample(ch, s);
                    s = eqMid.processSample(ch, s);
                    s = eqHiMid.processSample(ch, s);
                    s = eqTreble.processSample(ch, s);
                }
                data[i] = s * outGain;
            }
        }
    }

private:
    void updateVoicing(int m, float toneVal, float bassVal, float midVal, float hiMidVal, float trebVal,
                       float extraA, float extraB, int modeChoice, bool toggleA, bool toggleB) noexcept {
        lastModel = m;

        double hpfHz = 180.0;
        double midHz = 720.0;
        double midDb = 3.2;
        double midQ  = 0.85;
        double scoopHz = 900.0;
        double scoopDb = 0.0;
        double lpfMin = 1500.0;
        double lpfSpan = 5200.0;

        clipMode = 0;
        gainMax = 19.0f;
        asymBias = 0.14f;
        cleanBlend = 0.0f;
        levelOffsetDb = 0.0f;
        useMultiBandEq = false;

        if (m == 0 || m == 1 || m == 6) { // Green OD / OD 9 / Scream OD
            hpfHz = (m == 6) ? (220.0 - static_cast<double>(extraA) * 14.0) : 230.0;
            midHz = 720.0; midDb = (m == 1) ? 4.4 : 3.8;
            gainMax = (m == 6) ? 24.0f : 18.0f;
            if (m == 6) { scoopHz = 4500.0; scoopDb = (static_cast<double>(extraB) - 5.0) * 1.4; }
        } else if (m == 3) { // Penesas OD (Klon)
            hpfHz = 125.0; midHz = 1050.0; midDb = 2.8;
            clipMode = 3; cleanBlend = 0.42f; gainMax = 15.0f; asymBias = 0.08f; lpfMin = 2000.0; lpfSpan = 6500.0;
        } else if (m == 2 || m == 4 || m == 5 || m == 7 || m == 9 || m == 11 || m == 12) {
            hpfHz = (m == 12) ? 90.0 : 145.0;
            midHz = (m == 11) ? (550.0 + static_cast<double>(extraA) * 110.0) : 640.0;
            midDb = (m == 12) ? 0.5 : ((m == 11) ? 3.2 : 1.8);
            gainMax = (m == 4 || m == 11) ? 21.0f : 16.0f;
            asymBias = (m == 12) ? (modeChoice == 0 ? 0.04f : (modeChoice == 1 ? 0.18f : 0.28f)) : 0.19f;
            if (m == 12) {
                useMultiBandEq = true;
                eqBass.setLowShelf(sampleRate, 130.0, (static_cast<double>(bassVal) - 5.0) * 1.8, 0.8);
                eqMid.setPeaking(sampleRate, 700.0, 0.9, 0.0);
                eqHiMid.setPeaking(sampleRate, 1800.0, 0.9, 0.0);
                eqTreble.setHighShelf(sampleRate, 3200.0, (static_cast<double>(trebVal) - 5.0) * 1.8, 0.8);
            }
        } else if (m == 8 || m == 10 || m == 13) { // Force OCD / OD JCM800 / Precise OD
            const double preciseHpf[6] = { 75.0, 110.0, 150.0, 195.0, 240.0, 290.0 };
            hpfHz = (m == 13) ? preciseHpf[std::clamp(modeChoice, 0, 5)] : 140.0;
            midHz = (m == 8 && modeChoice == 0) ? 1150.0 : ((m == 10) ? 880.0 : 1350.0);
            midDb = (m == 8) ? (modeChoice == 0 ? 4.2 : 1.0) : 3.6;
            clipMode = (m == 8) ? 1 : 0;
            gainMax = (m == 10) ? 30.0f : 24.0f;
            if (m == 10) {
                useMultiBandEq = true;
                eqBass.setLowShelf(sampleRate, 110.0, (static_cast<double>(bassVal) - 5.0) * 2.0, 0.8);
                eqMid.setPeaking(sampleRate, 750.0, 0.9, (static_cast<double>(midVal) - 5.0) * 2.0);
                eqHiMid.setPeaking(sampleRate, 1800.0, 0.9, 0.0);
                eqTreble.setHighShelf(sampleRate, 3200.0, (static_cast<double>(trebVal) - 5.0) * 2.0, 0.8);
            }
        } else if (m >= 14 && m <= 17) { // Fuzzes
            hpfHz = (m == 15) ? 65.0 : 95.0;
            midHz = 620.0; midDb = (m == 14) ? -3.5 : 1.5;
            scoopHz = 850.0; scoopDb = (m == 14) ? -5.8 : 0.0;
            clipMode = (m == 17) ? 1 : 2;
            gainMax = (m == 14) ? 46.0f : 38.0f;
            asymBias = (m == 15) ? 0.28f : 0.16f;
            lpfMin = 1200.0; lpfSpan = 4400.0;
        } else if (m >= 18 && m <= 22) { // Distortions (SM Dist, Rat, Chief, Shredmaster, La Charger)
            hpfHz = (m == 19) ? 135.0 : 155.0;
            midHz = (m == 19) ? 1150.0 : 780.0;
            midDb = (m == 19) ? 4.2 : 2.2;
            scoopHz = (m == 21) ? (450.0 + static_cast<double>(extraA) * 140.0) : 700.0;
            scoopDb = (m == 21) ? -5.5 : ((m == 18) ? -3.2 : 0.0);
            clipMode = 1;
            gainMax = (m == 21 || m == 22) ? 44.0f : 34.0f;
            asymBias = 0.20f;
            if (m == 20 || m == 21) {
                useMultiBandEq = true;
                eqBass.setLowShelf(sampleRate, 110.0, (static_cast<double>(bassVal) - 5.0) * 2.0, 0.8);
                eqMid.setPeaking(sampleRate, 700.0, 0.9, (m == 20) ? (static_cast<double>(midVal) - 5.0) * 2.0 : 0.0);
                eqHiMid.setPeaking(sampleRate, 1800.0, 0.9, 0.0);
                eqTreble.setHighShelf(sampleRate, 3200.0, (static_cast<double>(trebVal) - 5.0) * 2.0, 0.8);
            }
        } else if (m >= 23 && m <= 25) { // Flex OD, Bass OD, Black Bass B7K
            hpfHz = 40.0; midHz = 950.0; midDb = (m == 25) ? 3.5 : 1.5;
            clipMode = 3; cleanBlend = (m == 25) ? 0.38f : 0.45f; gainMax = (m == 25) ? 32.0f : 22.0f;
            if (m == 23) {
                scoopHz = (modeChoice == 1) ? 750.0 : 2800.0;
                scoopDb = (modeChoice == 1) ? -5.0 : ((modeChoice == 2) ? 4.2 : 0.0);
            } else if (m == 24 || m == 25) {
                useMultiBandEq = true;
                const double atkDb = (m == 25) ? (modeChoice == 0 ? -3.8 : (modeChoice == 2 ? 4.5 : 0.0)) : 0.0;
                eqBass.setLowShelf(sampleRate, 95.0, (static_cast<double>(bassVal) - 5.0) * 2.0, 0.8);
                eqMid.setPeaking(sampleRate, 450.0, 0.9, (m == 25) ? (static_cast<double>(midVal) - 5.0) * 2.0 : 0.0);
                eqHiMid.setPeaking(sampleRate, 1600.0, 0.9, (m == 25) ? (static_cast<double>(hiMidVal) - 5.0) * 2.0 : 0.0);
                eqTreble.setHighShelf(sampleRate, 3600.0, (static_cast<double>(trebVal) - 5.0) * 2.0 + atkDb, 0.8);
            }
        } else { // 26..32 Boosts
            hpfHz = (m == 30) ? 180.0 : ((m == 31 && toggleA) ? 150.0 : 65.0);
            midHz = (m == 30) ? 1850.0 : 650.0;
            midDb = (m == 30) ? 5.5 : ((m == 32) ? 2.0 : 1.2);
            scoopHz = 3400.0;
            scoopDb = (m == 32 && toggleB) ? 3.8 : 0.0;
            clipMode = 0; gainMax = 8.5f; asymBias = 0.05f; levelOffsetDb = 3.5f;
            lpfMin = 4200.0; lpfSpan = 8500.0;
            if (m == 27 || m == 28 || m == 29 || m == 31) {
                useMultiBandEq = true;
                eqBass.setLowShelf(sampleRate, 120.0, (static_cast<double>(bassVal) - 5.0) * 2.2, 0.8);
                eqMid.setPeaking(sampleRate, 700.0, 0.9, 0.0);
                eqHiMid.setPeaking(sampleRate, 1800.0, 0.9, 0.0);
                eqTreble.setHighShelf(sampleRate, 3200.0, (static_cast<double>(trebVal) - 5.0) * 2.2, 0.8);
            }
        }

        const double tNorm = std::clamp(static_cast<double>(toneVal), 0.0, 10.0) / 10.0;
        const double lpfHz = lpfMin + tNorm * lpfSpan;

        preHpf.setHighPass(sampleRate, hpfHz, 0.707);
        midHump.setPeaking(sampleRate, midHz, midQ, midDb);
        postTone.setLowPass(sampleRate, lpfHz, 0.72);
        postScoop.setPeaking(sampleRate, scoopHz, 0.85, scoopDb);
    }

    double sampleRate = 44100.0;
    int lastModel = -1;
    int clipMode = 0;
    float gainMax = 19.0f;
    float asymBias = 0.14f;
    float cleanBlend = 0.0f;
    float levelOffsetDb = 0.0f;
    bool useMultiBandEq = false;
    float gateEnv = 0.0f;
    float gateGain = 1.0f;

    juce::dsp::Oversampling<float> oversampling {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true
    };
    BiquadFilter preHpf;
    BiquadFilter midHump;
    BiquadFilter postTone;
    BiquadFilter postScoop;
    BiquadFilter eqBass, eqMid, eqHiMid, eqTreble;
};

} // namespace pedalboard
