#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace pedalboard {

struct TunerReading {
    bool valid = false;
    float frequencyHz = 0.0f;
    float cents = 0.0f;
    int midiNote = 0;
    juce::String noteName { "--" };
};

// Lock-free audio-to-UI chromatic guitar tuner using YIN pitch detection on the UI thread.
class ChromaticTuner {
public:
    static constexpr uint32_t kRingSize = 4096;
    static constexpr uint32_t kRingMask = kRingSize - 1;
    static constexpr int kAnalysisWindow = 2048;

    void prepare(double newSampleRate) noexcept {
        sampleRate.store(newSampleRate > 0.0 ? newSampleRate : 44100.0, std::memory_order_relaxed);
        ring.fill(0.0f);
        writeIndex.store(0, std::memory_order_release);
    }

    // Called from real-time audio thread: strictly wait-free and allocation-free.
    void pushBlock(const juce::AudioBuffer<float>& buffer) noexcept {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0) {
            return;
        }

        const float* ch0 = buffer.getReadPointer(0);
        const float* ch1 = numChannels > 1 ? buffer.getReadPointer(1) : nullptr;
        uint32_t wp = writeIndex.load(std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i) {
            const float mono = ch1 != nullptr ? 0.5f * (ch0[i] + ch1[i]) : ch0[i];
            ring[wp & kRingMask] = mono;
            ++wp;
        }
        writeIndex.store(wp, std::memory_order_release);
    }

    void setReferenceHz(float hz) noexcept {
        a4ReferenceHz.store(std::clamp(hz, 415.0f, 465.0f), std::memory_order_relaxed);
    }

    float getReferenceHz() const noexcept {
        return a4ReferenceHz.load(std::memory_order_relaxed);
    }

    // Called from UI timer thread (~25-30 Hz).
    TunerReading analyze() const {
        TunerReading reading;
        const double sr = sampleRate.load(std::memory_order_relaxed);
        if (sr < 8000.0) {
            return reading;
        }

        std::array<float, kAnalysisWindow> x {};
        const uint32_t wp = writeIndex.load(std::memory_order_acquire);
        const uint32_t start = wp - static_cast<uint32_t>(kAnalysisWindow);

        double energy = 0.0;
        for (int i = 0; i < kAnalysisWindow; ++i) {
            const float s = ring[(start + static_cast<uint32_t>(i)) & kRingMask];
            x[static_cast<size_t>(i)] = s;
            energy += static_cast<double>(s) * static_cast<double>(s);
        }

        const double rms = std::sqrt(energy / static_cast<double>(kAnalysisWindow));
        if (rms < 0.0025) { // Below ~ -52 dBFS
            return reading;
        }

        const int minLag = std::max(16, static_cast<int>(sr / 1200.0));
        const int maxLag = std::min(kAnalysisWindow / 2 - 2, static_cast<int>(sr / 55.0));
        if (maxLag <= minLag + 2) {
            return reading;
        }

        std::array<float, kAnalysisWindow / 2> diff {};
        std::array<float, kAnalysisWindow / 2> cmnd {};
        cmnd[0] = 1.0f;

        float runningSum = 0.0f;
        for (int tau = 1; tau <= maxLag; ++tau) {
            float sum = 0.0f;
            const int limit = kAnalysisWindow - maxLag;
            for (int j = 0; j < limit; ++j) {
                const float d = x[static_cast<size_t>(j)] - x[static_cast<size_t>(j + tau)];
                sum += d * d;
            }
            diff[static_cast<size_t>(tau)] = sum;
            runningSum += sum;
            cmnd[static_cast<size_t>(tau)] = (runningSum > 1e-12f) ? (sum * static_cast<float>(tau) / runningSum) : 1.0f;
        }

        // Find first local minimum below YIN threshold
        constexpr float kThreshold = 0.16f;
        int bestTau = -1;
        for (int tau = minLag; tau < maxLag; ++tau) {
            if (cmnd[static_cast<size_t>(tau)] < kThreshold) {
                while (tau + 1 < maxLag && cmnd[static_cast<size_t>(tau + 1)] < cmnd[static_cast<size_t>(tau)]) {
                    ++tau;
                }
                bestTau = tau;
                break;
            }
        }

        if (bestTau < minLag) {
            return reading;
        }

        // Parabolic interpolation around bestTau
        const float s0 = cmnd[static_cast<size_t>(bestTau - 1)];
        const float s1 = cmnd[static_cast<size_t>(bestTau)];
        const float s2 = cmnd[static_cast<size_t>(bestTau + 1)];
        const float denom = 2.0f * (2.0f * s1 - s2 - s0);
        float refinedTau = static_cast<float>(bestTau);
        if (std::abs(denom) > 1e-9f) {
            refinedTau += std::clamp((s2 - s0) / denom, -0.5f, 0.5f);
        }

        const float freq = static_cast<float>(sr / static_cast<double>(refinedTau));
        if (freq < 50.0f || freq > 1300.0f) {
            return reading;
        }

        const float refHz = std::clamp(a4ReferenceHz.load(std::memory_order_relaxed), 415.0f, 465.0f);
        const float midiFloat = 69.0f + 12.0f * std::log2(freq / refHz);
        const int nearestMidi = static_cast<int>(std::lround(midiFloat));
        const float cents = (midiFloat - static_cast<float>(nearestMidi)) * 100.0f;

        static constexpr const char* kNoteNames[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const int noteIdx = ((nearestMidi % 12) + 12) % 12;
        const int octave = (nearestMidi / 12) - 1;

        reading.valid = true;
        reading.frequencyHz = freq;
        reading.midiNote = nearestMidi;
        reading.cents = std::clamp(cents, -50.0f, 50.0f);
        reading.noteName = juce::String(kNoteNames[noteIdx]) + juce::String(octave);
        return reading;
    }

private:
    std::array<float, kRingSize> ring {};
    std::atomic<uint32_t> writeIndex { 0 };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<float> a4ReferenceHz { 440.0f };
};

} // namespace pedalboard
