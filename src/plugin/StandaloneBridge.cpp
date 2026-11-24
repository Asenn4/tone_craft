#include "StandaloneBridge.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#if defined(JucePlugin_Build_Standalone) && JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace pedalboard {
namespace {

StandaloneHostHandle queryStandaloneImpl() {
    StandaloneHostHandle h;
#if defined(JucePlugin_Build_Standalone) && JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance()) {
        h.deviceManager            = &holder->deviceManager;
        h.muteInputValue           = &holder->getMuteInputValue();
        h.hasPotentialFeedbackLoop = holder->getProcessorHasPotentialFeedbackLoop();
        h.saveAudioState           = [] {
            if (auto* current = juce::StandalonePluginHolder::getInstance()) {
                current->saveAudioDeviceState();
            }
        };
    }
#endif
    return h;
}

struct StandaloneBridgeRegistrar {
    StandaloneBridgeRegistrar() noexcept {
        registerStandaloneHostProvider(&queryStandaloneImpl);
    }
};

static const StandaloneBridgeRegistrar kStandaloneBridgeRegistrar;

} // namespace
} // namespace pedalboard
