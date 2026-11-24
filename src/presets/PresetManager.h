#pragma once

#include "../engine/MidiLearnManager.h"
#include "../engine/ModuleId.h"
#include "../plugin/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <map>
#include <vector>

namespace pedalboard {

struct PresetData {
    juce::String name;
    juce::String style { "Custom" };
    juce::String character { "Balanced" };
    juce::String pickup { "Any" };
    juce::String description;
    bool isFactory = false;
    bool clearStompboard = false;
    int factoryIndex = -1;
    juce::File sourceFile;

    ChainOrder chainOrder = defaultChainOrder();
    BusAssignments busAssignments = defaultBusAssignments();
    juce::String customIrPath;
    juce::String customIrBPath;
    juce::String customIrCPath;
    juce::String customNamPath;
    juce::String customNamBPath;
    juce::String customNamCPath;
    juce::var midiMappings;
    std::map<juce::String, float> values;

    bool isDualParallel() const {
        auto it = values.find(params::kRoutingMode);
        return it != values.end() && it->second > 0.5f;
    }

    bool usesNam() const {
        return customNamPath.isNotEmpty() || customNamBPath.isNotEmpty() || customNamCPath.isNotEmpty();
    }
};

// Clean-room JSON preset manager (v3 schema) with 8 factory presets, tag metadata,
// Dual Amp/Cab IR & NAM paths, MIDI CC bindings, and unified Factory + User library scanning.
class PresetManager {
public:
    PresetManager() {
        initFactoryPresets();
    }

    const std::vector<PresetData>& getFactoryPresets() const noexcept { return factoryPresets; }

    juce::File getUserPresetDirectory() const {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("PedalBoard")
                       .getChildFile("Presets");
        if (!dir.exists()) {
            dir.createDirectory();
        }
        return dir;
    }

    std::vector<PresetData> scanAllPresets() const {
        std::vector<PresetData> all = factoryPresets;
        const auto dir = getUserPresetDirectory();
        auto files = dir.findChildFiles(juce::File::findFiles, true, "*.pbpreset");
        files.sort();
        for (const auto& f : files) {
            PresetData loaded;
            if (loadPresetFromFile(f, loaded)) {
                loaded.isFactory = false;
                loaded.factoryIndex = -1;
                loaded.sourceFile = f;
                all.push_back(std::move(loaded));
            }
        }
        return all;
    }

    bool savePresetToFile(const juce::File& file,
                          const juce::String& presetName,
                          const juce::String& style,
                          const juce::String& character,
                          const juce::String& description,
                          const juce::AudioProcessorValueTreeState& apvts,
                          const ChainOrder& order,
                          const BusAssignments& buses,
                          const juce::String& customIrPath,
                          const juce::String& customIrBPath,
                          const juce::String& customNamPath,
                          const juce::String& customNamBPath,
                          const MidiLearnManager& midiLearn,
                          const juce::String& customIrCPath = {},
                          const juce::String& customNamCPath = {}) const {
        auto* root = new juce::DynamicObject();
        root->setProperty("schemaVersion", 3);
        root->setProperty("name", presetName.isNotEmpty() ? presetName : file.getFileNameWithoutExtension());
        root->setProperty("style", style.isNotEmpty() ? style : "Custom");
        root->setProperty("character", character.isNotEmpty() ? character : "User");
        root->setProperty("pickup", "Any");
        root->setProperty("description", description);
        root->setProperty("customIrPath", customIrPath);
        root->setProperty("customIrBPath", customIrBPath);
        root->setProperty("customIrCPath", customIrCPath);
        root->setProperty("customNamPath", customNamPath);
        root->setProperty("customNamBPath", customNamBPath);
        root->setProperty("customNamCPath", customNamCPath);

        juce::Array<juce::var> chainArr;
        for (const auto mod : order) {
            chainArr.add(juce::String(moduleKey(mod)));
        }
        root->setProperty("chainOrder", chainArr);

        auto* busObj = new juce::DynamicObject();
        for (int i = 0; i < kNumModules; ++i) {
            const auto mod = static_cast<ModuleId>(i);
            busObj->setProperty(moduleKey(mod), juce::String(busLaneKey(buses[static_cast<size_t>(i)])));
        }
        root->setProperty("busAssignments", juce::var(busObj));
        root->setProperty("midiMappings", midiLearn.toJsonArray());

        auto* paramsObj = new juce::DynamicObject();
        for (auto* param : apvts.processor.getParameters()) {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param)) {
                const auto val = ranged->convertFrom0to1(ranged->getValue());
                paramsObj->setProperty(ranged->getParameterID(), static_cast<double>(val));
            }
        }
        root->setProperty("parameters", juce::var(paramsObj));

        const juce::String jsonText = juce::JSON::toString(juce::var(root), true);
        return file.replaceWithText(jsonText);
    }

    bool loadPresetFromFile(const juce::File& file, PresetData& outPreset) const {
        if (!file.existsAsFile()) {
            return false;
        }
        const juce::var parsed = juce::JSON::parse(file.loadFileAsString());
        if (!parsed.isObject()) {
            return false;
        }
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr) {
            return false;
        }

        PresetData data;
        data.sourceFile = file;
        data.name = obj->getProperty("name").toString();
        if (data.name.isEmpty()) {
            data.name = file.getFileNameWithoutExtension();
        }
        data.style = obj->getProperty("style").toString();
        if (data.style.isEmpty()) data.style = "Custom";
        data.character = obj->getProperty("character").toString();
        if (data.character.isEmpty()) data.character = "User";
        data.pickup = obj->getProperty("pickup").toString();
        if (data.pickup.isEmpty()) data.pickup = "Any";
        data.description = obj->getProperty("description").toString();
        data.customIrPath = obj->getProperty("customIrPath").toString();
        data.customIrBPath = obj->getProperty("customIrBPath").toString();
        data.customIrCPath = obj->getProperty("customIrCPath").toString();
        data.customNamPath = obj->getProperty("customNamPath").toString();
        data.customNamBPath = obj->getProperty("customNamBPath").toString();
        data.customNamCPath = obj->getProperty("customNamCPath").toString();
        data.midiMappings = obj->getProperty("midiMappings");

        const auto chainVar = obj->getProperty("chainOrder");
        if (auto* arr = chainVar.getArray(); arr != nullptr && arr->size() == kNumModules) {
            ChainOrder parsedOrder {};
            std::array<bool, kNumModules> seen {};
            bool valid = true;
            for (int i = 0; i < kNumModules; ++i) {
                ModuleId id {};
                if (!moduleFromKey((*arr)[i].toString(), id) || seen[static_cast<size_t>(id)]) {
                    valid = false;
                    break;
                }
                seen[static_cast<size_t>(id)] = true;
                parsedOrder[static_cast<size_t>(i)] = id;
            }
            if (valid) {
                data.chainOrder = parsedOrder;
            }
        }

        const auto busVar = obj->getProperty("busAssignments");
        if (auto* bObj = busVar.getDynamicObject()) {
            BusAssignments parsedBuses = defaultBusAssignments();
            for (int i = 0; i < kNumModules; ++i) {
                const auto mod = static_cast<ModuleId>(i);
                const auto key = juce::Identifier(moduleKey(mod));
                if (bObj->hasProperty(key)) {
                    parsedBuses[static_cast<size_t>(i)] = busLaneFromKey(bObj->getProperty(key).toString());
                }
            }
            data.busAssignments = parsedBuses;
        }

        const auto paramsVar = obj->getProperty("parameters");
        if (auto* pObj = paramsVar.getDynamicObject()) {
            for (const auto& prop : pObj->getProperties()) {
                data.values[prop.name.toString()] = static_cast<float>(static_cast<double>(prop.value));
            }
        }

        outPreset = std::move(data);
        return true;
    }

    static void applyPresetToProcessor(const PresetData& preset,
                                       juce::AudioProcessorValueTreeState& apvts) {
        for (const auto& [paramId, realValue] : preset.values) {
            if (auto* param = apvts.getParameter(paramId)) {
                const float norm = param->convertTo0to1(realValue);
                param->beginChangeGesture();
                param->setValueNotifyingHost(norm);
                param->endChangeGesture();
            }
        }
    }

private:
    static std::map<juce::String, float> baseDefaults() {
        using namespace params;
        return {
            { kRoutingMode, 0.0f }, { kPathALevel, 0.0f }, { kPathAPan, 0.0f }, { kPathAMute, 0.0f },
            { kPathBLevel, -4.0f }, { kPathBPan, 0.0f },   { kPathBMute, 0.0f }, { kPathBPhase, 0.0f },
            { kGateBypass, 0.0f },  { kGateModel, 0.0f },  { kGateThreshold, -54.0f }, { kGateRelease, 110.0f }, { kGateDepth, -60.0f },
            { kCompBypass, 1.0f },  { kCompModel, 0.0f },  { kCompThreshold, -22.0f }, { kCompRatio, 4.0f }, { kCompAttack, 10.0f }, { kCompRelease, 120.0f }, { kCompMakeup, 4.0f },
            { kWahBypass, 1.0f },   { kWahModel, 0.0f },   { kWahPedal, 0.5f }, { kWahQ, 4.2f }, { kWahSensitivity, 0.6f }, { kWahMix, 1.0f }, { kWahAuto, 0.0f },
            { kOdBypass, 1.0f },    { kOdModel, 0.0f },    { kOdDrive, 3.5f }, { kOdTone, 5.8f }, { kOdLevel, 5.5f },
            { kAmpBypass, 0.0f },   { kAmpChannel, 1.0f }, { kAmpModel, 39.0f }, { kAmpGain, 5.5f }, { kAmpBass, 5.5f }, { kAmpMid, 5.2f }, { kAmpTreble, 5.8f }, { kAmpPresence, 5.2f }, { kAmpMaster, 6.0f },
            { kAmpBBypass, 0.0f },  { kAmpBChannel, 0.0f },{ kAmpBModel, 3.0f }, { kAmpBGain, 3.4f }, { kAmpBBass, 5.6f }, { kAmpBMid, 5.0f }, { kAmpBTreble, 6.6f }, { kAmpBPresence, 5.8f }, { kAmpBMaster, 6.5f },
            { kCabBypass, 0.0f },   { kCabModel, 14.0f },  { kCabLowCut, 85.0f }, { kCabHighCut, 7200.0f }, { kCabLevel, 0.0f },
            { kCabBBypass, 0.0f },  { kCabBModel, 7.0f },  { kCabBLowCut, 95.0f }, { kCabBHighCut, 8400.0f }, { kCabBLevel, 0.0f },
            { kEqBypass, 1.0f },    { kEqModel, 4.0f },    { kEq100, 0.0f }, { kEq200, 0.0f }, { kEq400, 0.0f }, { kEq800, 0.0f }, { kEq1600, 0.0f }, { kEq3200, 0.0f }, { kEq6400, 0.0f }, { kEqLevel, 0.0f },
            { kChorusBypass, 1.0f },{ kChorusModel, 1.0f },{ kChorusRate, 1.2f }, { kChorusDepth, 0.45f }, { kChorusDelay, 10.0f }, { kChorusMix, 0.40f },
            { kDelayBypass, 1.0f }, { kDelayModel, 16.0f },{ kDelayTime, 360.0f }, { kDelayFeedback, 0.35f }, { kDelayMix, 0.25f }, { kDelayPingPong, 1.0f },
            { kReverbBypass, 0.0f },{ kReverbModel, 3.0f },{ kReverbSize, 0.42f }, { kReverbDamping, 0.50f }, { kReverbMix, 0.18f }
        };
    }

    void addFactory(PresetData p) {
        p.isFactory = true;
        p.factoryIndex = static_cast<int>(factoryPresets.size());
        factoryPresets.push_back(std::move(p));
    }

    void initFactoryPresets() {
        using namespace params;

        // 0. Clean Init (Default startup: ZERO stompboxes, ALL rack FX OFF, Clean Amp + matching Clean Cab only!)
        {
            PresetData p;
            p.name = "Clean Init";
            p.style = "Clean";
            p.character = "Clean";
            p.pickup = "Any";
            p.description = "Pure clean Fender '65 Twin Reverb amp and matching 2x12 cab with zero effects loaded.";
            p.clearStompboard = true;
            p.values = baseDefaults();
            p.values[kGateBypass] = 1.0f;
            p.values[kCompBypass] = 1.0f;
            p.values[kWahBypass] = 1.0f;
            p.values[kOdBypass] = 1.0f;
            p.values[kEqBypass] = 1.0f;
            p.values[kChorusBypass] = 1.0f;
            p.values[kDelayBypass] = 1.0f;
            p.values[kReverbBypass] = 1.0f;
            p.values[kAmpBypass] = 0.0f;
            p.values[kAmpChannel] = 0.0f;
            p.values[kAmpModel] = 3.0f;   // Dark Twin (Fender '65 Twin Reverb Clean)
            p.values[kAmpGain] = 3.8f;
            p.values[kAmpBass] = 5.4f;
            p.values[kAmpMid] = 5.2f;
            p.values[kAmpTreble] = 6.2f;
            p.values[kAmpPresence] = 5.4f;
            p.values[kAmpMaster] = 6.5f;
            p.values[kCabBypass] = 0.0f;
            p.values[kCabModel] = 7.0f;   // Dark Twin 2x12"
            p.values[kCabLowCut] = 80.0f;
            p.values[kCabHighCut] = 8800.0f;
            p.values[kCabLevel] = 0.0f;
            addFactory(std::move(p));
        }

        // 1. Clean Sparkle
        {
            PresetData p;
            p.name = "Clean Sparkle";
            p.style = "Clean";
            p.character = "Clean";
            p.pickup = "Single-Coil";
            p.description = "Compressed glassy Silver Twin clean tone with C-Chorus and Hall reverb.";
            p.values = baseDefaults();
            p.values[kGateThreshold] = -64.0f;
            p.values[kCompBypass] = 0.0f; p.values[kCompModel] = 0.0f; p.values[kCompThreshold] = -24.0f; p.values[kCompRatio] = 3.5f; p.values[kCompMakeup] = 5.0f;
            p.values[kAmpChannel] = 0.0f; p.values[kAmpModel] = 6.0f; p.values[kAmpGain] = 3.0f; p.values[kAmpTreble] = 6.8f; p.values[kAmpMaster] = 6.8f;
            p.values[kCabModel] = 7.0f;   p.values[kCabHighCut] = 9400.0f;
            p.values[kChorusBypass] = 0.0f; p.values[kChorusModel] = 1.0f; p.values[kChorusRate] = 0.9f; p.values[kChorusMix] = 0.32f;
            p.values[kReverbModel] = 1.0f;  p.values[kReverbSize] = 0.56f; p.values[kReverbMix] = 0.28f;
            addFactory(std::move(p));
        }

        // 2. Funk Auto-Wah
        {
            PresetData p;
            p.name = "Funk Auto-Wah";
            p.style = "Funk";
            p.character = "Auto-Wah";
            p.pickup = "Single-Coil";
            p.description = "Snappy COMP4 + C-Wah envelope auto-wah into Dark Twin for percussive funk.";
            p.values = baseDefaults();
            p.values[kCompBypass] = 0.0f; p.values[kCompModel] = 1.0f; p.values[kCompThreshold] = -22.0f; p.values[kCompRatio] = 5.0f; p.values[kCompAttack] = 6.0f; p.values[kCompMakeup] = 6.0f;
            p.values[kWahBypass] = 0.0f;  p.values[kWahModel] = 1.0f; p.values[kWahAuto] = 1.0f; p.values[kWahQ] = 5.2f; p.values[kWahSensitivity] = 0.72f;
            p.values[kAmpChannel] = 0.0f; p.values[kAmpModel] = 3.0f; p.values[kAmpGain] = 4.2f; p.values[kAmpMid] = 6.2f; p.values[kAmpTreble] = 6.6f;
            p.values[kCabModel] = 7.0f;
            p.values[kReverbModel] = 0.0f; p.values[kReverbSize] = 0.30f; p.values[kReverbMix] = 0.14f;
            addFactory(std::move(p));
        }

        // 3. Classic Crunch
        {
            PresetData p;
            p.name = "Classic Crunch";
            p.style = "Rock";
            p.character = "Crunch";
            p.pickup = "Humbucker";
            p.description = "Green OD (TS-808) driving a UK SLP (Marshall Plexi) & UK GRN 2 Greenback 4x12.";
            p.values = baseDefaults();
            p.values[kOdBypass] = 0.0f; p.values[kOdModel] = 0.0f; p.values[kOdDrive] = 3.6f; p.values[kOdTone] = 5.8f; p.values[kOdLevel] = 5.6f;
            p.values[kAmpChannel] = 1.0f; p.values[kAmpModel] = 39.0f; p.values[kAmpGain] = 5.8f; p.values[kAmpMid] = 5.8f; p.values[kAmpTreble] = 6.0f;
            p.values[kCabModel] = 17.0f;
            p.values[kReverbModel] = 3.0f; p.values[kReverbSize] = 0.38f; p.values[kReverbMix] = 0.16f;
            addFactory(std::move(p));
        }

        // 4. Modern High-Gain
        {
            PresetData p;
            p.name = "Modern High-Gain";
            p.style = "Metal";
            p.character = "Hi-Gain";
            p.pickup = "Humbucker";
            p.description = "Gate 1 + Precise OD + Mess4 LD 3 (Mark IV Lead) + Mess 4x12 + Mess EQ V-curve.";
            p.values = baseDefaults();
            p.values[kGateModel] = 0.0f; p.values[kGateThreshold] = -46.0f; p.values[kGateRelease] = 65.0f; p.values[kGateDepth] = -74.0f;
            p.values[kOdBypass] = 0.0f;  p.values[kOdModel] = 13.0f; p.values[kOdDrive] = 1.8f; p.values[kOdTone] = 7.0f; p.values[kOdLevel] = 7.2f;
            p.values[kAmpChannel] = 2.0f; p.values[kAmpModel] = 51.0f; p.values[kAmpGain] = 7.4f; p.values[kAmpBass] = 6.0f; p.values[kAmpMid] = 4.5f; p.values[kAmpTreble] = 6.8f; p.values[kAmpPresence] = 6.5f;
            p.values[kCabModel] = 28.0f; p.values[kCabLowCut] = 95.0f; p.values[kCabHighCut] = 6400.0f;
            p.values[kEqBypass] = 0.0f;  p.values[kEqModel] = 4.0f; p.values[kEq100] = 2.0f; p.values[kEq400] = -2.8f; p.values[kEq800] = -2.2f; p.values[kEq1600] = 1.8f; p.values[kEq3200] = 2.5f;
            p.values[kReverbModel] = 5.0f; p.values[kReverbSize] = 0.24f; p.values[kReverbMix] = 0.09f;
            addFactory(std::move(p));
        }

        // 5. Parallel Clean + Dirt (Dual Amp A Crunch + Amp B Clean!)
        {
            PresetData p;
            p.name = "Parallel Clean + Dirt";
            p.style = "Fusion";
            p.character = "Dual Rig";
            p.pickup = "Any";
            p.description = "Dual Parallel Rig: Crunch Amp A (4x12) blended with Clean Amp B (2x12 Open + Chorus).";
            p.busAssignments = defaultBusAssignments();
            p.values = baseDefaults();
            p.values[kRoutingMode] = 1.0f;
            p.values[kPathALevel] = 0.0f;  p.values[kPathAPan] = -0.35f;
            p.values[kPathBLevel] = -3.0f; p.values[kPathBPan] = 0.35f;
            p.values[kCompBypass] = 0.0f; p.values[kCompThreshold] = -24.0f; p.values[kCompRatio] = 3.5f; p.values[kCompMakeup] = 4.0f;
            p.values[kOdBypass] = 0.0f; p.values[kOdDrive] = 4.4f;
            p.values[kAmpChannel] = 1.0f; p.values[kAmpGain] = 6.2f;
            p.values[kAmpBBypass] = 0.0f; p.values[kAmpBChannel] = 0.0f; p.values[kAmpBGain] = 3.2f; p.values[kAmpBTreble] = 7.0f;
            p.values[kCabBBypass] = 0.0f;
            p.values[kChorusBypass] = 0.0f; p.values[kChorusRate] = 1.0f; p.values[kChorusDepth] = 0.55f; p.values[kChorusMix] = 0.50f;
            p.values[kDelayBypass] = 0.0f; p.values[kDelayTime] = 390.0f; p.values[kDelayMix] = 0.20f;
            p.values[kReverbSize] = 0.56f; p.values[kReverbMix] = 0.24f;
            addFactory(std::move(p));
        }

        // 6. Dual Stereo Wall (A+B) (Wide Double-Tracked High-Gain + Crunch Stereo Rig!)
        {
            PresetData p;
            p.name = "Dual Stereo Wall (A+B)";
            p.style = "Metal";
            p.character = "Dual Rig";
            p.pickup = "Humbucker";
            p.description = "Hard-panned Dual Amp Wall: High-Gain Amp A + 4x12 (L) vs Tight Crunch Amp B + 2x12 (R).";
            p.busAssignments = defaultBusAssignments();
            p.values = baseDefaults();
            p.values[kRoutingMode] = 1.0f;
            p.values[kPathALevel] = 0.0f;  p.values[kPathAPan] = -0.75f;
            p.values[kPathBLevel] = -0.5f; p.values[kPathBPan] = 0.75f;
            p.values[kGateThreshold] = -47.0f; p.values[kGateRelease] = 70.0f;
            p.values[kOdBypass] = 0.0f; p.values[kOdDrive] = 2.2f; p.values[kOdTone] = 6.8f; p.values[kOdLevel] = 6.8f;
            p.values[kAmpChannel] = 2.0f; p.values[kAmpGain] = 7.2f; p.values[kAmpMid] = 4.6f; p.values[kAmpTreble] = 6.6f;
            p.values[kAmpBBypass] = 0.0f; p.values[kAmpBChannel] = 1.0f; p.values[kAmpBGain] = 8.4f; p.values[kAmpBMid] = 6.4f; p.values[kAmpBTreble] = 6.2f;
            p.values[kCabBBypass] = 0.0f; p.values[kCabBLowCut] = 100.0f; p.values[kCabBHighCut] = 6800.0f;
            p.values[kEqBypass] = 0.0f; p.values[kEq200] = -1.5f; p.values[kEq1600] = 2.2f;
            p.values[kReverbSize] = 0.28f; p.values[kReverbMix] = 0.11f;
            addFactory(std::move(p));
        }

        // 7. Ambient Stereo Lead
        {
            PresetData p;
            p.name = "Ambient Stereo Lead";
            p.style = "Lead";
            p.character = "Ambient";
            p.pickup = "Humbucker";
            p.description = "Singing lead gain into stereo chorus, ping-pong delay and wide reverb.";
            p.values = baseDefaults();
            p.values[kCompBypass] = 0.0f; p.values[kCompThreshold] = -20.0f; p.values[kCompRatio] = 3.0f; p.values[kCompMakeup] = 3.0f;
            p.values[kOdBypass] = 0.0f; p.values[kOdDrive] = 5.2f; p.values[kOdTone] = 5.6f; p.values[kOdLevel] = 6.0f;
            p.values[kAmpChannel] = 2.0f; p.values[kAmpGain] = 6.2f; p.values[kAmpMid] = 6.6f;
            p.values[kChorusBypass] = 0.0f; p.values[kChorusRate] = 0.8f; p.values[kChorusMix] = 0.25f;
            p.values[kDelayBypass] = 0.0f; p.values[kDelayTime] = 440.0f; p.values[kDelayFeedback] = 0.48f; p.values[kDelayMix] = 0.34f; p.values[kDelayPingPong] = 1.0f;
            p.values[kReverbSize] = 0.78f; p.values[kReverbDamping] = 0.36f; p.values[kReverbMix] = 0.36f;
            addFactory(std::move(p));
        }

        // 8. Shimmer Ambient Swell
        {
            PresetData p;
            p.name = "Dreamy Ambient Pad";
            p.style = "Clean";
            p.character = "Ambient";
            p.pickup = "Any";
            p.description = "Warm clean tone drenched in deep chorus, dotted ping-pong delay and huge reverb.";
            p.values = baseDefaults();
            p.values[kGateBypass] = 1.0f;
            p.values[kCompBypass] = 0.0f; p.values[kCompThreshold] = -26.0f; p.values[kCompRatio] = 4.5f; p.values[kCompMakeup] = 5.5f;
            p.values[kAmpChannel] = 0.0f; p.values[kAmpGain] = 2.6f; p.values[kAmpTreble] = 6.2f;
            p.values[kChorusBypass] = 0.0f; p.values[kChorusRate] = 0.65f; p.values[kChorusDepth] = 0.72f; p.values[kChorusMix] = 0.48f;
            p.values[kDelayBypass] = 0.0f; p.values[kDelayTime] = 520.0f; p.values[kDelayFeedback] = 0.58f; p.values[kDelayMix] = 0.42f; p.values[kDelayPingPong] = 1.0f;
            p.values[kReverbSize] = 0.88f; p.values[kReverbDamping] = 0.32f; p.values[kReverbMix] = 0.48f;
            addFactory(std::move(p));
        }
    }

    std::vector<PresetData> factoryPresets;
};

} // namespace pedalboard
