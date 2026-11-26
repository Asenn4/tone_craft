#pragma once

// ==============================================================================
// TEMPORARY CLEAN GUITAR DI SAMPLE TESTER (guarded by PEDALBOARD_TEMP_DI_TESTER)
// Plays test_audio/Electric_Guitar_Rhythm_Clean.wav or Clean_Guitar_Solo.wav
// directly into the front of SignalChain so all 265 Amps, Cabs & Effects can be
// auditioned without a physical guitar plugged in.
//
// To remove completely later: set PEDALBOARD_TEMP_DI_TESTER=0 in CMakeLists.txt
// and delete src/dsp/TempDiSamplePlayer.h + test_audio/.
// ==============================================================================

#if defined(PEDALBOARD_TEMP_DI_TESTER) && PEDALBOARD_TEMP_DI_TESTER

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

namespace pedalboard {

class TempDiSamplePlayer {
public:
    enum class Track : int {
        Rhythm = 0,
        Solo   = 1
    };

    ~TempDiSamplePlayer() {
        playing.store(false, std::memory_order_release);
        delete pendingTrack.exchange(nullptr, std::memory_order_acq_rel);
        delete activeTrack;
        activeTrack = nullptr;
        delete retiredTrack.exchange(nullptr, std::memory_order_acq_rel);
    }

    void prepare(double newSampleRate) noexcept {
        hostSampleRate.store(std::max(8000.0, newSampleRate), std::memory_order_relaxed);
    }

    void togglePlay() {
        if (playing.load(std::memory_order_acquire)) {
            stop();
        } else {
            startTrack(getSelectedTrack());
        }
    }

    void stop() noexcept {
        playing.store(false, std::memory_order_release);
    }

    void selectTrack(Track t) {
        const int idx = (t == Track::Solo) ? 1 : 0;
        const bool wasPlaying = playing.load(std::memory_order_acquire);
        selectedTrack.store(idx, std::memory_order_release);
        if (wasPlaying) {
            startTrack(t);
        }
    }

    bool startTrack(Track t) {
        reclaimRetired();
        const int idx = (t == Track::Solo) ? 1 : 0;
        selectedTrack.store(idx, std::memory_order_release);

        if (loadedTrackIdx.load(std::memory_order_acquire) == idx && hasLoadedData.load(std::memory_order_acquire)) {
            resetReadPosFlag.store(true, std::memory_order_release);
            playing.store(true, std::memory_order_release);
            return true;
        }

        const auto wavFile = resolveSampleFile(idx);
        if (!wavFile.existsAsFile()) {
            playing.store(false, std::memory_order_release);
            return false;
        }

        juce::WavAudioFormat wavFmt;
        auto stream = wavFile.createInputStream();
        if (stream == nullptr) return false;
        std::unique_ptr<juce::AudioFormatReader> reader(wavFmt.createReaderFor(stream.release(), true));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0) {
            return false;
        }

        auto* td = new TrackData();
        td->sampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;
        const int nFrames = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, 48000ll * 600ll));
        const int nCh = std::min<int>(2, static_cast<int>(reader->numChannels));
        td->buffer.setSize(2, nFrames, false, true, true);
        reader->read(&td->buffer, 0, nFrames, 0, true, nCh > 1);
        if (nCh == 1) {
            td->buffer.copyFrom(1, 0, td->buffer, 0, 0, nFrames);
        }

        // Smooth 8 ms fade-in & fade-out for clickless looping
        const int fadeLen = std::min(nFrames / 4, static_cast<int>(td->sampleRate * 0.008));
        for (int i = 0; i < fadeLen; ++i) {
            const float g = static_cast<float>(i) / static_cast<float>(std::max(1, fadeLen));
            td->buffer.getWritePointer(0)[i] *= g;
            td->buffer.getWritePointer(1)[i] *= g;
            td->buffer.getWritePointer(0)[nFrames - 1 - i] *= g;
            td->buffer.getWritePointer(1)[nFrames - 1 - i] *= g;
        }

        auto* prevPending = pendingTrack.exchange(td, std::memory_order_acq_rel);
        delete prevPending;

        loadedTrackIdx.store(idx, std::memory_order_release);
        hasLoadedData.store(true, std::memory_order_release);
        resetReadPosFlag.store(true, std::memory_order_release);
        playing.store(true, std::memory_order_release);
        return true;
    }

    bool isPlaying() const noexcept { return playing.load(std::memory_order_acquire); }
    Track getSelectedTrack() const noexcept {
        return selectedTrack.load(std::memory_order_acquire) == 1 ? Track::Solo : Track::Rhythm;
    }

    void reclaimRetired() {
        if (auto* old = retiredTrack.exchange(nullptr, std::memory_order_acq_rel)) {
            delete old;
        }
    }

    // Audio-thread safe: replaces input buffer with clean DI guitar loop when playing.
    void injectIfPlaying(juce::AudioBuffer<float>& buffer) noexcept {
        if (auto* incoming = pendingTrack.exchange(nullptr, std::memory_order_acq_rel)) {
            auto* old = activeTrack;
            activeTrack = incoming;
            readPos = 0.0;
            if (old != nullptr) {
                auto* dropped = retiredTrack.exchange(old, std::memory_order_acq_rel);
                juce::ignoreUnused(dropped);
            }
        }

        if (!playing.load(std::memory_order_acquire) || activeTrack == nullptr) {
            return;
        }

        if (resetReadPosFlag.exchange(false, std::memory_order_acq_rel)) {
            readPos = 0.0;
        }

        const int totalFrames = activeTrack->buffer.getNumSamples();
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (totalFrames < 2 || numSamples <= 0 || numChannels <= 0) {
            return;
        }

        const double step = activeTrack->sampleRate / hostSampleRate.load(std::memory_order_relaxed);
        const float* srcL = activeTrack->buffer.getReadPointer(0);
        const float* srcR = activeTrack->buffer.getReadPointer(1);
        float* dstL = buffer.getWritePointer(0);
        float* dstR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i) {
            const int i0 = static_cast<int>(readPos) % totalFrames;
            const int i1 = (i0 + 1) % totalFrames;
            const float frac = static_cast<float>(readPos - std::floor(readPos));

            const float sL = srcL[i0] + (srcL[i1] - srcL[i0]) * frac;
            const float sR = srcR[i0] + (srcR[i1] - srcR[i0]) * frac;

            dstL[i] = sL;
            if (dstR != nullptr) {
                dstR[i] = sR;
            }

            readPos += step;
            if (readPos >= static_cast<double>(totalFrames)) {
                readPos -= static_cast<double>(totalFrames);
            }
        }
    }

private:
    struct TrackData {
        double sampleRate = 48000.0;
        juce::AudioBuffer<float> buffer;
    };

    static juce::File resolveSampleFile(int idx) {
        const juce::String fileName = (idx == 1)
            ? "Clean_Guitar_Solo.wav"
            : "Electric_Guitar_Rhythm_Clean.wav";

        // 1. Walk up from executable / VST3 DLL location to find workspace test_audio/
        auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        for (int depth = 0; depth < 8; ++depth) {
            auto cand = dir.getChildFile("test_audio").getChildFile(fileName);
            if (cand.existsAsFile()) return cand;
            auto parent = dir.getParentDirectory();
            if (parent == dir) break;
            dir = parent;
        }

        // 2. Current working directory
        auto cwdCand = juce::File::getCurrentWorkingDirectory().getChildFile("test_audio").getChildFile(fileName);
        if (cwdCand.existsAsFile()) return cwdCand;

        // 3. Compile-time workspace source directory fallback (works even if VST3 is installed in Common Files!)
#if defined(PEDALBOARD_TEMP_DI_DIR)
        auto devCand = juce::File(PEDALBOARD_TEMP_DI_DIR).getChildFile(fileName);
        if (devCand.existsAsFile()) return devCand;
#endif
        return {};
    }

    TrackData* activeTrack = nullptr;
    std::atomic<TrackData*> pendingTrack { nullptr };
    std::atomic<TrackData*> retiredTrack { nullptr };

    std::atomic<double> hostSampleRate { 48000.0 };
    std::atomic<bool> playing { false };
    std::atomic<int> selectedTrack { 0 };
    std::atomic<int> loadedTrackIdx { -1 };
    std::atomic<bool> hasLoadedData { false };
    std::atomic<bool> resetReadPosFlag { false };
    double readPos = 0.0;
};

} // namespace pedalboard

#endif // PEDALBOARD_TEMP_DI_TESTER
