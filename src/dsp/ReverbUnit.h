#pragma once

#include "../engine/Gp200Catalogue.h"
#include "../engine/ModelPackRegistry.h"
#include "BiquadFilter.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <numbers>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

namespace pedalboard {

// GP-200 RVB Module (All 13 Reverb Models from Daftar_Effect_GP-200.xlsx rows 252..264)
// - Models 0..8 (#252..#260: Room, Hall, Church, Plate, Spring, Studio, Club, Concert, Arena):
//   Hybrid Stereo Convolution Reverb (.wav IR lazy-loaded from bundled ModelPacks.zip / local override)
//   + real-time DSP Pre-Delay (0..150 ms), dynamic Decay IR envelope shaping, Low/High Damp,
//   Spring Tone EQ, Stereo Modulation, and genuine Trail bypass decay!
//   Falls back seamlessly to algorithmic Reverb DSP if an IR is unavailable.
// - Models 9..12 (#261..#264: N-Star, Deepsea, Sweet Space, Shimmer):
//   Pure algorithmic C++ DSP with pitch-shifted +1 octave Hann-grain Shimmer feedback.
class ReverbUnit : private juce::AsyncUpdater {
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

    ~ReverbUnit() override {
        cancelPendingUpdate();
    }

    static bool isIrCapableReverbModel(int modelIndex) noexcept {
        return modelIndex >= 0 && modelIndex <= 8;
    }

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = std::max(8000.0, spec.sampleRate);
        juce::dsp::ProcessSpec stereoSpec = spec;
        stereoSpec.numChannels = 2;
        reverb.prepare(stereoSpec);
        irConv.prepare(stereoSpec);
        wetBuffer.setSize(2, static_cast<int>(spec.maximumBlockSize), false, true, true);
        lastModel = -1;

        loadedIrModel.store(-1, std::memory_order_release);
        loadedDecayBucket.store(-1, std::memory_order_release);
        observedRegistryVer.store(0, std::memory_order_release);
        irActiveForModel.store(false, std::memory_order_release);

        reset();
    }

    void reset() noexcept {
        reverb.reset();
        irConv.reset();
        preHpf.reset();
        postTone.reset();
        shimmerLpf.reset();
        shimmerHpf.reset();
        preDelayBufL.fill(0.0f);
        preDelayBufR.fill(0.0f);
        preDelayWritePos = 0;
        shimmerBufL.fill(0.0f);
        shimmerBufR.fill(0.0f);
        shimmerWritePos = 0;
        shPhaseAL = 0.0;
        shPhaseBL = 0.5;
        shPhaseAR = 0.25;
        shPhaseBR = 0.75;
        shimmerFbL = 0.0f;
        shimmerFbR = 0.0f;
        modPhase = 0.0;
    }

    bool isUsingPackIr() const noexcept {
        return irActiveForModel.load(std::memory_order_acquire);
    }

    juce::String getPackIrDisplayName() const {
        std::lock_guard<std::mutex> lock(irMetaMutex);
        return irDisplayName;
    }

    void requestPackRefresh() {
        observedRegistryVer.store(0, std::memory_order_release);
        irReloadPending.store(true, std::memory_order_release);
        triggerAsyncUpdate();
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

        // Strictly lock-free lazy check for bundled/local Stereo Convolution Reverb IR (#252..#260)
        const int decayBucket = std::clamp(static_cast<int>(std::lround(std::clamp(decay, 0.0f, 1.0f) * 10.0f)), 0, 10);
        if (isIrCapableReverbModel(m)) {
            const uint64_t regVer = ModelPackRegistry::instance().getVersion();
            if (m != loadedIrModel.load(std::memory_order_acquire)
                || decayBucket != loadedDecayBucket.load(std::memory_order_acquire)
                || regVer != observedRegistryVer.load(std::memory_order_acquire)) {
                const int prevM = requestedIrModel.exchange(m, std::memory_order_acq_rel);
                const int prevD = requestedDecayBucket.exchange(decayBucket, std::memory_order_acq_rel);
                const bool wasPending = irReloadPending.exchange(true, std::memory_order_acq_rel);
                if (!wasPending || prevM != m || prevD != decayBucket) {
                    triggerAsyncUpdate();
                }
            }
        } else if (irActiveForModel.load(std::memory_order_acquire)) {
            irActiveForModel.store(false, std::memory_order_release);
        }

        if (p.bypass && !trailOn) {
            return;
        }
        if (mix <= 0.0001f && !p.bypass) {
            return;
        }

        configureFilters(m, lowDamp, hiDamp, toneVal);

        const int preDelaySamples = std::clamp(static_cast<int>(preDelayMs * 0.001f * static_cast<float>(sampleRate)), 0, kPreDelaySize - 4);
        const float inputSend = p.bypass ? 0.0f : 1.0f;
        const bool useConvolution = isIrCapableReverbModel(m)
                                 && (m == loadedIrModel.load(std::memory_order_acquire))
                                 && irActiveForModel.load(std::memory_order_acquire);

        if (useConvolution) {
            // Hybrid Stereo Convolution Path: Pre-Delay + Low Damp HPF -> True Stereo IR -> Post High Damp / Spring Tone + Stereo Mod
            for (int i = 0; i < numSamples; ++i) {
                const float inL = buffer.getReadPointer(0)[i] * inputSend;
                const float inR = (numChannels > 1 ? buffer.getReadPointer(1)[i] : buffer.getReadPointer(0)[i]) * inputSend;

                preDelayBufL[static_cast<size_t>(preDelayWritePos)] = inL;
                preDelayBufR[static_cast<size_t>(preDelayWritePos)] = inR;
                const int readPos = (preDelayWritePos - preDelaySamples) & (kPreDelaySize - 1);
                preDelayWritePos = (preDelayWritePos + 1) & (kPreDelaySize - 1);

                wetBuffer.getWritePointer(0)[i] = preHpf.processSample(0, preDelayBufL[static_cast<size_t>(readPos)]);
                wetBuffer.getWritePointer(1)[i] = preHpf.processSample(1, preDelayBufR[static_cast<size_t>(readPos)]);
            }

            juce::dsp::AudioBlock<float> wetBlock(wetBuffer);
            auto subWet = wetBlock.getSubBlock(0, static_cast<size_t>(numSamples));
            juce::dsp::ProcessContextReplacing<float> ctx(subWet);
            irConv.process(ctx);

            // Normalized IR wet level matches algorithmic reverb loudness across Room/Hall/Plate/Spring/Arena
            constexpr float kIrMakeUp[9] = { 1.15f, 1.25f, 1.30f, 1.18f, 1.35f, 1.18f, 1.20f, 1.25f, 1.28f };
            const float irGain = kIrMakeUp[m] * (0.72f + 0.36f * std::clamp(decay, 0.0f, 1.0f));
            const float wet = std::clamp(mix, 0.0f, 1.0f);
            const float dry = p.bypass ? 1.0f : (1.0f - wet * 0.42f);
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
                    const float srcW = wetBuffer.getReadPointer(ch)[i] * irGain;
                    const float w = postTone.processSample(ch, srcW) * (ch == 0 ? modL : modR);
                    out[i] = out[i] * dry + w * wet;
                }
            }
            return;
        }

        // Algorithmic Reverb Path (Models 9..12 N-Star/Deepsea/Sweet Space/Shimmer + Seamless Fallback)
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

        const bool isShimmer = (m == 12 || m == 9);
        const float shimmerWetGain = (m == 12)
            ? (p.useSlotParams ? (0.32f + std::clamp(sp[1] * 0.0050f, 0.0f, 0.50f)) : 0.58f)
            : 0.26f; // N-Star subtle celestial octave sparkle
        const float shimmerFbGain = (m == 12)
            ? std::clamp(shimmerWetGain * 0.44f, 0.08f, 0.34f)
            : 0.16f;

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
                sL = std::tanh(sL + shimmerFbL * shimmerFbGain);
                sR = std::tanh(sR + shimmerFbR * shimmerFbGain);
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
        constexpr double kShimmerGrainSamples = 2048.0; // ~42 ms smooth Hann window
        constexpr double kOctaveUpDelta = -1.0 / kShimmerGrainSamples; // pitchRatio = 2.0 -> (1 - 2) / N

        for (int i = 0; i < numSamples; ++i) {
            float modL = 1.0f, modR = 1.0f;
            if (modAmt > 0.01f) {
                const float ms = static_cast<float>(std::sin(modPhase));
                modPhase += modInc;
                if (modPhase >= 2.0 * std::numbers::pi) modPhase -= 2.0 * std::numbers::pi;
                modL = 1.0f + modAmt * 0.14f * ms;
                modR = 1.0f - modAmt * 0.14f * ms;
            }

            float rawWetL = wetBuffer.getReadPointer(0)[i];
            float rawWetR = wetBuffer.getReadPointer(1)[i];

            if (isShimmer) {
                shimmerBufL[static_cast<size_t>(shimmerWritePos)] = rawWetL;
                shimmerBufR[static_cast<size_t>(shimmerWritePos)] = rawWetR;

                const float octL = readOctaveGrain(0, shPhaseAL, shPhaseBL, kShimmerGrainSamples);
                const float octR = readOctaveGrain(1, shPhaseAR, shPhaseBR, kShimmerGrainSamples);
                advanceGrain(shPhaseAL, shPhaseBL, kOctaveUpDelta);
                advanceGrain(shPhaseAR, shPhaseBR, kOctaveUpDelta);
                shimmerWritePos = (shimmerWritePos + 1) & (kShimmerBufSize - 1);

                const float silkL = shimmerLpf.processSample(0, shimmerHpf.processSample(0, octL));
                const float silkR = shimmerLpf.processSample(1, shimmerHpf.processSample(1, octR));
                shimmerFbL = silkL;
                shimmerFbR = silkR;

                rawWetL += silkL * shimmerWetGain;
                rawWetR += silkR * shimmerWetGain;
            }

            for (int ch = 0; ch < numChannels; ++ch) {
                float* out = buffer.getWritePointer(ch);
                const float srcW = (ch == 0) ? rawWetL : rawWetR;
                const float w = postTone.processSample(ch, srcW) * (ch == 0 ? modL : modR);
                out[i] = out[i] * dry + w * wet;
            }
        }
    }

private:
    void handleAsyncUpdate() override {
        if (!irReloadPending.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        const int targetM = requestedIrModel.load(std::memory_order_acquire);
        const int targetDecayB = requestedDecayBucket.load(std::memory_order_acquire);
        const uint64_t regVer = ModelPackRegistry::instance().getVersion();

        if (!isIrCapableReverbModel(targetM)) {
            irActiveForModel.store(false, std::memory_order_release);
            loadedIrModel.store(targetM, std::memory_order_release);
            loadedDecayBucket.store(targetDecayB, std::memory_order_release);
            observedRegistryVer.store(regVer, std::memory_order_release);
            return;
        }

        const bool needReloadBytes = (targetM != cachedRawModel)
                                  || (regVer != observedRegistryVer.load(std::memory_order_acquire))
                                  || (cachedRawIr.getNumSamples() == 0);

        if (needReloadBytes) {
            const auto& entry = gp200::getModelEntry(ModuleId::Reverb, targetM);
            auto asset = ModelPackRegistry::instance().loadCaptureBytes(entry.excelNo, true);
            if (!asset.has_value() || asset->bytes.getSize() < 64) {
                cachedRawModel = -1;
                cachedRawIr.setSize(0, 0);
                irActiveForModel.store(false, std::memory_order_release);
                loadedIrModel.store(targetM, std::memory_order_release);
                loadedDecayBucket.store(targetDecayB, std::memory_order_release);
                observedRegistryVer.store(regVer, std::memory_order_release);
                std::lock_guard<std::mutex> lock(irMetaMutex);
                irDisplayName.clear();
                return;
            }

            // Decode WAV from RAM with zero temp files
            juce::WavAudioFormat wavFmt;
            auto memStream = std::make_unique<juce::MemoryInputStream>(asset->bytes.getData(), asset->bytes.getSize(), true);
            std::unique_ptr<juce::AudioFormatReader> reader(wavFmt.createReaderFor(memStream.release(), true));
            if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0) {
                cachedRawModel = -1;
                cachedRawIr.setSize(0, 0);
                irActiveForModel.store(false, std::memory_order_release);
                loadedIrModel.store(targetM, std::memory_order_release);
                loadedDecayBucket.store(targetDecayB, std::memory_order_release);
                observedRegistryVer.store(regVer, std::memory_order_release);
                return;
            }

            const int maxSamples = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, static_cast<juce::int64>(reader->sampleRate * 5.0)));
            const int chCount = std::min<int>(2, static_cast<int>(reader->numChannels));
            juce::AudioBuffer<float> decoded(2, maxSamples);
            decoded.clear();
            reader->read(&decoded, 0, maxSamples, 0, true, chCount > 1);
            if (chCount == 1) {
                decoded.copyFrom(1, 0, decoded, 0, 0, maxSamples);
            }

            cachedRawIr = std::move(decoded);
            cachedIrSampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
            cachedRawModel = targetM;
            {
                std::lock_guard<std::mutex> lock(irMetaMutex);
                irDisplayName = asset->displayName + " [" + asset->originLabel + "]";
            }
        }

        // Apply musical exponential Decay envelope shaping to a copy of the cached Stereo IR!
        const int totalSamples = cachedRawIr.getNumSamples();
        if (totalSamples <= 0) {
            irActiveForModel.store(false, std::memory_order_release);
            return;
        }

        const float decayNorm = std::clamp(static_cast<float>(targetDecayB) * 0.1f, 0.0f, 1.0f);
        // 0% decay -> tight 20% tail with fast exponential damping; 100% decay -> full natural stereo IR
        const float lenFrac = 0.20f + 0.80f * std::pow(decayNorm, 0.72f);
        const int targetLen = std::clamp(static_cast<int>(static_cast<float>(totalSamples) * lenFrac),
                                         std::min(totalSamples, 1024), totalSamples);

        juce::AudioBuffer<float> shaped(2, targetLen);
        shaped.copyFrom(0, 0, cachedRawIr, 0, 0, targetLen);
        shaped.copyFrom(1, 0, cachedRawIr, 1, 0, targetLen);

        const float dampK = (1.0f - decayNorm) * 4.2f; // Exponential decay shaping factor
        const int fadeLen = std::min(targetLen / 4, static_cast<int>(cachedIrSampleRate * 0.030));
        const int fadeStart = targetLen - fadeLen;
        const float invLen = 1.0f / static_cast<float>(std::max(1, targetLen));

        for (int i = 0; i < targetLen; ++i) {
            const float t = static_cast<float>(i) * invLen;
            float env = (dampK > 0.01f) ? std::exp(-dampK * t) : 1.0f;
            if (i >= fadeStart && fadeLen > 0) {
                const float f = static_cast<float>(targetLen - i) / static_cast<float>(fadeLen);
                env *= 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * f);
            }
            shaped.getWritePointer(0)[i] *= env;
            shaped.getWritePointer(1)[i] *= env;
        }

        irConv.loadImpulseResponse(
            std::move(shaped),
            cachedIrSampleRate,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::yes
        );

        loadedIrModel.store(targetM, std::memory_order_release);
        loadedDecayBucket.store(targetDecayB, std::memory_order_release);
        observedRegistryVer.store(regVer, std::memory_order_release);
        irActiveForModel.store(true, std::memory_order_release);
    }

    void configureFilters(int m, float lowDamp, float hiDamp, float toneVal) noexcept {
        lastModel = m;
        const double hpfHz = 85.0 + std::clamp(static_cast<double>(lowDamp), 0.0, 1.0) * 340.0;
        preHpf.setHighPass(sampleRate, hpfHz, 0.72);

        if (m == 4) { // Spring Reverb: Tone knob controls resonant spring tank brightness
            const double tHz = 1350.0 + std::clamp(static_cast<double>(toneVal), 0.0, 10.0) * 360.0;
            postTone.setPeaking(sampleRate, tHz, 1.25, 4.2);
        } else if (m == 12 || m == 9) {
            const double shelfDb = (1.0 - std::clamp(static_cast<double>(hiDamp), 0.0, 1.0)) * 5.0;
            postTone.setHighShelf(sampleRate, 3600.0, shelfDb, 0.8);
            const double shimLpfHz = 3400.0 + (1.0 - std::clamp(static_cast<double>(hiDamp), 0.0, 1.0)) * 3400.0;
            shimmerLpf.setLowPass(sampleRate, std::clamp(shimLpfHz, 2600.0, 7200.0), 0.72);
            shimmerHpf.setHighPass(sampleRate, 260.0, 0.72);
        } else {
            const double lpfHz = 11000.0 - std::clamp(static_cast<double>(hiDamp), 0.0, 1.0) * 8200.0;
            postTone.setLowPass(sampleRate, std::clamp(lpfHz, 1800.0, 14000.0), 0.72);
        }
    }

    float readShimmerLinear(int ch, double delaySamples) const noexcept {
        const auto& buf = (ch == 0) ? shimmerBufL : shimmerBufR;
        double readPos = static_cast<double>(shimmerWritePos) - delaySamples;
        while (readPos < 0.0) readPos += static_cast<double>(kShimmerBufSize);
        const int idx0 = static_cast<int>(readPos) & (kShimmerBufSize - 1);
        const int idx1 = (idx0 + 1) & (kShimmerBufSize - 1);
        const float frac = static_cast<float>(readPos - std::floor(readPos));
        return buf[static_cast<size_t>(idx0)] * (1.0f - frac) + buf[static_cast<size_t>(idx1)] * frac;
    }

    float readOctaveGrain(int ch, double phA, double phB, double grainWinSamples) const noexcept {
        const double dA = 32.0 + phA * grainWinSamples;
        const double dB = 32.0 + phB * grainWinSamples;
        const float wA = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * phA));
        const float wB = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * phB));
        return readShimmerLinear(ch, dA) * wA + readShimmerLinear(ch, dB) * wB;
    }

    static void advanceGrain(double& phA, double& phB, double delta) noexcept {
        phA += delta;
        phB += delta;
        while (phA < 0.0) phA += 1.0;
        while (phA >= 1.0) phA -= 1.0;
        while (phB < 0.0) phB += 1.0;
        while (phB >= 1.0) phB -= 1.0;
    }

    static constexpr int kPreDelaySize = 16384;
    static constexpr int kShimmerBufSize = 8192;

    double sampleRate = 44100.0;
    int lastModel = -1;
    int preDelayWritePos = 0;
    int shimmerWritePos = 0;
    double shPhaseAL = 0.0, shPhaseBL = 0.5;
    double shPhaseAR = 0.25, shPhaseBR = 0.75;
    float shimmerFbL = 0.0f, shimmerFbR = 0.0f;
    double modPhase = 0.0;
    std::array<float, kPreDelaySize> preDelayBufL {};
    std::array<float, kPreDelaySize> preDelayBufR {};
    std::array<float, kShimmerBufSize> shimmerBufL {};
    std::array<float, kShimmerBufSize> shimmerBufR {};

    juce::dsp::Reverb reverb;
    juce::dsp::Convolution irConv;
    juce::AudioBuffer<float> wetBuffer;

    // Lazy IR cache (accessed ONLY on Message Thread inside handleAsyncUpdate)
    int cachedRawModel = -1;
    double cachedIrSampleRate = 44100.0;
    juce::AudioBuffer<float> cachedRawIr;

    std::atomic<int> loadedIrModel { -1 };
    std::atomic<int> requestedIrModel { -1 };
    std::atomic<int> loadedDecayBucket { -1 };
    std::atomic<int> requestedDecayBucket { 5 };
    std::atomic<uint64_t> observedRegistryVer { 0 };
    std::atomic<bool> irReloadPending { false };
    std::atomic<bool> irActiveForModel { false };
    mutable std::mutex irMetaMutex;
    juce::String irDisplayName;

    BiquadFilter preHpf;
    BiquadFilter postTone;
    BiquadFilter shimmerLpf;
    BiquadFilter shimmerHpf;
};

} // namespace pedalboard
