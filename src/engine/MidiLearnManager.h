#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

namespace pedalboard {

struct MidiCcBindingInfo {
    juce::String paramId;
    int ccNumber = -1;
    float minNorm = 0.0f;
    float maxNorm = 1.0f;
};

// Lock-free MIDI CC Learn and real-time controller mapping manager.
class MidiLearnManager {
public:
    static constexpr int kMaxBindings = 64;

    void bindToAPVTS(juce::AudioProcessorValueTreeState& state) noexcept {
        apvts = &state;
    }

    void armGlobalLearn() {
        {
            std::lock_guard<std::mutex> lock(metaMutex);
            learningParamId.clear();
        }
        learningParamPtr.store(nullptr, std::memory_order_release);
        learnArmed.store(true, std::memory_order_release);
        version.fetch_add(1, std::memory_order_release);
    }

    void armLearn(const juce::String& paramId) {
        if (apvts == nullptr) return;
        auto* param = apvts->getParameter(paramId);
        if (param == nullptr) return;
        {
            std::lock_guard<std::mutex> lock(metaMutex);
            learningParamId = paramId;
        }
        learningParamPtr.store(param, std::memory_order_release);
        learnArmed.store(true, std::memory_order_release);
        version.fetch_add(1, std::memory_order_release);
    }

    void cancelLearn() {
        learnArmed.store(false, std::memory_order_release);
        learningParamPtr.store(nullptr, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(metaMutex);
            learningParamId.clear();
        }
        version.fetch_add(1, std::memory_order_release);
    }

    bool isLearnArmed() const noexcept {
        return learnArmed.load(std::memory_order_acquire);
    }

    juce::String getLearningParamId() const {
        if (!isLearnArmed()) return {};
        std::lock_guard<std::mutex> lock(metaMutex);
        return learningParamId;
    }

    void assignMapping(const juce::String& paramId, int ccNumber, float minNorm = 0.0f, float maxNorm = 1.0f) {
        if (apvts == nullptr || ccNumber < 0 || ccNumber > 127) return;
        auto* param = apvts->getParameter(paramId);
        if (param == nullptr) return;

        const bool isBool = (dynamic_cast<juce::AudioParameterBool*>(param) != nullptr);
        std::lock_guard<std::mutex> lock(metaMutex);

        // Reuse existing slot for same paramId, or pick first empty slot
        int targetSlot = -1;
        for (int i = 0; i < kMaxBindings; ++i) {
            if (slots[static_cast<size_t>(i)].param.load(std::memory_order_relaxed) == param) {
                targetSlot = i;
                break;
            }
            if (targetSlot < 0 && slots[static_cast<size_t>(i)].cc.load(std::memory_order_relaxed) < 0) {
                targetSlot = i;
            }
        }
        if (targetSlot < 0) return;

        auto& s = slots[static_cast<size_t>(targetSlot)];
        s.minNorm.store(std::clamp(minNorm, 0.0f, 1.0f), std::memory_order_relaxed);
        s.maxNorm.store(std::clamp(maxNorm, 0.0f, 1.0f), std::memory_order_relaxed);
        s.isBool.store(isBool, std::memory_order_relaxed);
        s.param.store(param, std::memory_order_relaxed);
        slotParamIds[static_cast<size_t>(targetSlot)] = paramId;
        s.cc.store(ccNumber, std::memory_order_release);
        version.fetch_add(1, std::memory_order_release);
    }

    void clearMappingForParam(const juce::String& paramId) {
        std::lock_guard<std::mutex> lock(metaMutex);
        for (int i = 0; i < kMaxBindings; ++i) {
            if (slotParamIds[static_cast<size_t>(i)] == paramId) {
                slots[static_cast<size_t>(i)].cc.store(-1, std::memory_order_release);
                slots[static_cast<size_t>(i)].param.store(nullptr, std::memory_order_release);
                slotParamIds[static_cast<size_t>(i)].clear();
            }
        }
        version.fetch_add(1, std::memory_order_release);
    }

    void clearAllMappings() {
        std::lock_guard<std::mutex> lock(metaMutex);
        for (int i = 0; i < kMaxBindings; ++i) {
            slots[static_cast<size_t>(i)].cc.store(-1, std::memory_order_release);
            slots[static_cast<size_t>(i)].param.store(nullptr, std::memory_order_release);
            slotParamIds[static_cast<size_t>(i)].clear();
        }
        version.fetch_add(1, std::memory_order_release);
    }

    int getCcForParam(const juce::String& paramId) const {
        std::lock_guard<std::mutex> lock(metaMutex);
        for (int i = 0; i < kMaxBindings; ++i) {
            if (slotParamIds[static_cast<size_t>(i)] == paramId) {
                return slots[static_cast<size_t>(i)].cc.load(std::memory_order_acquire);
            }
        }
        return -1;
    }

    std::vector<MidiCcBindingInfo> getAllMappings() const {
        std::vector<MidiCcBindingInfo> out;
        std::lock_guard<std::mutex> lock(metaMutex);
        for (int i = 0; i < kMaxBindings; ++i) {
            const int cc = slots[static_cast<size_t>(i)].cc.load(std::memory_order_acquire);
            if (cc >= 0 && slotParamIds[static_cast<size_t>(i)].isNotEmpty()) {
                MidiCcBindingInfo info;
                info.paramId = slotParamIds[static_cast<size_t>(i)];
                info.ccNumber = cc;
                info.minNorm = slots[static_cast<size_t>(i)].minNorm.load(std::memory_order_relaxed);
                info.maxNorm = slots[static_cast<size_t>(i)].maxNorm.load(std::memory_order_relaxed);
                out.push_back(std::move(info));
            }
        }
        return out;
    }

    uint64_t getVersion() const noexcept {
        return version.load(std::memory_order_acquire);
    }

    // Called from real-time audio thread: strictly lock-free & allocation-free.
    void processMidiBuffer(const juce::MidiBuffer& midiMessages) noexcept {
        for (const auto metadata : midiMessages) {
            const auto msg = metadata.getMessage();
            if (!msg.isController()) {
                continue;
            }

            const int ccNum = msg.getControllerNumber();
            const int ccVal = msg.getControllerValue();
            if (ccNum < 0 || ccNum > 127) {
                continue;
            }

            if (learnArmed.exchange(false, std::memory_order_acq_rel)) {
                auto* targetParam = learningParamPtr.exchange(nullptr, std::memory_order_acq_rel);
                if (targetParam != nullptr) {
                    const auto paramId = targetParam->getParameterID();
                    // Bind off the audio thread safely via MessageManager
                    juce::MessageManager::callAsync([this, paramId, ccNum] {
                        assignMapping(paramId, ccNum, 0.0f, 1.0f);
                    });
                }
                continue;
            }

            const float normCc = static_cast<float>(ccVal) / 127.0f;
            for (int i = 0; i < kMaxBindings; ++i) {
                const int slotCc = slots[static_cast<size_t>(i)].cc.load(std::memory_order_acquire);
                if (slotCc == ccNum) {
                    if (auto* param = slots[static_cast<size_t>(i)].param.load(std::memory_order_acquire)) {
                        const bool isBool = slots[static_cast<size_t>(i)].isBool.load(std::memory_order_relaxed);
                        float mapped = 0.0f;
                        if (isBool) {
                            mapped = (ccVal >= 64) ? 1.0f : 0.0f;
                        } else {
                            const float lo = slots[static_cast<size_t>(i)].minNorm.load(std::memory_order_relaxed);
                            const float hi = slots[static_cast<size_t>(i)].maxNorm.load(std::memory_order_relaxed);
                            mapped = lo + normCc * (hi - lo);
                        }
                        param->setValueNotifyingHost(std::clamp(mapped, 0.0f, 1.0f));
                    }
                }
            }
        }
    }

    juce::Array<juce::var> toJsonArray() const {
        juce::Array<juce::var> arr;
        for (const auto& m : getAllMappings()) {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("paramId", m.paramId);
            obj->setProperty("cc", m.ccNumber);
            obj->setProperty("min", static_cast<double>(m.minNorm));
            obj->setProperty("max", static_cast<double>(m.maxNorm));
            arr.add(juce::var(obj));
        }
        return arr;
    }

    void loadFromJsonArray(const juce::var& arrVar) {
        clearAllMappings();
        if (auto* arr = arrVar.getArray()) {
            for (const auto& item : *arr) {
                if (auto* obj = item.getDynamicObject()) {
                    const auto pid = obj->getProperty("paramId").toString();
                    const int cc = static_cast<int>(obj->getProperty("cc"));
                    const float lo = static_cast<float>(static_cast<double>(obj->getProperty("min")));
                    const float hi = obj->hasProperty("max")
                                         ? static_cast<float>(static_cast<double>(obj->getProperty("max")))
                                         : 1.0f;
                    if (pid.isNotEmpty() && cc >= 0 && cc <= 127) {
                        assignMapping(pid, cc, lo, hi);
                    }
                }
            }
        }
    }

private:
    struct Slot {
        std::atomic<int> cc { -1 };
        std::atomic<juce::RangedAudioParameter*> param { nullptr };
        std::atomic<float> minNorm { 0.0f };
        std::atomic<float> maxNorm { 1.0f };
        std::atomic<bool> isBool { false };
    };

    juce::AudioProcessorValueTreeState* apvts = nullptr;
    std::array<Slot, kMaxBindings> slots;
    std::array<juce::String, kMaxBindings> slotParamIds;

    std::atomic<bool> learnArmed { false };
    std::atomic<juce::RangedAudioParameter*> learningParamPtr { nullptr };
    std::atomic<uint64_t> version { 1 };

    mutable std::mutex metaMutex;
    juce::String learningParamId;
};

} // namespace pedalboard
