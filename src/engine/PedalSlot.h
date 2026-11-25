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
#include "ModelPackRegistry.h"
#include "ModuleId.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <mutex>
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

inline StereoChorus::Params buildChorusParamsFromSlot(const PedalSlotSnapshot& s, float tempoBpm = 120.0f) noexcept {
    StereoChorus::Params p;
    p.bypass        = s.bypassed;
    p.modelIndex    = s.modelIndex;
    p.useSlotParams = true;
    p.slotParams    = s.params;
    p.tempoBpm      = tempoBpm;
    return p;
}

inline StereoDelay::Params buildDelayParamsFromSlot(const PedalSlotSnapshot& s, float tempoBpm = 120.0f) noexcept {
    StereoDelay::Params p;
    p.bypass        = s.bypassed;
    p.modelIndex    = s.modelIndex;
    p.useSlotParams = true;
    p.slotParams    = s.params;
    p.tempoBpm      = tempoBpm;
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

// True for the 43 GP-200 stomp models backed by ModelPacks (.nam for DST #26..#58, PRE Boosts #4..#10, #12 AC Sim, #20 Saturate; .wav IR for #11 AC Refiner).
inline bool isPackCapableStompModel(ModuleId cat, int modelIndex) noexcept {
    if (cat == ModuleId::Overdrive && modelIndex >= 0 && modelIndex < kNamPedalModelIndex) {
        return true;
    }
    if (cat == ModuleId::Compressor && ((modelIndex >= 3 && modelIndex <= 11) || modelIndex == 19)) {
        return true;
    }
    return false;
}

// True for the 9 GP-200 Space/Plate/Spring Reverbs (#252..#260) backed by Stereo Convolution .wav IRs in ModelPacks.
inline bool isPackCapableReverbModel(ModuleId cat, int modelIndex) noexcept {
    return cat == ModuleId::Reverb && ReverbUnit::isIrCapableReverbModel(modelIndex);
}

// True for time-based classic units with an optional Hybrid Preamp .NAM Color stage before White-Box C++ DSP:
// - Boss CE-1 Chorus / Vibrato (MOD: 0 = G-Chorus, 9 = G-Roto)
// - Roland RE-201 Tape Echo family (DLY: 2 = Tape Delay S, 7 = Tape Delay, 13 = M-Echo)
inline bool isHybridPreampColorModel(ModuleId cat, int modelIndex) noexcept {
    if (cat == ModuleId::StereoChorus && (modelIndex == 0 || modelIndex == 9)) return true;
    if (cat == ModuleId::StereoDelay && (modelIndex == 2 || modelIndex == 7 || modelIndex == 13)) return true;
    return false;
}

// Pre-allocated multi-effect DSP instance for a single pedal slot (zero audio-thread allocation).
// Automatically lazy-loads bundled/local ModelPack .nam / .wav captures for all 43 pack-capable DST/PRE models!
class UniversalPedalSlotDsp : private juce::AsyncUpdater {
public:
    ~UniversalPedalSlotDsp() override {
        cancelPendingUpdate();
    }

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
        packNamRunner.prepare(namSampleRate, static_cast<int>(spec.maximumBlockSize));
        packIrConv.prepare(spec);
        namDryBuf.setSize(
            std::max(2, static_cast<int>(spec.numChannels)),
            std::max(256, static_cast<int>(spec.maximumBlockSize)),
            false, true, true
        );
        lastNamTone = -1.0f;
        namToneFilter.reset();
        packHpf.reset();
        packBassEq.reset();
        packMidEq.reset();
        packTrebleEq.reset();
        packLpf.reset();
        packGateEnv = 0.0f;

        loadedPackKey.store(-1, std::memory_order_release);
        observedRegistryVer.store(0, std::memory_order_release);
        packNamActive.store(false, std::memory_order_release);
        packIrActive.store(false, std::memory_order_release);
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
        packNamRunner.reset();
        packIrConv.reset();
        namToneFilter.reset();
        packHpf.reset();
        packBassEq.reset();
        packMidEq.reset();
        packTrebleEq.reset();
        packLpf.reset();
        packGateEnv = 0.0f;
    }

    NamModelRunner& getNamRunner() noexcept { return namPedalRunner; }
    const NamModelRunner& getNamRunner() const noexcept { return namPedalRunner; }

    NamModelRunner& getPackNamRunner() noexcept { return packNamRunner; }
    const NamModelRunner& getPackNamRunner() const noexcept { return packNamRunner; }

    bool isUsingPackCapture() const noexcept {
        const auto cat = static_cast<ModuleId>(lastActiveCategory.load(std::memory_order_acquire));
        if (cat == ModuleId::Reverb) {
            return reverb.isUsingPackIr();
        }
        if (cat == ModuleId::StereoChorus || cat == ModuleId::StereoDelay) {
            return preampColorEnabled.load(std::memory_order_acquire)
                && packNamActive.load(std::memory_order_acquire)
                && packNamRunner.hasLoadedModel();
        }
        return (packNamActive.load(std::memory_order_acquire) && packNamRunner.hasLoadedModel())
            || packIrActive.load(std::memory_order_acquire);
    }

    juce::String getPackCaptureName() const {
        const auto cat = static_cast<ModuleId>(lastActiveCategory.load(std::memory_order_acquire));
        if (cat == ModuleId::Reverb) {
            return reverb.getPackIrDisplayName();
        }
        if (packNamActive.load(std::memory_order_acquire) && packNamRunner.hasLoadedModel()) {
            return packNamRunner.getDisplayName();
        }
        std::lock_guard<std::mutex> lock(packMetaMutex);
        return packIrDisplayName;
    }

    void setPreampColorEnabled(bool enabled) {
        if (preampColorEnabled.exchange(enabled, std::memory_order_acq_rel) != enabled) {
            loadedPackKey.store(-1, std::memory_order_release);
            packReloadPending.store(true, std::memory_order_release);
            triggerAsyncUpdate();
        }
    }

    bool isPreampColorEnabled() const noexcept {
        return preampColorEnabled.load(std::memory_order_acquire);
    }

    void requestPackRefresh() {
        reverb.requestPackRefresh();
        observedRegistryVer.store(0, std::memory_order_release);
        packReloadPending.store(true, std::memory_order_release);
        triggerAsyncUpdate();
    }

    void process(juce::AudioBuffer<float>& buffer, const PedalSlotSnapshot& s, float tempoBpm = 120.0f) noexcept {
        if (!s.occupied) {
            return;
        }
        lastActiveCategory.store(static_cast<int>(s.category), std::memory_order_release);

        // Delay & Reverb support Trail bypass (tails continue ringing out when bypassed with Trail ON!)
        if (s.bypassed && s.category != ModuleId::StereoDelay && s.category != ModuleId::Reverb) {
            return;
        }

        // Check lock-free lazy ModelPack lookup for pack-capable DST & PRE models
        if (isPackCapableStompModel(s.category, s.modelIndex)) {
            const int key = packKeyFor(s.category, s.modelIndex);
            const uint64_t regVer = ModelPackRegistry::instance().getVersion();
            if (key != loadedPackKey.load(std::memory_order_acquire)
                || regVer != observedRegistryVer.load(std::memory_order_acquire)) {
                const int prevReq = requestedPackKey.exchange(key, std::memory_order_acq_rel);
                const bool wasPending = packReloadPending.exchange(true, std::memory_order_acq_rel);
                if (!wasPending || prevReq != key) {
                    triggerAsyncUpdate();
                }
            }

            if (key == loadedPackKey.load(std::memory_order_acquire)) {
                if (packNamActive.load(std::memory_order_acquire) && packNamRunner.hasLoadedModel()) {
                    processPackNamPedal(buffer, s);
                    return;
                }
                if (packIrActive.load(std::memory_order_acquire)) {
                    processPackIrPedal(buffer, s);
                    return;
                }
            }
        } else if (isHybridPreampColorModel(s.category, s.modelIndex) && preampColorEnabled.load(std::memory_order_acquire)) {
            // Optional Hybrid Preamp .NAM Color stage (Boss CE-1 & RE-201 Tape) in series BEFORE White-Box C++ DSP!
            const int key = packKeyFor(s.category, s.modelIndex);
            const uint64_t regVer = ModelPackRegistry::instance().getVersion();
            if (key != loadedPackKey.load(std::memory_order_acquire)
                || regVer != observedRegistryVer.load(std::memory_order_acquire)) {
                const int prevReq = requestedPackKey.exchange(key, std::memory_order_acq_rel);
                const bool wasPending = packReloadPending.exchange(true, std::memory_order_acq_rel);
                if (!wasPending || prevReq != key) {
                    triggerAsyncUpdate();
                }
            }

            if (!s.bypassed
                && key == loadedPackKey.load(std::memory_order_acquire)
                && packNamActive.load(std::memory_order_acquire)
                && packNamRunner.hasLoadedModel()) {
                // Subtle musical input preamp warmth (~40% wet blend preserves clean headroom while adding real analog texture)
                const int nSamp = buffer.getNumSamples();
                const int nCh   = std::min(2, buffer.getNumChannels());
                const bool canBlend = (namDryBuf.getNumSamples() >= nSamp && namDryBuf.getNumChannels() >= nCh);
                if (canBlend) {
                    for (int ch = 0; ch < nCh; ++ch) {
                        namDryBuf.copyFrom(ch, 0, buffer, ch, 0, nSamp);
                    }
                }
                if (packNamRunner.process(buffer, 0.85f) && canBlend) {
                    constexpr float kPreWet = 0.42f;
                    constexpr float kPreDry = 1.0f - kPreWet;
                    for (int ch = 0; ch < nCh; ++ch) {
                        float* dst = buffer.getWritePointer(ch);
                        const float* dSrc = namDryBuf.getReadPointer(ch);
                        for (int i = 0; i < nSamp; ++i) {
                            dst[i] = dSrc[i] * kPreDry + std::tanh(dst[i] * 0.95f) * kPreWet;
                        }
                    }
                }
            }
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
                p.tempoBpm      = tempoBpm;
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
                chorus.process(buffer, buildChorusParamsFromSlot(s, tempoBpm));
                break;
            }
            case ModuleId::StereoDelay: {
                delay.process(buffer, buildDelayParamsFromSlot(s, tempoBpm));
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
    static int packKeyFor(ModuleId cat, int modelIndex) noexcept {
        return (static_cast<int>(cat) << 8) | (modelIndex & 0xff);
    }

    void handleAsyncUpdate() override {
        if (!packReloadPending.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        const int key = requestedPackKey.load(std::memory_order_acquire);
        const uint64_t regVer = ModelPackRegistry::instance().getVersion();
        if (key < 0) {
            observedRegistryVer.store(regVer, std::memory_order_release);
            return;
        }
        if (key == loadedPackKey.load(std::memory_order_acquire)
            && regVer == observedRegistryVer.load(std::memory_order_acquire)) {
            return;
        }

        const auto cat = static_cast<ModuleId>(key >> 8);
        const int modelIdx = key & 0xff;
        const auto& entry = gp200::getModelEntry(cat, modelIdx);

        observedRegistryVer.store(regVer, std::memory_order_release);
        loadedPackKey.store(key, std::memory_order_release);

        // Optional Hybrid Preamp .NAM Color for Boss CE-1 (#209/#218) & RE-201 Tape (#235/#240/#246)
        if (isHybridPreampColorModel(cat, modelIdx)) {
            packIrActive.store(false, std::memory_order_release);
            if (!preampColorEnabled.load(std::memory_order_acquire)) {
                packNamActive.store(false, std::memory_order_release);
                packNamRunner.clearModel();
                return;
            }
            // Priority: exact user override (e.g. 209_CE1_Preamp.nam / 235_RE201_Preamp.nam),
            // otherwise use bundled warm tape saturation (#020) for RE-201 or EP/Klon warm preamp (#010) for CE-1!
            auto asset = ModelPackRegistry::instance().loadCaptureBytes(entry.excelNo, false);
            if (!asset.has_value()) {
                const int fallbackNo = (cat == ModuleId::StereoDelay) ? 20 : 10;
                asset = ModelPackRegistry::instance().loadCaptureBytes(fallbackNo, false);
            }
            if (!asset.has_value()) {
                packNamActive.store(false, std::memory_order_release);
                packNamRunner.clearModel();
                return;
            }
            const juce::String preLabel = (cat == ModuleId::StereoDelay)
                ? ("Tape Preamp: " + asset->displayName)
                : ("CE-1 Preamp: " + asset->displayName);
            packNamActive.store(true, std::memory_order_release);
            packNamRunner.loadModelFromMemoryAsync(std::move(asset->bytes), preLabel, asset->originLabel);
            return;
        }

        const auto info = ModelPackRegistry::getExcelPackInfo(entry.excelNo);
        if (!info.supportsPack) {
            packNamActive.store(false, std::memory_order_release);
            packIrActive.store(false, std::memory_order_release);
            packNamRunner.clearModel();
            return;
        }

        auto asset = ModelPackRegistry::instance().loadCaptureBytes(entry.excelNo, info.isIr);
        if (!asset.has_value()) {
            packNamActive.store(false, std::memory_order_release);
            packIrActive.store(false, std::memory_order_release);
            packNamRunner.clearModel();
            std::lock_guard<std::mutex> lock(packMetaMutex);
            packIrDisplayName.clear();
            return;
        }

        if (info.isIr) {
            packNamActive.store(false, std::memory_order_release);
            packNamRunner.clearModel();
            packIrConv.loadImpulseResponse(
                asset->bytes.getData(),
                asset->bytes.getSize(),
                juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::yes,
                0,
                juce::dsp::Convolution::Normalise::yes
            );
            {
                std::lock_guard<std::mutex> lock(packMetaMutex);
                packIrDisplayName = asset->displayName + " [" + asset->originLabel + "]";
            }
            packIrActive.store(true, std::memory_order_release);
        } else {
            packIrActive.store(false, std::memory_order_release);
            packNamActive.store(true, std::memory_order_release);
            packNamRunner.loadModelFromMemoryAsync(std::move(asset->bytes), asset->displayName, asset->originLabel);
        }
    }

    // Authentic GP-200 knob mapping around a bundled/local .nam capture for all 42 DST & PRE Boost/Sim/Tape models!
    void processPackNamPedal(juce::AudioBuffer<float>& buffer, const PedalSlotSnapshot& s) noexcept {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min(2, buffer.getNumChannels());
        if (numSamples <= 0 || numChannels <= 0) return;

        float inputDb   = 0.0f;
        float outputDb  = 0.0f;
        float wetMix    = 1.0f;
        double hpfHz    = 0.0;
        double lpfHz    = 0.0;
        double bassDb   = 0.0;
        double midHz    = 750.0;
        double midDb    = 0.0;
        double trebleDb = 0.0;
        float gateKnob  = 0.0f;

        if (s.category == ModuleId::Compressor) {
            const int m = s.modelIndex;
            if (m == 3 || m == 7) { // #4 Micro Boost, #8 14 Boost: Gain (0..20 dB)
                inputDb  = (std::clamp(s.params[0], 0.0f, 20.0f) - 10.0f) * 0.6f;
                outputDb = (std::clamp(s.params[0], 0.0f, 20.0f) - 10.0f) * 0.6f;
                if (m == 7) hpfHz = 115.0; // Fortin Grind tight low-end
            } else if (m == 4 || m == 5 || m == 6) { // #5 AC, #6 BB, #7 RC: Gain, Vol, Bass, Treble
                inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.4f;
                outputDb = (std::clamp(s.params[1], 0.0f, 10.0f) - 5.0f) * 2.4f;
                bassDb   = (static_cast<double>(std::clamp(s.params[2], 0.0f, 10.0f)) - 5.0) * 2.0;
                trebleDb = (static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 2.0;
            } else if (m == 8) { // #9 FAT BB: Bass, Treble, Volume, Low Cut
                bassDb   = (static_cast<double>(std::clamp(s.params[0], 0.0f, 10.0f)) - 5.0) * 2.0;
                trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 2.0;
                outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                if (s.params[3] > 0.5f) hpfHz = 120.0;
            } else if (m == 9) { // #10 EP Booster: Gain (0..20 dB), +3dB, Bright
                inputDb  = (std::clamp(s.params[0], 0.0f, 20.0f) - 8.0f) * 0.6f + (s.params[1] > 0.5f ? 3.0f : 0.0f);
                outputDb = (std::clamp(s.params[0], 0.0f, 20.0f) - 8.0f) * 0.5f;
                if (s.params[2] > 0.5f) trebleDb = 3.0;
            } else if (m == 11) { // #12 AC Sim: Body, Top, Volume, Mode
                bassDb   = (static_cast<double>(std::clamp(s.params[0], 0.0f, 10.0f)) - 5.0) * 1.8;
                trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 1.8;
                outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.2f;
                const int mode = static_cast<int>(std::lround(s.params[3]));
                if (mode == 1) bassDb += 2.2;      // Jumbo
                else if (mode == 2) midDb = 2.0;   // Enhanced
                else if (mode == 3) trebleDb += 2.5; // Piezo
            } else if (m == 19) { // #20 Saturate: Sat, Mix%, Volume, High Cut Hz
                inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.4f;
                wetMix   = std::clamp(s.params[1] * 0.01f, 0.0f, 1.0f);
                outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.2f;
                lpfHz    = std::clamp(static_cast<double>(s.params[3]), 1500.0, 12000.0);
            }
        } else {
            // ModuleId::Overdrive (DST models 0..32)
            const int m = s.modelIndex;
            switch (m) {
                case 2: case 15: case 16: case 17: // 2-knob: Gain/Fuzz, Volume
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.6f;
                    outputDb = (std::clamp(s.params[1], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    break;
                case 6: // #32 Scream OD: Gain, Tone, Volume, Fat, Air
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.5f;
                    trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 1.4
                             + (static_cast<double>(std::clamp(s.params[4], 0.0f, 10.0f)) - 5.0) * 1.2;
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    bassDb   = (static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 1.8;
                    break;
                case 8: // #34 Force OD (OCD): Gain, Tone, Volume, Mode (0=HP, 1=LP)
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.5f + (s.params[3] < 0.5f ? 1.5f : 0.0f);
                    trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 1.8;
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    midDb    = (s.params[3] < 0.5f) ? 2.2 : 0.0;
                    break;
                case 10: case 20: // #36 OD JCM800, #46 Chief (Guv'nor): Gain, Vol, Bass, Mid, Treble
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.6f;
                    outputDb = (std::clamp(s.params[1], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    bassDb   = (static_cast<double>(std::clamp(s.params[2], 0.0f, 10.0f)) - 5.0) * 2.0;
                    midDb    = (static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 2.0;
                    trebleDb = (static_cast<double>(std::clamp(s.params[4], 0.0f, 10.0f)) - 5.0) * 2.0;
                    break;
                case 11: // #37 TaiChi OD (Zendrive): Gain, Tone, Volume, Voice
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 1.8;
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    midHz    = 1150.0;
                    midDb    = (static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 1.6;
                    break;
                case 12: { // #38 Timmy OD: Gain, Volume, Bass, Treble, Mode (0/1/2)
                    const int mode = static_cast<int>(std::lround(s.params[4]));
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.4f + static_cast<float>(mode) * 1.2f;
                    outputDb = (std::clamp(s.params[1], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    bassDb   = (static_cast<double>(std::clamp(s.params[2], 0.0f, 10.0f)) - 5.0) * 2.0;
                    trebleDb = (static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 2.0;
                    break;
                }
                case 13: { // #39 Precise OD (Precision Drive): Gain, Tone, Volume, Gate, Attack (0..5)
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 1.8;
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    gateKnob = std::clamp(s.params[3], 0.0f, 10.0f);
                    const int atk = std::clamp(static_cast<int>(std::lround(s.params[4])), 0, 5);
                    hpfHz = 70.0 + static_cast<double>(atk) * 38.0; // Tighter low-cut per Attack step!
                    break;
                }
                case 19: // #45 Darktale (ProCo Rat): Gain, Filter (INVERTED — higher = darker!), Volume
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.6f;
                    lpfHz    = 11500.0 * std::pow(1100.0 / 11500.0, std::clamp(static_cast<double>(s.params[1]) / 10.0, 0.0, 1.0));
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    break;
                case 21: // #47 Master Dist (Shredmaster): Gain, Volume, Bass, Contour (mid scoop), Treble
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.6f;
                    outputDb = (std::clamp(s.params[1], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    bassDb   = (static_cast<double>(std::clamp(s.params[2], 0.0f, 10.0f)) - 5.0) * 2.0;
                    midHz    = 680.0;
                    midDb    = -(static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 2.2;
                    trebleDb = (static_cast<double>(std::clamp(s.params[4], 0.0f, 10.0f)) - 5.0) * 2.0;
                    break;
                case 23: { // #49 Flex OD: Gain, Tone, Volume, Blend%, Mode (Normal/Scoop/Edge)
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.5f;
                    trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 1.8;
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    wetMix   = std::clamp(s.params[3] * 0.01f, 0.0f, 1.0f);
                    const int mode = static_cast<int>(std::lround(s.params[4]));
                    if (mode == 1) midDb = -4.0;      // Scoop
                    else if (mode == 2) trebleDb += 2.8; // Edge
                    break;
                }
                case 24: // #50 Bass OD (ODB-3): Gain, Blend%, Volume, Bass, Treble
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.5f;
                    wetMix   = std::clamp(s.params[1] * 0.01f, 0.0f, 1.0f);
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    bassDb   = (static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 2.2;
                    trebleDb = (static_cast<double>(std::clamp(s.params[4], 0.0f, 10.0f)) - 5.0) * 2.2;
                    break;
                case 25: { // #51 Black Bass (Darkglass B7K): Gain, Blend%, Vol, Low, Lo-Mid, Hi-Mid, Treble, Attack
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.6f;
                    wetMix   = std::clamp(s.params[1] * 0.01f, 0.0f, 1.0f);
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    bassDb   = (static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 2.2;
                    midHz    = 950.0;
                    midDb    = ((static_cast<double>(std::clamp(s.params[4], 0.0f, 10.0f)) - 5.0) * 1.1)
                             + ((static_cast<double>(std::clamp(s.params[5], 0.0f, 10.0f)) - 5.0) * 1.3);
                    const int atk = static_cast<int>(std::lround(s.params[7]));
                    trebleDb = (static_cast<double>(std::clamp(s.params[6], 0.0f, 10.0f)) - 5.0) * 2.0
                             + (atk == 0 ? -3.0 : (atk == 2 ? 3.2 : 0.0));
                    break;
                }
                case 26: case 30: // #52 Micro Boost, #56 14 Boost: Gain (0..20 dB)
                    inputDb  = (std::clamp(s.params[0], 0.0f, 20.0f) - 10.0f) * 0.6f;
                    outputDb = (std::clamp(s.params[0], 0.0f, 20.0f) - 10.0f) * 0.6f;
                    if (m == 30) hpfHz = 115.0;
                    break;
                case 27: case 28: case 29: // #53 AC, #54 BB, #55 RC Boost: Gain, Vol, Bass, Treble
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    outputDb = (std::clamp(s.params[1], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    bassDb   = (static_cast<double>(std::clamp(s.params[2], 0.0f, 10.0f)) - 5.0) * 2.0;
                    trebleDb = (static_cast<double>(std::clamp(s.params[3], 0.0f, 10.0f)) - 5.0) * 2.0;
                    break;
                case 31: // #57 FAT BB: Bass, Treble, Volume, Low Cut
                    bassDb   = (static_cast<double>(std::clamp(s.params[0], 0.0f, 10.0f)) - 5.0) * 2.0;
                    trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 2.0;
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    if (s.params[3] > 0.5f) hpfHz = 120.0;
                    break;
                case 32: // #58 EP Booster: Gain (0..20 dB), +3dB, Bright
                    inputDb  = (std::clamp(s.params[0], 0.0f, 20.0f) - 8.0f) * 0.6f + (s.params[1] > 0.5f ? 3.0f : 0.0f);
                    outputDb = (std::clamp(s.params[0], 0.0f, 20.0f) - 8.0f) * 0.5f;
                    if (s.params[2] > 0.5f) trebleDb = 3.0;
                    break;
                default: // Standard 3-knob OD / Fuzz / Distortion: Gain, Tone, Volume
                    inputDb  = (std::clamp(s.params[0], 0.0f, 10.0f) - 5.0f) * 2.5f;
                    trebleDb = (static_cast<double>(std::clamp(s.params[1], 0.0f, 10.0f)) - 5.0) * 1.8;
                    outputDb = (std::clamp(s.params[2], 0.0f, 10.0f) - 5.0f) * 2.4f;
                    break;
            }
        }

        const bool useDryBlend = (wetMix < 0.999f && namDryBuf.getNumSamples() >= numSamples && namDryBuf.getNumChannels() >= numChannels);
        if (useDryBlend) {
            for (int ch = 0; ch < numChannels; ++ch) {
                namDryBuf.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            }
        }

        // Optional tight Pre-NAM High-Pass (Precision Drive Attack / FAT BB Low Cut / Fortin Grind)
        if (hpfHz > 20.0) {
            packHpf.setHighPass(namSampleRate, hpfHz, 0.707);
            for (int ch = 0; ch < numChannels; ++ch) {
                float* ptr = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i) {
                    ptr[i] = packHpf.processSample(ch, ptr[i]);
                }
            }
        }

        // Optional built-in Noise Gate (Precision Drive Gate knob)
        if (gateKnob > 0.2f) {
            const float thresh = juce::Decibels::decibelsToGain(-68.0f + gateKnob * 4.2f);
            const float relCoef = std::exp(-1.0f / static_cast<float>(namSampleRate * 0.045));
            for (int i = 0; i < numSamples; ++i) {
                float pk = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch) {
                    pk = std::max(pk, std::abs(buffer.getSample(ch, i)));
                }
                packGateEnv = (pk > packGateEnv) ? pk : (packGateEnv * relCoef + pk * (1.0f - relCoef));
                const float g = (packGateEnv >= thresh) ? 1.0f : std::clamp(packGateEnv / std::max(1e-5f, thresh), 0.0f, 1.0f);
                const float smoothG = g * g;
                for (int ch = 0; ch < numChannels; ++ch) {
                    buffer.getWritePointer(ch)[i] *= smoothG;
                }
            }
        }

        const float inputGain = juce::Decibels::decibelsToGain(std::clamp(inputDb, -18.0f, 18.0f));
        if (!packNamRunner.process(buffer, inputGain)) {
            return;
        }

        const bool useBass   = (std::abs(bassDb) > 0.08);
        const bool useMid    = (std::abs(midDb) > 0.08);
        const bool useTreble = (std::abs(trebleDb) > 0.08);
        const bool useLpf    = (lpfHz > 100.0);

        if (useBass)   packBassEq.setLowShelf(namSampleRate, 140.0, bassDb, 0.8);
        if (useMid)    packMidEq.setPeaking(namSampleRate, midHz, 0.9, midDb);
        if (useTreble) packTrebleEq.setHighShelf(namSampleRate, 2000.0, trebleDb, 0.75);
        if (useLpf)    packLpf.setLowPass(namSampleRate, lpfHz, 0.75);

        const float outputGain = juce::Decibels::decibelsToGain(std::clamp(outputDb, -18.0f, 18.0f));
        const float dryMix = 1.0f - wetMix;

        for (int ch = 0; ch < numChannels; ++ch) {
            float* dst = buffer.getWritePointer(ch);
            const float* dry = useDryBlend ? namDryBuf.getReadPointer(ch) : nullptr;
            for (int i = 0; i < numSamples; ++i) {
                float y = dst[i];
                if (useBass)   y = packBassEq.processSample(ch, y);
                if (useMid)    y = packMidEq.processSample(ch, y);
                if (useTreble) y = packTrebleEq.processSample(ch, y);
                if (useLpf)    y = packLpf.processSample(ch, y);
                y = std::tanh(y * outputGain * 0.96f);
                if (dry != nullptr) {
                    y = dry[i] * dryMix + y * wetMix;
                }
                dst[i] = y;
            }
        }
    }

    // Acoustic Body IR convolution for #11 AC Refiner (blended cleanly via Shape 0..10)
    void processPackIrPedal(juce::AudioBuffer<float>& buffer, const PedalSlotSnapshot& s) noexcept {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min(2, buffer.getNumChannels());
        if (numSamples <= 0 || numChannels <= 0) return;

        const float wet = std::clamp(s.params[0] * 0.1f, 0.0f, 1.0f);
        const bool hasDry = (namDryBuf.getNumSamples() >= numSamples && namDryBuf.getNumChannels() >= numChannels);
        if (hasDry) {
            for (int ch = 0; ch < numChannels; ++ch) {
                namDryBuf.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            }
        }

        juce::dsp::AudioBlock<float> block(buffer);
        auto sub = block.getSubsetChannelBlock(0, static_cast<size_t>(numChannels));
        juce::dsp::ProcessContextReplacing<float> ctx(sub);
        packIrConv.process(ctx);

        if (hasDry && wet < 0.999f) {
            const float dry = 1.0f - wet;
            for (int ch = 0; ch < numChannels; ++ch) {
                float* dst = buffer.getWritePointer(ch);
                const float* dSrc = namDryBuf.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i) {
                    dst[i] = dSrc[i] * dry + dst[i] * wet;
                }
            }
        }
    }

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
            return;
        }

        const bool applyTone = (std::abs(tone - 5.0f) > 0.05f);
        if (applyTone && std::abs(tone - lastNamTone) > 0.03f) {
            lastNamTone = tone;
            const double shelfDb = static_cast<double>((tone - 5.0f) * 1.6f);
            namToneFilter.setHighShelf(namSampleRate, 1800.0, shelfDb, 0.75);
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
    NamModelRunner packNamRunner;
    juce::dsp::Convolution packIrConv;
    BiquadFilter namToneFilter;
    BiquadFilter packHpf, packBassEq, packMidEq, packTrebleEq, packLpf;
    float packGateEnv = 0.0f;

    std::atomic<int> lastActiveCategory { static_cast<int>(ModuleId::Overdrive) };
    std::atomic<bool> preampColorEnabled { false };
    std::atomic<int> loadedPackKey { -1 };
    std::atomic<int> requestedPackKey { -1 };
    std::atomic<uint64_t> observedRegistryVer { 0 };
    std::atomic<bool> packReloadPending { false };
    std::atomic<bool> packNamActive { false };
    std::atomic<bool> packIrActive { false };
    mutable std::mutex packMetaMutex;
    juce::String packIrDisplayName;

    juce::AudioBuffer<float> namDryBuf;
    double namSampleRate = 48000.0;
    float lastNamTone = -1.0f;
};

} // namespace pedalboard
