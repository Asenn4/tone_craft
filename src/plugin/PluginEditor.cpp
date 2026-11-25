#include "PluginEditor.h"

#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace pedalboard {

PedalBoardAudioProcessorEditor::PedalBoardAudioProcessorEditor(PedalBoardAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p) {
    setLookAndFeel(&lookAndFeel);

    // Hardware-accelerated GPU OpenGL 2D compositing (zero webview, crisp PNG plates & 64-frame PBR filmstrips!)
    openGLContext.setComponentPaintingEnabled(true);
    openGLContext.setContinuousRepainting(false);
    openGLContext.attachTo(*this);

    // 1. Sleek Top Studio Bar (with integrated Studio Settings console!)
    topBar = std::make_unique<TopRackBar>(
        processor,
        [this] { togglePresetBrowser(); },
        [this] { handleSavePreset(); },
        [this] { handleLoadPreset(); },
        [this] { openStudioSettings(StudioSettingsContent::Tab::AudioIo); }
    );
    addAndMakeVisible(*topBar);

    // 2. Architectural Stage Navigation Bar (Flat underline tabs + 1-click Serial/Dual A/B + TONE3000 + Gear drawer toggle)
    stageNavBar = std::make_unique<StageNavBar>(
        processor,
        [this](StageViewTab tab) { selectStageTab(tab); },
        [this] { toggleGearSidebar(); },
        [this] { openTone3000Browser(CloudLoadTargetKind::Auto, 0); }
    );
    addAndMakeVisible(*stageNavBar);
    setWantsKeyboardFocus(true);

    // 3. Four Dedicated Photorealistic Native Hardware Views
    stompView = std::make_unique<StompBoardView>(processor);
    stompView->setLaneChangedCallback([this](int laneFilter) {
        if (signalChainStrip != nullptr) {
            signalChainStrip->setHighlightedStompLane(laneFilter);
        }
    });
    stompView->setLoadSlotNamCallback([this](int slotId) { handleLoadSlotNamModel(slotId); });
    stompView->setBrowseSlotNamCloudCallback([this](int slotId) { openTone3000Browser(CloudLoadTargetKind::NamPedal, slotId); });
    ampView = std::make_unique<AmpHeadView>(
        processor,
        [this](int rigIdx) { handleLoadNamModel(rigIdx); },
        [this](int rigIdx) { processor.clearNamModel(rigIdx); }
    );
    ampView->setBrowseCloudNamCallback([this](int rigIdx) { openTone3000Browser(CloudLoadTargetKind::AmpRig, rigIdx); });
    ampView->setFindPackOnCloudCallback([this](int rigIdx, const juce::String& query) {
        openTone3000Browser(CloudLoadTargetKind::AmpRig, rigIdx, query);
    });
    cabView = std::make_unique<CabRoomView>(
        processor,
        [this](int rigIdx) { handleLoadCabinetIR(rigIdx); },
        [this](int rigIdx) { processor.resetCabinetIR(rigIdx); }
    );
    cabView->setBrowseCloudIrCallback([this](int rigIdx) { openTone3000Browser(CloudLoadTargetKind::CabRig, rigIdx); });
    cabView->setFindPackOnCloudCallback([this](int rigIdx, const juce::String& query, const juce::String& extUrl) {
        if (extUrl.isNotEmpty()) {
            juce::URL(extUrl).launchInDefaultBrowser();
        } else {
            openTone3000Browser(CloudLoadTargetKind::CabRig, rigIdx, query);
        }
    });
    rackView = std::make_unique<RackAndMixerView>(processor);

    addChildComponent(*stompView);
    addChildComponent(*ampView);
    addChildComponent(*cabView);
    addChildComponent(*rackView);

    // 4. Right-Side Gear Browser Sidebar (matching Reference Screenshot 2 + direct drag-and-drop!)
    gearSidebar = std::make_unique<GearSidebar>(
        processor,
        [this](StageViewTab tab, int rigIdx) { selectStageTab(tab, rigIdx); }
    );
    gearSidebar->setSidebarDragCallback(
        [this](bool active, bool dropped, ModuleId mod, int modelIdx,
               const juce::String& name, juce::Colour col, juce::Point<int> screenPos) {
            handleSidebarDrag(active, dropped, mod, modelIdx, name, col, screenPos);
        }
    );
    gearSidebar->setInsertLaneProvider([this](ModuleId cat) {
        return stompView != nullptr ? stompView->getEffectiveInsertLane(cat) : -1;
    });
    addAndMakeVisible(*gearSidebar);

    // 5. Matte-Black Amber Signal Chain Strip (matching Reference Screenshot 1!)
    signalChainStrip = std::make_unique<SignalChainStrip>(
        processor,
        [this](StageViewTab tab, int rigIdx) { selectStageTab(tab, rigIdx); }
    );
    addAndMakeVisible(*signalChainStrip);

    // 6. Bottom Looper & 24-Bit WAV Recorder Studio Dock
    looperBar = std::make_unique<LooperRecorderBar>(processor);
    addAndMakeVisible(*looperBar);

    // 7. Slide-in Tag Preset Browser Drawer Overlay
    browserOverlay = std::make_unique<PresetBrowserOverlay>(
        processor,
        [this] { browserOverlay->setVisible(false); }
    );
    addChildComponent(*browserOverlay);

    // 8. Native TONE3000 API v1 Client (floating WebView2 popup window is created lazily on first open!)
    tone3000Client = std::make_unique<t3k::Tone3000Client>();
    stageNavBar->setCloudConnected(tone3000Client->isConnected());

    // Default open on the iconic StompBoard + Signal Chain + Right Gear Sidebar!
    selectStageTab(StageViewTab::Stomp);

    // Compute a smart default size that always fits comfortably inside the user's monitor (including 125%/150% DPI!)
    int initW = 1320;
    int initH = 740;
    if (const auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
        const auto ua = disp->userArea;
        initW = std::clamp(ua.getWidth() - 100, 1024, 1340);
        initH = std::clamp(ua.getHeight() - 110, 600, 750);
    }

    setResizable(true, true);
    setResizeLimits(1024, 600, 4096, 2400);
    setSize(initW, initH);

    lastObservedStateVersion = processor.getUiStateVersion();
    lastObservedRoutingMode = processor.getRoutingMode();
    topBar->syncFromProcessor();
    startTimerHz(28);

    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<PedalBoardAudioProcessorEditor>(this)] {
        if (safeThis != nullptr) {
            safeThis->applyNativeWindowFrame();
        }
    });
}

void PedalBoardAudioProcessorEditor::parentHierarchyChanged() {
    applyNativeWindowFrame();
}

void PedalBoardAudioProcessorEditor::applyDarkTitleBarToWindow(juce::DocumentWindow& win) {
#if JUCE_WINDOWS
    if (auto* peer = win.getPeer()) {
        if (auto* hwnd = static_cast<HWND>(peer->getNativeHandle())) {
            if (HMODULE dwm = LoadLibraryA("dwmapi.dll")) {
                using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
                if (auto fn = reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"))) {
                    const BOOL useDark = TRUE;
                    fn(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &useDark, sizeof(useDark));
                    fn(hwnd, 19 /*Win10 pre-2004 fallback*/, &useDark, sizeof(useDark));
                }
                FreeLibrary(dwm);
            }
        }
    }
#else
    juce::ignoreUnused(win);
#endif
}

void PedalBoardAudioProcessorEditor::applyNativeWindowFrame() {
    if (processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone || nativeFrameApplied) {
        return;
    }
    if (auto* topWin = dynamic_cast<juce::DocumentWindow*>(getTopLevelComponent())) {
        nativeFrameApplied = true;

        // Detach OpenGL cleanly before JUCE recreates the Win32 HWND for native title bar
        openGLContext.detach();

        topWin->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, false);
        if (!topWin->isUsingNativeTitleBar()) {
            topWin->setUsingNativeTitleBar(true);
        }
        topWin->setResizable(true, false);

        // Hide the old custom JUCE "Options" titlebar button now that we use the native Windows title bar
        for (int i = 0; i < topWin->getNumChildComponents(); ++i) {
            if (auto* btn = dynamic_cast<juce::TextButton*>(topWin->getChildComponent(i))) {
                if (btn->getButtonText() == "Options") {
                    btn->setVisible(false);
                }
            }
        }

        // Clamp and center window strictly inside userArea accounting for the Native Windows Title Bar frame!
        if (const auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
            const auto ua = disp->userArea;
            juce::BorderSize<int> frame(34, 8, 8, 8);
            if (auto* peer = topWin->getPeer()) {
                const auto f = peer->getFrameSize();
                if (f.getTop() > 0) frame = f;
            }
            const int maxClientW = std::max(1024, ua.getWidth() - frame.getLeftAndRight() - 24);
            const int maxClientH = std::max(600,  ua.getHeight() - frame.getTopAndBottom() - 24);
            const int targetW = std::min(topWin->getWidth(), maxClientW);
            const int targetH = std::min(topWin->getHeight(), maxClientH);

            const int totalW = targetW + frame.getLeftAndRight();
            const int totalH = targetH + frame.getTopAndBottom();
            const int outerX = ua.getX() + std::max(0, (ua.getWidth() - totalW) / 2);
            const int outerY = ua.getY() + std::max(8, (ua.getHeight() - totalH) / 2);

            topWin->setBounds(outerX + frame.getLeft(),
                              outerY + frame.getTop(),
                              targetW,
                              targetH);
        }

#if JUCE_WINDOWS
        // Enable sleek Windows 10/11 Dark Mode on the native OS title bar!
        if (auto* peer = topWin->getPeer()) {
            if (auto* hwnd = static_cast<HWND>(peer->getNativeHandle())) {
                if (HMODULE dwm = LoadLibraryA("dwmapi.dll")) {
                    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
                    if (auto fn = reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"))) {
                        const BOOL useDark = TRUE;
                        fn(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &useDark, sizeof(useDark));
                        fn(hwnd, 19 /*Win10 pre-2004 fallback*/, &useDark, sizeof(useDark));
                    }
                    FreeLibrary(dwm);
                }
            }
        }
#endif

        // Re-attach OpenGL to the newly created native Windows HWND
        openGLContext.setComponentPaintingEnabled(true);
        openGLContext.setContinuousRepainting(false);
        openGLContext.attachTo(*this);
        topWin->toFront(true);
    }
}

void PedalBoardAudioProcessorEditor::openStudioSettings(StudioSettingsContent::Tab initialTab) {
    if (settingsWindow == nullptr) {
        settingsWindow = std::make_unique<StudioSettingsWindow>(
            processor,
            tone3000Client.get(),
            &lookAndFeel,
            [this] { openTone3000Browser(CloudLoadTargetKind::Auto, 0); },
            [](juce::DocumentWindow& w) { applyDarkTitleBarToWindow(w); }
        );
    }

    if (settingsWindow->isVisible()) {
        settingsWindow->hideWindow();
        return;
    }

    settingsWindow->showCentered(this, initialTab);
}

PedalBoardAudioProcessorEditor::~PedalBoardAudioProcessorEditor() {
    stopTimer();
    settingsWindow.reset();
    cloudWindow.reset();
    tone3000Client.reset();
    openGLContext.detach();
    setLookAndFeel(nullptr);
}

void PedalBoardAudioProcessorEditor::selectStageTab(StageViewTab tab, int rigIndex) {
    activeTab = tab;

    stompView->setVisible(tab == StageViewTab::Stomp);
    ampView->setVisible(tab == StageViewTab::Amp);
    cabView->setVisible(tab == StageViewTab::Cab);
    rackView->setVisible(tab == StageViewTab::Rack);

    if (tab == StageViewTab::Stomp && rigIndex >= -1) {
        stompView->setActiveLaneFilter(rigIndex);
        if (signalChainStrip != nullptr) {
            signalChainStrip->setHighlightedStompLane(stompView->getActiveLaneFilter());
        }
    } else if (tab == StageViewTab::Amp && rigIndex >= 0) {
        ampView->selectRig(rigIndex);
    } else if (tab == StageViewTab::Cab && rigIndex >= 0) {
        cabView->selectRig(rigIndex);
    }

    if (stageNavBar != nullptr) {
        stageNavBar->setActiveTab(tab);
    }
    if (signalChainStrip != nullptr) {
        signalChainStrip->setActiveTab(tab);
    }
}

void PedalBoardAudioProcessorEditor::toggleGearSidebar() {
    sidebarVisible = !sidebarVisible;
    gearSidebar->setVisible(sidebarVisible);
    if (stageNavBar != nullptr) {
        stageNavBar->setSidebarOpen(sidebarVisible);
    }
    resized();
}

bool PedalBoardAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    // Do not hijack typing inside Search / Preset Name TextEditors
    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr) {
        return false;
    }

    if (key == juce::KeyPress::escapeKey) {
        if (settingsWindow != nullptr && settingsWindow->isVisible()) {
            settingsWindow->hideWindow();
            return true;
        }
        if (cloudWindow != nullptr && cloudWindow->isVisible()) {
            cloudWindow->setVisible(false);
            return true;
        }
        if (browserOverlay != nullptr && browserOverlay->isVisible()) {
            browserOverlay->setVisible(false);
            return true;
        }
        return false;
    }

    if (key == juce::KeyPress::spaceKey && looperBar != nullptr) {
        looperBar->triggerLooperRecPlayDub();
        return true;
    }

    const auto ch = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    if (ch == '1') { selectStageTab(StageViewTab::Stomp); return true; }
    if (ch == '2') { selectStageTab(StageViewTab::Amp);   return true; }
    if (ch == '3') { selectStageTab(StageViewTab::Cab);   return true; }
    if (ch == '4') { selectStageTab(StageViewTab::Rack);  return true; }
    if (ch == 'g') { toggleGearSidebar(); return true; }
    if (ch == 'p') { togglePresetBrowser(); return true; }
    if (ch == 't') { openTone3000Browser(CloudLoadTargetKind::Auto, 0); return true; }
    if (ch == 'b') { processor.triggerTapTempo(); if (stageNavBar != nullptr) stageNavBar->repaint(); return true; }
    if (ch == ',') { openStudioSettings(StudioSettingsContent::Tab::AudioIo); return true; }
    if (ch == 'm' && topBar != nullptr) { topBar->toggleTunerMute(); return true; }
    if (ch == 's' && looperBar != nullptr) { looperBar->triggerLooperStopToggle(); return true; }
    if (ch == '[' && topBar != nullptr) { topBar->stepFactoryPreset(-1); return true; }
    if (ch == ']' && topBar != nullptr) { topBar->stepFactoryPreset(+1); return true; }
    return false;
}

void PedalBoardAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0d1016));
}

void PedalBoardAudioProcessorEditor::paintOverChildren(juce::Graphics& g) {
    if (!sidebarDragActive) return;

    const auto ghost = juce::Rectangle<float>(
        static_cast<float>(sidebarDragLocalPos.x) - 82.0f,
        static_cast<float>(sidebarDragLocalPos.y) - 27.0f,
        164.0f, 54.0f
    );

    g.setColour(juce::Colours::black.withAlpha(0.60f));
    g.fillRoundedRectangle(ghost.translated(0.0f, 4.0f), 8.0f);

    g.setColour(juce::Colour(0xff131b2e).withAlpha(0.96f));
    g.fillRoundedRectangle(ghost, 8.0f);
    g.setColour(sidebarDragColour);
    g.drawRoundedRectangle(ghost, 8.0f, 2.0f);

    auto iconArea = juce::Rectangle<float>(ghost.getX() + 6.0f, ghost.getY() + 5.0f, 36.0f, 44.0f);
    valeton::drawStylizedPedalIcon(g, iconArea, sidebarDragModule, true, sidebarDragColour);

    auto textArea = ghost.withTrimmedLeft(46.0f).reduced(4.0f, 5.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(10.8f, juce::Font::bold)));
    g.drawText(sidebarDragName, textArea.withHeight(18.0f), juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xff38bdf8));
    g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
    const juce::String hint = (sidebarDragModule == ModuleId::AmpHead)
        ? "Drop to Load Amp"
        : ((sidebarDragModule == ModuleId::CabinetIR) ? "Drop to Load Cabinet" : "Drop on Board or Chain");
    g.drawText(hint, textArea.withTop(textArea.getY() + 20.0f), juce::Justification::centredLeft, true);
}

void PedalBoardAudioProcessorEditor::handleSidebarDrag(bool active,
                                                       bool dropped,
                                                       ModuleId mod,
                                                       int modelIdx,
                                                       const juce::String& name,
                                                       juce::Colour col,
                                                       juce::Point<int> screenPos) {
    sidebarDragLocalPos = getLocalPoint(nullptr, screenPos);
    sidebarDragModule   = mod;
    sidebarDragModelIdx = modelIdx;
    sidebarDragName     = name;
    sidebarDragColour   = col;
    sidebarDragActive   = active;

    const auto stompPt = stompView->getLocalPoint(nullptr, screenPos);
    const auto chainPt = signalChainStrip->getLocalPoint(nullptr, screenPos);
    const bool overStomp = stompView->getLocalBounds().contains(stompPt);
    const bool overChain = signalChainStrip->getLocalBounds().contains(chainPt);

    if (active) {
        // Auto-switch stage view when dragging onto stage area
        if (overStomp) {
            if (mod == ModuleId::AmpHead && activeTab != StageViewTab::Amp) {
                selectStageTab(StageViewTab::Amp);
            } else if (mod == ModuleId::CabinetIR && activeTab != StageViewTab::Cab) {
                selectStageTab(StageViewTab::Cab);
            } else if (mod != ModuleId::AmpHead && mod != ModuleId::CabinetIR && activeTab != StageViewTab::Stomp) {
                selectStageTab(StageViewTab::Stomp);
            }
        }
        stompView->setSidebarDragHover(overStomp && activeTab == StageViewTab::Stomp, stompPt, mod, name, col);
        signalChainStrip->setSidebarDragHover(overChain, chainPt, mod, name);
        repaint();
        return;
    }

    stompView->setSidebarDragHover(false, stompPt, mod, name, col);
    signalChainStrip->setSidebarDragHover(false, chainPt, mod, name);

    if (dropped) {
        if (overChain) {
            signalChainStrip->handleSidebarDrop(chainPt, mod, modelIdx);
        } else if (overStomp) {
            if (mod == ModuleId::AmpHead) {
                const int targetRig = (ampView != nullptr) ? ampView->getSelectedRig() : 0;
                if (auto* p = processor.getAPVTS().getParameter(params::modelParamFor(ModuleId::AmpHead, targetRig))) {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(modelIdx)));
                    p->endChangeGesture();
                }
                const char* chId = (targetRig == 2) ? params::kAmpCChannel : ((targetRig == 1) ? params::kAmpBChannel : params::kAmpChannel);
                if (auto* cp = processor.getAPVTS().getParameter(chId)) {
                    cp->beginChangeGesture();
                    cp->setValueNotifyingHost(cp->convertTo0to1(static_cast<float>(AmpHead::categoryForModel(modelIdx))));
                    cp->endChangeGesture();
                }
                selectStageTab(StageViewTab::Amp, targetRig);
            } else if (mod == ModuleId::CabinetIR) {
                const int targetRig = (cabView != nullptr) ? cabView->getSelectedRig() : 0;
                if (auto* p = processor.getAPVTS().getParameter(params::modelParamFor(ModuleId::CabinetIR, targetRig))) {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(modelIdx)));
                    p->endChangeGesture();
                }
                selectStageTab(StageViewTab::Cab, targetRig);
            } else {
                selectStageTab(StageViewTab::Stomp);
                stompView->handleSidebarDrop(stompPt, mod, modelIdx);
            }
        }
    }
    repaint();
}

void PedalBoardAudioProcessorEditor::resized() {
    auto area = getLocalBounds();
    const bool shortWin = (area.getHeight() < 660);

    const int topH    = shortWin ? 46 : 52;
    const int navH    = shortWin ? 32 : 36;
    const int looperH = shortWin ? 44 : 50;
    // Constant height across all 4 routing modes so stage & signal chain never jump when switching topologies!
    const int chainH  = shortWin ? 148 : 168;

    topBar->setBounds(area.removeFromTop(topH));
    stageNavBar->setBounds(area.removeFromTop(navH));

    looperBar->setBounds(area.removeFromBottom(looperH));

    // Docked Right-Side Gear Sidebar spans from below nav bar down to Looper dock
    if (sidebarVisible && gearSidebar != nullptr) {
        const int sideW = std::clamp(area.getWidth() * 19 / 100, 224, 290);
        gearSidebar->setBounds(area.removeFromRight(sideW));
    }

    // 3-Row Valeton GP-200 Signal Chain Strip (top blue pills + stylized mini pedals + bottom model dropdowns)
    signalChainStrip->setBounds(area.removeFromBottom(chainH));

    // Hero Stage occupies the remaining center canvas
    stompView->setBounds(area);
    ampView->setBounds(area);
    cabView->setBounds(area);
    rackView->setBounds(area);

    if (browserOverlay != nullptr) {
        browserOverlay->setBounds(getLocalBounds());
    }
}

void PedalBoardAudioProcessorEditor::togglePresetBrowser() {
    if (browserOverlay == nullptr) return;
    const bool show = !browserOverlay->isVisible();
    if (show) {
        browserOverlay->reloadLibrary();
        browserOverlay->toFront(true);
    }
    browserOverlay->setVisible(show);
}

void PedalBoardAudioProcessorEditor::openTone3000Browser(CloudLoadTargetKind kind, int rigOrSlotIndex, const juce::String& searchQuery) {
    if (tone3000Client == nullptr) return;

    if (cloudWindow == nullptr) {
        cloudWindow = std::make_unique<Tone3000PopupWindow>(
            processor,
            *tone3000Client,
            &lookAndFeel,
            [this](const CloudLoadRequest& req) { handleCloudCaptureLoaded(req); },
            [](juce::DocumentWindow& w) { applyDarkTitleBarToWindow(w); }
        );
    }

    // Toggle off if triggered via NavBar/T while already visible in Auto mode with no search query
    if (kind == CloudLoadTargetKind::Auto && searchQuery.isEmpty() && cloudWindow->isVisible()) {
        cloudWindow->setVisible(false);
        return;
    }

    cloudWindow->showForTarget(this, kind, rigOrSlotIndex, searchQuery);
}

void PedalBoardAudioProcessorEditor::handleCloudCaptureLoaded(const CloudLoadRequest& req) {
    if (!req.file.existsAsFile()) return;

    CloudLoadTargetKind resolved = req.targetKind;
    if (resolved == CloudLoadTargetKind::Auto) {
        if (req.isIr)         resolved = CloudLoadTargetKind::CabRig;
        else if (req.isPedal) resolved = CloudLoadTargetKind::NamPedal;
        else                  resolved = CloudLoadTargetKind::AmpRig;
    }

    // Never send a .wav IR to a .nam model or vice versa
    if (req.isIr && resolved != CloudLoadTargetKind::CabRig) {
        resolved = CloudLoadTargetKind::CabRig;
    } else if (!req.isIr && resolved == CloudLoadTargetKind::CabRig) {
        resolved = req.isPedal ? CloudLoadTargetKind::NamPedal : CloudLoadTargetKind::AmpRig;
    }

    if (resolved == CloudLoadTargetKind::CabRig) {
        const int rigIdx = std::clamp(req.targetRigIndex, 0, 2);
        processor.loadCustomCabinetIR(req.file, rigIdx);
        selectStageTab(StageViewTab::Cab, rigIdx);
        return;
    }

    if (resolved == CloudLoadTargetKind::AmpRig) {
        const int rigIdx = std::clamp(req.targetRigIndex, 0, 2);
        processor.loadCustomNamModel(req.file, rigIdx);
        selectStageTab(StageViewTab::Amp, rigIdx);
        return;
    }

    // NAM Pedal slot target: use explicit slotId if valid, else first existing NAM Pedal slot, else create new slot!
    int targetSlot = -1;
    if (req.targetSlotId >= 0 && req.targetSlotId < kMaxPedalSlots) {
        const auto snap = processor.getPedalSlot(req.targetSlotId);
        if (snap.occupied && isNamPedalSlotModel(snap.category, snap.modelIndex)) {
            targetSlot = req.targetSlotId;
        }
    }
    if (targetSlot < 0) {
        for (const int sId : processor.getActivePedalSlotIds()) {
            const auto snap = processor.getPedalSlot(sId);
            if (snap.occupied && isNamPedalSlotModel(snap.category, snap.modelIndex)) {
                targetSlot = sId;
                break;
            }
        }
    }
    if (targetSlot < 0) {
        const int lane = (stompView != nullptr) ? stompView->getEffectiveInsertLane(ModuleId::Overdrive) : -1;
        targetSlot = processor.addPedalSlot(ModuleId::Overdrive, kNamPedalModelIndex, -1, lane);
    }
    if (targetSlot >= 0) {
        processor.setPedalSlotBypass(targetSlot, false);
        processor.loadCustomSlotNamModel(targetSlot, req.file);
        selectStageTab(StageViewTab::Stomp);
        if (stompView != nullptr) stompView->refreshAll();
    }
}

void PedalBoardAudioProcessorEditor::timerCallback() {
    topBar->updateLiveMetersAndTuner();
    if (stageNavBar != nullptr) {
        stageNavBar->refreshLiveState();
        if (tone3000Client != nullptr) {
            stageNavBar->setCloudConnected(tone3000Client->isConnected());
        }
    }
    looperBar->refreshLiveState();

    if (stompView->isVisible()) stompView->updateLive();
    if (ampView->isVisible())   ampView->updateLive();
    if (cabView->isVisible())   cabView->updateLive();
    if (rackView->isVisible())  rackView->updateLive();

    processor.getSignalChain().reclaimAllRetiredNamModels();

    const auto curMode = processor.getRoutingMode();
    if (curMode != lastObservedRoutingMode) {
        lastObservedRoutingMode = curMode;
        resized();
        stompView->refreshAll();
        if (signalChainStrip != nullptr) {
            signalChainStrip->setHighlightedStompLane(stompView->getActiveLaneFilter());
            signalChainStrip->resized();
            signalChainStrip->repaint();
        }
        ampView->refreshAll();
        cabView->refreshAll();
        rackView->refreshAll();
    }

    signalChainStrip->repaint();
    if (sidebarVisible && gearSidebar != nullptr) {
        gearSidebar->refreshLiveState();
    }

    const uint64_t ver = processor.getUiStateVersion();
    if (ver != lastObservedStateVersion) {
        lastObservedStateVersion = ver;
        topBar->syncFromProcessor();
        stompView->refreshAll();
        ampView->refreshAll();
        cabView->refreshAll();
        rackView->refreshAll();
        signalChainStrip->resized();
        signalChainStrip->repaint();
    }
}

void PedalBoardAudioProcessorEditor::handleSavePreset() {
    const auto dir = processor.getPresetManager().getUserPresetDirectory();
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save PedalBoard Preset",
        dir.getChildFile("MyPreset.pbpreset"),
        "*.pbpreset"
    );
    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting;
    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser) {
        auto file = chooser.getResult();
        if (file == juce::File()) return;
        if (!file.hasFileExtension(".pbpreset")) {
            file = file.withFileExtension(".pbpreset");
        }
        processor.saveUserPreset(file, file.getFileNameWithoutExtension());
        if (browserOverlay != nullptr && browserOverlay->isVisible()) {
            browserOverlay->reloadLibrary();
        }
    });
}

void PedalBoardAudioProcessorEditor::handleLoadPreset() {
    const auto dir = processor.getPresetManager().getUserPresetDirectory();
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load PedalBoard Preset",
        dir,
        "*.pbpreset"
    );
    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser) {
        auto file = chooser.getResult();
        if (file.existsAsFile()) {
            processor.loadUserPreset(file);
        }
    });
}

void PedalBoardAudioProcessorEditor::handleLoadCabinetIR(int rigIndex) {
    const juce::String title = (rigIndex == 2)
        ? "Select Cabinet C Impulse Response (.wav / .aiff)"
        : ((rigIndex == 1) ? "Select Cabinet B Impulse Response (.wav / .aiff)"
                           : "Select Cabinet A Impulse Response (.wav / .aiff)");
    fileChooser = std::make_unique<juce::FileChooser>(
        title,
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.wav;*.aif;*.aiff"
    );
    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(chooserFlags, [this, rigIndex](const juce::FileChooser& chooser) {
        auto file = chooser.getResult();
        if (file.existsAsFile()) {
            processor.loadCustomCabinetIR(file, rigIndex);
        }
    });
}

void PedalBoardAudioProcessorEditor::handleLoadNamModel(int rigIndex) {
    const juce::String title = (rigIndex == 2)
        ? "Select Neural Amp Modeler Capture for Amp C (.nam)"
        : ((rigIndex == 1) ? "Select Neural Amp Modeler Capture for Amp B (.nam)"
                           : "Select Neural Amp Modeler Capture for Amp A (.nam)");
    fileChooser = std::make_unique<juce::FileChooser>(
        title,
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.nam"
    );
    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(chooserFlags, [this, rigIndex](const juce::FileChooser& chooser) {
        auto file = chooser.getResult();
        if (file.existsAsFile()) {
            processor.loadCustomNamModel(file, rigIndex);
        }
    });
}

void PedalBoardAudioProcessorEditor::handleLoadSlotNamModel(int slotId) {
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Neural Amp Modeler Pedal Capture (.nam)",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.nam"
    );
    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(chooserFlags, [this, slotId](const juce::FileChooser& chooser) {
        auto file = chooser.getResult();
        if (file.existsAsFile()) {
            processor.loadCustomSlotNamModel(slotId, file);
        }
    });
}

} // namespace pedalboard
