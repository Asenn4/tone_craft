#pragma once

#include <functional>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

namespace pedalboard {

// Clean host bridge exposing JUCE Standalone's AudioDeviceManager & feedback-mute state
// to SharedCode UI without coupling VST3 targets to Standalone symbols.
struct StandaloneHostHandle {
    juce::AudioDeviceManager* deviceManager = nullptr;
    juce::Value* muteInputValue = nullptr;
    bool hasPotentialFeedbackLoop = false;
    std::function<void()> saveAudioState;

    bool isAvailable() const noexcept { return deviceManager != nullptr; }
};

using StandaloneHostProviderFn = StandaloneHostHandle (*)();

namespace detail {
inline StandaloneHostProviderFn& standaloneHostProviderSlot() noexcept {
    static StandaloneHostProviderFn provider = nullptr;
    return provider;
}
} // namespace detail

inline void registerStandaloneHostProvider(StandaloneHostProviderFn fn) noexcept {
    detail::standaloneHostProviderSlot() = fn;
}

inline StandaloneHostHandle queryStandaloneAudioHost() {
    if (auto fn = detail::standaloneHostProviderSlot(); fn != nullptr) {
        return fn();
    }
    return {};
}

} // namespace pedalboard
