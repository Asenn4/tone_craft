#pragma once

#include "BiquadFilter.h"
#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 WAH Module (All 5 Models from Daftar_Effect_GP-200.xlsx rows 21..25):
// - 0: V-Wah (VOX V846): Range, Q, Volume
// - 1: C-Wah (Dunlop CryBaby): Range, Q, Volume
// - 2: P-Wah (John Petrucci Rack CryBaby): Range, Q, Volume, EQ
// - 3: S-Wah (60s Classic Wah): Range, Q, Volume
// - 4: B-Wah (Bass Wah): Range, Q, Volume
class WahStomp {
public:
    struct Params {
        bool bypass = false;
        int modelIndex = 1;       // 0..4 (default 1 = C-Wah)
        float pedal = 0.5f;       // 0..1 heel-to-toe position
        float q = 4.2f;           // 1.2..12.0 resonance
        float sensitivity = 0.6f; // 0..1 auto-wah sensitivity (legacy)
        float mix = 1.0f;         // 0..1 dry/wet (legacy)
        bool autoWah = false;

        // Authentic GP-200 Excel controls
        bool useExcelParams = false;
        float range = 6.5f;       // 0..10 sweep frequency span
        float volume = 7.0f;      // 0..10 output volume
        float eq = 5.5f;          // 0..10 P-Wah rack EQ tilt
    };

    void prepare(const juce::dsp::ProcessSpec& spec) noexcept {
        sampleRate = std::max(8000.0, spec.sampleRate);
        smoothedPos.reset(sampleRate, 0.012);
        smoothedPos.setCurrentAndTargetValue(0.5f);
        reset();
    }

    void reset() noexcept {
        wahPeak.reset();
        wahLowShelf.reset();
        pWahEq.reset();
        env = 0.0f;
        sampleCounter = 0;
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

        const int m = std::clamp(p.modelIndex, 0, 4);
        constexpr double kMinHz[5]    = { 420.0, 350.0, 285.0, 380.0, 135.0 };
        constexpr double kSpanMult[5] = { 5.83,  6.28,  9.75,  4.87,  8.74  };
        constexpr double kQScale[5]   = { 1.04,  1.00,  1.22,  0.88,  1.10  };
        constexpr double kPeakDb[5]   = { 11.2,  11.8,  13.0,  10.4,  11.5  };
        constexpr double kShelfDb[5]  = { -4.8,  -5.5,  -4.2,  -5.0,  -1.2  };

        const float atkCoeff = std::exp(-1.0f / static_cast<float>(0.008 * sampleRate));
        const float relCoeff = std::exp(-1.0f / static_cast<float>(0.140 * sampleRate));

        const float rangeNorm = p.useExcelParams ? std::clamp(p.range / 10.0f, 0.0f, 1.0f) : 0.65f;
        const double spanMult = 1.0 + (kSpanMult[m] - 1.0) * (0.45 + 0.80 * static_cast<double>(rangeNorm));
        const float outGain = p.useExcelParams
            ? juce::Decibels::decibelsToGain(-12.0f + (std::clamp(p.volume, 0.0f, 10.0f) / 10.0f) * 18.0f)
            : 0.78f;
        const float wet = p.useExcelParams ? 1.0f : std::clamp(p.mix, 0.0f, 1.0f);
        const float dry = 1.0f - wet;

        if (m == 2 && p.useExcelParams) {
            const double eqDb = (static_cast<double>(std::clamp(p.eq, 0.0f, 10.0f)) - 5.0) * 1.6;
            pWahEq.setPeaking(sampleRate, 920.0, 0.9, eqDb);
        }

        float* ch0 = buffer.getWritePointer(0);
        float* ch1 = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        if (!p.autoWah) {
            smoothedPos.setTargetValue(std::clamp(p.pedal, 0.0f, 1.0f));
        }

        for (int i = 0; i < numSamples; ++i) {
            float pos = 0.5f;
            if (p.autoWah) {
                float peak = std::abs(ch0[i]);
                if (ch1 != nullptr) peak = std::max(peak, std::abs(ch1[i]));
                const float c = (peak > env) ? atkCoeff : relCoeff;
                env = c * env + (1.0f - c) * peak;
                const float sens = p.useExcelParams ? (0.45f + 0.55f * rangeNorm) : p.sensitivity;
                const float drive = std::clamp(env * (3.0f + sens * 14.0f), 0.0f, 1.0f);
                pos = std::clamp(0.10f + drive * 0.90f, 0.0f, 1.0f);
            } else {
                pos = smoothedPos.getNextValue();
            }

            if ((sampleCounter++ & 7) == 0) {
                const double cutoffHz = kMinHz[m] * std::pow(spanMult, static_cast<double>(pos));
                const double qVal = std::clamp(static_cast<double>(p.q) * kQScale[m], 1.2, 12.0);
                wahPeak.setPeaking(sampleRate, cutoffHz, qVal, kPeakDb[m]);
                wahLowShelf.setLowShelf(sampleRate, cutoffHz * 0.55, kShelfDb[m], 0.8);
            }

            const float in0 = ch0[i];
            float w0 = wahLowShelf.processSample(0, wahPeak.processSample(0, in0));
            if (m == 2 && p.useExcelParams) w0 = pWahEq.processSample(0, w0);
            ch0[i] = (in0 * dry + w0 * wet) * outGain;

            if (ch1 != nullptr) {
                const float in1 = ch1[i];
                float w1 = wahLowShelf.processSample(1, wahPeak.processSample(1, in1));
                if (m == 2 && p.useExcelParams) w1 = pWahEq.processSample(1, w1);
                ch1[i] = (in1 * dry + w1 * wet) * outGain;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    float env = 0.0f;
    uint32_t sampleCounter = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPos;
    BiquadFilter wahPeak;
    BiquadFilter wahLowShelf;
    BiquadFilter pWahEq;
};

} // namespace pedalboard
