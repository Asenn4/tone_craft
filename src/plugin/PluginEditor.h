#pragma once

#include "../cloud/Tone3000Client.h"
#include "../ui/AmpHeadView.h"
#include "../ui/CabRoomView.h"
#include "../ui/GearSidebar.h"
#include "../ui/LooperRecorderBar.h"
#include "../ui/MeterAndTunerBar.h"
#include "../ui/PedalLookAndFeel.h"
#include "../ui/PresetBrowserOverlay.h"
#include "../ui/RackAndMixerView.h"
#include "../ui/SignalChainStrip.h"
#include "../ui/StageNavBar.h"
#include "../ui/StompBoardView.h"
#include "../ui/StudioSettingsWindow.h"
#include "../ui/Tone3000PopupWindow.h"
#include "PluginProcessor.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include <memory>

namespace pedalboard {

class PedalBoardAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::DragAndDropContainer,
                                       private juce::Timer {
public:
    explicit PedalBoardAudioProcessorEditor(PedalBoardAudioProcessor&);
    ~PedalBoardAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    static void applyDarkTitleBarToWindow(juce::DocumentWindow& win);
    void applyNativeWindowFrame();
    void openStudioSettings(StudioSettingsContent::Tab initialTab = StudioSettingsContent::Tab::AudioIo);
    void handleSidebarDrag(bool active,
                           bool dropped,
                           ModuleId mod,
                           int modelIdx,
                           const juce::String& name,
                           juce::Colour col,
                           juce::Point<int> screenPos);
    void timerCallback() override;
    void selectStageTab(StageViewTab tab, int rigIndex = -2);
    void togglePresetBrowser();
    void openTone3000Browser(CloudLoadTargetKind kind, int rigOrSlotIndex = 0, const juce::String& searchQuery = {});
    void handleCloudCaptureLoaded(const CloudLoadRequest& req);
    void toggleGearSidebar();
    void handleSavePreset();
    void handleLoadPreset();
    void handleLoadCabinetIR(int rigIndex);
    void handleLoadNamModel(int rigIndex);
    void handleLoadSlotNamModel(int slotId);

    PedalBoardAudioProcessor& processor;
    juce::OpenGLContext openGLContext;
    PedalLookAndFeel lookAndFeel;

    std::unique_ptr<TopRackBar> topBar;
    std::unique_ptr<StageNavBar> stageNavBar;
    juce::TooltipWindow tooltipWindow { this, 520 };

    // 4 Photorealistic Native Hardware Hero Views
    std::unique_ptr<StompBoardView> stompView;
    std::unique_ptr<AmpHeadView> ampView;
    std::unique_ptr<CabRoomView> cabView;
    std::unique_ptr<RackAndMixerView> rackView;

    // Docked Right-Side AmpliTube-style Gear Browser Sidebar
    std::unique_ptr<GearSidebar> gearSidebar;
    bool sidebarVisible = true;

    std::unique_ptr<SignalChainStrip> signalChainStrip;
    std::unique_ptr<LooperRecorderBar> looperBar;
    std::unique_ptr<PresetBrowserOverlay> browserOverlay;
    std::unique_ptr<t3k::Tone3000Client> tone3000Client;
    std::unique_ptr<Tone3000PopupWindow> cloudWindow;
    std::unique_ptr<StudioSettingsWindow> settingsWindow;

    std::unique_ptr<juce::FileChooser> fileChooser;
    StageViewTab activeTab = StageViewTab::Stomp;
    uint64_t lastObservedStateVersion = 0;
    RoutingMode lastObservedRoutingMode = RoutingMode::Serial;
    bool nativeFrameApplied = false;

    bool sidebarDragActive = false;
    ModuleId sidebarDragModule = ModuleId::Overdrive;
    int sidebarDragModelIdx = 0;
    juce::String sidebarDragName;
    juce::Colour sidebarDragColour;
    juce::Point<int> sidebarDragLocalPos {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalBoardAudioProcessorEditor)
};

} // namespace pedalboard
