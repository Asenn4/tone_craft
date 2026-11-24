#pragma once

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

enum class LooperState : int {
    Empty       = 0,
    Recording   = 1,
    Playing     = 2,
    Overdubbing = 3,
    Stopped     = 4
};

// Real-time safe stereo guitar looper (up to 45 seconds) with 1-level Undo,
// Pre-DI / Post-FX placement, and live waveform overview for native UI rendering.
class StereoLooper {
public:
    static constexpr int kMaxSeconds = 45;
    static constexpr int kWaveformBins = 96;

    void prepare(double newSampleRate) {
        const double sr = std::max(8000.0, newSampleRate);
        sampleRate.store(sr, std::memory_order_relaxed);
        const int targetCap = static_cast<int>(sr * static_cast<double>(kMaxSeconds));
        if (maxSamples != targetCap) {
            maxSamples = targetCap;
            loopBuf.setSize(2, maxSamples, false, true, false);
            undoBuf.setSize(2, maxSamples, false, true, false);
        }
        clear();
    }

    void clear() noexcept {
        pendingCommand.store(kCmdClear, std::memory_order_release);
    }

    void triggerRecPlayOverdub() noexcept {
        pendingCommand.store(kCmdRecPlayDub, std::memory_order_release);
    }

    void triggerStopToggle() noexcept {
        pendingCommand.store(kCmdStopToggle, std::memory_order_release);
    }

    void triggerUndo() noexcept {
        pendingCommand.store(kCmdUndo, std::memory_order_release);
    }

    void setLoopLevelDb(float db) noexcept {
        loopGain.store(juce::Decibels::decibelsToGain(std::clamp(db, -36.0f, 12.0f)), std::memory_order_relaxed);
    }

    void setPreFxMode(bool preFx) noexcept {
        preFxMode.store(preFx, std::memory_order_release);
    }

    bool isPreFxMode() const noexcept {
        return preFxMode.load(std::memory_order_acquire);
    }

    LooperState getState() const noexcept {
        return static_cast<LooperState>(state.load(std::memory_order_acquire));
    }

    bool canUndo() const noexcept {
        return undoAvailable.load(std::memory_order_acquire);
    }

    float getProgress() const noexcept {
        const int len = loopLength.load(std::memory_order_acquire);
        if (len <= 0) return 0.0f;
        const int pos = playhead.load(std::memory_order_relaxed);
        return std::clamp(static_cast<float>(pos) / static_cast<float>(len), 0.0f, 1.0f);
    }

    double getLoopSeconds() const noexcept {
        const int len = loopLength.load(std::memory_order_acquire);
        const double sr = sampleRate.load(std::memory_order_relaxed);
        return (sr > 0.0) ? static_cast<double>(len) / sr : 0.0;
    }

    void getWaveformPreview(std::array<float, kWaveformBins>& outBins) const noexcept {
        for (int i = 0; i < kWaveformBins; ++i) {
            outBins[static_cast<size_t>(i)] = waveformPeaks[static_cast<size_t>(i)].load(std::memory_order_relaxed);
        }
    }

    // Strictly real-time safe: zero locks, zero heap allocations.
    void process(juce::AudioBuffer<float>& buffer) noexcept {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0 || maxSamples <= 0) {
            return;
        }

        handlePendingCommands();

        auto curState = static_cast<LooperState>(state.load(std::memory_order_relaxed));
        if (curState == LooperState::Empty || curState == LooperState::Stopped) {
            return;
        }

        float* chL = buffer.getWritePointer(0);
        float* chR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
        float* loopL = loopBuf.getWritePointer(0);
        float* loopR = loopBuf.getWritePointer(1);
        float* undoL = undoBuf.getWritePointer(0);
        float* undoR = undoBuf.getWritePointer(1);
        const float gain = loopGain.load(std::memory_order_relaxed);

        int pos = playhead.load(std::memory_order_relaxed);
        int len = loopLength.load(std::memory_order_relaxed);

        if (curState == LooperState::Recording) {
            for (int i = 0; i < numSamples; ++i) {
                const float inL = chL[i];
                const float inR = (chR != nullptr) ? chR[i] : inL;
                if (pos < maxSamples) {
                    loopL[pos] = inL;
                    loopR[pos] = inR;
                    ++pos;
                    len = pos;
                } else {
                    // Max capacity reached -> transition cleanly to Playing
                    len = maxSamples;
                    pos = 0;
                    curState = LooperState::Playing;
                    state.store(static_cast<int>(curState), std::memory_order_release);
                    rebuildWaveformPreview(len);
                    break;
                }
            }
            playhead.store(pos, std::memory_order_relaxed);
            loopLength.store(len, std::memory_order_release);
            // Update live recording peak into current bin
            if (len > 0) {
                const int bin = std::clamp((len * kWaveformBins) / std::max(1, maxSamples), 0, kWaveformBins - 1);
                const float pk = std::max(buffer.getMagnitude(0, 0, numSamples),
                                          numChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : 0.0f);
                const float prev = waveformPeaks[static_cast<size_t>(bin)].load(std::memory_order_relaxed);
                waveformPeaks[static_cast<size_t>(bin)].store(std::max(prev, pk), std::memory_order_relaxed);
            }
            return;
        }

        if (len <= 0) {
            return;
        }

        if (curState == LooperState::Playing) {
            for (int i = 0; i < numSamples; ++i) {
                chL[i] += loopL[pos] * gain;
                if (chR != nullptr) {
                    chR[i] += loopR[pos] * gain;
                }
                if (++pos >= len) {
                    pos = 0;
                }
            }
            playhead.store(pos, std::memory_order_relaxed);
        } else if (curState == LooperState::Overdubbing) {
            for (int i = 0; i < numSamples; ++i) {
                const float inL = chL[i];
                const float inR = (chR != nullptr) ? chR[i] : inL;

                // Incrementally snapshot previous layer into undoBuf once per position
                if (overdubCopiedSamples < len) {
                    undoL[pos] = loopL[pos];
                    undoR[pos] = loopR[pos];
                    ++overdubCopiedSamples;
                }

                const float prevL = loopL[pos];
                const float prevR = loopR[pos];
                loopL[pos] = std::clamp(prevL + inL, -1.5f, 1.5f);
                loopR[pos] = std::clamp(prevR + inR, -1.5f, 1.5f);

                chL[i] = inL + prevL * gain;
                if (chR != nullptr) {
                    chR[i] = inR + prevR * gain;
                }

                // Update preview bin peak
                const int bin = std::clamp((pos * kWaveformBins) / len, 0, kWaveformBins - 1);
                const float pk = std::max(std::abs(loopL[pos]), std::abs(loopR[pos]));
                const float prevPk = waveformPeaks[static_cast<size_t>(bin)].load(std::memory_order_relaxed);
                if (pk > prevPk) {
                    waveformPeaks[static_cast<size_t>(bin)].store(pk, std::memory_order_relaxed);
                }

                if (++pos >= len) {
                    pos = 0;
                }
            }
            playhead.store(pos, std::memory_order_relaxed);
            if (overdubCopiedSamples >= len) {
                undoAvailable.store(true, std::memory_order_release);
            }
        }
    }

private:
    static constexpr int kCmdNone       = 0;
    static constexpr int kCmdRecPlayDub = 1;
    static constexpr int kCmdStopToggle = 2;
    static constexpr int kCmdUndo       = 3;
    static constexpr int kCmdClear      = 4;

    void handlePendingCommands() noexcept {
        const int cmd = pendingCommand.exchange(kCmdNone, std::memory_order_acq_rel);
        if (cmd == kCmdNone) {
            return;
        }

        auto cur = static_cast<LooperState>(state.load(std::memory_order_relaxed));
        int len = loopLength.load(std::memory_order_relaxed);

        if (cmd == kCmdClear) {
            state.store(static_cast<int>(LooperState::Empty), std::memory_order_release);
            loopLength.store(0, std::memory_order_release);
            playhead.store(0, std::memory_order_relaxed);
            undoAvailable.store(false, std::memory_order_release);
            overdubCopiedSamples = 0;
            for (auto& w : waveformPeaks) {
                w.store(0.0f, std::memory_order_relaxed);
            }
            return;
        }

        if (cmd == kCmdRecPlayDub) {
            if (cur == LooperState::Empty) {
                for (auto& w : waveformPeaks) w.store(0.0f, std::memory_order_relaxed);
                loopLength.store(0, std::memory_order_release);
                playhead.store(0, std::memory_order_relaxed);
                undoAvailable.store(false, std::memory_order_release);
                state.store(static_cast<int>(LooperState::Recording), std::memory_order_release);
            } else if (cur == LooperState::Recording) {
                if (len > 256) {
                    // Apply tiny 4 ms crossfade at loop seam for click-free looping
                    applyLoopSeamFade(len);
                    rebuildWaveformPreview(len);
                    playhead.store(0, std::memory_order_relaxed);
                    state.store(static_cast<int>(LooperState::Playing), std::memory_order_release);
                } else {
                    state.store(static_cast<int>(LooperState::Empty), std::memory_order_release);
                    loopLength.store(0, std::memory_order_release);
                }
            } else if (cur == LooperState::Playing || cur == LooperState::Stopped) {
                // Prepare 1-level undo copy incrementally
                overdubCopiedSamples = 0;
                undoAvailable.store(false, std::memory_order_release);
                // Snapshot existing loop into undoBuf so even partial overdubs can be undone safely
                if (len > 0 && len <= maxSamples) {
                    undoBuf.copyFrom(0, 0, loopBuf, 0, 0, len);
                    undoBuf.copyFrom(1, 0, loopBuf, 1, 0, len);
                    overdubCopiedSamples = len;
                    undoAvailable.store(true, std::memory_order_release);
                }
                state.store(static_cast<int>(LooperState::Overdubbing), std::memory_order_release);
            } else if (cur == LooperState::Overdubbing) {
                state.store(static_cast<int>(LooperState::Playing), std::memory_order_release);
            }
            return;
        }

        if (cmd == kCmdStopToggle) {
            if (cur == LooperState::Recording && len > 256) {
                applyLoopSeamFade(len);
                rebuildWaveformPreview(len);
                playhead.store(0, std::memory_order_relaxed);
                state.store(static_cast<int>(LooperState::Stopped), std::memory_order_release);
            } else if (cur == LooperState::Playing || cur == LooperState::Overdubbing) {
                playhead.store(0, std::memory_order_relaxed);
                state.store(static_cast<int>(LooperState::Stopped), std::memory_order_release);
            } else if (cur == LooperState::Stopped && len > 0) {
                playhead.store(0, std::memory_order_relaxed);
                state.store(static_cast<int>(LooperState::Playing), std::memory_order_release);
            }
            return;
        }

        if (cmd == kCmdUndo) {
            if (undoAvailable.load(std::memory_order_acquire) && len > 0 && len <= maxSamples) {
                loopBuf.copyFrom(0, 0, undoBuf, 0, 0, len);
                loopBuf.copyFrom(1, 0, undoBuf, 1, 0, len);
                undoAvailable.store(false, std::memory_order_release);
                rebuildWaveformPreview(len);
                if (cur == LooperState::Overdubbing) {
                    state.store(static_cast<int>(LooperState::Playing), std::memory_order_release);
                }
            }
        }
    }

    void applyLoopSeamFade(int len) noexcept {
        const int fadeLen = std::min(192, len / 4);
        if (fadeLen <= 1) return;
        for (int ch = 0; ch < 2; ++ch) {
            float* data = loopBuf.getWritePointer(ch);
            for (int i = 0; i < fadeLen; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(fadeLen);
                data[i] *= t;
                data[len - 1 - i] *= t;
            }
        }
    }

    void rebuildWaveformPreview(int len) noexcept {
        if (len <= 0) return;
        const float* l = loopBuf.getReadPointer(0);
        const float* r = loopBuf.getReadPointer(1);
        const int step = std::max(1, len / kWaveformBins);
        for (int b = 0; b < kWaveformBins; ++b) {
            const int start = b * step;
            const int end = std::min(len, start + step);
            float pk = 0.0f;
            const int stride = std::max(1, (end - start) / 32);
            for (int i = start; i < end; i += stride) {
                pk = std::max(pk, std::max(std::abs(l[i]), std::abs(r[i])));
            }
            waveformPeaks[static_cast<size_t>(b)].store(std::min(1.0f, pk), std::memory_order_relaxed);
        }
    }

    juce::AudioBuffer<float> loopBuf;
    juce::AudioBuffer<float> undoBuf;
    int maxSamples = 0;
    int overdubCopiedSamples = 0;

    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> state { static_cast<int>(LooperState::Empty) };
    std::atomic<int> pendingCommand { kCmdNone };
    std::atomic<int> loopLength { 0 };
    std::atomic<int> playhead { 0 };
    std::atomic<float> loopGain { 1.0f };
    std::atomic<bool> preFxMode { false };
    std::atomic<bool> undoAvailable { false };
    std::array<std::atomic<float>, kWaveformBins> waveformPeaks {};
};

// Real-time safe background 24-bit stereo WAV Audio Recorder using JUCE ThreadedWriter.
class WavAudioRecorder {
public:
    WavAudioRecorder() {
        backgroundThread.startThread();
    }

    ~WavAudioRecorder() {
        stopRecording();
        backgroundThread.stopThread(2000);
    }

    void prepare(double newSampleRate) noexcept {
        sampleRate.store(std::max(8000.0, newSampleRate), std::memory_order_relaxed);
    }

    juce::File getRecordingsDirectory() const {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("PedalBoard")
                       .getChildFile("Recordings");
        if (!dir.exists()) {
            dir.createDirectory();
        }
        return dir;
    }

    bool startRecording() {
        stopRecording();

        const double sr = sampleRate.load(std::memory_order_relaxed);
        if (sr <= 0.0) {
            return false;
        }

        const auto now = juce::Time::getCurrentTime();
        const auto fileName = "PedalBoard_" + now.formatted("%Y%m%d_%H%M%S") + ".wav";
        const auto file = getRecordingsDirectory().getNonexistentChildFile(fileName.upToLastOccurrenceOf(".wav", false, false), ".wav");

        std::unique_ptr<juce::OutputStream> fileStream(file.createOutputStream());
        if (fileStream == nullptr) {
            return false;
        }

        juce::WavAudioFormat wavFormat;
        auto writer = wavFormat.createWriterFor(
            fileStream,
            juce::AudioFormatWriterOptions()
                .withSampleRate(sr)
                .withNumChannels(2)
                .withBitsPerSample(24)
        );
        if (writer == nullptr) {
            return false;
        }

        auto newThreadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
            writer.release(), backgroundThread, 65536
        );

        {
            std::lock_guard<std::mutex> lock(writerMutex);
            lastRecordedFile = file;
            threadedWriter = std::move(newThreadedWriter);
            recordedSamples.store(0, std::memory_order_relaxed);
            activeWriter.store(threadedWriter.get(), std::memory_order_release);
        }
        return true;
    }

    void stopRecording() {
        std::lock_guard<std::mutex> lock(writerMutex);
        activeWriter.store(nullptr, std::memory_order_release);
        // Wait briefly for any in-flight audio callback to finish using activeWriter
        while (audioCallbackInUse.load(std::memory_order_acquire)) {
            juce::Thread::yield();
        }
        threadedWriter.reset();
    }

    bool isRecording() const noexcept {
        return activeWriter.load(std::memory_order_acquire) != nullptr;
    }

    double getRecordedSeconds() const noexcept {
        const double sr = sampleRate.load(std::memory_order_relaxed);
        return (sr > 0.0) ? static_cast<double>(recordedSamples.load(std::memory_order_relaxed)) / sr : 0.0;
    }

    juce::File getLastRecordedFile() const {
        std::lock_guard<std::mutex> lock(writerMutex);
        return lastRecordedFile;
    }

    // Called from real-time audio thread: strictly lock-free FIFO push.
    void pushBlock(const juce::AudioBuffer<float>& buffer) noexcept {
        audioCallbackInUse.store(true, std::memory_order_release);
        if (auto* writer = activeWriter.load(std::memory_order_acquire)) {
            const int n = buffer.getNumSamples();
            const float* ptrs[2] = {
                buffer.getReadPointer(0),
                buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0)
            };
            writer->write(ptrs, n);
            recordedSamples.fetch_add(static_cast<int64_t>(n), std::memory_order_relaxed);
        }
        audioCallbackInUse.store(false, std::memory_order_release);
    }

private:
    juce::TimeSliceThread backgroundThread { "PedalBoard WAV Recorder" };
    mutable std::mutex writerMutex;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<bool> audioCallbackInUse { false };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int64_t> recordedSamples { 0 };
    juce::File lastRecordedFile;
};

} // namespace pedalboard
