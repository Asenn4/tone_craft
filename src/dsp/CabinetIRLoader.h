#pragma once

#include "../engine/Gp200Catalogue.h"
#include "BiquadFilter.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 CAB Module (All 71 Cabinet Models from Daftar_Effect_GP-200.xlsx rows 132..202):
// - 0..37:  Vintage & Boutique 1x6", 1x8", 1x10", 2x10", 4x10", 1x12", 2x12", and Marshall/VOX/Mesa 4x12" Cabs
// - 38..49: Modern High-Gain 4x12" Cabs (EV 6505, Bogner, ENGL, Uberkab, Soldano, Orange PPC412, Hiwatt, Diezel, Road King)
// - 50..59: Bass Cabs (Eden 1x15"/4x10", SWR, Mesa 2x10", MarkBass, Ampeg PF-115 / SVT-410 / SVT-810, Hartke)
// - 60..69: Acoustic Body IR Simulations (Dreadnought AC/AC Dream, OM, Jumbo, H-Bird, GA, Classic, Mandolin, Fretless/Double Bass)
// - 70:     User IR 1~20 (.wav / .aiff custom IR loader)
class CabinetIRLoader {
public:
    enum class BuiltInVoicing {
        Closed4x12 = 0,
        Open2x12   = 1
    };

    struct Params {
        bool bypass = false;
        int modelIndex = 14; // Default 14 = UK LD (Marshall 1960AV 4x12") for Cab A, 7 = Dark Twin 2x12" for Cab B
        float lowCutHz = 80.0f;
        float highCutHz = 7500.0f;
        float levelDb = 0.0f;
    };

    explicit CabinetIRLoader(BuiltInVoicing defaultVoicing = BuiltInVoicing::Closed4x12)
        : voicing(defaultVoicing),
          activeModelIndex(defaultVoicing == BuiltInVoicing::Closed4x12 ? 14 : 7) {
        const auto& entry = gp200::getModelEntry(ModuleId::CabinetIR, activeModelIndex);
        displayName = juce::String(entry.name) + " (" + entry.subType + ")";
    }

    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = std::max(8000.0, spec.sampleRate);
        convolution.prepare(spec);
        lastLowCut = lastHighCut = -1.0f;
        updateFilters(80.0f, 7500.0f);

        juce::String existingPath;
        {
            std::lock_guard<std::mutex> lock(pathMutex);
            existingPath = customIrPath;
        }

        if (existingPath.isNotEmpty() && juce::File(existingPath).existsAsFile()) {
            loadCustomIR(juce::File(existingPath));
        } else {
            loadBuiltInSyntheticCab(activeModelIndex);
        }
    }

    void reset() noexcept {
        convolution.reset();
        lowCutFilter.reset();
        highCutFilter.reset();
    }

    bool loadCustomIR(const juce::File& file) {
        if (!file.existsAsFile()) {
            return false;
        }
        convolution.loadImpulseResponse(
            file,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            0,
            juce::dsp::Convolution::Normalise::yes
        );
        activeModelIndex = 70;
        usingCustomIr.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lock(pathMutex);
        customIrPath = file.getFullPathName();
        customIrName = file.getFileNameWithoutExtension();
        displayName = "User IR: " + customIrName;
        return true;
    }

    bool reloadSavedCustomIR() {
        juce::String saved;
        {
            std::lock_guard<std::mutex> lock(pathMutex);
            saved = customIrPath;
        }
        if (saved.isNotEmpty() && juce::File(saved).existsAsFile()) {
            return loadCustomIR(juce::File(saved));
        }
        activeModelIndex = 70;
        usingCustomIr.store(false, std::memory_order_release);
        return false;
    }

    void clearCustomIR(int fallbackFactoryModel = -1) {
        {
            std::lock_guard<std::mutex> lock(pathMutex);
            customIrPath.clear();
            customIrName.clear();
        }
        usingCustomIr.store(false, std::memory_order_release);
        loadBuiltInSyntheticCab(fallbackFactoryModel, false);
    }

    void loadBuiltInSyntheticCab(int modelIdx = -1, bool preserveCustomIrPath = false) {
        if (modelIdx < 0 || modelIdx >= 70) {
            modelIdx = (voicing == BuiltInVoicing::Closed4x12) ? 14 : 7;
        }
        modelIdx = std::clamp(modelIdx, 0, 69);
        activeModelIndex = modelIdx;
        usingCustomIr.store(false, std::memory_order_release);
        const auto& entry = gp200::getModelEntry(ModuleId::CabinetIR, modelIdx);
        const juce::String sub(entry.subType);

        constexpr int kIrLength = 1024;
        const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        juce::AudioBuffer<float> ir(2, kIrLength);
        ir.clear();

        ir.setSample(0, 0, 1.0f);
        ir.setSample(1, 0, 1.0f);

        // Determine physical enclosure geometry from GP-200 cabinet type
        const bool isAcoustic = sub.containsIgnoreCase("Acoustic");
        const bool isBass     = sub.containsIgnoreCase("bass");
        const bool isSmall    = sub.containsIgnoreCase("1 x 6") || sub.containsIgnoreCase("1 x 8") || sub.containsIgnoreCase("1 x 10");
        const bool is4x12     = sub.containsIgnoreCase("4 x 12") || sub.containsIgnoreCase("8 x 10");

        // Early cabinet panel/back reflection taps
        if constexpr (kIrLength > 140) {
            const int tap1L = isSmall ? 19 : (is4x12 ? 39 : 29) + (modelIdx % 7);
            const int tap1R = tap1L + 11;
            const int tap2L = isAcoustic ? 63 : (83 + (modelIdx % 13));
            const int tap2R = tap2L + 14;
            const float rGain = isAcoustic ? 0.14f : (is4x12 ? 0.07f : 0.095f);
            ir.setSample(0, tap1L, rGain);
            ir.setSample(1, tap1R, rGain);
            ir.setSample(0, tap2L, -rGain * 0.6f);
            ir.setSample(1, tap2R, -rGain * 0.6f);
        }

        // Deterministic per-model speaker voicing parameters
        const double variant = static_cast<double>((modelIdx % 9) - 4);
        double hpfHz   = 88.0 + variant * 2.5;
        double resHz   = 128.0 + variant * 4.5;
        double resDb   = 3.8;
        double scoopHz = 440.0 + variant * 18.0;
        double scoopDb = -2.4;
        double biteHz  = 2450.0 + variant * 95.0;
        double biteDb  = 3.6;
        double lpfHz   = 5600.0 + variant * 160.0;

        if (isSmall) {
            // 1x6" Supro / 1x8" Champ / 1x10" Princeton: boxy vintage midrange, tighter low roll-off
            hpfHz = 135.0 + variant * 4.0;
            resHz = 210.0 + variant * 6.0;
            resDb = 2.6;
            scoopDb = -0.8;
            biteHz = 2150.0 + variant * 80.0;
            lpfHz = 5100.0;
        } else if (isBass) {
            // 1x15" / 4x10" / 8x10" Bass Cabs: deep 48 Hz fundamental thump + punchy 1.8 kHz tweeter/cone detail
            hpfHz = 46.0 + variant * 1.5;
            resHz = 82.0 + variant * 3.0;
            resDb = 4.6;
            scoopHz = 320.0;
            scoopDb = -2.0;
            biteHz = 1850.0 + variant * 70.0;
            biteDb = 2.8;
            lpfHz = 6400.0;
        } else if (isAcoustic) {
            // Acoustic Dreadnought / OM / Jumbo / Mandolin / Double Bass body resonance IR
            hpfHz = (modelIdx >= 68) ? 42.0 : 72.0;
            resHz = (modelIdx == 67) ? 310.0 : 178.0 + variant * 8.0; // Mandolin higher body resonance
            resDb = 5.4;
            scoopHz = 520.0;
            scoopDb = -3.8;
            biteHz = 4600.0;
            biteDb = 5.0;
            lpfHz = 13500.0;
        } else if (!is4x12) {
            // 1x12" & 2x12" Open-Back (Deluxe, Twin, AC30, Matchless, Lonestar, Two-Rock)
            hpfHz = 98.0 + variant * 2.5;
            resHz = 152.0 + variant * 5.0;
            resDb = 2.7;
            scoopHz = 560.0 + variant * 15.0;
            scoopDb = -1.4;
            biteHz = 2780.0 + variant * 90.0;
            biteDb = 4.0;
            lpfHz = 6300.0 + variant * 150.0;
        }

        BiquadFilter hpf, cabRes, scoop, bite, notch, lpf1, lpf2;
        hpf.setHighPass(sr, hpfHz, 0.82);
        cabRes.setPeaking(sr, resHz, 1.65, resDb);
        scoop.setPeaking(sr, scoopHz, 1.1, scoopDb);
        bite.setPeaking(sr, biteHz, 1.3, biteDb);
        notch.setPeaking(sr, isAcoustic ? 2400.0 : (4200.0 + variant * 110.0), 3.0, isAcoustic ? -2.0 : -5.2);
        lpf1.setLowPass(sr, lpfHz, 0.78);
        lpf2.setLowPass(sr, std::min(16000.0, lpfHz * 1.16), 0.707);

        for (int ch = 0; ch < 2; ++ch) {
            float* ptr = ir.getWritePointer(ch);
            for (int i = 0; i < kIrLength; ++i) {
                float s = ptr[i];
                s = hpf.processSample(ch, s);
                s = cabRes.processSample(ch, s);
                s = scoop.processSample(ch, s);
                s = bite.processSample(ch, s);
                s = notch.processSample(ch, s);
                s = lpf1.processSample(ch, s);
                s = lpf2.processSample(ch, s);
                if (i > kIrLength - 128) {
                    const float t = static_cast<float>(kIrLength - 1 - i) / 128.0f;
                    s *= t;
                }
                ptr[i] = s;
            }
        }

        convolution.loadImpulseResponse(
            std::move(ir),
            sr,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::yes
        );

        std::lock_guard<std::mutex> lock(pathMutex);
        if (!preserveCustomIrPath) {
            customIrPath.clear();
            customIrName.clear();
        }
        displayName = juce::String(entry.name) + "  |  " + entry.description;
    }

    int getActiveModelIndex() const noexcept { return activeModelIndex; }
    bool isUsingCustomIr() const noexcept { return usingCustomIr.load(std::memory_order_acquire); }

    bool hasCustomIrFile() const {
        std::lock_guard<std::mutex> lock(pathMutex);
        return customIrPath.isNotEmpty();
    }

    juce::String getCustomIrName() const {
        std::lock_guard<std::mutex> lock(pathMutex);
        return customIrName.isNotEmpty() ? customIrName : juce::String("No User IR loaded");
    }

    juce::String getDisplayName() const {
        std::lock_guard<std::mutex> lock(pathMutex);
        return displayName;
    }

    juce::String getCustomIrPath() const {
        std::lock_guard<std::mutex> lock(pathMutex);
        return customIrPath;
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

        // If user switched GP-200 factory cabinet model (0..69), re-voice synthetic IR while preserving any saved custom IR for instant A/B!
        const int targetModel = std::clamp(p.modelIndex, 0, static_cast<int>(gp200::kCabModels.size()) - 1);
        if (targetModel < 70 && (targetModel != activeModelIndex || usingCustomIr.load(std::memory_order_relaxed))) {
            loadBuiltInSyntheticCab(targetModel, true);
        }

        if (std::abs(p.lowCutHz - lastLowCut) > 0.5f || std::abs(p.highCutHz - lastHighCut) > 1.0f) {
            updateFilters(p.lowCutHz, p.highCutHz);
        }

        // In User IR slot (70) with no custom IR file loaded yet, pass audio cleanly through HPF/LPF/Volume without stale synthetic cab coloration
        if (targetModel < 70 || usingCustomIr.load(std::memory_order_relaxed)) {
            juce::dsp::AudioBlock<float> block(buffer);
            auto subBlock = block.getSubsetChannelBlock(0, static_cast<size_t>(numChannels));
            juce::dsp::ProcessContextReplacing<float> context(subBlock);
            convolution.process(context);
        }

        const float gain = juce::Decibels::decibelsToGain(p.levelDb);
        for (int ch = 0; ch < numChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                float s = lowCutFilter.processSample(ch, data[i]);
                s = highCutFilter.processSample(ch, s);
                data[i] = s * gain;
            }
        }
    }

private:
    void updateFilters(float lowHz, float highHz) noexcept {
        lastLowCut = lowHz;
        lastHighCut = highHz;
        lowCutFilter.setHighPass(sampleRate, std::clamp(static_cast<double>(lowHz), 30.0, 350.0), 0.707);
        highCutFilter.setLowPass(sampleRate, std::clamp(static_cast<double>(highHz), 2000.0, 18000.0), 0.707);
    }

    BuiltInVoicing voicing = BuiltInVoicing::Closed4x12;
    int activeModelIndex = 14;
    double sampleRate = 44100.0;
    float lastLowCut = -1.0f;
    float lastHighCut = -1.0f;

    juce::dsp::Convolution convolution;
    BiquadFilter lowCutFilter;
    BiquadFilter highCutFilter;

    mutable std::mutex pathMutex;
    std::atomic<bool> usingCustomIr { false };
    juce::String customIrPath;
    juce::String customIrName;
    juce::String displayName;
};

} // namespace pedalboard
