#pragma once

#include "BiquadFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 RVB Module (All 13 Reverb Models from Daftar_Effect_GP-200.xlsx rows 252..264)
// Supports every authentic per-model control: Pre Delay (0..150 ms), Decay, High Damp,
// Low Damp, Spring Tone, Mod, Shimmer Lo/High End, and genuine Trail bypass decay!
class ReverbUnit {
public:
    struct Params {
        bool bypass = false;
        int modelIndex = 3;    // 0..12
        float size = 0.45f;    // legacy
        float damping = 0.50f; // legacy
        float mix = 0.22f;     // legacy

        bool useSlotParams = false;
        std::array<float, 12> slotParams {};
    };

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = std::max(8000.0, spec.sampleRate);
        reverb.prepare(spec);
        wetBuffer.setSize(2, static_cast<int>(spec.maximumBlockSize), false, true, true);
        lastModel = -1;
        reset();
    }

    void reset() noexcept {
        reverb.reset();
        preHpf.reset();
        postTone.reset();
        preDelayBufL.fill(0.0f);
        preDelayBufR.fill(0.0f);
        preDelayWritePos = 0;
        modPhase = 0.0;
    }

    void process(juce::AudioBuffer<float>& buffer, const Params& p) noexcept {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = std::min(2, buffer.getNumChannels());
        if (numSamples <= 0 || numChannels <= 0 || wetBuffer.getNumSamples() < numSamples) {
            return;
        }

        const int m = std::clamp(p.modelIndex, 0, 12);
        const auto& sp = p.slotParams;

        float mix        = p.mix;
        float preDelayMs = 15.0f;
        float decay      = p.size;
        float lowDamp    = 0.30f;
        float hiDamp     = p.damping;
        float modAmt     = 0.0f;
        float toneVal    = 5.8f;
        bool trailOn     = true;

        if (p.useSlotParams) {
            switch (m) {
                case 0: case 1: case 2: // Room, Hall, Church: Mix, Pre Delay, Decay, Trail
                    mix = sp[0] * 0.01f; preDelayMs = sp[1]; decay = sp[2] * 0.01f; trailOn = sp[3] > 0.5f;
                    break;
                case 3: // Plate: Mix, Decay, High Damp, Trail
                    mix = sp[0] * 0.01f; decay = sp[1] * 0.01f; hiDamp = sp[2] * 0.01f; preDelayMs = 8.0f; trailOn = sp[3] > 0.5f;
                    break;
                case 4: // Spring: Mix, Decay, Tone, Trail
                    mix = sp[0] * 0.01f; decay = sp[1] * 0.01f; toneVal = sp[2]; preDelayMs = 4.0f; trailOn = sp[3] > 0.5f;
                    break;
                case 5: case 6: case 7: case 8: // Studio, Club, Concert, Arena: Mix, Pre Delay, Decay, Low Damp, Hi Damp, Mod, Trail
                    mix = sp[0] * 0.01f; preDelayMs = sp[1]; decay = sp[2] * 0.01f;
                    lowDamp = sp[3] * 0.01f; hiDamp = sp[4] * 0.01f; modAmt = sp[5] * 0.01f; trailOn = sp[6] > 0.5f;
                    break;
                case 9: case 10: // N-Star, Deepsea: Mix, Decay, Trail
                    mix = sp[0] * 0.01f; decay = sp[1] * 0.01f; preDelayMs = 20.0f; trailOn = sp[2] > 0.5f;
                    break;
                case 11: // Sweet Space: Mix, Pre Delay, Decay
                    mix = sp[0] * 0.01f; preDelayMs = sp[1]; decay = sp[2] * 0.01f; modAmt = 0.45f; trailOn = true;
                    break;
                case 12: // Shimmer: Lo End, High End, Trail
                    lowDamp = 1.0f - std::clamp(sp[0] * 0.01f, 0.0f, 1.0f);
                    hiDamp  = 1.0f - std::clamp(sp[1] * 0.01f, 0.0f, 1.0f);
                    mix     = 0.36f + std::clamp(sp[1] * 0.0025f, 0.0f, 0.25f);
                    decay   = 0.78f;
                    modAmt  = 0.35f;
                    trailOn = sp[2] > 0.5f;
                    break;
            }
        }

        if (p.bypass && !trailOn) {
            return;
        }
        if (mix <= 0.0001f && !p.bypass) {
            return;
        }

        configureFilters(m, lowDamp, hiDamp, toneVal);

        constexpr float kSizeScale[13] = {
            0.68f, 0.95f, 1.15f, 0.88f, 0.72f, 0.78f, 0.82f, 1.02f, 1.18f, 1.10f, 1.22f, 1.08f, 1.18f
        };
        constexpr float kWidth[13] = {
            0.70f, 0.92f, 1.00f, 0.88f, 0.55f, 0.82f, 0.84f, 0.96f, 1.00f, 1.00f, 1.00f, 0.98f, 1.00f
        };

        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = std::clamp(decay * kSizeScale[m], 0.08f, 0.99f);
        rp.damping    = std::clamp(hiDamp, 0.05f, 0.95f);
        rp.wetLevel   = 1.0f;
        rp.dryLevel   = 0.0f;
        rp.width      = kWidth[m];
        rp.freezeMode = 0.0f;
        reverb.setParameters(rp);

        const int preDelaySamples = std::clamp(static_cast<int>(preDelayMs * 0.001f * static_cast<float>(sampleRate)), 0, kPreDelaySize - 4);
        const float inputSend = p.bypass ? 0.0f : 1.0f;
        const bool isShimmer = (m == 12 || m == 9);
        const float shimmerAmt = (m == 12 && p.useSlotParams) ? (0.25f + std::clamp(sp[1] * 0.0055f, 0.0f, 0.55f)) : 0.25f;

        for (int i = 0; i < numSamples; ++i) {
            const float inL = buffer.getReadPointer(0)[i] * inputSend;
            const float inR = (numChannels > 1 ? buffer.getReadPointer(1)[i] : buffer.getReadPointer(0)[i]) * inputSend;

            preDelayBufL[static_cast<size_t>(preDelayWritePos)] = inL;
            preDelayBufR[static_cast<size_t>(preDelayWritePos)] = inR;
            const int readPos = (preDelayWritePos - preDelaySamples) & (kPreDelaySize - 1);
            preDelayWritePos = (preDelayWritePos + 1) & (kPreDelaySize - 1);

            float sL = preHpf.processSample(0, preDelayBufL[static_cast<size_t>(readPos)]);
            float sR = preHpf.processSample(1, preDelayBufR[static_cast<size_t>(readPos)]);
            if (isShimmer) {
                sL += (std::abs(sL) * 2.0f - 0.35f * std::abs(sL)) * shimmerAmt;
                sR += (std::abs(sR) * 2.0f - 0.35f * std::abs(sR)) * shimmerAmt;
            }
            wetBuffer.getWritePointer(0)[i] = sL;
            wetBuffer.getWritePointer(1)[i] = sR;
        }

        juce::dsp::AudioBlock<float> wetBlock(wetBuffer);
        auto subWet = wetBlock.getSubBlock(0, static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> ctx(subWet);
        reverb.process(ctx);

        const float wet = std::clamp(mix, 0.0f, 1.0f);
        const float dry = p.bypass ? 1.0f : (1.0f - wet * 0.45f);
        const double modInc = 2.0 * std::numbers::pi * 1.15 / sampleRate;

        for (int i = 0; i < numSamples; ++i) {
            float modL = 1.0f, modR = 1.0f;
            if (modAmt > 0.01f) {
                const float ms = static_cast<float>(std::sin(modPhase));
                modPhase += modInc;
                if (modPhase >= 2.0 * std::numbers::pi) modPhase -= 2.0 * std::numbers::pi;
                modL = 1.0f + modAmt * 0.14f * ms;
                modR = 1.0f - modAmt * 0.14f * ms;
            }
            for (int ch = 0; ch < numChannels; ++ch) {
                float* out = buffer.getWritePointer(ch);
                const float w = postTone.processSample(ch, wetBuffer.getReadPointer(ch)[i]) * (ch == 0 ? modL : modR);
                out[i] = out[i] * dry + w * wet;
            }
        }
    }

private:
    void configureFilters(int m, float lowDamp, float hiDamp, float toneVal) noexcept {
        lastModel = m;
        const double hpfHz = 90.0 + std::clamp(static_cast<double>(lowDamp), 0.0, 1.0) * 340.0;
        preHpf.setHighPass(sampleRate, hpfHz, 0.72);

        if (m == 4) { // Spring Reverb: Tone knob controls resonant spring brightness
            const double tHz = 1400.0 + std::clamp(static_cast<double>(toneVal), 0.0, 10.0) * 340.0;
            postTone.setPeaking(sampleRate, tHz, 1.35, 4.5);
        } else if (m == 12 || m == 9) {
            const double shelfDb = (1.0 - std::clamp(static_cast<double>(hiDamp), 0.0, 1.0)) * 5.5;
            postTone.setHighShelf(sampleRate, 3800.0, shelfDb, 0.8);
        } else {
            const double lpfHz = 9500.0 - std::clamp(static_cast<double>(hiDamp), 0.0, 1.0) * 6800.0;
            postTone.setLowPass(sampleRate, std::clamp(lpfHz, 1800.0, 12000.0), 0.72);
        }
    }

    static constexpr int kPreDelaySize = 16384;

    double sampleRate = 44100.0;
    int lastModel = -1;
    int preDelayWritePos = 0;
    double modPhase = 0.0;
    std::array<float, kPreDelaySize> preDelayBufL {};
    std::array<float, kPreDelaySize> preDelayBufR {};
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;
    BiquadFilter preHpf;
    BiquadFilter postTone;
};

} // namespace pedalboard
