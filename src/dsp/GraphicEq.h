#pragma once

#include "BiquadFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace pedalboard {

// GP-200 EQ Module (All 6 Models from Daftar_Effect_GP-200.xlsx rows 203..208):
// - 0: Guitar EQ 1 (5 bands: 125Hz, 400Hz, 800Hz, 1.6kHz, 4kHz + Volume)
// - 1: Guitar EQ 2 (5 bands: 100Hz, 500Hz, 1kHz, 3kHz, 6kHz + Volume)
// - 2: Bass EQ 1   (5 bands: 33Hz, 150Hz, 600Hz, 2kHz, 8kHz + Volume)
// - 3: Bass EQ 2   (5 bands: 50Hz, 120Hz, 400Hz, 800Hz, 4.5kHz + Volume)
// - 4: Mess EQ     (5 bands: 80Hz, 240Hz, 750Hz, 2.2kHz, 6.6kHz — classic Mesa Mark V-EQ)
// - 5: Hyper EQ    (10 bands: 31, 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz + Volume)
class GraphicEq {
public:
    static constexpr int kNumBands = 7;   // Legacy APVTS band count
    static constexpr int kMaxBands = 10;  // Full 10-band Hyper EQ support

    struct Params {
        bool bypass = false;
        int modelIndex = 0; // 0..5 (kEqModels)
        std::array<float, kMaxBands> bandsDb {};
        int activeBandCount = 5;
        float levelDb = 0.0f;
    };

    static int bandCountForModel(int modelIdx) noexcept {
        return (std::clamp(modelIdx, 0, 5) == 5) ? 10 : 5;
    }

    static bool hasVolumeForModel(int modelIdx) noexcept {
        return (std::clamp(modelIdx, 0, 5) != 4); // Mess EQ has 5 bands only
    }

    static double bandFreqForModel(int modelIdx, int bandIdx) noexcept {
        static constexpr double kFreqs[6][kMaxBands] = {
            { 125.0, 400.0, 800.0, 1600.0, 4000.0,   0.0,    0.0,    0.0,    0.0,     0.0 }, // 0: Guitar EQ 1
            { 100.0, 500.0, 1000.0, 3000.0, 6000.0,  0.0,    0.0,    0.0,    0.0,     0.0 }, // 1: Guitar EQ 2
            {  33.0, 150.0, 600.0, 2000.0, 8000.0,   0.0,    0.0,    0.0,    0.0,     0.0 }, // 2: Bass EQ 1
            {  50.0, 120.0, 400.0, 800.0, 4500.0,    0.0,    0.0,    0.0,    0.0,     0.0 }, // 3: Bass EQ 2
            {  80.0, 240.0, 750.0, 2200.0, 6600.0,   0.0,    0.0,    0.0,    0.0,     0.0 }, // 4: Mess EQ
            {  31.0,  63.0, 125.0, 250.0, 500.0,  1000.0, 2000.0, 4000.0, 8000.0, 16000.0 }  // 5: Hyper EQ
        };
        const int m = std::clamp(modelIdx, 0, 5);
        const int b = std::clamp(bandIdx, 0, kMaxBands - 1);
        return kFreqs[m][b];
    }

    static const char* bandLabelForModel(int modelIdx, int bandIdx) noexcept {
        static constexpr const char* kLabels[6][kMaxBands] = {
            { "125", "400", "800", "1.6k", "4k",   "",   "",   "",   "",   ""   },
            { "100", "500", "1k",  "3k",   "6k",   "",   "",   "",   "",   ""   },
            { "33",  "150", "600", "2k",   "8k",   "",   "",   "",   "",   ""   },
            { "50",  "120", "400", "800",  "4.5k", "",   "",   "",   "",   ""   },
            { "80",  "240", "750", "2.2k", "6.6k", "",   "",   "",   "",   ""   },
            { "31",  "63",  "125", "250",  "500",  "1k", "2k", "4k", "8k", "16k"}
        };
        const int m = std::clamp(modelIdx, 0, 5);
        const int b = std::clamp(bandIdx, 0, kMaxBands - 1);
        return kLabels[m][b];
    }

    void prepare(const juce::dsp::ProcessSpec& spec) noexcept {
        sampleRate = std::max(8000.0, spec.sampleRate);
        lastModel = -1;
        cachedDb.fill(999.0f);
        reset();
    }

    void reset() noexcept {
        for (auto& f : filters) {
            f.reset();
        }
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

        const int m = std::clamp(p.modelIndex, 0, 5);
        const int count = bandCountForModel(m);
        const bool modelChanged = (m != lastModel);
        lastModel = m;
        const double qVal = (m == 4) ? 1.65 : ((m == 5) ? 1.85 : 1.40);

        for (int b = 0; b < count; ++b) {
            const float gDb = std::clamp(p.bandsDb[static_cast<size_t>(b)], -12.0f, 12.0f);
            if (modelChanged || std::abs(gDb - cachedDb[static_cast<size_t>(b)]) > 1e-3f) {
                cachedDb[static_cast<size_t>(b)] = gDb;
                const double hz = std::min(bandFreqForModel(m, b), sampleRate * 0.44);
                if (b == 0) {
                    filters[0].setLowShelf(sampleRate, hz, gDb, 0.9);
                } else if (b == count - 1) {
                    filters[static_cast<size_t>(b)].setHighShelf(sampleRate, hz, gDb, 0.9);
                } else {
                    filters[static_cast<size_t>(b)].setPeaking(sampleRate, hz, qVal, gDb);
                }
            }
        }

        const float outGain = hasVolumeForModel(m) ? juce::Decibels::decibelsToGain(p.levelDb) : 1.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                float s = data[i];
                for (int b = 0; b < count; ++b) {
                    s = filters[static_cast<size_t>(b)].processSample(ch, s);
                }
                data[i] = s * outGain;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    int lastModel = -1;
    std::array<BiquadFilter, kMaxBands> filters;
    std::array<float, kMaxBands> cachedDb { 999.0f, 999.0f, 999.0f, 999.0f, 999.0f, 999.0f, 999.0f, 999.0f, 999.0f, 999.0f };
};

} // namespace pedalboard
