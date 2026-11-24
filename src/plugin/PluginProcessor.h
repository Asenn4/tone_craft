#pragma once

#include "../engine/MidiLearnManager.h"
#include "../engine/PedalSlot.h"
#include "../engine/SignalChain.h"
#include "../presets/PresetManager.h"
#include "Parameters.h"

#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <mutex>
#include <vector>

namespace pedalboard {

struct DynamicBoardState {
    int numNodes = 0;
    std::array<ChainNode, 16> nodes {};
    std::array<PedalSlotSnapshot, kMaxPedalSlots> slots {};
    std::array<PedalSlotSnapshot, 3> rackSlots {}; // 0=RackMod, 1=RackDelay, 2=RackReverb
};

struct MidiActivitySnapshot {
    int type = 0;        // 0 = none, 1 = CC, 2 = Program Change, 3 = Note On
    int channel = 1;     // 1..16
    int data1 = 0;       // CC number / program / note
    int data2 = 0;       // CC value / velocity
    int64_t timestampMs = 0;
};

class PedalBoardAudioProcessor : public juce::AudioProcessor {
public:
    PedalBoardAudioProcessor();
    ~PedalBoardAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    SignalChain& getSignalChain() noexcept { return signalChain; }
    const SignalChain& getSignalChain() const noexcept { return signalChain; }
    PresetManager& getPresetManager() noexcept { return presetManager; }
    MidiLearnManager& getMidiLearnManager() noexcept { return midiLearnManager; }
    const MidiLearnManager& getMidiLearnManager() const noexcept { return midiLearnManager; }

    void setChainOrder(const ChainOrder& order);
    ChainOrder getChainOrder() const noexcept;
    void moveModule(ModuleId id, int delta);
    void moveModuleToSlot(ModuleId dragged, int targetSlotIndex);

    void setBusAssignments(const BusAssignments& buses);
    BusAssignments getBusAssignments() const noexcept;
    void setModuleBusLane(ModuleId id, BusLane lane);
    void cycleModuleBusLane(ModuleId id);

    // AmpliTube-style Dynamic Multi-Slot Pedalboard & Signal Chain API
    DynamicBoardState getDynamicBoardState() const noexcept;
    std::vector<ChainNode> getDynamicChainNodes() const;
    std::vector<int> getActivePedalSlotIds() const;
    PedalSlotSnapshot getPedalSlot(int slotId) const noexcept;

    int addPedalSlot(ModuleId category, int modelIndex, int insertAtPedalOrderIdx = -1, int explicitLane = -1);
    int addPedalSlotAtChainIndex(ModuleId category, int modelIndex, int targetChainIdx, BusLane lane = BusLane::Pre);
    void removePedalSlot(int slotId);
    void replacePedalSlot(int slotId, ModuleId newCategory, int newModelIndex);
    void setPedalSlotModel(int slotId, int newModelIndex);
    void setPedalSlotBypass(int slotId, bool bypassed);
    void setPedalSlotParam(int slotId, int paramIdx, float value);
    BusLane getPedalSlotBus(int slotId) const noexcept;
    void setPedalSlotBus(int slotId, BusLane lane);
    void movePedalSlotToOtherSlot(int draggedSlotId, int targetSlotId);

    // Per-Model Dynamic Studio Rack API (0=MOD, 1=DLY, 2=RVB)
    PedalSlotSnapshot getRackSlot(int rackIdx) const noexcept;
    void setRackSlotModel(int rackIdx, int newModelIndex);
    void setRackSlotBypass(int rackIdx, bool bypassed);
    void setRackSlotParam(int rackIdx, int paramIdx, float value);
    void movePedalSlotInStompOrder(int slotId, int targetStompIdx);
    void moveDynamicChainNode(int fromChainIdx, int toChainIdx);
    void cycleDynamicChainNodeBus(int chainIdx);
    void setDynamicChainNodeBus(int chainIdx, BusLane lane);
    void syncDynamicBoardFromApvts(bool emptyStompboard = false);

    // Hardware / MIDI CC Expression Pedal Support (controls Wah, P-Bend Whammy, Pitch, OCTA)
    void setExpressionPedalCc(int ccNumber);
    int getExpressionPedalCc() const noexcept { return expressionPedalCc.load(std::memory_order_acquire); }
    void armExpressionPedalMidiLearn(bool armed = true) noexcept {
        expressionLearnArmed.store(armed, std::memory_order_release);
        notifyUiStateChanged();
    }
    bool isExpressionPedalMidiLearnArmed() const noexcept {
        return expressionLearnArmed.load(std::memory_order_acquire);
    }
    void setExpressionPedalPosition(float normPos);

    // Global Studio Settings (persisted to %APPDATA%/PedalBoard/studio_settings.json)
    void setInputChannelMode(InputChannelMode mode);
    InputChannelMode getInputChannelMode() const noexcept {
        return static_cast<InputChannelMode>(std::clamp(inputChannelMode.load(std::memory_order_relaxed), 0, 3));
    }
    void setInputPhaseInvert(bool invert);
    bool isInputPhaseInvert() const noexcept { return inputPhaseInvert.load(std::memory_order_relaxed); }

    void setInputRumbleHpfHz(float hz);
    float getInputRumbleHpfHz() const noexcept { return inputRumbleHpfHz.load(std::memory_order_relaxed); }

    void setOutputSafetyLimiter(bool enabled);
    bool isOutputSafetyLimiter() const noexcept { return outputSafetyLimiter.load(std::memory_order_relaxed); }

    void setNamLoudnessNormalization(bool enabled);
    bool isNamLoudnessNormalization() const noexcept { return namLoudnessNorm.load(std::memory_order_relaxed); }

    void setTunerReferenceHz(float hz);
    float getTunerReferenceHz() const noexcept { return signalChain.getTuner().getReferenceHz(); }

    void setMidiProgramChangeEnabled(bool enabled);
    bool isMidiProgramChangeEnabled() const noexcept { return midiProgramChangeEnabled.load(std::memory_order_relaxed); }

    MidiActivitySnapshot getLastMidiActivity() const noexcept;
    float getDspCpuLoadPercent() const noexcept { return dspCpuLoadPercent.load(std::memory_order_relaxed); }
    int getCurrentAudioBlockSize() const noexcept { return currentBlockSize.load(std::memory_order_relaxed); }
    void resetStudioSettingsToDefaults();
    static juce::File getStudioSettingsFile();

    RoutingMode getRoutingMode() const noexcept;
    juce::String getActivePresetName() const;

    uint64_t getUiStateVersion() const noexcept {
        return uiStateVersion.load(std::memory_order_acquire) + midiLearnManager.getVersion();
    }

    void applyPresetData(const PresetData& preset);
    bool loadUserPreset(const juce::File& file);
    bool saveUserPreset(const juce::File& file,
                        const juce::String& presetName,
                        const juce::String& style = "Custom",
                        const juce::String& character = "User",
                        const juce::String& description = {});

    bool loadCustomCabinetIR(const juce::File& file, int rigIndex = 0);
    void activateSavedCabinetIR(int rigIndex = 0);
    void resetCabinetIR(int rigIndex = 0);
    void loadCustomNamModel(const juce::File& file, int rigIndex = 0);
    void clearNamModel(int rigIndex = 0);
    void loadCustomSlotNamModel(int slotId, const juce::File& file);
    void clearSlotNamModel(int slotId);

private:
    ChainParams readCurrentParams() const noexcept;
    void notifyUiStateChanged() noexcept { uiStateVersion.fetch_add(1, std::memory_order_release); }
    void publishBoardStateLocked() noexcept;
    void syncPrimaryApvtsFromSlot(const PedalSlotSnapshot& s);
    void loadStudioSettings();
    void saveStudioSettings() const;

    mutable std::mutex boardStateMutex;
    DynamicBoardState workingBoardState {};
    std::array<DynamicBoardState, 2> audioBoardBuffers {};
    std::atomic<int> activeAudioBoardIdx { 0 };
    std::atomic<int> expressionPedalCc { 11 }; // Default: MIDI CC #11 (Expression Controller)
    std::atomic<bool> expressionLearnArmed { false };

    // Global studio settings & real-time telemetry (strictly lock-free for audio thread)
    std::atomic<int> inputChannelMode { static_cast<int>(InputChannelMode::Stereo) };
    std::atomic<bool> inputPhaseInvert { false };
    std::atomic<float> inputRumbleHpfHz { 0.0f };
    std::atomic<bool> outputSafetyLimiter { true };
    std::atomic<bool> namLoudnessNorm { true };
    std::atomic<bool> midiProgramChangeEnabled { true };
    std::atomic<int> currentBlockSize { 512 };
    std::atomic<float> dspCpuLoadPercent { 0.0f };

    std::atomic<int> lastMidiType { 0 };
    std::atomic<int> lastMidiChannel { 1 };
    std::atomic<int> lastMidiData1 { 0 };
    std::atomic<int> lastMidiData2 { 0 };
    std::atomic<int64_t> lastMidiTimestampMs { 0 };

    juce::AudioProcessorValueTreeState apvts;
    SignalChain signalChain;
    PresetManager presetManager;
    MidiLearnManager midiLearnManager;

    std::atomic<int> currentProgramIndex { 0 }; // Default: Clean Init (0 effects + Clean Amp & Cab)
    std::atomic<uint64_t> uiStateVersion { 1 };
    mutable std::mutex presetNameMutex;
    juce::String activePresetName { "Clean Init" };

    // Cached raw parameter pointers for fast, lock-free reads on the audio thread
    std::atomic<float>* pInputGain = nullptr;
    std::atomic<float>* pOutputGain = nullptr;
    std::atomic<float>* pTunerMute = nullptr;
    std::atomic<float>* pLooperLevel = nullptr;
    std::atomic<float>* pLooperPreFx = nullptr;

    std::atomic<float>* pRoutingMode = nullptr;
    std::atomic<float>* pPathALevel = nullptr;
    std::atomic<float>* pPathAPan = nullptr;
    std::atomic<float>* pPathAMute = nullptr;
    std::atomic<float>* pPathBLevel = nullptr;
    std::atomic<float>* pPathBPan = nullptr;
    std::atomic<float>* pPathBMute = nullptr;
    std::atomic<float>* pPathBPhase = nullptr;
    std::atomic<float>* pPathCLevel = nullptr;
    std::atomic<float>* pPathCPan = nullptr;
    std::atomic<float>* pPathCMute = nullptr;
    std::atomic<float>* pPathCPhase = nullptr;

    std::atomic<float>* pGateBypass = nullptr;
    std::atomic<float>* pGateModel = nullptr;
    std::atomic<float>* pGateThreshold = nullptr;
    std::atomic<float>* pGateRelease = nullptr;
    std::atomic<float>* pGateDepth = nullptr;

    std::atomic<float>* pCompBypass = nullptr;
    std::atomic<float>* pCompModel = nullptr;
    std::atomic<float>* pCompThreshold = nullptr;
    std::atomic<float>* pCompRatio = nullptr;
    std::atomic<float>* pCompAttack = nullptr;
    std::atomic<float>* pCompRelease = nullptr;
    std::atomic<float>* pCompMakeup = nullptr;

    std::atomic<float>* pWahBypass = nullptr;
    std::atomic<float>* pWahModel = nullptr;
    std::atomic<float>* pWahPedal = nullptr;
    std::atomic<float>* pWahQ = nullptr;
    std::atomic<float>* pWahSensitivity = nullptr;
    std::atomic<float>* pWahMix = nullptr;
    std::atomic<float>* pWahAuto = nullptr;

    std::atomic<float>* pOdBypass = nullptr;
    std::atomic<float>* pOdModel = nullptr;
    std::atomic<float>* pOdDrive = nullptr;
    std::atomic<float>* pOdTone = nullptr;
    std::atomic<float>* pOdLevel = nullptr;

    std::atomic<float>* pAmpBypass = nullptr;
    std::atomic<float>* pAmpChannel = nullptr;
    std::atomic<float>* pAmpModel = nullptr;
    std::atomic<float>* pAmpGain = nullptr;
    std::atomic<float>* pAmpBass = nullptr;
    std::atomic<float>* pAmpMid = nullptr;
    std::atomic<float>* pAmpTreble = nullptr;
    std::atomic<float>* pAmpPresence = nullptr;
    std::atomic<float>* pAmpMaster = nullptr;
    std::atomic<float>* pAmpExtra = nullptr;
    std::atomic<float>* pAmpSwitch = nullptr;

    std::atomic<float>* pAmpBBypass = nullptr;
    std::atomic<float>* pAmpBChannel = nullptr;
    std::atomic<float>* pAmpBModel = nullptr;
    std::atomic<float>* pAmpBGain = nullptr;
    std::atomic<float>* pAmpBBass = nullptr;
    std::atomic<float>* pAmpBMid = nullptr;
    std::atomic<float>* pAmpBTreble = nullptr;
    std::atomic<float>* pAmpBPresence = nullptr;
    std::atomic<float>* pAmpBMaster = nullptr;
    std::atomic<float>* pAmpBExtra = nullptr;
    std::atomic<float>* pAmpBSwitch = nullptr;

    std::atomic<float>* pAmpCBypass = nullptr;
    std::atomic<float>* pAmpCChannel = nullptr;
    std::atomic<float>* pAmpCModel = nullptr;
    std::atomic<float>* pAmpCGain = nullptr;
    std::atomic<float>* pAmpCBass = nullptr;
    std::atomic<float>* pAmpCMid = nullptr;
    std::atomic<float>* pAmpCTreble = nullptr;
    std::atomic<float>* pAmpCPresence = nullptr;
    std::atomic<float>* pAmpCMaster = nullptr;
    std::atomic<float>* pAmpCExtra = nullptr;
    std::atomic<float>* pAmpCSwitch = nullptr;

    std::atomic<float>* pCabBypass = nullptr;
    std::atomic<float>* pCabModel = nullptr;
    std::atomic<float>* pCabLowCut = nullptr;
    std::atomic<float>* pCabHighCut = nullptr;
    std::atomic<float>* pCabLevel = nullptr;

    std::atomic<float>* pCabBBypass = nullptr;
    std::atomic<float>* pCabBModel = nullptr;
    std::atomic<float>* pCabBLowCut = nullptr;
    std::atomic<float>* pCabBHighCut = nullptr;
    std::atomic<float>* pCabBLevel = nullptr;

    std::atomic<float>* pCabCBypass = nullptr;
    std::atomic<float>* pCabCModel = nullptr;
    std::atomic<float>* pCabCLowCut = nullptr;
    std::atomic<float>* pCabCHighCut = nullptr;
    std::atomic<float>* pCabCLevel = nullptr;

    std::atomic<float>* pEqBypass = nullptr;
    std::atomic<float>* pEqModel = nullptr;
    std::array<std::atomic<float>*, GraphicEq::kNumBands> pEqBands {};
    std::atomic<float>* pEqLevel = nullptr;

    std::atomic<float>* pChorusBypass = nullptr;
    std::atomic<float>* pChorusModel = nullptr;
    std::atomic<float>* pChorusRate = nullptr;
    std::atomic<float>* pChorusDepth = nullptr;
    std::atomic<float>* pChorusDelay = nullptr;
    std::atomic<float>* pChorusMix = nullptr;

    std::atomic<float>* pDelayBypass = nullptr;
    std::atomic<float>* pDelayModel = nullptr;
    std::atomic<float>* pDelayTime = nullptr;
    std::atomic<float>* pDelayFeedback = nullptr;
    std::atomic<float>* pDelayMix = nullptr;
    std::atomic<float>* pDelayPingPong = nullptr;

    std::atomic<float>* pReverbBypass = nullptr;
    std::atomic<float>* pReverbModel = nullptr;
    std::atomic<float>* pReverbSize = nullptr;
    std::atomic<float>* pReverbDamping = nullptr;
    std::atomic<float>* pReverbMix = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE(PedalBoardAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalBoardAudioProcessor)
};

} // namespace pedalboard
