#pragma once

#include "../dsp/BiquadFilter.h"
#include "../dsp/CompressorStomp.h"
#include "../dsp/GraphicEq.h"
#include "../dsp/NamModelRunner.h"
#include "../dsp/NoiseGate.h"
#include "../dsp/OverdriveStomp.h"
#include "../dsp/ReverbUnit.h"
#include "../dsp/StereoChorus.h"
#include "../dsp/StereoDelay.h"
#include "../dsp/WahStomp.h"
#include "Gp200Catalogue.h"
#include "Gp200ParamSpecs.h"
#include "ModuleId.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pedalboard {

// Up to 8 independent stompbox slots can be placed on the StompBoard & Signal Chain simultaneously.
// Any slot can load ANY effect category (DST, PRE, WAH, NR, EQ, MOD, DLY, RVB) and ANY GP-200 model.
constexpr int kMaxPedalSlots = 8;

enum class ChainNodeKind : uint8_t {
    PedalSlot  = 0, // Instance index 0..kMaxPedalSlots-1
    AmpHead    = 1, // Main Amp A/B stage
    CabinetIR  = 2, // Main Cab A/B stage
    RackMod    = 3, // Studio Rack Modulation unit
    RackDelay  = 4, // Studio Rack Delay unit
    RackReverb = 5  // Studio Rack Reverb unit
};

struct ChainNode {
    ChainNodeKind kind = ChainNodeKind::PedalSlot;
    int slotId = 0; // Used when kind == ChainNodeKind::PedalSlot (0..kMaxPedalSlots-1)
    BusLane bus = BusLane::PathA;

    bool operator==(const ChainNode& o) const noexcept {
        return kind == o.kind && (kind != ChainNodeKind::PedalSlot || slotId == o.slotId);
    }
};

struct PedalSlotSnapshot {
    bool occupied = false;
    bool bypassed = false;
    ModuleId category = ModuleId::Overdrive;
    int modelIndex = 0;
    std::array<float, kMaxSlotParams> params {};
};

inline StereoChorus::Params buildChorusParamsFromSlot(const PedalSlotSnapshot& s) noexcept {
    StereoChorus::Params p;
    p.bypass        = s.bypassed;
    p.modelIndex    = s.modelIndex;
    p.useSlotParams = true;
    p.slotParams    = s.params;
    return p;
}

inline StereoDelay::Params buildDelayParamsFromSlot(const PedalSlotSnapshot& s) noexcept {
    StereoDelay::Params p;
    p.bypass        = s.bypassed;
    p.modelIndex    = s.modelIndex;
    p.useSlotParams = true;
    p.slotParams    = s.params;
    return p;
}

inline ReverbUnit::Params buildReverbParamsFromSlot(const PedalSlotSnapshot& s) noexcept {
    ReverbUnit::Params p;
    p.bypass        = s.bypassed;
    p.modelIndex    = s.modelIndex;
    p.useSlotParams = true;
    p.slotParams    = s.params;
    return p;
}

// Pre-allocated multi-effect DSP instance for a single pedal slot (zero audio-thread allocation).
class UniversalPedalSlotDsp {
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        gate.prepare(spec);
        comp.prepare(spec);
        wah.prepare(spec);
        od.prepare(spec);
        eq.prepare(spec);
        chorus.prepare(spec);
        delay.prepare(spec);
        reverb.prepare(spec);

        namSampleRate = std::max(8000.0, spec.sampleRate);
        namPedalRunner.prepare(namSampleRate, static_cast<int>(spec.maximumBlockSize));
        namDryBuf.setSize(
            std::max(2, static_cast<int>(spec.numChannels)),
            std::max(256, static_cast<int>(spec.maximumBlockSize)),
            false, true, true
        );
        lastNamTone = -1.0f;
        namToneFilter.reset();
    }

    void reset() noexcept {
        gate.reset();
        comp.reset();
        wah.reset();
        od.reset();
        eq.reset();
        chorus.reset();
        delay.reset();
        reverb.reset();
        namPedalRunner.reset();
        namToneFilter.reset();
    }

    NamModelRunner& getNamRunner() noexcept { return namPedalRunner; }
    const NamModelRunner& getNamRunner() const noexcept { return namPedalRunner; }

    void process(juce::AudioBuffer<float>& buffer, const PedalSlotSnapshot& s) noexcept {
        if (!s.occupied) {
            return;
        }
        // Delay & Reverb support Trail bypass (tails continue ringing out when bypassed with Trail ON!)
        if (s.bypassed && s.category != ModuleId::StereoDelay && s.category != ModuleId::Reverb) {
            return;
        }

        switch (s.category) {
            case ModuleId::NoiseGate: {
                NoiseGate::Params p;
                p.bypass      = s.bypassed;
                p.modelIndex  = s.modelIndex;
                p.thresholdDb = s.params[0];
                if (s.modelIndex >= 1) {
                    p.attackMs  = s.params[1];
                    p.releaseMs = s.params[2];
                }
                gate.process(buffer, p);
                break;
            }
            case ModuleId::Compressor: {
                CompressorStomp::Params p;
                p.bypass        = s.bypassed;
                p.modelIndex    = s.modelIndex;
                p.useSlotParams = true;
                p.slotParams    = s.params;
                comp.process(buffer, p);
                break;
            }
            case ModuleId::Wah: {
                WahStomp::Params p;
                p.bypass         = s.bypassed;
                p.modelIndex     = s.modelIndex;
                p.useExcelParams = true;
                p.pedal          = s.params[0];
                p.range          = s.params[1];
                p.q              = s.params[2];
                p.volume         = s.params[3];
                if (s.modelIndex == 2) {
                    p.eq      = s.params[4];
                    p.autoWah = s.params[5] > 0.5f;
                } else {
                    p.autoWah = s.params[4] > 0.5f;
                }
                wah.process(buffer, p);
                break;
            }
            case ModuleId::Overdrive: {
                if (isNamPedalSlotModel(s.category, s.modelIndex)) {
                    processNamPedal(buffer, s);
                    break;
                }
                OverdriveStomp::Params p;
                p.bypass        = s.bypassed;
                p.modelIndex    = s.modelIndex;
                p.useSlotParams = true;
                p.slotParams    = s.params;
                od.process(buffer, p);
                break;
            }
            case ModuleId::GraphicEq: {
                GraphicEq::Params p;
                p.bypass          = s.bypassed;
                p.modelIndex      = s.modelIndex;
                const int bands   = GraphicEq::bandCountForModel(s.modelIndex);
                p.activeBandCount = bands;
                for (int b = 0; b < bands && b < GraphicEq::kMaxBands; ++b) {
                    p.bandsDb[static_cast<size_t>(b)] = s.params[static_cast<size_t>(b)];
                }
                p.levelDb = GraphicEq::hasVolumeForModel(s.modelIndex) ? s.params[static_cast<size_t>(bands)] : 0.0f;
                eq.process(buffer, p);
                break;
            }
            case ModuleId::StereoChorus: {
                chorus.process(buffer, buildChorusParamsFromSlot(s));
                break;
            }
            case ModuleId::StereoDelay: {
                delay.process(buffer, buildDelayParamsFromSlot(s));
                break;
            }
            case ModuleId::Reverb:
            default: {
                reverb.process(buffer, buildReverbParamsFromSlot(s));
                break;
            }
        }
    }

    float getGainReductionDb() const noexcept { return comp.getGainReductionDb(); }

private:
    void processNamPedal(juce::AudioBuffer<float>& buffer, const PedalSlotSnapshot& s) noexcept {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min(2, buffer.getNumChannels());
        if (numSamples <= 0 || numChannels <= 0) return;

        const float inputGain  = juce::Decibels::decibelsToGain(std::clamp(s.params[0], -18.0f, 18.0f));
        const float outputGain = juce::Decibels::decibelsToGain(std::clamp(s.params[1], -18.0f, 18.0f));
        const float tone       = std::clamp(s.params[2], 0.0f, 10.0f);
        const float wetMix     = std::clamp(s.params[3] * 0.01f, 0.0f, 1.0f);

        const bool useDryBlend = (wetMix < 0.999f && namDryBuf.getNumSamples() >= numSamples && namDryBuf.getNumChannels() >= numChannels);
        if (useDryBlend) {
            for (int ch = 0; ch < numChannels; ++ch) {
                namDryBuf.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            }
        }

        const bool ranNam = namPedalRunner.process(buffer, inputGain);
        if (!ranNam) {
            // Transparent unity pass-through while slot has no .nam file loaded yet
            return;
        }

        const bool applyTone = (std::abs(tone - 5.0f) > 0.05f);
        if (applyTone && std::abs(tone - lastNamTone) > 0.03f) {
            lastNamTone = tone;
            const double shelfDb = static_cast<double>((tone - 5.0f) * 1.6f); // -8..+8 dB gentle tilt shelf
            namToneFilter.setHighShelf(namSampleRate, 1800.0, 0.707, shelfDb);
        }

        const float dryMix = 1.0f - wetMix;
        for (int ch = 0; ch < numChannels; ++ch) {
            float* dst = buffer.getWritePointer(ch);
            const float* dry = useDryBlend ? namDryBuf.getReadPointer(ch) : nullptr;
            for (int i = 0; i < numSamples; ++i) {
                float y = dst[i];
                if (applyTone) {
                    y = namToneFilter.processSample(ch, y);
                }
                y = std::tanh(y * outputGain * 0.96f);
                if (dry != nullptr) {
                    y = dry[i] * dryMix + y * wetMix;
                }
                dst[i] = y;
            }
        }
    }

    NoiseGate gate;
    CompressorStomp comp;
    WahStomp wah;
    OverdriveStomp od;
    GraphicEq eq;
    StereoChorus chorus;
    StereoDelay delay;
    ReverbUnit reverb;

    NamModelRunner namPedalRunner;
    BiquadFilter namToneFilter;
    juce::AudioBuffer<float> namDryBuf;
    double namSampleRate = 48000.0;
    float lastNamTone = -1.0f;
};

} // namespace pedalboard
