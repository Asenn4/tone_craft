#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#if defined(PEDALBOARD_HAS_NAM) && PEDALBOARD_HAS_NAM
  #if defined(_MSC_VER)
    #pragma warning(push, 0)
  #endif
  #include <NAM/dsp.h>
  #include <NAM/get_dsp.h>
  #if defined(_MSC_VER)
    #pragma warning(pop)
  #endif
#endif

namespace pedalboard {

// Real-time safe wrapper around NeuralAmpModelerCore v0.6.0.
// Loads and prewarms .nam neural captures off the audio thread, atomically swaps models,
// and resamples transparently when host sample rate differs from model training rate.
class NamModelRunner {
public:
    NamModelRunner() = default;

    ~NamModelRunner() {
#if defined(PEDALBOARD_HAS_NAM) && PEDALBOARD_HAS_NAM
        // Wait briefly if a background load is still finishing, then clean up models safely
        while (loadingInProgress.load(std::memory_order_acquire)) {
            juce::Thread::sleep(5);
        }
        delete activeModel;
        activeModel = nullptr;
        delete pendingModel.exchange(nullptr, std::memory_order_acq_rel);
        delete retiredModel.exchange(nullptr, std::memory_order_acq_rel);
        std::lock_guard<std::mutex> lock(garbageMutex);
        garbageBin.clear();
#endif
    }

    void prepare(double newSampleRate, int newMaxBlockSize) {
        hostSampleRate.store(std::max(8000.0, newSampleRate), std::memory_order_relaxed);
        hostMaxBlockSize.store(std::max(64, newMaxBlockSize), std::memory_order_relaxed);

        const size_t cap = static_cast<size_t>(std::max(4096, newMaxBlockSize * 4));
        inBuf.assign(cap, 0.0);
        outBuf.assign(cap, 0.0);
        resampleInF.assign(cap, 0.0f);
        resampleMidF.assign(cap, 0.0f);
        resampleOutF.assign(cap, 0.0f);
        upResampler.reset();
        downResampler.reset();

        juce::String existingPath;
        {
            std::lock_guard<std::mutex> lock(metaMutex);
            existingPath = currentModelPath;
        }
        if (existingPath.isNotEmpty() && juce::File(existingPath).existsAsFile()) {
            loadModelAsync(juce::File(existingPath));
        }
    }

    void reset() noexcept {
        upResampler.reset();
        downResampler.reset();
    }

    void loadModelAsync(const juce::File& file, std::function<void(bool)> onDone = {}) {
#if defined(PEDALBOARD_HAS_NAM) && PEDALBOARD_HAS_NAM
        if (!file.existsAsFile()) {
            if (onDone) onDone(false);
            return;
        }

        // Collect any previously retired audio-thread models safely on the caller thread
        reclaimRetired();

        const double sr = hostSampleRate.load(std::memory_order_relaxed);
        const int maxBlock = hostMaxBlockSize.load(std::memory_order_relaxed);
        const auto pathStr = file.getFullPathName();
        const auto fileName = file.getFileNameWithoutExtension();

        {
            std::lock_guard<std::mutex> lock(metaMutex);
            statusText = "Loading " + fileName + "...";
        }

        loadingInProgress.store(true, std::memory_order_release);

        juce::Thread::launch([this, pathStr, fileName, sr, maxBlock, cb = std::move(onDone)]() mutable {
            bool ok = false;
            juce::String errMsg;
            std::unique_ptr<nam::DSP> loaded;
            double expectedSr = 48000.0;
            float loudnessNormGain = 1.0f;

            try {
                const std::filesystem::path fsPath(pathStr.toWideCharPointer());
                loaded = nam::get_dsp(fsPath);
                if (loaded != nullptr) {
                    expectedSr = loaded->GetExpectedSampleRate();
                    if (expectedSr <= 0.0) {
                        expectedSr = sr;
                    }
                    const int modelMaxBlock = std::max(512, static_cast<int>(std::ceil(maxBlock * (expectedSr / sr))) + 64);
                    loaded->Reset(expectedSr, modelMaxBlock);

                    if (loaded->HasLoudness()) {
                        const double targetLoudness = -18.0;
                        const double gainDb = std::clamp(targetLoudness - loaded->GetLoudness(), -24.0, 24.0);
                        loudnessNormGain = static_cast<float>(juce::Decibels::decibelsToGain(gainDb));
                    }
                    ok = true;
                }
            } catch (const std::exception& e) {
                errMsg = e.what();
            } catch (...) {
                errMsg = "Unknown error loading .nam model";
            }

            if (ok && loaded != nullptr) {
                auto* wrapper = new LoadedModel();
                wrapper->dsp = std::move(loaded);
                wrapper->modelSampleRate = expectedSr;
                wrapper->loudnessGain = loudnessNormGain;

                auto* prevPending = pendingModel.exchange(wrapper, std::memory_order_acq_rel);
                if (prevPending != nullptr) {
                    std::lock_guard<std::mutex> gLock(garbageMutex);
                    garbageBin.emplace_back(prevPending);
                }

                {
                    std::lock_guard<std::mutex> lock(metaMutex);
                    currentModelPath = pathStr;
                    displayName = fileName;
                    statusText = fileName + " (" + juce::String(static_cast<int>(std::lround(expectedSr / 1000.0))) + "kHz)";
                }
                modelLoadedFlag.store(true, std::memory_order_release);
            } else {
                std::lock_guard<std::mutex> lock(metaMutex);
                statusText = errMsg.isNotEmpty() ? ("Load failed: " + errMsg) : "Load failed";
            }

            loadingInProgress.store(false, std::memory_order_release);
            if (cb) {
                juce::MessageManager::callAsync([cb = std::move(cb), ok] { cb(ok); });
            }
        });
#else
        juce::ignoreUnused(file);
        if (onDone) onDone(false);
#endif
    }

    void clearModel() {
#if defined(PEDALBOARD_HAS_NAM) && PEDALBOARD_HAS_NAM
        auto* sentinel = new LoadedModel(); // empty dsp pointer signals clear
        auto* prevPending = pendingModel.exchange(sentinel, std::memory_order_acq_rel);
        if (prevPending != nullptr) {
            std::lock_guard<std::mutex> gLock(garbageMutex);
            garbageBin.emplace_back(prevPending);
        }
        reclaimRetired();
        modelLoadedFlag.store(false, std::memory_order_release);
        std::lock_guard<std::mutex> lock(metaMutex);
        currentModelPath.clear();
        displayName = "No .nam loaded";
        statusText = "No .nam loaded";
#endif
    }

    bool hasLoadedModel() const noexcept {
        return modelLoadedFlag.load(std::memory_order_acquire);
    }

    void setLoudnessNormalizationEnabled(bool enabled) noexcept {
        loudnessNormEnabled.store(enabled, std::memory_order_relaxed);
    }

    bool isLoudnessNormalizationEnabled() const noexcept {
        return loudnessNormEnabled.load(std::memory_order_relaxed);
    }

    juce::String getDisplayName() const {
        std::lock_guard<std::mutex> lock(metaMutex);
        return statusText;
    }

    juce::String getModelPath() const {
        std::lock_guard<std::mutex> lock(metaMutex);
        return currentModelPath;
    }

    void reclaimRetired() {
#if defined(PEDALBOARD_HAS_NAM) && PEDALBOARD_HAS_NAM
        if (auto* old = retiredModel.exchange(nullptr, std::memory_order_acq_rel)) {
            delete old;
        }
        std::lock_guard<std::mutex> gLock(garbageMutex);
        garbageBin.clear();
#endif
    }

    // Strictly real-time safe: zero locks, zero heap allocations, zero deletes.
    bool process(juce::AudioBuffer<float>& buffer, float inputDriveGain) noexcept {
#if defined(PEDALBOARD_HAS_NAM) && PEDALBOARD_HAS_NAM
        // Atomically check for newly loaded model from background thread
        if (auto* incoming = pendingModel.exchange(nullptr, std::memory_order_acq_rel)) {
            auto* old = activeModel;
            activeModel = (incoming->dsp != nullptr) ? incoming : nullptr;
            if (incoming->dsp == nullptr) {
                // Hand back empty sentinel for deletion off audio thread
                auto* prevRet = retiredModel.exchange(incoming, std::memory_order_acq_rel);
                juce::ignoreUnused(prevRet);
            }
            if (old != nullptr) {
                auto* prevRet = retiredModel.exchange(old, std::memory_order_acq_rel);
                juce::ignoreUnused(prevRet);
            }
            upResampler.reset();
            downResampler.reset();
        }

        if (activeModel == nullptr || activeModel->dsp == nullptr) {
            return false;
        }

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0 || static_cast<size_t>(numSamples) > inBuf.size()) {
            return false;
        }

        const double sr = hostSampleRate.load(std::memory_order_relaxed);
        const double modelSr = activeModel->modelSampleRate;
        const bool needsResample = (modelSr > 0.0 && std::abs(sr - modelSr) > 1.0);

        const float* srcL = buffer.getReadPointer(0);
        const float* srcR = numChannels > 1 ? buffer.getReadPointer(1) : nullptr;
        const float normOut = loudnessNormEnabled.load(std::memory_order_relaxed) ? activeModel->loudnessGain : 1.0f;

        if (!needsResample) {
            for (int i = 0; i < numSamples; ++i) {
                const float mono = srcR != nullptr ? 0.5f * (srcL[i] + srcR[i]) : srcL[i];
                inBuf[static_cast<size_t>(i)] = static_cast<double>(mono * inputDriveGain);
            }

            double* inPtr = inBuf.data();
            double* outPtr = outBuf.data();
            activeModel->dsp->process(&inPtr, &outPtr, numSamples);

            for (int ch = 0; ch < numChannels; ++ch) {
                float* dst = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i) {
                    dst[i] = static_cast<float>(outBuf[static_cast<size_t>(i)]) * normOut;
                }
            }
        } else {
            // Resample mono input hostSr -> modelSr -> process NAM -> resample modelSr -> hostSr
            for (int i = 0; i < numSamples; ++i) {
                const float mono = srcR != nullptr ? 0.5f * (srcL[i] + srcR[i]) : srcL[i];
                resampleInF[static_cast<size_t>(i)] = mono * inputDriveGain;
            }

            const double ratioUp = sr / modelSr;
            const int targetModelSamples = std::clamp(
                static_cast<int>(std::lround(static_cast<double>(numSamples) * modelSr / sr)),
                1,
                static_cast<int>(inBuf.size()) - 1
            );

            upResampler.process(ratioUp, resampleInF.data(), resampleMidF.data(), targetModelSamples);

            for (int i = 0; i < targetModelSamples; ++i) {
                inBuf[static_cast<size_t>(i)] = static_cast<double>(resampleMidF[static_cast<size_t>(i)]);
            }

            double* inPtr = inBuf.data();
            double* outPtr = outBuf.data();
            activeModel->dsp->process(&inPtr, &outPtr, targetModelSamples);

            for (int i = 0; i < targetModelSamples; ++i) {
                resampleMidF[static_cast<size_t>(i)] = static_cast<float>(outBuf[static_cast<size_t>(i)]) * normOut;
            }

            const double ratioDown = modelSr / sr;
            downResampler.process(ratioDown, resampleMidF.data(), resampleOutF.data(), numSamples);

            for (int ch = 0; ch < numChannels; ++ch) {
                float* dst = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i) {
                    dst[i] = resampleOutF[static_cast<size_t>(i)];
                }
            }
        }
        return true;
#else
        juce::ignoreUnused(buffer, inputDriveGain);
        return false;
#endif
    }

private:
#if defined(PEDALBOARD_HAS_NAM) && PEDALBOARD_HAS_NAM
    struct LoadedModel {
        std::unique_ptr<nam::DSP> dsp;
        double modelSampleRate = 48000.0;
        float loudnessGain = 1.0f;
    };

    LoadedModel* activeModel = nullptr;
    std::atomic<LoadedModel*> pendingModel { nullptr };
    std::atomic<LoadedModel*> retiredModel { nullptr };
    std::atomic<bool> loadingInProgress { false };
    std::mutex garbageMutex;
    std::vector<std::unique_ptr<LoadedModel>> garbageBin;
#endif

    std::atomic<double> hostSampleRate { 44100.0 };
    std::atomic<int> hostMaxBlockSize { 512 };
    std::atomic<bool> modelLoadedFlag { false };
    std::atomic<bool> loudnessNormEnabled { true };

    std::vector<double> inBuf;
    std::vector<double> outBuf;
    std::vector<float> resampleInF;
    std::vector<float> resampleMidF;
    std::vector<float> resampleOutF;
    juce::LagrangeInterpolator upResampler;
    juce::LagrangeInterpolator downResampler;

    mutable std::mutex metaMutex;
    juce::String currentModelPath;
    juce::String displayName { "No .nam loaded" };
    juce::String statusText { "No .nam loaded" };
};

} // namespace pedalboard
