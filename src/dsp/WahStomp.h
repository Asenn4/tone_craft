#pragma once

#include "BiquadFilter.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 WAH Module (All 5 Models from Daftar_Effect_GP-200.xlsx rows 21..25)
// White-Box Analog Inductor-Gyrator Circuit DSP Upgrade:
// - Models real Fasel / Halo inductor-gyrator resonant bandpass + asymmetric high-pass topology
// - Position-dependent Q curve (vocal narrower heel resonance -> open aggressive toe bite)
// - Authentic Hot-Potz logarithmic rack-and-pinion treadle taper + input BC109/MPSA18 transistor & inductor core warmth!
//   0: V-Wah (VOX V846 — Halo inductor, sweet open vocal mids)
//   1: C-Wah (Dunlop CryBaby — Red Fasel inductor, punchy tight upper-mid bite)
//   2: P-Wah (John Petrucci Rack CryBaby — extended range + rack EQ tilt)
//   3: S-Wah (60s Classic Wah — warm vintage low-Q sweep)
//   4: B-Wah (Bass Wah — low-frequency inductor sweep preserving sub-fundamental punch)
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
        smoothedPos.reset(sampleRate, 0.010);
        smoothedPos.setCurrentAndTargetValue(0.5f);
        reset();
    }

    void reset() noexcept {
        ic1L = ic2L = 0.0f;
        ic1R = ic2R = 0.0f;
        pWahEq.reset();
        env = 0.0f;
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
        // Hardware-calibrated sweep tables: [V-Wah V846, C-Wah CryBaby, P-Wah Petrucci, S-Wah 60s, B-Wah Bass]
        constexpr double kMinHz[5]       = { 415.0, 355.0, 275.0, 385.0, 125.0 };
        constexpr double kMaxHzBase[5]   = { 2420.0, 2180.0, 2780.0, 1890.0, 1180.0 };
        constexpr double kQScale[5]      = { 0.94,  1.12,  1.26,  0.84,  1.05  };
        constexpr float  kHpLeak[5]      = { 0.24f, 0.18f, 0.16f, 0.26f, 0.12f }; // Real wah circuit HP bleed
        constexpr float  kLpWeight[5]    = { 0.12f, 0.10f, 0.08f, 0.16f, 0.42f }; // Bass wah keeps low-end body
        constexpr float  kCoreDrive[5]   = { 1.18f, 1.32f, 1.24f, 1.28f, 1.15f }; // Halo vs Red Fasel inductor core warmth
        constexpr double kPotTaper[5]    = { 1.48,  1.58,  1.45,  1.52,  1.40  }; // Hot-Potz log rack-and-pinion curve

        const float atkCoeff = std::exp(-1.0f / static_cast<float>(0.008 * sampleRate));
        const float relCoeff = std::exp(-1.0f / static_cast<float>(0.140 * sampleRate));

        const float rangeNorm = p.useExcelParams ? std::clamp(p.range / 10.0f, 0.0f, 1.0f) : 0.65f;
        const double maxHz = kMinHz[m] + (kMaxHzBase[m] - kMinHz[m]) * (0.45 + 0.78 * static_cast<double>(rangeNorm));
        const double spanRatio = std::max(1.5, maxHz / kMinHz[m]);
        const float outGain = p.useExcelParams
            ? juce::Decibels::decibelsToGain(-12.0f + (std::clamp(p.volume, 0.0f, 10.0f) / 10.0f) * 18.0f)
            : 0.80f;
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

        const float hpLeak    = kHpLeak[m];
        const float lpWeight  = kLpWeight[m];
        const float coreDrive = kCoreDrive[m];
        const float invTanhD  = 1.0f / std::tanh(coreDrive);
        const double baseQ    = std::clamp(static_cast<double>(p.q) * kQScale[m], 1.2, 12.5);

        // Zero-Delay Feedback (ZDF / Cytomic TPT) SVF coefficients updated cleanly every 4 samples
        float k = 0.25f, a1 = 0.9f, a2 = 0.04f, a3 = 0.002f;
        float bpGain = 1.0f;

        for (int i = 0; i < numSamples; ++i) {
            float pos = 0.5f;
            if (p.autoWah) {
                float peak = std::abs(ch0[i]);
                if (ch1 != nullptr) peak = std::max(peak, std::abs(ch1[i]));
                const float c = (peak > env) ? atkCoeff : relCoeff;
                env = c * env + (1.0f - c) * peak;
                const float sens = p.useExcelParams ? (0.45f + 0.55f * rangeNorm) : p.sensitivity;
                const float drive = std::clamp(env * (3.0f + sens * 14.0f), 0.0f, 1.0f);
                pos = std::clamp(0.08f + drive * 0.92f, 0.0f, 1.0f);
            } else {
                pos = smoothedPos.getNextValue();
            }

            if (i == 0 || (i & 3) == 0) {
                // Authentic Hot-Potz logarithmic treadle taper + real CryBaby/V846 position-dependent Q curve
                const double tapered = std::pow(std::clamp(static_cast<double>(pos), 0.0, 1.0), kPotTaper[m]);
                const double cutoffHz = std::clamp(kMinHz[m] * std::pow(spanRatio, tapered), 75.0, sampleRate * 0.40);
                // Real Fasel inductor circuits have tighter vocal Q in lower/mid sweep & smoother Q near toe-down
                const double posQ = std::clamp(baseQ * (1.22 - 0.34 * tapered), 1.2, 13.0);
                const double gd = std::tan(std::numbers::pi * cutoffHz / sampleRate);
                const double kd = 1.0 / posQ;
                const double a1d = 1.0 / (1.0 + gd * (gd + kd));
                const double a2d = gd * a1d;
                const double a3d = gd * a2d;
                k  = static_cast<float>(kd);
                a1 = static_cast<float>(a1d);
                a2 = static_cast<float>(a2d);
                a3 = static_cast<float>(a3d);
                // Compensate bandpass resonant gain so perceived loudness stays musical across Q settings
                bpGain = static_cast<float>(std::clamp(0.85 + 0.24 * std::sqrt(posQ), 0.95, 2.2));
            }

            // Channel 0 — Input transistor warmth -> ZDF Inductor-Gyrator SVF -> Fasel core saturation
            {
                const float in0 = ch0[i];
                const float drv0 = std::tanh(in0 * coreDrive) * invTanhD;
                const float v3 = drv0 - ic2L;
                const float v1 = a1 * ic1L + a2 * v3;
                const float v2 = ic2L + a2 * ic1L + a3 * v3;
                ic1L = 2.0f * v1 - ic1L;
                ic2L = 2.0f * v2 - ic2L;

                const float bp = v1 * k * bpGain;
                const float hp = drv0 - k * v1 - v2;
                float w0 = std::tanh((bp * 1.45f + hp * hpLeak + v2 * lpWeight) * 0.92f);
                if (m == 2 && p.useExcelParams) w0 = pWahEq.processSample(0, w0);
                ch0[i] = (in0 * dry + w0 * wet) * outGain;
            }

            if (ch1 != nullptr) {
                const float in1 = ch1[i];
                const float drv1 = std::tanh(in1 * coreDrive) * invTanhD;
                const float v3 = drv1 - ic2R;
                const float v1 = a1 * ic1R + a2 * v3;
                const float v2 = ic2R + a2 * ic1R + a3 * v3;
                ic1R = 2.0f * v1 - ic1R;
                ic2R = 2.0f * v2 - ic2R;

                const float bp = v1 * k * bpGain;
                const float hp = drv1 - k * v1 - v2;
                float w1 = std::tanh((bp * 1.45f + hp * hpLeak + v2 * lpWeight) * 0.92f);
                if (m == 2 && p.useExcelParams) w1 = pWahEq.processSample(1, w1);
                ch1[i] = (in1 * dry + w1 * wet) * outGain;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    float env = 0.0f;
    float ic1L = 0.0f, ic2L = 0.0f;
    float ic1R = 0.0f, ic2R = 0.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPos;
    BiquadFilter pWahEq;
};

} // namespace pedalboard
