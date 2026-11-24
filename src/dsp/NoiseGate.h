#pragma once

#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 NR Module (2 Models from Daftar_Effect_GP-200.xlsx):
// - Model 0: Gate 1 (ISP Decimator fast high-hysteresis adaptive gate — Threshold)
// - Model 1: Gate 2 (Flexible studio noise gate — Threshold, Attack, Release)
class NoiseGate {
public:
    struct Params {
        bool bypass = false;
        int modelIndex = 0; // 0 = Gate 1, 1 = Gate 2
        float thresholdDb = -52.0f;
        float attackMs = 3.0f;
        float releaseMs = 120.0f;
        float depthDb = -60.0f;
    };

    void prepare(const juce::dsp::ProcessSpec& spec) noexcept {
        sampleRate = std::max(8000.0, spec.sampleRate);
        reset();
    }

    void reset() noexcept {
        envLevel = 0.0f;
        gateGain = 1.0f;
        gateOpen = false;
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

        const bool isDecimator = (p.modelIndex <= 0);
        const float hystDb = isDecimator ? 4.5f : 2.5f;
        const float openThresh = juce::Decibels::decibelsToGain(p.thresholdDb);
        const float closeThresh = juce::Decibels::decibelsToGain(p.thresholdDb - hystDb);
        const float floorGain = juce::Decibels::decibelsToGain(isDecimator ? -78.0f : std::min(p.depthDb, -66.0f));

        const float atkSec = isDecimator ? 0.0006f : std::clamp(p.attackMs * 0.001f, 0.0005f, 0.080f);
        const float relSec = isDecimator ? 0.075f  : std::clamp(p.releaseMs * 0.001f, 0.010f, 0.800f);

        const float envAttackCoeff = std::exp(-1.0f / static_cast<float>((isDecimator ? 0.0007 : 0.0020) * sampleRate));
        const float envReleaseCoeff = std::exp(-1.0f / static_cast<float>((isDecimator ? 0.020 : 0.035) * sampleRate));
        const float gainAttackCoeff = std::exp(-1.0f / static_cast<float>(atkSec * static_cast<float>(sampleRate)));
        const float gainReleaseCoeff = std::exp(-1.0f / static_cast<float>(relSec * static_cast<float>(sampleRate)));

        float* ch0 = buffer.getWritePointer(0);
        float* ch1 = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i) {
            float peak = std::abs(ch0[i]);
            if (ch1 != nullptr) {
                peak = std::max(peak, std::abs(ch1[i]));
            }

            const float ec = (peak > envLevel) ? envAttackCoeff : envReleaseCoeff;
            envLevel = ec * envLevel + (1.0f - ec) * peak;

            if (!gateOpen && envLevel >= openThresh) {
                gateOpen = true;
            } else if (gateOpen && envLevel <= closeThresh) {
                gateOpen = false;
            }

            const float targetGain = gateOpen ? 1.0f : floorGain;
            const float gc = (targetGain > gateGain) ? gainAttackCoeff : gainReleaseCoeff;
            gateGain = gc * gateGain + (1.0f - gc) * targetGain;

            ch0[i] *= gateGain;
            if (ch1 != nullptr) {
                ch1[i] *= gateGain;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    float envLevel = 0.0f;
    float gateGain = 1.0f;
    bool gateOpen = false;
};

} // namespace pedalboard
