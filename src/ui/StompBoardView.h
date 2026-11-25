#pragma once

#include "../engine/Gp200Catalogue.h"
#include "../engine/ModuleId.h"
#include "../engine/PedalSlot.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "HardwarePrimitives.h"
#include "MidiBindableControl.h"
#include "SkinManager.h"
#include "ValetonTheme.h"

#include <array>
#include <cmath>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <memory>
#include <vector>

namespace pedalboard {

class StompFootswitchButton : public juce::ToggleButton {
public:
    explicit StompFootswitchButton(juce::Colour accentCol) : accent(accentCol) {
        setClickingTogglesState(true);
    }

    void setAccent(juce::Colour c) noexcept { accent = c; }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override {
        const auto bounds = getLocalBounds().toFloat();
        const bool active = !getToggleState();
        const auto& strip = SkinManager::instance().getImage(SkinAsset::FootswitchChrome2);

        const float sz = std::min(bounds.getWidth(), bounds.getHeight()) * 0.84f;
        auto dest = juce::Rectangle<float>(bounds.getCentreX() - sz * 0.5f,
                                           bounds.getCentreY() - sz * 0.5f,
                                           sz, sz);

        if (active) {
            g.setColour(accent.withAlpha(highlighted ? 0.40f : 0.22f));
            g.drawEllipse(dest.expanded(2.0f), 2.0f);
        }

        if (strip.isValid()) {
            SkinManager::drawSwitchFrame(g, strip, down ? 1 : 0, 2, dest, highlighted ? 1.0f : 0.96f);
        } else {
            hw::drawFootswitchStomp(g, bounds.getCentreX(), bounds.getCentreY(), sz * 0.34f, highlighted);
        }
    }

private:
    juce::Colour accent;
};

class SlotWahTreadleWidget : public juce::Component {
public:
    SlotWahTreadleWidget(PedalBoardAudioProcessor& proc, int slotIndex, juce::Colour accentCol)
        : processor(proc), slotId(slotIndex), accent(accentCol) {}

    void setAccent(juce::Colour c) noexcept { accent = c; }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat().reduced(3.0f, 2.0f);
        const auto snap = processor.getPedalSlot(slotId);
        const bool active = !snap.bypassed;
        const float pos = std::clamp(snap.params[0], 0.0f, 1.0f);
        const bool isWhammy = (snap.category == ModuleId::Compressor && snap.modelIndex == 17);
        const bool isPitch  = (snap.category == ModuleId::Compressor && snap.modelIndex == 16);
        const bool isOcta   = (snap.category == ModuleId::Compressor && snap.modelIndex == 15);
        const bool autoEnabled = (snap.category == ModuleId::Wah)
            ? ((snap.modelIndex == 2 ? snap.params[5] : snap.params[4]) > 0.5f)
            : ((isOcta ? snap.params[4] : snap.params[6]) > 0.5f);

        g.setColour(juce::Colour(0xff0d1016));
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(juce::Colour(0xff2c3342));
        g.drawRoundedRectangle(area, 5.0f, 1.0f);

        auto tread = area.reduced(4.0f, 4.0f);
        juce::ColourGradient rocker(juce::Colour(0xff252a36).brighter(active ? pos * 0.22f : 0.04f), tread.getX(), tread.getY(),
                                    juce::Colour(0xff14171f), tread.getX(), tread.getBottom(), false);
        g.setGradientFill(rocker);
        g.fillRoundedRectangle(tread, 4.0f);

        const float glowY = tread.getBottom() - pos * tread.getHeight();
        for (float y = tread.getY() + 18.0f; y < tread.getBottom() - 16.0f; y += 6.5f) {
            const float dist = std::abs(y - glowY);
            const float prox = std::max(0.0f, 1.0f - dist / 24.0f);
            g.setColour((active && prox > 0.08f)
                            ? accent.withAlpha(0.28f + 0.70f * prox)
                            : juce::Colours::white.withAlpha(active ? 0.10f : 0.06f));
            g.fillRoundedRectangle(tread.getX() + 5.0f, y, tread.getWidth() - 10.0f, 2.5f, 1.2f);
        }

        // Top live expression / pitch readout
        juce::String topText;
        if (autoEnabled) {
            topText = "AUTO SWEEP";
        } else if (isWhammy) {
            const float rangeS = std::clamp(snap.params[5] * 0.01f, 0.1f, 1.0f);
            const float curSt = snap.params[2] + pos * rangeS * (snap.params[1] - snap.params[2]);
            const juce::String sign = (curSt > 0.05f) ? "+" : "";
            topText = sign + juce::String(curSt, 1) + " st (" + juce::String(static_cast<int>(pos * 100.0f)) + "%)";
        } else if (isPitch) {
            const int lowSt = static_cast<int>(std::lround(snap.params[1]));
            const int hiSt  = static_cast<int>(std::lround(snap.params[2]));
            topText = juce::String(lowSt) + "/" + (hiSt > 0 ? "+" : "") + juce::String(hiSt) + " st";
        } else if (isOcta) {
            topText = "OCT " + juce::String(static_cast<int>(pos * 100.0f)) + "%";
        } else {
            topText = "TOE " + juce::String(static_cast<int>(pos * 100.0f)) + "%";
        }

        const bool narrowTreadle = (tread.getWidth() < 64.0f);
        g.setColour(active ? accent.brighter(0.25f) : juce::Colour(0xff64748b));
        g.setFont(juce::Font(juce::FontOptions(narrowTreadle ? 7.2f : 8.2f, juce::Font::bold)));
        g.drawText(topText, tread.withHeight(13.0f), juce::Justification::centred, true);

        // Bottom MIDI CC Expression badge
        const bool learning = processor.isExpressionPedalMidiLearnArmed();
        const int cc = processor.getExpressionPedalCc();
        g.setColour((active && learning) ? juce::Colour(0xfff59e0b) : juce::Colour(active ? 0xff94a3b8 : 0xff525b6c));
        g.setFont(juce::Font(juce::FontOptions(narrowTreadle ? 6.8f : 7.5f, juce::Font::bold)));
        g.drawText(learning ? "MOVE EXP..." : ("CC #" + juce::String(cc)),
                   tread.withTop(tread.getBottom() - 13.0f), juce::Justification::centred, true);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) {
            openExpressionContextMenu();
            return;
        }
        updateFromY(e.position.y);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (!e.mods.isPopupMenu()) {
            updateFromY(e.position.y);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent&) override {
        const auto snap = processor.getPedalSlot(slotId);
        const float next = (snap.params[0] > 0.5f) ? 0.0f : 1.0f;
        processor.setPedalSlotParam(slotId, 0, next);
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        const auto snap = processor.getPedalSlot(slotId);
        const float next = std::clamp(snap.params[0] + wheel.deltaY * 0.12f, 0.0f, 1.0f);
        processor.setPedalSlotParam(slotId, 0, next);
        repaint();
    }

private:
    void updateFromY(float y) {
        const float h = std::max(1.0f, static_cast<float>(getHeight()) - 20.0f);
        const float norm = std::clamp(1.0f - (y - 10.0f) / h, 0.0f, 1.0f);
        processor.setPedalSlotParam(slotId, 0, norm);
        repaint();
    }

    void openExpressionContextMenu() {
        const auto snap = processor.getPedalSlot(slotId);
        const bool isWhammy = (snap.category == ModuleId::Compressor && snap.modelIndex == 17);
        const int curCc = processor.getExpressionPedalCc();

        juce::PopupMenu m;
        m.addSectionHeader("Expression Pedal & MIDI CC");
        m.addItem(1, "MIDI Learn Physical Expression Pedal (Move pedal...)", true, processor.isExpressionPedalMidiLearnArmed());
        m.addItem(2, "Use MIDI CC #11 (Standard Expression)", true, curCc == 11);
        m.addItem(3, "Use MIDI CC #1 (Modulation Wheel)", true, curCc == 1);
        m.addItem(4, "Use MIDI CC #4 (Foot Controller)", true, curCc == 4);
        m.addItem(5, "Use MIDI CC #7 (Volume Pedal)", true, curCc == 7);

        if (isWhammy) {
            m.addSeparator();
            m.addSectionHeader("Whammy (P-Bend) Interval Presets");
            m.addItem(10, "Whammy +1 Octave (Heel 0 -> Toe +12 st)");
            m.addItem(11, "Whammy +2 Octaves (Heel 0 -> Toe +24 st)");
            m.addItem(12, "Dive Bomb -1 Octave (Heel 0 -> Toe -12 st)");
            m.addItem(13, "Dive Bomb -2 Octaves (Heel 0 -> Toe -24 st)");
            m.addItem(14, "5th Harmony Bend (Heel 0 -> Toe +7 st)");
            m.addItem(15, "4th -> 5th Bend (Heel +5 -> Toe +7 st)");
        }

        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(getLocalBounds())),
                        [this](int res) {
                            if (res == 1)      processor.armExpressionPedalMidiLearn(true);
                            else if (res == 2) processor.setExpressionPedalCc(11);
                            else if (res == 3) processor.setExpressionPedalCc(1);
                            else if (res == 4) processor.setExpressionPedalCc(4);
                            else if (res == 5) processor.setExpressionPedalCc(7);
                            else if (res == 10) { processor.setPedalSlotParam(slotId, 1, 12.0f);  processor.setPedalSlotParam(slotId, 2, 0.0f); }
                            else if (res == 11) { processor.setPedalSlotParam(slotId, 1, 24.0f);  processor.setPedalSlotParam(slotId, 2, 0.0f); }
                            else if (res == 12) { processor.setPedalSlotParam(slotId, 1, -12.0f); processor.setPedalSlotParam(slotId, 2, 0.0f); }
                            else if (res == 13) { processor.setPedalSlotParam(slotId, 1, -24.0f); processor.setPedalSlotParam(slotId, 2, 0.0f); }
                            else if (res == 14) { processor.setPedalSlotParam(slotId, 1, 7.0f);   processor.setPedalSlotParam(slotId, 2, 0.0f); }
                            else if (res == 15) { processor.setPedalSlotParam(slotId, 1, 7.0f);   processor.setPedalSlotParam(slotId, 2, 5.0f); }
                            repaint();
                        });
    }

    PedalBoardAudioProcessor& processor;
    int slotId = 0;
    juce::Colour accent;
};

// Universal Dynamic Stompbox Component (supports ANY of the 8 effect categories:
// DST, PRE, WAH, NR, EQ, MOD, DLY, RVB with independent per-slot parameters & pure drag-and-drop UX!)
class DynamicSlotPedal : public juce::Component {
public:
    DynamicSlotPedal(PedalBoardAudioProcessor& proc, int slotIndex)
        : processor(proc),
          slotId(slotIndex),
          footswitch(juce::Colour(0xff3b82f6)) {
        footswitch.onClick = [this] {
            processor.setPedalSlotBypass(slotId, footswitch.getToggleState());
            applyBypassAppearance(!footswitch.getToggleState());
            repaint();
        };
        footswitch.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible(footswitch);

        namLoadBtn.setButtonText("LOAD .NAM");
        namLoadBtn.setTooltip("Load a Neural Amp Modeler (.nam) pedal capture");
        namLoadBtn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        namLoadBtn.onClick = [this] {
            if (loadSlotNamCb) loadSlotNamCb(slotId);
        };
        addChildComponent(namLoadBtn);

        namCloudBtn.setButtonText("T3K");
        namCloudBtn.setTooltip("Browse & load .nam pedals from TONE3000 Cloud");
        namCloudBtn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        namCloudBtn.onClick = [this] {
            if (browseSlotNamCloudCb) browseSlotNamCloudCb(slotId);
        };
        addChildComponent(namCloudBtn);

        namClearBtn.setButtonText("X");
        namClearBtn.setTooltip("Clear loaded .nam pedal capture");
        namClearBtn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        namClearBtn.onClick = [this] {
            processor.clearSlotNamModel(slotId);
            syncFromProcessor(false);
        };
        addChildComponent(namClearBtn);

        setMouseCursor(juce::MouseCursor::DraggingHandCursor);

        syncFromProcessor(true);
    }

    void setSelectedOnBoard(bool sel) noexcept {
        if (selectedOnBoard != sel) {
            selectedOnBoard = sel;
            repaint();
        }
    }

    int getSlotId() const noexcept { return slotId; }
    ModuleId getCategory() const noexcept { return currentCategory; }
    int getModelIndex() const noexcept { return currentModelIndex; }
    juce::String getActiveModelName() const { return activeModelName; }
    juce::Colour getActiveAccent() const noexcept { return chassisColour; }
    bool isEngaged() const noexcept { return !footswitch.getToggleState(); }
    bool hasNamLoaded() const noexcept { return namModelLoaded; }
    const juce::String& getNamLoadedName() const noexcept { return namLoadedName; }

    void setLoadSlotNamCallback(std::function<void(int)> cb) {
        loadSlotNamCb = std::move(cb);
    }

    void setBrowseSlotNamCloudCallback(std::function<void(int)> cb) {
        browseSlotNamCloudCb = std::move(cb);
    }

    int getWidthWeight() const noexcept {
        if (currentCategory == ModuleId::GraphicEq && currentModelIndex == 5) return 28; // 10-band Hyper EQ
        if (currentCategory == ModuleId::StereoDelay && currentModelIndex == 2) return 26; // 12-param Tape Delay S
        if (isExpressionPedalSlotModel(currentCategory, currentModelIndex)) return 23;
        if (isNamPedalSlotModel(currentCategory, currentModelIndex)) return 22; // OLED capture strip + 2x2 knobs
        const int total = static_cast<int>(controls.size() + toggles.size() + choices.size());
        if (total >= 7) return 24;
        if (total >= 4) return 20;
        if (total <= 2) return 16;
        return 18;
    }

    void setCallbacks(std::function<void(int, juce::Point<int>)> onDown,
                      std::function<void(int, juce::Point<int>)> onDrag,
                      std::function<void(int, juce::Point<int>)> onUp,
                      std::function<void(int, int)> onStepOrder,
                      std::function<void(int)> onRemove) {
        boardDownCb = std::move(onDown);
        boardDragCb = std::move(onDrag);
        boardUpCb   = std::move(onUp);
        stepOrderCb = std::move(onStepOrder);
        removeCb    = std::move(onRemove);
    }

    void setSlotPositionInfo(int orderIdx, int totalSlots) noexcept {
        if (currentOrderIndex != orderIdx || totalActiveSlots != totalSlots) {
            currentOrderIndex = orderIdx;
            totalActiveSlots = totalSlots;
            repaint();
        }
    }

    void setGrDb(float gr) noexcept {
        if (std::abs(gr - grDb) > 0.1f) {
            grDb = gr;
            if (currentCategory == ModuleId::Compressor) repaint();
        }
    }

    void toggleBypass() {
        const auto snap = processor.getPedalSlot(slotId);
        processor.setPedalSlotBypass(slotId, !snap.bypassed);
        syncFromProcessor(false);
    }

    void syncFromProcessor(bool forceRebuild) {
        const auto snap = processor.getPedalSlot(slotId);
        if (!snap.occupied) return;

        const bool categoryChanged = (snap.category != currentCategory);
        const bool modelChanged    = (snap.modelIndex != currentModelIndex);
        currentCategory   = snap.category;
        currentModelIndex = snap.modelIndex;

        const bool emptyUi = controls.empty() && toggles.empty() && choices.empty() && wahTreadle == nullptr;
        if (forceRebuild || categoryChanged || modelChanged || emptyUi) {
            rebuildControlsForCategory(currentCategory, currentModelIndex);
            if (!forceRebuild && (categoryChanged || modelChanged)) {
                if (auto* parent = getParentComponent()) {
                    parent->resized();
                }
            }
        }

        const auto& entry = gp200::getModelEntry(currentCategory, snap.modelIndex);
        activeModelName = juce::String::fromUTF8(entry.name);
        activeModelDesc = juce::String::fromUTF8(entry.description);

        const bool isNamPedal = isNamPedalSlotModel(currentCategory, currentModelIndex);
        namLoadBtn.setVisible(isNamPedal);
        namCloudBtn.setVisible(isNamPedal);
        namClearBtn.setVisible(isNamPedal);
        if (isNamPedal) {
            const auto& runner = processor.getSignalChain().getSlotNamRunner(slotId);
            namModelLoaded = runner.hasLoadedModel();
            const auto statusStr = runner.getDisplayName();
            namLoadedName = namModelLoaded ? statusStr : juce::String();
            namStatusText = statusStr;
            if (namModelLoaded) {
                activeModelName = namLoadedName;
                activeModelDesc = "Neural NAM Capture";
            }
            namClearBtn.setEnabled(namModelLoaded);
        } else {
            namModelLoaded = false;
            namLoadedName.clear();
            namStatusText.clear();
        }

        chassisColour   = (currentCategory == ModuleId::Overdrive ||
                           currentCategory == ModuleId::Compressor ||
                           currentCategory == ModuleId::StereoChorus ||
                           currentCategory == ModuleId::StereoDelay ||
                           currentCategory == ModuleId::Reverb ||
                           currentCategory == ModuleId::Wah)
                              ? juce::Colour(entry.chassisArgb)
                              : valeton::colourFor(currentCategory);
        footswitch.setAccent(chassisColour);
        if (wahTreadle != nullptr) wahTreadle->setAccent(chassisColour);

        ignoreCallbacks = true;
        footswitch.setToggleState(snap.bypassed, juce::dontSendNotification);
        if (wahTreadle != nullptr) wahTreadle->repaint();

        for (auto& c : controls) {
            const size_t pi = static_cast<size_t>(c->paramIdx);
            if (pi < kMaxSlotParams) {
                c->slider.setValue(snap.params[pi], juce::dontSendNotification);
            }
        }
        for (auto& t : toggles) {
            const size_t pi = static_cast<size_t>(t->paramIdx);
            if (pi < kMaxSlotParams) {
                t->btn.setToggleState(snap.params[pi] > 0.5f, juce::dontSendNotification);
            }
        }
        for (auto& ch : choices) {
            const size_t pi = static_cast<size_t>(ch->paramIdx);
            if (pi < kMaxSlotParams) {
                const int chosen = std::clamp(static_cast<int>(std::lround(snap.params[pi])),
                                              0, std::max(0, ch->numChoices - 1));
                if (ch->useButtons) {
                    for (int b = 0; b < static_cast<int>(ch->segButtons.size()); ++b) {
                        ch->segButtons[static_cast<size_t>(b)]->setToggleState(b == chosen, juce::dontSendNotification);
                    }
                } else if (ch->combo != nullptr && ch->combo->getSelectedItemIndex() != chosen) {
                    ch->combo->setSelectedItemIndex(chosen, juce::dontSendNotification);
                }
            }
        }

        ignoreCallbacks = false;
        applyBypassAppearance(!snap.bypassed);
        repaint();
    }

    void applyBypassAppearance(bool active) {
        const bool bypassed = !active;
        for (auto& c : controls) {
            c->slider.getProperties().set("bypassed", bypassed);
            c->slider.setColour(juce::Slider::textBoxTextColourId,
                                active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
            c->label.setColour(juce::Label::textColourId,
                               active ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff5a6373));
            c->slider.repaint();
        }
        for (auto& t : toggles) {
            t->btn.getProperties().set("bypassed", bypassed);
            t->btn.setColour(juce::TextButton::buttonOnColourId,
                             active ? chassisColour.darker(0.2f) : juce::Colour(0xff2c3240));
            t->btn.setColour(juce::TextButton::textColourOnId,
                             active ? juce::Colours::white : juce::Colour(0xff64748b));
            t->btn.setColour(juce::TextButton::textColourOffId,
                             active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff5a6373));
            t->btn.repaint();
        }
        for (auto& ch : choices) {
            ch->label.setColour(juce::Label::textColourId,
                                active ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff5a6373));
            if (ch->combo != nullptr) {
                ch->combo->setAlpha(active ? 1.0f : 0.60f);
            }
            for (auto& sb : ch->segButtons) {
                sb->getProperties().set("bypassed", bypassed);
                sb->setColour(juce::TextButton::buttonOnColourId,
                              active ? chassisColour.darker(0.18f) : juce::Colour(0xff2c3240));
                sb->setColour(juce::TextButton::textColourOnId,
                              active ? juce::Colours::white : juce::Colour(0xff64748b));
                sb->setColour(juce::TextButton::textColourOffId,
                              active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff5a6373));
                sb->repaint();
            }
        }
        if (wahTreadle != nullptr) wahTreadle->repaint();

        if (isNamPedalSlotModel(currentCategory, currentModelIndex)) {
            namLoadBtn.getProperties().set("bypassed", bypassed);
            namLoadBtn.setColour(juce::TextButton::buttonColourId,
                                 active ? juce::Colour(0xff0f2a38) : juce::Colour(0xff1e232d));
            namLoadBtn.setColour(juce::TextButton::textColourOffId,
                                 active ? juce::Colour(0xff67e8f9) : juce::Colour(0xff64748b));
            namLoadBtn.repaint();

            namCloudBtn.getProperties().set("bypassed", bypassed);
            namCloudBtn.setColour(juce::TextButton::buttonColourId,
                                  active ? juce::Colour(0xff0f302e) : juce::Colour(0xff1d222b));
            namCloudBtn.setColour(juce::TextButton::textColourOffId,
                                  active ? juce::Colour(0xff5eead4) : juce::Colour(0xff64748b));
            namCloudBtn.repaint();

            namClearBtn.getProperties().set("bypassed", bypassed);
            namClearBtn.setColour(juce::TextButton::buttonColourId,
                                  (active && namModelLoaded) ? juce::Colour(0xff2c1a1f) : juce::Colour(0xff1b2029));
            namClearBtn.setColour(juce::TextButton::textColourOffId,
                                  (active && namModelLoaded) ? juce::Colour(0xfffca5a5) : juce::Colour(0xff525b6c));
            namClearBtn.repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragArmed = false;
        const bool packStomp  = isPackCapableStompModel(currentCategory, currentModelIndex);
        const bool packReverb = isPackCapableReverbModel(currentCategory, currentModelIndex);
        const bool hybridPre  = isHybridPreampColorModel(currentCategory, currentModelIndex);
        if (e.mods.isRightButtonDown() && (packStomp || packReverb || hybridPre)) {
            const auto& entry = gp200::getModelEntry(currentCategory, currentModelIndex);
            const auto info = ModelPackRegistry::getExcelPackInfo(entry.excelNo);
            const auto hint = ModelPackRegistry::getExpectedFilenameHint(currentCategory, currentModelIndex);
            const bool loaded = processor.getSignalChain().isSlotUsingPackCapture(slotId);
            const bool preOn  = hybridPre && processor.getSignalChain().isSlotPreampColorEnabled(slotId);

            juce::PopupMenu menu;
            menu.addSectionHeader(juce::String::fromUTF8(entry.name)
                                  + (hybridPre ? " — Hybrid Preamp + White-Box DSP"
                                               : (" — ModelPack (#" + juce::String(entry.excelNo) + ")")));

            if (hybridPre) {
                menu.addItem(4, "Hybrid Preamp .NAM Color (Warm Analog Input)", true, preOn);
                if (preOn && loaded) {
                    menu.addItem(99, "Active Preamp: " + processor.getSignalChain().getSlotPackCaptureName(slotId), false, true);
                } else {
                    menu.addItem(99, "White-Box Analog C++ Circuit Active", false, false);
                }
                menu.addSeparator();
                menu.addItem(2, "Open Local ModelPacks Folder...");
                menu.addItem(3, "Rescan ModelPacks Now");
            } else {
                if (loaded) {
                    menu.addItem(99, "Active: " + processor.getSignalChain().getSlotPackCaptureName(slotId), false, true);
                } else {
                    menu.addItem(99, "Expected file: " + hint + " (using DSP fallback)", false, false);
                }
                menu.addSeparator();
                const juce::String q = (info.searchQuery != nullptr && info.searchQuery[0] != '\0')
                    ? juce::String::fromUTF8(info.searchQuery)
                    : juce::String::fromUTF8(entry.description);
                const bool hasExt = (info.externalFreeUrl != nullptr && info.externalFreeUrl[0] != '\0');
                menu.addItem(1, hasExt ? ("Open Free IR Source (" + juce::String::fromUTF8(info.externalFreeUrl) + ")")
                                       : ("Find \"" + q + "\" on TONE3000..."));
                menu.addItem(2, "Open Local ModelPacks Folder...");
                menu.addItem(3, "Rescan ModelPacks Now");
            }

            const juce::String qStr = (info.searchQuery != nullptr && info.searchQuery[0] != '\0')
                ? juce::String::fromUTF8(info.searchQuery)
                : juce::String::fromUTF8(entry.description);
            const bool hasExt = (info.externalFreeUrl != nullptr && info.externalFreeUrl[0] != '\0');
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(getLocalBounds())),
                               [this, qStr, hasExt, packReverb, preOn,
                                extUrl = hasExt ? juce::String::fromUTF8(info.externalFreeUrl) : juce::String()](int res) {
                                   if (res == 1) {
                                       if (hasExt) {
                                           juce::URL(extUrl).launchInDefaultBrowser();
                                       } else {
                                           auto url = juce::URL("https://www.tone3000.com/search").withParameter("q", qStr);
                                           if (packReverb) url = url.withParameter("format", "ir");
                                           else            url = url.withParameter("gears", "pedal");
                                           url.launchInDefaultBrowser();
                                       }
                                   } else if (res == 2) {
                                       ModelPackRegistry::getUserOverrideDirectory().revealToUser();
                                   } else if (res == 3) {
                                       processor.getSignalChain().refreshAllModelPacks();
                                       repaint();
                                   } else if (res == 4) {
                                       processor.getSignalChain().setSlotPreampColorEnabled(slotId, !preOn);
                                       repaint();
                                   }
                               });
            return;
        }
        if (e.mods.isLeftButtonDown() && getParentComponent() != nullptr) {
            dragArmed = true;
            if (boardDownCb) {
                boardDownCb(slotId, getParentComponent()->getLocalPoint(nullptr, e.getScreenPosition()));
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragArmed && getParentComponent() != nullptr && boardDragCb) {
            boardDragCb(slotId, getParentComponent()->getLocalPoint(nullptr, e.getScreenPosition()));
        }
    }

    void mouseUp(const juce::MouseEvent& e) override {
        if (dragArmed && getParentComponent() != nullptr && boardUpCb) {
            boardUpCb(slotId, getParentComponent()->getLocalPoint(nullptr, e.getScreenPosition()));
        }
        dragArmed = false;
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(4.0f, 3.0f);
        const bool active = isEngaged();

        float bayTop = 0.0f, bayBottom = 0.0f, bumperTop = 0.0f;
        valeton::drawRefTwoPartPedalChassis(g, bounds, chassisColour, active, bayTop, bayBottom, bumperTop);

        if (selectedOnBoard) {
            g.setColour(active ? juce::Colour(0xff60a5fa).withAlpha(0.70f)
                               : juce::Colour(0xff475569).withAlpha(0.45f));
            g.drawRoundedRectangle(bounds.expanded(1.5f), 12.5f, active ? 2.0f : 1.2f);
        }

        // Glowing jewel LED top-right of recessed control bay (completely unlit neutral dark glass when bypassed!)
        hw::drawJewelLamp(g, bounds.getRight() - 18.0f, bayTop + 13.0f, 4.0f,
                          active ? juce::Colour(0xffef4444) : juce::Colour(0xff3a4252), active);

        // Category code badge + Hardware model subtitle at top of dark bay
        const bool isNamPedal = isNamPedalSlotModel(currentCategory, currentModelIndex);
        const bool hasPackCap = !isNamPedal && processor.getSignalChain().isSlotUsingPackCapture(slotId);
        const juce::String catCode = isNamPedal ? juce::String("NAM") : juce::String(valeton::gpCodeFor(currentCategory));
        auto descRect = juce::Rectangle<float>(bounds.getX() + 12.0f, bayTop + 6.0f,
                                               bounds.getWidth() - (hasPackCap ? 66.0f : 34.0f), 14.0f);
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::italic)));
        g.drawText(juce::String("[") + catCode + "] " + activeModelDesc,
                   descRect, juce::Justification::centredLeft, true);

        if (hasPackCap) {
            const bool isIrPack = (currentCategory == ModuleId::Compressor && currentModelIndex == 10)
                               || isPackCapableReverbModel(currentCategory, currentModelIndex);
            auto badge = juce::Rectangle<float>(bounds.getRight() - 56.0f, bayTop + 6.5f, 30.0f, 13.0f);
            g.setColour(active ? juce::Colour(0xff0891b2).withAlpha(0.24f) : juce::Colour(0xff232834));
            g.fillRoundedRectangle(badge, 3.0f);
            g.setColour(active ? juce::Colour(0xff22d3ee) : juce::Colour(0xff64748b));
            g.drawRoundedRectangle(badge, 3.0f, 0.9f);
            g.setFont(juce::Font(juce::FontOptions(7.6f, juce::Font::bold)));
            g.drawText(isIrPack ? "IR" : "NAM", badge, juce::Justification::centred);
        }

        // Neural NAM Pedal Capture OLED strip (zero color highlight when bypassed!)
        if (isNamPedal && !namOledRect.isEmpty()) {
            auto oled = namOledRect.toFloat();
            g.setColour(juce::Colour(0xff080c12));
            g.fillRoundedRectangle(oled, 4.0f);
            g.setColour((active && namModelLoaded) ? chassisColour.withAlpha(0.75f)
                                                   : juce::Colour(0xff262e3d));
            g.drawRoundedRectangle(oled, 4.0f, 1.0f);

            const bool hasError = !namModelLoaded && namStatusText.startsWithIgnoreCase("Load failed");
            const float dotR = 2.6f;
            const float dotX = oled.getX() + 7.5f;
            const float dotY = oled.getCentreY();
            g.setColour(hasError ? juce::Colour(0xffef4444)
                                 : ((active && namModelLoaded) ? juce::Colour(0xff22d3ee) : juce::Colour(0xff475569)));
            g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

            g.setColour(hasError ? juce::Colour(0xfffca5a5)
                                 : ((active && namModelLoaded) ? juce::Colour(0xffe0f2fe)
                                                               : juce::Colour(active ? 0xff7c8a9e : 0xff525b6c)));
            g.setFont(juce::Font(juce::FontOptions(oled.getWidth() < 120.0f ? 8.2f : 9.0f,
                                                   namModelLoaded ? juce::Font::bold : juce::Font::italic)));
            const juce::String oledTxt = namModelLoaded
                ? namLoadedName
                : (namStatusText.isNotEmpty() ? namStatusText : juce::String("No .nam loaded"));
            g.drawText(oledTxt,
                       oled.withTrimmedLeft(14.0f).withTrimmedRight(5.0f),
                       juce::Justification::centredLeft, true);
        }

        // Optional Compressor 10-segment Gain Reduction LED Ladder (ONLY for actual Compressor models 0..2: COMP, COMP4, S-Comp)
        if (currentCategory == ModuleId::Compressor && currentModelIndex <= 2) {
            auto ladder = juce::Rectangle<float>(bounds.getX() + 14.0f, bayTop + 23.0f, bounds.getWidth() - 28.0f, 12.0f);
            g.setColour(juce::Colour(0xff090c12));
            g.fillRoundedRectangle(ladder, 3.0f);

            const int segs = 10;
            const float sw = (ladder.getWidth() - 20.0f) / static_cast<float>(segs);
            g.setColour(juce::Colour(0xff8c96a6));
            g.setFont(juce::Font(juce::FontOptions(7.0f, juce::Font::bold)));
            g.drawText("GR", ladder.withWidth(18.0f), juce::Justification::centred);

            for (int i = 0; i < segs; ++i) {
                const float thresh = static_cast<float>(i + 1) * 1.8f;
                const bool lit = active && grDb >= thresh;
                const auto segCol = (i < 5) ? juce::Colour(0xff38bdf8)
                                            : ((i < 8) ? juce::Colour(0xfff59e0b) : juce::Colour(0xffef4444));
                g.setColour(lit ? segCol : juce::Colour(0xff1e2430));
                g.fillRoundedRectangle(ladder.getX() + 19.0f + static_cast<float>(i) * sw + 1.0f,
                                       ladder.getY() + 2.5f, std::max(2.0f, sw - 2.0f), ladder.getHeight() - 5.0f, 1.2f);
            }
        }

        // Bold Stylized GP-200 Model Name across the Raised Colored Front Bumper (clean ref/215659 look, no arrows!)
        const float luma = 0.299f * chassisColour.getFloatRed()
                         + 0.587f * chassisColour.getFloatGreen()
                         + 0.114f * chassisColour.getFloatBlue();
        const auto titleCol = (luma > 0.56f)
                                  ? (active ? juce::Colour(0xff101319) : juce::Colour(0xff94a3b8))
                                  : (active ? juce::Colours::white : juce::Colour(0xff94a3b8));

        const float bumperH = bounds.getBottom() - bumperTop;
        const float titleH = (bumperH < 72.0f) ? 16.0f : 20.0f;
        const float titleFont = (bounds.getWidth() < 105.0f) ? 9.8f
                                                             : ((bounds.getWidth() < 138.0f) ? 11.2f : 12.5f);
        auto titleRect = juce::Rectangle<float>(bounds.getX() + 6.0f, bumperTop + 4.0f, bounds.getWidth() - 12.0f, titleH);
        g.setColour(titleCol);
        g.setFont(juce::Font(juce::FontOptions(titleFont, juce::Font::bold | juce::Font::italic)));
        g.drawText(activeModelName.toUpperCase(), titleRect, juce::Justification::centred, true);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(4, 3);
        const int bumperH = static_cast<int>(std::lround(valeton::pedalBumperHeight(static_cast<float>(bounds.getHeight()))));
        auto bumperArea = bounds.removeFromBottom(bumperH);
        bumperArea.removeFromTop((bumperH < 72) ? 19 : 24);
        const int swSz = std::min(50, std::min(bumperArea.getWidth() - 4, bumperArea.getHeight()));
        footswitch.setBounds(bumperArea.withSizeKeepingCentre(swSz, swSz));

        // Recessed dark upper control bay (responsive padding & authentic per-model controls!)
        const bool narrowPedal = (bounds.getWidth() < 118);
        const bool shortPedal  = (bounds.getHeight() < 230);
        auto bay = bounds.reduced(narrowPedal ? 6 : 9, shortPedal ? 7 : 10);
        bay.removeFromTop(15); // space for top jewel LED & hardware description subtitle
        if (currentCategory == ModuleId::Compressor && currentModelIndex <= 2) {
            bay.removeFromTop(14);
        }
        if (isNamPedalSlotModel(currentCategory, currentModelIndex)) {
            bay.removeFromTop(6); // crisp breathing room below [NAM] subtitle before OLED
            const int oledH = shortPedal ? 19 : 21;
            const int btnH  = shortPedal ? 18 : 20;
            namOledRect = bay.removeFromTop(oledH);
            bay.removeFromTop(3);
            auto btnRow = bay.removeFromTop(btnH);
            bay.removeFromTop(4);
            const int clearW = std::min(22, std::max(18, btnRow.getWidth() / 6));
            const int cloudW = std::min(36, std::max(28, btnRow.getWidth() * 24 / 100));
            namClearBtn.setBounds(btnRow.removeFromRight(clearW));
            btnRow.removeFromRight(2);
            namCloudBtn.setBounds(btnRow.removeFromRight(cloudW));
            btnRow.removeFromRight(2);
            namLoadBtn.setButtonText(btnRow.getWidth() < 64 ? "LOAD" : "LOAD .NAM");
            namLoadBtn.setBounds(btnRow);
        } else {
            namOledRect = {};
            namLoadBtn.setBounds(0, 0, 0, 0);
            namCloudBtn.setBounds(0, 0, 0, 0);
            namClearBtn.setBounds(0, 0, 0, 0);
        }

        auto applyAdaptiveRotary = [this](juce::Slider& s, int cellW, int cellH) {
            if (cellH < 46 || cellW < 42) {
                s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                s.setPopupDisplayEnabled(true, false, this);
            } else {
                const int tbW = std::clamp(cellW - 4, 34, 54);
                const int tbH = (cellH < 64) ? 11 : 13;
                s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, tbW, tbH);
                s.setPopupDisplayEnabled(false, false, nullptr);
            }
        };

        // 1. Bottom Toggle Row (e.g., LOW CUT, +3dB, BRIGHT, SYNC, TRAIL, AUTO-WAH, AUTO-SWEEP)
        if (!toggles.empty()) {
            const int tH = shortPedal ? 17 : 20;
            auto tRow = bay.removeFromBottom(tH);
            bay.removeFromBottom(3);
            const int tn = static_cast<int>(toggles.size());
            const int tGap = 3;
            const int tw = std::max(28, (tRow.getWidth() - tGap * (tn - 1)) / tn);
            for (int i = 0; i < tn; ++i) {
                auto r = (i + 1 == tn) ? tRow : tRow.removeFromLeft(tw);
                toggles[static_cast<size_t>(i)]->btn.setBounds(r);
                if (i + 1 < tn) tRow.removeFromLeft(tGap);
            }
        }

        // 2. Standalone Button-Choice Hero Layout (e.g., C-Chorus 4-button Dimension C!)
        if (controls.empty() && wahTreadle == nullptr && !choices.empty() && choices[0]->useButtons) {
            auto& ch = *choices[0];
            auto box = bay.reduced(4, std::max(4, (bay.getHeight() - 108) / 2));
            ch.label.setBounds(box.removeFromTop(14));
            box.removeFromTop(4);
            const int n = static_cast<int>(ch.segButtons.size());
            const int cols = (n == 4 && box.getWidth() >= 92) ? 2 : 1;
            const int rows = (n + cols - 1) / cols;
            const int cw = box.getWidth() / cols;
            const int chH = std::min(34, box.getHeight() / std::max(1, rows));
            for (int i = 0; i < n; ++i) {
                const int r = i / cols;
                const int c = i % cols;
                ch.segButtons[static_cast<size_t>(i)]->setBounds(
                    juce::Rectangle<int>(box.getX() + c * cw, box.getY() + r * chH, cw, chH).reduced(2, 2)
                );
            }
            return;
        }

        // 3. Choice selector strip above toggles (e.g., Mode HP/LP, Mode I/II/III, Attack 1-6, Curve, Head 1..12)
        for (auto& ch : choices) {
            const int cH = shortPedal ? 19 : 22;
            auto row = bay.removeFromBottom(cH);
            bay.removeFromBottom(3);
            if (ch->useButtons) {
                ch->label.setBounds(0, 0, 0, 0);
                const int n = static_cast<int>(ch->segButtons.size());
                const int bw = row.getWidth() / std::max(1, n);
                for (int b = 0; b < n; ++b) {
                    ch->segButtons[static_cast<size_t>(b)]->setBounds(row.removeFromLeft(bw).reduced(1, 0));
                }
            } else if (ch->combo != nullptr) {
                ch->label.setBounds(0, 0, 0, 0);
                ch->combo->setBounds(row);
            }
        }

        // 4. Expression Pedal Treadle + Right-Hand Parameter Knobs (WAH, OCTA, Pitch, P-Bend)
        if (wahTreadle != nullptr) {
            const int n = static_cast<int>(controls.size());
            const int rightPct = (n >= 4) ? 56 : (narrowPedal ? 50 : 46);
            auto rightKnobs = bay.removeFromRight(bay.getWidth() * rightPct / 100);
            wahTreadle->setBounds(bay.reduced(0, 2));

            if (n <= 0) return;
            const int cols = (n >= 4 && rightKnobs.getWidth() >= 76) ? 2 : 1;
            const int rows = (n + cols - 1) / cols;
            const int cw = rightKnobs.getWidth() / cols;
            const int maxRowH = rightKnobs.getHeight() / std::max(1, rows);
            const int lblH = (maxRowH < 48) ? 10 : 12;
            const int tbH  = (maxRowH < 64) ? 11 : 13;
            const int idealRowH = std::min(maxRowH, std::min(cw, 74) + lblH + tbH + 8);
            const int rowGap = (rows > 1) ? std::min(8, std::max(2, (rightKnobs.getHeight() - idealRowH * rows) / (rows + 1))) : 0;
            const int blockH = idealRowH * rows + rowGap * (rows - 1);
            const int startY = rightKnobs.getCentreY() - blockH / 2;

            for (int i = 0; i < n; ++i) {
                const int r = i / cols;
                const int c = i % cols;
                auto cell = juce::Rectangle<int>(rightKnobs.getX() + c * cw, startY + r * (idealRowH + rowGap), cw, idealRowH).reduced(2, 1);
                controls[static_cast<size_t>(i)]->label.setBounds(cell.removeFromTop(lblH));
                applyAdaptiveRotary(controls[static_cast<size_t>(i)]->slider, cell.getWidth(), cell.getHeight());
                controls[static_cast<size_t>(i)]->slider.setBounds(cell);
            }
            return;
        }

        const int count = static_cast<int>(controls.size());
        if (count <= 0) return;

        // 5. Vertical Graphic EQ Faders (5-band or 10-band Hyper EQ + optional VOL)
        if (controls[0]->isVertical) {
            const int fw = std::max(10, bay.getWidth() / count);
            const int startX = bay.getCentreX() - (fw * count) / 2;
            const float fnt = (count >= 10 || fw < 20) ? 6.6f : 7.8f;
            for (int i = 0; i < count; ++i) {
                auto col = juce::Rectangle<int>(startX + i * fw, bay.getY(), fw, bay.getHeight()).reduced(0, 2);
                controls[static_cast<size_t>(i)]->label.setFont(juce::Font(juce::FontOptions(fnt, juce::Font::bold)));
                controls[static_cast<size_t>(i)]->label.setBounds(col.removeFromBottom(12));
                controls[static_cast<size_t>(i)]->slider.setBounds(col);
            }
            return;
        }

        // 6. Standard Rotary Knobs Grid (1..10 knobs) with tight proportional knob-to-valuebox height & vertical centering!
        int cols = 1;
        if (count == 1)      cols = 1;
        else if (count <= 4) cols = 2;
        else if (count <= 6) cols = (bay.getWidth() < 115) ? 2 : 3;
        else if (count <= 9) cols = (bay.getWidth() < 120) ? 2 : 3;
        else                 cols = (bay.getWidth() < 140) ? 3 : 4;

        const int rows = (count + cols - 1) / cols;
        const int cellW = bay.getWidth() / cols;
        const int maxRowH = bay.getHeight() / std::max(1, rows);
        const int lblH  = (maxRowH < 50) ? 10 : 12;
        const int tbH   = (maxRowH < 64) ? 11 : 13;

        // Keep knob circle + value capsule tight (never floating 40px apart on tall pedals!)
        const int maxKnobD = (count == 1) ? 94 : ((count <= 2) ? 84 : ((cols == 2) ? 78 : 64));
        const int knobD = std::min(cellW - 4, maxKnobD);
        const int idealRowH = std::min(maxRowH, knobD + lblH + tbH + 10);
        const int rowGap = (rows > 1) ? std::min(14, std::max(3, (bay.getHeight() - idealRowH * rows) / (rows + 1))) : 0;
        const int totalGridH = idealRowH * rows + rowGap * (rows - 1);
        const int gridTop = bay.getCentreY() - totalGridH / 2;

        for (int i = 0; i < count; ++i) {
            const int r = i / cols;
            const int c = i % cols;
            const int itemsInRow = (r == rows - 1) ? (count - r * cols) : cols;
            const int rowStartX = bay.getCentreX() - (itemsInRow * cellW) / 2;
            const int xPos = (itemsInRow < cols) ? (rowStartX + c * cellW) : (bay.getX() + c * cellW);

            auto cell = juce::Rectangle<int>(xPos, gridTop + r * (idealRowH + rowGap), cellW, idealRowH).reduced(2, 1);
            if (count == 1) {
                const int heroW = std::min(cell.getWidth(), 112);
                cell = cell.withSizeKeepingCentre(heroW, idealRowH);
            }
            controls[static_cast<size_t>(i)]->label.setBounds(cell.removeFromTop(lblH));
            applyAdaptiveRotary(controls[static_cast<size_t>(i)]->slider, cell.getWidth(), cell.getHeight());
            controls[static_cast<size_t>(i)]->slider.setBounds(cell);
        }
    }

private:
    struct ControlItem {
        bool isVertical = false;
        int paramIdx = 0;
        juce::Slider slider;
        juce::Label label;
    };

    struct ToggleItem {
        int paramIdx = 0;
        juce::TextButton btn;
    };

    struct ChoiceItem {
        int paramIdx = 0;
        int numChoices = 0;
        bool useButtons = false;
        juce::Label label;
        std::unique_ptr<juce::ComboBox> combo;
        std::vector<std::unique_ptr<juce::TextButton>> segButtons;
    };

    const char* knobStyleForCategory(ModuleId cat) const noexcept {
        switch (cat) {
            case ModuleId::Compressor:
            case ModuleId::StereoChorus:
                return "cyan_stomp";
            case ModuleId::Wah:
            case ModuleId::GraphicEq:
            case ModuleId::StereoDelay:
            case ModuleId::Reverb:
                return "chrome_knurled";
            default:
                return "vintage_skirted";
        }
    }

    void rebuildControlsForCategory(ModuleId cat, int modelIdx = 0) {
        controls.clear();
        toggles.clear();
        choices.clear();
        wahTreadle.reset();

        const auto specs = getSlotParamSpecs(cat, modelIdx);
        const auto& entry = gp200::getModelEntry(cat, modelIdx);
        const auto accent = juce::Colour(entry.chassisArgb);
        const char* knobStyle = isExpressionPedalSlotModel(cat, modelIdx)
            ? "chrome_knurled"
            : (isNamPedalSlotModel(cat, modelIdx) ? "cyan_stomp" : knobStyleForCategory(cat));

        for (size_t i = 0; i < specs.size(); ++i) {
            const auto& d = specs[i];
            const int pIdx = static_cast<int>(i);

            if (d.isExpression()) {
                wahTreadle = std::make_unique<SlotWahTreadleWidget>(processor, slotId, accent);
                addAndMakeVisible(*wahTreadle);
                continue;
            }

            if (d.isToggle()) {
                auto t = std::make_unique<ToggleItem>();
                t->paramIdx = pIdx;
                t->btn.setButtonText(d.label);
                t->btn.setClickingTogglesState(true);
                t->btn.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.2f));
                t->btn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
                auto* rawBtn = &t->btn;
                t->btn.onClick = [this, pIdx, rawBtn] {
                    if (!ignoreCallbacks) {
                        processor.setPedalSlotParam(slotId, pIdx, rawBtn->getToggleState() ? 1.0f : 0.0f);
                        for (auto& c : controls) c->slider.updateText();
                        repaint();
                    }
                };
                addAndMakeVisible(t->btn);
                toggles.push_back(std::move(t));
                continue;
            }

            if (d.isChoice()) {
                auto ch = std::make_unique<ChoiceItem>();
                ch->paramIdx = pIdx;
                ch->numChoices = d.numChoices;
                ch->useButtons = (d.numChoices <= 4 && specs.size() <= 2);
                ch->label.setText(d.label, juce::dontSendNotification);
                ch->label.setJustificationType(juce::Justification::centred);
                ch->label.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
                ch->label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
                addAndMakeVisible(ch->label);

                if (ch->useButtons) {
                    for (int b = 0; b < d.numChoices; ++b) {
                        auto sb = std::make_unique<juce::TextButton>(d.choices[static_cast<size_t>(b)]);
                        sb->setClickingTogglesState(false);
                        sb->setColour(juce::TextButton::buttonOnColourId, accent.darker(0.18f));
                        sb->setMouseCursor(juce::MouseCursor::PointingHandCursor);
                        sb->onClick = [this, pIdx, b] {
                            if (!ignoreCallbacks) {
                                processor.setPedalSlotParam(slotId, pIdx, static_cast<float>(b));
                                syncFromProcessor(false);
                            }
                        };
                        addAndMakeVisible(*sb);
                        ch->segButtons.push_back(std::move(sb));
                    }
                } else {
                    ch->combo = std::make_unique<juce::ComboBox>();
                    for (int b = 0; b < d.numChoices; ++b) {
                        ch->combo->addItem(juce::String(d.label) + ": " + d.choices[static_cast<size_t>(b)], b + 1);
                    }
                    auto* rawCombo = ch->combo.get();
                    ch->combo->onChange = [this, pIdx, rawCombo] {
                        if (!ignoreCallbacks && rawCombo->getSelectedItemIndex() >= 0) {
                            processor.setPedalSlotParam(slotId, pIdx, static_cast<float>(rawCombo->getSelectedItemIndex()));
                        }
                    };
                    addAndMakeVisible(*ch->combo);
                }
                choices.push_back(std::move(ch));
                continue;
            }

            auto item = std::make_unique<ControlItem>();
            item->isVertical = d.isVerticalFader();
            item->paramIdx = pIdx;
            item->slider.setRange(d.minVal, d.maxVal, d.step);
            item->slider.setValue(d.defaultVal, juce::dontSendNotification);
            item->slider.setDoubleClickReturnValue(true, d.defaultVal);

            if (d.isVerticalFader()) {
                item->slider.setSliderStyle(juce::Slider::LinearVertical);
                item->slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                item->slider.setPopupDisplayEnabled(true, false, this);
                item->slider.setColour(juce::Slider::trackColourId, accent);
                item->slider.getProperties().set("faderStyle", "eq");
            } else {
                item->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                item->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 13);
                item->slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
                item->slider.getProperties().set("knobStyle", knobStyle);
                item->slider.getProperties().set("showTicks", true);
                if (d.suffix != nullptr && d.suffix[0] != '\0') {
                    item->slider.setTextValueSuffix(juce::String(" ") + d.suffix);
                }
                bool isDelaySync = false;
                const int syncIdx = findSyncToggleIndexForParam(specs, pIdx, isDelaySync);
                if (syncIdx >= 0) {
                    const float minV = d.minVal;
                    const float maxV = d.maxVal;
                    const juce::String sfx = (d.suffix != nullptr && d.suffix[0] != '\0')
                                                 ? (juce::String(" ") + d.suffix)
                                                 : juce::String();
                    item->slider.textFromValueFunction = [this, syncIdx, isDelaySync, minV, maxV, sfx](double v) -> juce::String {
                        const auto cur = processor.getPedalSlot(slotId);
                        if (cur.params[static_cast<size_t>(syncIdx)] > 0.5f) {
                            return isDelaySync
                                ? juce::String(tempo_sync::delayDivisionLabel(static_cast<float>(v), minV, maxV))
                                : juce::String(tempo_sync::modDivisionLabel(static_cast<float>(v), minV, maxV));
                        }
                        const int dec = (maxV - minV) >= 40.0f ? 0 : ((maxV - minV) >= 10.0f ? 1 : 2);
                        return juce::String(v, dec) + sfx;
                    };
                    item->slider.updateText();
                }
            }

            auto* rawSlider = &item->slider;
            item->slider.setMouseCursor(juce::MouseCursor::PointingHandCursor);
            item->slider.onValueChange = [this, pIdx, rawSlider] {
                if (!ignoreCallbacks) {
                    processor.setPedalSlotParam(slotId, pIdx, static_cast<float>(rawSlider->getValue()));
                }
            };
            addAndMakeVisible(item->slider);

            item->label.setText(d.label, juce::dontSendNotification);
            item->label.setJustificationType(juce::Justification::centred);
            item->label.setFont(juce::Font(juce::FontOptions(d.isVerticalFader() ? 7.6f : 8.8f, juce::Font::bold)));
            item->label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
            item->label.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(item->label);

            controls.push_back(std::move(item));
        }

        resized();
    }

    PedalBoardAudioProcessor& processor;
    int slotId = 0;
    ModuleId currentCategory = ModuleId::Overdrive;
    int currentModelIndex = -1;
    juce::Colour chassisColour { 0xfff01e3c };
    juce::String activeModelName;
    juce::String activeModelDesc;
    bool ignoreCallbacks = false;

    juce::ComboBox modelBox;
    StompFootswitchButton footswitch;
    juce::TextButton namLoadBtn, namCloudBtn, namClearBtn;
    juce::Rectangle<int> namOledRect;
    bool namModelLoaded = false;
    juce::String namLoadedName;
    juce::String namStatusText;
    std::vector<std::unique_ptr<ControlItem>> controls;
    std::vector<std::unique_ptr<ToggleItem>> toggles;
    std::vector<std::unique_ptr<ChoiceItem>> choices;
    std::unique_ptr<SlotWahTreadleWidget> wahTreadle;

    float grDb = 0.0f;
    int currentOrderIndex = 0;
    int totalActiveSlots = 5;
    bool dragArmed = false;
    bool selectedOnBoard = false;

    std::function<void(int, juce::Point<int>)> boardDownCb;
    std::function<void(int, juce::Point<int>)> boardDragCb;
    std::function<void(int, juce::Point<int>)> boardUpCb;
    std::function<void(int, int)> stepOrderCb;
    std::function<void(int)> removeCb;
    std::function<void(int)> loadSlotNamCb;
    std::function<void(int)> browseSlotNamCloudCb;
};

class StompBoardView : public juce::Component {
public:
    explicit StompBoardView(PedalBoardAudioProcessor& proc) : processor(proc) {
        for (int i = 0; i < kMaxPedalSlots; ++i) {
            auto p = std::make_unique<DynamicSlotPedal>(processor, i);
            p->setCallbacks(
                [this](int sId, juce::Point<int> pt) { beginPedalDragBySlotId(sId, pt); },
                [this](int,     juce::Point<int> pt) { updatePedalDrag(pt); },
                [this](int,     juce::Point<int> pt) { endPedalDrag(pt); },
                [this](int sId, int delta)           { stepStompPedal(sId, delta); },
                [this](int sId)                      { processor.removePedalSlot(sId); syncOrderFromChain(true); }
            );
            addChildComponent(*p);
            slotPool[static_cast<size_t>(i)] = std::move(p);
        }

        syncOrderFromChain(true);
    }

    void setLaneChangedCallback(std::function<void(int)> cb) {
        laneChangedCb = std::move(cb);
    }

    void setLoadSlotNamCallback(std::function<void(int)> cb) {
        loadSlotNamCb = std::move(cb);
        for (auto& p : slotPool) {
            if (p != nullptr) {
                p->setLoadSlotNamCallback([this](int sId) {
                    if (loadSlotNamCb) loadSlotNamCb(sId);
                });
            }
        }
    }

    void setBrowseSlotNamCloudCallback(std::function<void(int)> cb) {
        browseSlotNamCloudCb = std::move(cb);
        for (auto& p : slotPool) {
            if (p != nullptr) {
                p->setBrowseSlotNamCloudCallback([this](int sId) {
                    if (browseSlotNamCloudCb) browseSlotNamCloudCb(sId);
                });
            }
        }
    }

    int getActiveLaneFilter() const noexcept {
        return isParallelRouting(processor.getRoutingMode()) ? activeLaneFilter : -1;
    }

    void setActiveLaneFilter(int laneFilter) {
        laneFilter = normalizeLaneFilter(laneFilter, processor.getRoutingMode());
        if (activeLaneFilter != laneFilter) {
            activeLaneFilter = laneFilter;
            if (laneChangedCb) laneChangedCb(activeLaneFilter);
            syncOrderFromChain(true);
            repaint();
        }
    }

    int getEffectiveInsertLane(ModuleId cat = ModuleId::Overdrive) const noexcept {
        const auto mode = processor.getRoutingMode();
        if (!isParallelRouting(mode)) return -1;
        const bool isPostTimeFx = (cat == ModuleId::StereoDelay || cat == ModuleId::Reverb);
        if (mode == RoutingMode::DualIndependent) {
            if (activeLaneFilter == static_cast<int>(BusLane::PathB)) {
                return static_cast<int>(isPostTimeFx ? BusLane::Post : BusLane::PathB);
            }
            return static_cast<int>(isPostTimeFx ? BusLane::PostA : BusLane::PathA);
        }
        return (activeLaneFilter >= 0) ? activeLaneFilter : -1;
    }

    void updateLive() {
        if (!isDraggingPedal) {
            syncOrderFromChain(false);
        }
        for (auto* p : activePedals) {
            if (p == nullptr) continue;
            const auto snap = processor.getPedalSlot(p->getSlotId());
            if (isExpressionPedalSlotModel(snap.category, snap.modelIndex)) {
                p->syncFromProcessor(false);
            } else if (isNamPedalSlotModel(snap.category, snap.modelIndex)) {
                const auto& runner = processor.getSignalChain().getSlotNamRunner(p->getSlotId());
                const bool loadedNow = runner.hasLoadedModel();
                const juce::String nameNow = loadedNow ? runner.getDisplayName() : juce::String();
                if (loadedNow != p->hasNamLoaded() || nameNow != p->getNamLoadedName()) {
                    p->syncFromProcessor(false);
                    repaint();
                }
            } else if (p->getCategory() == ModuleId::Compressor) {
                p->setGrDb(processor.getSignalChain().getSlotGainReductionDb(p->getSlotId()));
            }
        }
    }

    void refreshAll() {
        if (!isDraggingPedal) {
            syncOrderFromChain(true);
        }
        for (auto* p : activePedals) {
            if (p != nullptr) p->syncFromProcessor(false);
        }
        repaint();
    }

    // External drag-and-drop hover from Right-Side GearSidebar
    void setSidebarDragHover(bool active, juce::Point<int> localPt, ModuleId cat, const juce::String& modelName, juce::Colour col) {
        sidebarDragActive = active;
        sidebarDragPt = localPt;
        sidebarDragCat = cat;
        sidebarDragModelName = modelName;
        sidebarDragColour = col;
        repaint();
    }

    // Determine whether a sidebar drag point over StompBoardView means REPLACE a pedal (center 56% of a pedal)
    // or INSERT a new pedal (between pedals / left & right 22% edges, when < 8 slots).
    bool isSidebarDropReplaceMode(juce::Point<int> localPt, int& outTargetOrderIdx) const noexcept {
        outTargetOrderIdx = -1;
        const int numActive = static_cast<int>(activePedals.size());
        if (numActive <= 0) return false;

        for (int i = 0; i < numActive; ++i) {
            auto* p = activePedals[static_cast<size_t>(i)];
            if (p == nullptr) continue;
            const auto b = p->getBounds();
            if (b.contains(localPt)) {
                if (numActive >= kMaxPedalSlots) {
                    outTargetOrderIdx = i;
                    return true;
                }
                const int edgeMargin = std::max(22, b.getWidth() * 22 / 100);
                if (localPt.x >= b.getX() + edgeMargin && localPt.x <= b.getRight() - edgeMargin) {
                    outTargetOrderIdx = i;
                    return true;
                }
            }
        }
        if (numActive >= kMaxPedalSlots) {
            outTargetOrderIdx = findClosestOrderIndex(localPt);
            return true;
        }
        return false;
    }

    // Handle drop from Right-Side GearSidebar onto StompBoardView
    bool handleSidebarDrop(juce::Point<int> localPt, ModuleId cat, int modelIndex) {
        sidebarDragActive = false;
        if (cat == ModuleId::AmpHead || cat == ModuleId::CabinetIR) {
            repaint();
            return false;
        }

        int replaceIdx = -1;
        if (isSidebarDropReplaceMode(localPt, replaceIdx) && replaceIdx >= 0 &&
            replaceIdx < static_cast<int>(activePedals.size())) {
            const int sId = activePedals[static_cast<size_t>(replaceIdx)]->getSlotId();
            processor.replacePedalSlot(sId, cat, modelIndex);
            selectedSlotId = sId;
            syncOrderFromChain(true);
            return true;
        }

        // Otherwise INSERT as a brand-new independent pedal slot onto the currently viewed lane!
        const int insertOrderIdx = findInsertionOrderIndex(localPt);
        const int laneForAdd = getEffectiveInsertLane(cat);
        const int newSlotId = processor.addPedalSlot(cat, modelIndex,
                                                     (activeLaneFilter < 0) ? insertOrderIdx : -1,
                                                     laneForAdd);
        if (newSlotId >= 0) {
            selectedSlotId = newSlotId;
        }
        syncOrderFromChain(true);
        return true;
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto pos = e.getPosition();
        int nextTab = -1;
        for (int i = 0; i < laneTabCount; ++i) {
            if (laneTabRects[static_cast<size_t>(i)].contains(pos)) {
                nextTab = i;
                break;
            }
        }
        const bool nextAdd = !laneAddBtnRect.isEmpty() && laneAddBtnRect.contains(pos);
        if (nextTab != hoveredLaneTab || nextAdd != hoveredLaneAddBtn) {
            hoveredLaneTab = nextTab;
            hoveredLaneAddBtn = nextAdd;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoveredLaneTab != -1 || hoveredLaneAddBtn) {
            hoveredLaneTab = -1;
            hoveredLaneAddBtn = false;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragCandidateIdx = -1;
        isDraggingPedal = false;
        dropHoverIdx = -1;
        const auto pos = e.getPosition();

        // Click on top Per-Lane Switcher Tabs (ALL | PRE | PATH A | PATH B | POST) or + ADD button
        if (isParallelRouting(processor.getRoutingMode())) {
            for (int i = 0; i < laneTabCount; ++i) {
                if (laneTabRects[static_cast<size_t>(i)].contains(pos)) {
                    setActiveLaneFilter(laneTabFilterIds[static_cast<size_t>(i)]);
                    return;
                }
            }
            if (!laneAddBtnRect.isEmpty() && laneAddBtnRect.contains(pos)) {
                openAddPedalPopupMenu(laneAddBtnRect);
                return;
            }
        }

        // Click on empty-board card or '+ ADD PEDAL' trigger
        if (addPedalStageRect.contains(pos) || addPedalBarRect.contains(pos)) {
            openAddPedalPopupMenu(addPedalStageRect.contains(pos) ? addPedalStageRect : addPedalBarRect);
            return;
        }

        const int numActive = static_cast<int>(activePedals.size());
        for (int i = 0; i < numActive; ++i) {
            const auto slot = slotBarRects[static_cast<size_t>(i)];
            if (!slot.contains(pos)) continue;

            if (pos.x < slot.getX() + 26) {
                activePedals[static_cast<size_t>(i)]->toggleBypass();
                repaint();
                return;
            }
            if (e.mods.isPopupMenu()) {
                openSlotContextMenu(i, slot);
                return;
            }
            beginSlotDrag(i, pos);
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        updatePedalDrag(e.getPosition());
    }

    void mouseUp(const juce::MouseEvent& e) override {
        endPedalDrag(e.getPosition());
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        const auto mode = processor.getRoutingMode();
        const bool showLaneBar = isParallelRouting(mode);
        const float laneH = showLaneBar ? static_cast<float>(kLaneBarHeight) : 0.0f;

        if (showLaneBar) {
            paintLaneSwitcherBar(g, bounds.withHeight(laneH), mode);
        }

        auto stageArea = bounds.withTrimmedTop(laneH).withTrimmedBottom(static_cast<float>(kSlotBarHeight));
        valeton::drawCleanStageBackdrop(g, stageArea);

        // Subtle Pedaltrain-style dual horizontal mounting rails behind stompboxes
        const auto refDeck = pedalGroupRect.isEmpty() ? stageArea.reduced(14.0f, 20.0f) : pedalGroupRect.toFloat();
        const float upperRailY = refDeck.getY() + refDeck.getHeight() * 0.22f;
        const float lowerRailY = refDeck.getY() + refDeck.getHeight() * 0.78f;
        auto drawBoardRail = [&](float ry) {
            auto rRect = juce::Rectangle<float>(stageArea.getX() + 14.0f, ry - 9.0f, stageArea.getWidth() - 28.0f, 18.0f);
            juce::ColourGradient rg(juce::Colour(0xff1b202b), 0.0f, rRect.getY(),
                                    juce::Colour(0xff12151d), 0.0f, rRect.getBottom(), false);
            g.setGradientFill(rg);
            g.fillRoundedRectangle(rRect, 4.0f);
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawHorizontalLine(static_cast<int>(rRect.getY() + 1.0f), rRect.getX() + 4.0f, rRect.getRight() - 4.0f);
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.drawHorizontalLine(static_cast<int>(rRect.getBottom()), rRect.getX() + 4.0f, rRect.getRight() - 4.0f);
        };
        drawBoardRail(upperRailY);
        drawBoardRail(lowerRailY);

        // Glowing patch cable aligned directly to every pedal's 1/4" side jack stubs (color-coded to active lane!)
        const float jackY = refDeck.getY() + refDeck.getHeight() * 0.44f;
        const auto cableAccent = laneColourForFilter(showLaneBar ? activeLaneFilter : -1);
        g.setColour(cableAccent.withAlpha(showLaneBar && activeLaneFilter >= 0 ? 0.24f : 0.16f));
        g.drawLine(stageArea.getX() + 14.0f, jackY, stageArea.getRight() - 14.0f, jackY, 6.0f);
        g.setColour(showLaneBar && activeLaneFilter >= 0 ? cableAccent.withAlpha(0.75f) : juce::Colour(0xff52607a));
        g.drawLine(stageArea.getX() + 14.0f, jackY, stageArea.getRight() - 14.0f, jackY, 2.2f);

        // In Dual Independent Chain A/B view, show a crisp inline AMP/CAB stage divider between Pre-Amp & Post-Amp pedals!
        if (mode == RoutingMode::DualIndependent && !dualIndAmpDividerRect.isEmpty()) {
            const auto dr = dualIndAmpDividerRect.toFloat();
            const bool isChainB = (activeLaneFilter == static_cast<int>(BusLane::PathB));
            const auto divCol = isChainB ? juce::Colour(0xff38bdf8) : juce::Colour(0xfff59e0b);
            g.setColour(juce::Colour(0xff0d1118));
            g.fillRoundedRectangle(dr, 5.0f);
            g.setColour(divCol.withAlpha(0.65f));
            g.drawRoundedRectangle(dr, 5.0f, 1.1f);
            g.setColour(divCol.brighter(0.25f));
            g.setFont(juce::Font(juce::FontOptions(7.6f, juce::Font::bold)));
            g.drawText(isChainB ? "AMP B\nCAB B" : "AMP A\nCAB A", dr.reduced(2.0f, 3.0f), juce::Justification::centred);
        }

        // Draw '+ ADD PEDAL' drop/click card on right side of stage when 1..7 pedals are active
        if (!activePedals.empty() && !addPedalStageRect.isEmpty()) {
            auto ar = addPedalStageRect.toFloat().reduced(3.0f, 6.0f);
            g.setColour(juce::Colour(0xff141822).withAlpha(0.82f));
            g.fillRoundedRectangle(ar, 10.0f);
            g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.55f));
            g.drawRoundedRectangle(ar, 10.0f, 1.4f);

            const float cx = ar.getCentreX();
            const float cy = ar.getCentreY() - 14.0f;
            g.setColour(juce::Colour(0xff2563eb).withAlpha(0.28f));
            g.fillEllipse(cx - 18.0f, cy - 18.0f, 36.0f, 36.0f);
            g.setColour(juce::Colour(0xff60a5fa));
            g.drawEllipse(cx - 18.0f, cy - 18.0f, 36.0f, 36.0f, 1.5f);
            g.drawLine(cx - 8.0f, cy, cx + 8.0f, cy, 2.2f);
            g.drawLine(cx, cy - 8.0f, cx, cy + 8.0f, 2.2f);

            g.setColour(juce::Colour(0xffcbd5e1));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText("+ ADD PEDAL", ar.withTop(cy + 24.0f).withHeight(16.0f), juce::Justification::centred);
            g.setColour(juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::plain)));
            g.drawText("Click or Drag\nfrom Gear List", ar.withTop(cy + 40.0f).withHeight(26.0f), juce::Justification::centred);
        }

        auto barBg = bounds.withTop(stageArea.getBottom());
        g.setColour(juce::Colour(0xff14161b));
        g.fillRect(barBg);
        g.setColour(juce::Colour(0xff2a2f3b));
        g.drawHorizontalLine(static_cast<int>(barBg.getY()), 0.0f, bounds.getWidth());

        const int numActive = static_cast<int>(activePedals.size());
        if (numActive == 0) {
            const float cw = std::clamp(stageArea.getWidth() * 0.44f, 280.0f, 400.0f);
            const float ch = std::clamp(stageArea.getHeight() * 0.40f, 134.0f, 176.0f);
            auto emptyCard = stageArea.withSizeKeepingCentre(cw, ch);

            g.setColour(juce::Colour(0xff0f131c).withAlpha(0.92f));
            g.fillRoundedRectangle(emptyCard, 10.0f);
            g.setColour(juce::Colour(0xff263042));
            g.drawRoundedRectangle(emptyCard, 10.0f, 1.1f);

            const float cx = emptyCard.getCentreX();
            const float cy = emptyCard.getY() + emptyCard.getHeight() * 0.34f;
            g.setColour(juce::Colour(0xff181f2c));
            g.fillEllipse(cx - 18.0f, cy - 18.0f, 36.0f, 36.0f);
            g.setColour(juce::Colour(0xff334155));
            g.drawEllipse(cx - 18.0f, cy - 18.0f, 36.0f, 36.0f, 1.2f);
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawLine(cx - 7.0f, cy, cx + 7.0f, cy, 1.8f);
            g.drawLine(cx, cy - 7.0f, cx, cy + 7.0f, 1.8f);

            g.setColour(juce::Colour(0xffe2e8f0));
            g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            const juce::String emptyTitle = (showLaneBar && activeLaneFilter >= 0)
                ? ("No Pedals in " + laneDisplayName(activeLaneFilter, mode))
                : "No Effects Loaded";
            g.drawText(emptyTitle,
                       juce::Rectangle<float>(emptyCard.getX() + 16.0f, cy + 25.0f, emptyCard.getWidth() - 32.0f, 18.0f),
                       juce::Justification::centred);

            g.setColour(juce::Colour(0xff6b778c));
            g.setFont(juce::Font(juce::FontOptions(10.4f, juce::Font::plain)));
            const juce::String emptySub = (showLaneBar && activeLaneFilter >= 0)
                ? ("Click to add a pedal to " + laneDisplayName(activeLaneFilter, mode) + ", or drag from Gear Library")
                : "Click to add a pedal, or drag from the Gear Library";
            g.drawText(emptySub,
                       juce::Rectangle<float>(emptyCard.getX() + 16.0f, cy + 46.0f, emptyCard.getWidth() - 32.0f, 16.0f),
                       juce::Justification::centred);
        }

        for (int i = 0; i < numActive; ++i) {
            auto* p = activePedals[static_cast<size_t>(i)];
            if (p == nullptr) continue;
            const auto sr = slotBarRects[static_cast<size_t>(i)].toFloat().reduced(2.0f, 3.0f);
            const bool active = p->isEngaged();
            const auto accent = p->getActiveAccent();

            g.setColour(juce::Colour(0xff1c2028));
            g.fillRoundedRectangle(sr, 3.0f);
            g.setColour(active ? accent.withAlpha(0.55f) : juce::Colour(0xff2d3340));
            g.drawRoundedRectangle(sr, 3.0f, 1.0f);

            const float px = sr.getX() + 12.0f;
            const float py = sr.getCentreY();
            const auto powCol = active ? juce::Colour(0xfff59e0b) : juce::Colour(0xff64748b);
            if (active) {
                g.setColour(powCol.withAlpha(0.20f));
                g.fillEllipse(px - 7.0f, py - 7.0f, 14.0f, 14.0f);
            }
            g.setColour(powCol);
            juce::Path arc;
            arc.addCentredArc(px, py, 4.5f, 4.5f, 0.0f, 0.55f, 5.73f, true);
            g.strokePath(arc, juce::PathStrokeType(1.5f));
            g.drawLine(px, py - 5.8f, px, py - 1.2f, 1.5f);

            juce::String laneTag;
            if (showLaneBar) {
                const auto pBus = processor.getPedalSlotBus(p->getSlotId());
                if (activeLaneFilter < 0) {
                    laneTag = juce::String(busLaneShortLabelForMode(pBus, mode)) + " | ";
                } else if (mode == RoutingMode::DualIndependent) {
                    const bool isPost = (pBus == BusLane::Post || pBus == BusLane::PostA || pBus == BusLane::PathC);
                    laneTag = isPost ? "POST | " : "PRE | ";
                }
            }
            const juce::String codeTag = isNamPedalSlotModel(p->getCategory(), p->getModelIndex())
                ? juce::String("NAM")
                : juce::String(valeton::gpCodeFor(p->getCategory()));
            const juce::String slotPrefix = laneTag + "#" + juce::String(i + 1) + " ["
                                          + codeTag + "] ";
            g.setColour(active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(sr.getWidth() < 120.0f ? 8.6f : 9.8f, juce::Font::bold)));
            g.drawText(slotPrefix + p->getActiveModelName(),
                       sr.withTrimmedLeft(22.0f).withTrimmedRight(6.0f),
                       juce::Justification::centred, true);
        }

        if (!addPedalBarRect.isEmpty()) {
            auto abr = addPedalBarRect.toFloat().reduced(2.0f, 3.0f);
            g.setColour(juce::Colour(0xff1e293b));
            g.fillRoundedRectangle(abr, 3.0f);
            g.setColour(juce::Colour(0xff3b82f6));
            g.drawRoundedRectangle(abr, 3.0f, 1.0f);
            g.setColour(juce::Colour(0xff60a5fa));
            g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            g.drawText("+ ADD", abr, juce::Justification::centred);
        }
    }

    void paintOverChildren(juce::Graphics& g) override {
        const int numActive = static_cast<int>(activePedals.size());

        // 1. Highlight when dragging a gear item from the right-hand GearSidebar over StompBoardView!
        //    - Center of an existing pedal = REPLACE that pedal
        //    - Between pedals / edges = + INSERT NEW PEDAL at that gap
        if (sidebarDragActive && sidebarDragCat != ModuleId::AmpHead && sidebarDragCat != ModuleId::CabinetIR) {
            int replaceIdx = -1;
            const bool replaceMode = isSidebarDropReplaceMode(sidebarDragPt, replaceIdx);

            if (replaceMode && replaceIdx >= 0 && replaceIdx < numActive) {
                auto r = activePedals[static_cast<size_t>(replaceIdx)]->getBounds().toFloat().reduced(2.0f);
                g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.20f));
                g.fillRoundedRectangle(r, 12.0f);
                g.setColour(juce::Colour(0xfff59e0b));
                g.drawRoundedRectangle(r, 12.0f, 2.8f);

                auto badge = juce::Rectangle<float>(r.getCentreX() - 82.0f, r.getY() + 10.0f, 164.0f, 26.0f);
                g.setColour(juce::Colour(0xff0f172a).withAlpha(0.96f));
                g.fillRoundedRectangle(badge, 6.0f);
                g.setColour(juce::Colour(0xfff59e0b));
                g.drawRoundedRectangle(badge, 6.0f, 1.4f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                g.drawText("REPLACE SLOT #" + juce::String(replaceIdx + 1), badge, juce::Justification::centred);
            } else {
                const int insIdx = findInsertionOrderIndex(sidebarDragPt);
                float lineX = static_cast<float>(getWidth() / 2);
                if (numActive == 0) {
                    lineX = static_cast<float>(getWidth() / 2);
                } else if (insIdx <= 0) {
                    lineX = static_cast<float>(activePedals[0]->getX() - 4);
                } else if (insIdx >= numActive) {
                    lineX = static_cast<float>(activePedals.back()->getRight() + 5);
                } else {
                    lineX = 0.5f * static_cast<float>(activePedals[static_cast<size_t>(insIdx - 1)]->getRight()
                                                      + activePedals[static_cast<size_t>(insIdx)]->getX());
                }

                // Glowing vertical insertion slot gap
                g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.25f));
                g.fillRoundedRectangle(lineX - 5.0f, 14.0f, 10.0f, static_cast<float>(getHeight() - kSlotBarHeight - 24), 4.0f);
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawLine(lineX, 14.0f, lineX, static_cast<float>(getHeight() - kSlotBarHeight - 14), 3.5f);

                auto badge = juce::Rectangle<float>(
                    std::clamp(lineX - 92.0f, 10.0f, static_cast<float>(getWidth()) - 194.0f),
                    16.0f, 184.0f, 26.0f
                );
                g.setColour(juce::Colour(0xff0f172a).withAlpha(0.96f));
                g.fillRoundedRectangle(badge, 6.0f);
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawRoundedRectangle(badge, 6.0f, 1.5f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                g.drawText("+ INSERT " + sidebarDragModelName.toUpperCase() + " (#" + juce::String(insIdx + 1) + ")",
                           badge, juce::Justification::centred, true);
            }
        }

        // 2. Internal pedal reorder / remove drag preview + Visible Top Trash Drop Zone Banner!
        if (!isDraggingPedal || dragCandidateIdx < 0 || dragCandidateIdx >= numActive) return;

        const bool inTrashZone = isPointOutsideForRemoval(dragCurrentPos);
        const float tbW = std::min(290.0f, static_cast<float>(getWidth()) - 20.0f);
        auto trashBanner = juce::Rectangle<float>(static_cast<float>(getWidth()) * 0.5f - tbW * 0.5f, 6.0f, tbW, 28.0f);
        g.setColour(inTrashZone ? juce::Colour(0xffdc2626).withAlpha(0.95f)
                                : juce::Colour(0xff451a1a).withAlpha(0.88f));
        g.fillRoundedRectangle(trashBanner, 7.0f);
        g.setColour(inTrashZone ? juce::Colour(0xfffca5a5) : juce::Colour(0xffef4444).withAlpha(0.75f));
        g.drawRoundedRectangle(trashBanner, 7.0f, inTrashZone ? 2.0f : 1.2f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText(inTrashZone ? "RELEASE MOUSE TO REMOVE PEDAL" : "DRAG UP HERE (OR OFF BOARD) TO REMOVE PEDAL",
                   trashBanner, juce::Justification::centred);

        if (dropHoverIdx >= 0 && dropHoverIdx < numActive) {
            auto* targetPedal = activePedals[static_cast<size_t>(dropHoverIdx)];
            if (targetPedal != nullptr) {
                auto r = targetPedal->getBounds().toFloat().reduced(2.0f);
                g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.16f));
                g.fillRoundedRectangle(r, 12.0f);
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawRoundedRectangle(r, 12.0f, 2.6f);

                auto badge = juce::Rectangle<float>(r.getCentreX() - 64.0f, r.getY() + 8.0f, 128.0f, 22.0f);
                g.setColour(juce::Colour(0xff0f172a).withAlpha(0.94f));
                g.fillRoundedRectangle(badge, 5.0f);
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawRoundedRectangle(badge, 5.0f, 1.2f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                g.drawText("DROP TO SLOT #" + juce::String(dropHoverIdx + 1), badge, juce::Justification::centred);
            }
        }

        if (auto* srcPedal = activePedals[static_cast<size_t>(dragCandidateIdx)]) {
            const auto mod = srcPedal->getCategory();
            const auto ghost = juce::Rectangle<float>(
                static_cast<float>(dragCurrentPos.x) - 72.0f,
                static_cast<float>(dragCurrentPos.y) - 24.0f,
                144.0f, 48.0f
            );

            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRoundedRectangle(ghost.translated(0.0f, 4.0f), 8.0f);

            const bool dragOutsideToRemove = isPointOutsideForRemoval(dragCurrentPos);
            g.setColour(dragOutsideToRemove ? juce::Colour(0xff7f1d1d).withAlpha(0.96f)
                                            : juce::Colour(0xff172033).withAlpha(0.96f));
            g.fillRoundedRectangle(ghost, 8.0f);
            g.setColour(dragOutsideToRemove ? juce::Colour(0xffef4444) : srcPedal->getActiveAccent());
            g.drawRoundedRectangle(ghost, 8.0f, 2.0f);

            auto iconArea = juce::Rectangle<float>(ghost.getX() + 6.0f, ghost.getY() + 4.0f, 34.0f, 40.0f);
            const juce::String ghostCode = isNamPedalSlotModel(mod, srcPedal->getModelIndex())
                ? juce::String("NAM")
                : juce::String(valeton::gpCodeFor(mod));
            valeton::drawStylizedPedalIcon(g, iconArea, mod, true, srcPedal->getActiveAccent(), ghostCode);

            auto textArea = ghost.withTrimmedLeft(44.0f).reduced(4.0f, 4.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            g.drawText(srcPedal->getActiveModelName(), textArea.withHeight(18.0f), juce::Justification::centredLeft, true);

            g.setColour(dragOutsideToRemove ? juce::Colour(0xfffca5a5) : juce::Colour(0xff38bdf8));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            const juce::String destStr = dragOutsideToRemove
                ? "Release to Remove"
                : ((dropHoverIdx >= 0) ? ("Move to Slot #" + juce::String(dropHoverIdx + 1)) : "Drag Left / Right");
            g.drawText(destStr, textArea.withTop(textArea.getY() + 19.0f), juce::Justification::centredLeft, true);
        }
    }

    void resized() override {
        auto area = getLocalBounds();
        if (area.isEmpty()) return;

        const auto mode = processor.getRoutingMode();
        const bool showLaneBar = isParallelRouting(mode);
        laneTabCount = 0;
        laneAddBtnRect = {};

        if (showLaneBar) {
            auto laneBar = area.removeFromTop(kLaneBarHeight);
            layoutLaneSwitcherBar(laneBar, mode);
        }

        auto barArea = area.removeFromBottom(kSlotBarHeight);
        auto deck = area.reduced(12, 10);

        addPedalStageRect = {};
        addPedalBarRect = {};
        dualIndAmpDividerRect = {};

        const int numActive = static_cast<int>(activePedals.size());
        if (numActive <= 0) {
            pedalGroupRect = {};
            const int cw = std::clamp(area.getWidth() * 44 / 100, 280, 400);
            const int ch = std::clamp(area.getHeight() * 40 / 100, 134, 176);
            addPedalStageRect = area.withSizeKeepingCentre(cw, ch);
            return;
        }

        const bool hasAmpDivider = (mode == RoutingMode::DualIndependent &&
                                    dualIndPostSplitIndex > 0 &&
                                    dualIndPostSplitIndex < numActive);
        const int dividerExtraGap = hasAmpDivider ? 42 : 0;
        const int gap = (numActive >= 7 || deck.getWidth() < 860) ? 5 : 10;
        const int availW = std::max(100, deck.getWidth() - gap * (numActive - 1) - dividerExtraGap);

        // Authentic physical stompbox proportions (~1 : 1.80 width-to-height ratio):
        // Cap pedal height at 376 px (centered vertically on the board rails) so pedals never look like
        // tall skinny skyscrapers on large/maximized screens, and keep 1..5 pedals at natural boutique width!
        const int pedalH = std::min(deck.getHeight(), 376);
        const int pedalY = deck.getCentreY() - pedalH / 2;
        const float baseUnitW = std::clamp(static_cast<float>(pedalH) * 0.545f, 156.0f, 206.0f);

        std::array<int, kMaxPedalSlots> widths {};
        int idealSum = 0;
        for (int i = 0; i < numActive; ++i) {
            const int wt = (activePedals[static_cast<size_t>(i)] != nullptr)
                               ? activePedals[static_cast<size_t>(i)]->getWidthWeight()
                               : 20;
            const int iw = std::clamp(static_cast<int>(std::lround(baseUnitW * (static_cast<float>(wt) / 20.0f))), 134, 276);
            widths[static_cast<size_t>(i)] = iw;
            idealSum += iw;
        }

        int totalPedalsW = idealSum;
        if (idealSum > availW && idealSum > 0) {
            int usedW = 0;
            for (int i = 0; i < numActive; ++i) {
                if (i + 1 == numActive) {
                    widths[static_cast<size_t>(i)] = std::max(60, availW - usedW);
                } else {
                    const int scaled = std::max(60, availW * widths[static_cast<size_t>(i)] / idealSum);
                    widths[static_cast<size_t>(i)] = scaled;
                    usedW += scaled;
                }
            }
            totalPedalsW = availW;
        }

        const int groupW = totalPedalsW + gap * (numActive - 1) + dividerExtraGap;
        const int startX = deck.getX() + std::max(0, (deck.getWidth() - groupW) / 2);
        pedalGroupRect = juce::Rectangle<int>(startX, pedalY, groupW, pedalH);

        int x = startX;
        for (int i = 0; i < numActive; ++i) {
            if (hasAmpDivider && i == dualIndPostSplitIndex) {
                const int divY = pedalY + static_cast<int>(static_cast<float>(pedalH) * 0.44f) - 18;
                dualIndAmpDividerRect = juce::Rectangle<int>(x - 2, divY, dividerExtraGap - 2, 36);
                x += dividerExtraGap;
            }
            const int w = widths[static_cast<size_t>(i)];
            if (auto* p = activePedals[static_cast<size_t>(i)]) {
                p->setSlotPositionInfo(i, numActive);
                p->setBounds(x, pedalY, w, pedalH);
            }
            slotBarRects[static_cast<size_t>(i)] = juce::Rectangle<int>(x, barArea.getY(), w, barArea.getHeight());
            x += w + gap;
        }
    }

private:
    static constexpr int kSlotBarHeight = 28;
    static constexpr int kLaneBarHeight = 32;

    static int normalizeLaneFilter(int filter, RoutingMode mode) noexcept {
        if (!isParallelRouting(mode)) return -1;
        filter = std::clamp(filter, -1, static_cast<int>(BusLane::PostA));
        if (filter < 0) return -1;
        const auto bus = static_cast<BusLane>(filter);
        if (mode == RoutingMode::DualIndependent) {
            if (bus == BusLane::PathB || bus == BusLane::Post) return static_cast<int>(BusLane::PathB);
            return static_cast<int>(BusLane::PathA);
        }
        if (mode == RoutingMode::DualParallel) {
            if (bus == BusLane::PathC) return static_cast<int>(BusLane::PathB);
            if (bus == BusLane::PostA) return static_cast<int>(BusLane::Post);
            return filter;
        }
        if (mode == RoutingMode::TripleParallel) {
            if (bus == BusLane::PostA) return static_cast<int>(BusLane::Post);
            return filter;
        }
        return filter;
    }

    static juce::Colour laneColourForFilter(int filter) noexcept {
        switch (filter) {
            case 0:  return juce::Colour(0xff14b8a6); // PRE    - Teal
            case 1:  return juce::Colour(0xfff59e0b); // A      - Amber
            case 2:  return juce::Colour(0xff38bdf8); // B      - Sky Blue
            case 4:  return juce::Colour(0xffa855f7); // C      - Violet
            case 3:  return juce::Colour(0xff22c55e); // POST   - Emerald
            default: return juce::Colour(0xff3b82f6); // ALL    - Blue
        }
    }

    static juce::String laneDisplayName(int filter, RoutingMode mode) {
        if (mode == RoutingMode::DualIndependent) {
            if (filter == static_cast<int>(BusLane::PathA)) return "Chain A";
            if (filter == static_cast<int>(BusLane::PathB)) return "Chain B";
            return "All Lanes";
        }
        switch (filter) {
            case 0:  return "Pre";
            case 1:  return "Path A";
            case 2:  return "Path B";
            case 4:  return "Path C";
            case 3:  return "Post";
            default: return "All Lanes";
        }
    }

    bool doesBusMatchFilter(BusLane bus, int filter, RoutingMode mode) const noexcept {
        if (!isParallelRouting(mode) || filter < 0) return true;
        if (mode == RoutingMode::DualIndependent) {
            if (filter == static_cast<int>(BusLane::PathA)) {
                return (bus == BusLane::Pre || bus == BusLane::PathA || bus == BusLane::PostA || bus == BusLane::PathC);
            }
            if (filter == static_cast<int>(BusLane::PathB)) {
                return (bus == BusLane::PathB || bus == BusLane::Post);
            }
            return true;
        }
        if (mode == RoutingMode::DualParallel) {
            if (filter == static_cast<int>(BusLane::PathB)) {
                return (bus == BusLane::PathB || bus == BusLane::PathC);
            }
            if (filter == static_cast<int>(BusLane::Post)) {
                return (bus == BusLane::Post || bus == BusLane::PostA);
            }
            return static_cast<int>(bus) == filter;
        }
        if (mode == RoutingMode::TripleParallel) {
            if (filter == static_cast<int>(BusLane::Post)) {
                return (bus == BusLane::Post || bus == BusLane::PostA);
            }
            return static_cast<int>(bus) == filter;
        }
        return static_cast<int>(bus) == filter;
    }

    int countPedalsForFilter(int filter, RoutingMode mode) const {
        const auto bs = processor.getDynamicBoardState();
        int cnt = 0;
        for (int i = 0; i < bs.numNodes; ++i) {
            const auto& nd = bs.nodes[static_cast<size_t>(i)];
            if (nd.kind == ChainNodeKind::PedalSlot &&
                nd.slotId >= 0 && nd.slotId < kMaxPedalSlots &&
                bs.slots[static_cast<size_t>(nd.slotId)].occupied &&
                doesBusMatchFilter(nd.bus, filter, mode)) {
                ++cnt;
            }
        }
        return cnt;
    }

    void layoutLaneSwitcherBar(juce::Rectangle<int> bar, RoutingMode mode) {
        if (mode == RoutingMode::DualIndependent) {
            laneTabCount = 3;
            laneTabFilterIds[0] = -1;
            laneTabFilterIds[1] = static_cast<int>(BusLane::PathA);
            laneTabFilterIds[2] = static_cast<int>(BusLane::PathB);
        } else if (mode == RoutingMode::TripleParallel) {
            laneTabCount = 6;
            laneTabFilterIds[0] = -1;
            laneTabFilterIds[1] = static_cast<int>(BusLane::Pre);
            laneTabFilterIds[2] = static_cast<int>(BusLane::PathA);
            laneTabFilterIds[3] = static_cast<int>(BusLane::PathB);
            laneTabFilterIds[4] = static_cast<int>(BusLane::PathC);
            laneTabFilterIds[5] = static_cast<int>(BusLane::Post);
        } else {
            laneTabCount = 5;
            laneTabFilterIds[0] = -1;
            laneTabFilterIds[1] = static_cast<int>(BusLane::Pre);
            laneTabFilterIds[2] = static_cast<int>(BusLane::PathA);
            laneTabFilterIds[3] = static_cast<int>(BusLane::PathB);
            laneTabFilterIds[4] = static_cast<int>(BusLane::Post);
        }

        auto inner = bar.reduced(10, 4);
        const int totalSlots = countPedalsForFilter(-1, mode);
        if (totalSlots < kMaxPedalSlots) {
            laneAddBtnRect = inner.removeFromRight(92);
        } else {
            laneAddBtnRect = {};
        }

        const int tabW = (mode == RoutingMode::DualIndependent) ? 98
                       : ((mode == RoutingMode::TripleParallel) ? 79 : 86);
        const int gap = (mode == RoutingMode::TripleParallel) ? 3 : 4;
        const int maxTrackW = std::max(220, inner.getWidth() - 108);
        const int trackW = laneTabCount * tabW + (laneTabCount - 1) * gap + 6;
        laneTrackRect = bar.withSizeKeepingCentre(std::min(trackW, maxTrackW), bar.getHeight() - 6);
        auto tabCursor = laneTrackRect.reduced(3, 2);
        const int eachW = (tabCursor.getWidth() - gap * (laneTabCount - 1)) / std::max(1, laneTabCount);
        for (int i = 0; i < laneTabCount; ++i) {
            laneTabRects[static_cast<size_t>(i)] = (i + 1 == laneTabCount) ? tabCursor : tabCursor.removeFromLeft(eachW);
            if (i + 1 < laneTabCount) tabCursor.removeFromLeft(gap);
        }
    }

    void paintLaneSwitcherBar(juce::Graphics& g, juce::Rectangle<float> bar, RoutingMode mode) {
        g.setColour(juce::Colour(0xff10141c));
        g.fillRect(bar);
        g.setColour(juce::Colour(0xff202736));
        g.drawHorizontalLine(static_cast<int>(bar.getBottom()) - 1, bar.getX(), bar.getRight());

        // Left understated title (compact when track is wide)
        const float leftTitleW = std::max(0.0f, static_cast<float>(laneTrackRect.getX()) - bar.getX() - 16.0f);
        if (leftTitleW >= 68.0f) {
            g.setColour(juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
            g.drawText(leftTitleW >= 104.0f ? "STOMPBOARD VIEW" : "STOMPBOARD",
                       bar.withLeft(bar.getX() + 12.0f).withWidth(leftTitleW),
                       juce::Justification::centredLeft, true);
        }

        // Centered recessed segmented lane switcher track
        auto tr = laneTrackRect.toFloat();
        g.setColour(juce::Colour(0xff090c12));
        g.fillRoundedRectangle(tr, 5.0f);
        g.setColour(juce::Colour(0xff232b3a));
        g.drawRoundedRectangle(tr, 5.0f, 0.9f);

        for (int i = 0; i < laneTabCount; ++i) {
            const int fId = laneTabFilterIds[static_cast<size_t>(i)];
            const bool sel = (activeLaneFilter == fId);
            const bool hov = (hoveredLaneTab == i);
            const auto dotCol = laneColourForFilter(fId);
            const int cnt = countPedalsForFilter(fId, mode);
            auto r = laneTabRects[static_cast<size_t>(i)].toFloat();

            if (sel) {
                g.setColour(juce::Colour(0xff1e2636));
                g.fillRoundedRectangle(r, 3.8f);
                g.setColour(dotCol.withAlpha(0.55f));
                g.drawRoundedRectangle(r, 3.8f, 0.9f);
            } else if (hov) {
                g.setColour(juce::Colours::white.withAlpha(0.045f));
                g.fillRoundedRectangle(r, 3.8f);
            }

            const float dotR = 2.1f;
            const float dotX = r.getX() + 6.5f;
            g.setColour(sel ? dotCol : (cnt > 0 ? dotCol.withAlpha(0.55f) : juce::Colour(0xff3e4759)));
            g.fillEllipse(dotX - dotR, r.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);

            juce::String title = (fId < 0) ? "ALL" : laneDisplayName(fId, mode).toUpperCase();
            if (r.getWidth() < 64.0f) {
                if (title == "PATH A") title = "A";
                else if (title == "PATH B") title = "B";
                else if (title == "PATH C") title = "C";
            }
            const juce::String txt = title + " (" + juce::String(cnt) + ")";
            g.setColour(sel ? juce::Colour(0xfff8fafc) : juce::Colour(hov ? 0xffcbd5e1 : 0xff7c889e));
            g.setFont(juce::Font(juce::FontOptions(r.getWidth() < 74.0f ? 8.1f : 8.6f, juce::Font::bold)));
            g.drawText(txt, r.withTrimmedLeft(11.0f).withTrimmedRight(3.0f), juce::Justification::centred, true);
        }

        // Right-aligned '+ ADD' button for quick pedal insertion onto the viewed lane
        if (!laneAddBtnRect.isEmpty()) {
            auto ar = laneAddBtnRect.toFloat();
            g.setColour(hoveredLaneAddBtn ? juce::Colour(0xff1d2536) : juce::Colour(0xff141923));
            g.fillRoundedRectangle(ar, 4.5f);
            g.setColour(hoveredLaneAddBtn ? juce::Colour(0xff3b82f6).withAlpha(0.75f) : juce::Colour(0xff252e40));
            g.drawRoundedRectangle(ar, 4.5f, 0.9f);
            g.setColour(hoveredLaneAddBtn ? juce::Colours::white : juce::Colour(0xff94a3b8));
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
            g.drawText("+ ADD PEDAL", ar, juce::Justification::centred);
        }
    }

    void syncOrderFromChain(bool forceLayout) {
        const auto mode = processor.getRoutingMode();
        if (mode != lastSyncedRoutingMode) {
            lastSyncedRoutingMode = mode;
            activeLaneFilter = normalizeLaneFilter(activeLaneFilter, mode);
            forceLayout = true;
        }

        const auto bs = processor.getDynamicBoardState();
        std::vector<DynamicSlotPedal*> desired;
        int nextPostSplitIdx = -1;

        if (mode == RoutingMode::DualIndependent && activeLaneFilter >= 0) {
            std::vector<DynamicSlotPedal*> prePart, postPart;
            for (int i = 0; i < bs.numNodes; ++i) {
                const auto& nd = bs.nodes[static_cast<size_t>(i)];
                if (nd.kind == ChainNodeKind::PedalSlot &&
                    nd.slotId >= 0 && nd.slotId < kMaxPedalSlots &&
                    bs.slots[static_cast<size_t>(nd.slotId)].occupied &&
                    doesBusMatchFilter(nd.bus, activeLaneFilter, mode) &&
                    slotPool[static_cast<size_t>(nd.slotId)] != nullptr) {
                    const bool isPost = (nd.bus == BusLane::Post || nd.bus == BusLane::PostA || nd.bus == BusLane::PathC);
                    if (isPost) postPart.push_back(slotPool[static_cast<size_t>(nd.slotId)].get());
                    else        prePart.push_back(slotPool[static_cast<size_t>(nd.slotId)].get());
                }
            }
            desired.insert(desired.end(), prePart.begin(), prePart.end());
            if (!prePart.empty() && !postPart.empty()) {
                nextPostSplitIdx = static_cast<int>(prePart.size());
            }
            desired.insert(desired.end(), postPart.begin(), postPart.end());
        } else {
            for (int i = 0; i < bs.numNodes; ++i) {
                const auto& nd = bs.nodes[static_cast<size_t>(i)];
                if (nd.kind == ChainNodeKind::PedalSlot &&
                    nd.slotId >= 0 && nd.slotId < kMaxPedalSlots &&
                    bs.slots[static_cast<size_t>(nd.slotId)].occupied &&
                    doesBusMatchFilter(nd.bus, activeLaneFilter, mode) &&
                    slotPool[static_cast<size_t>(nd.slotId)] != nullptr) {
                    desired.push_back(slotPool[static_cast<size_t>(nd.slotId)].get());
                }
            }
        }

        if (forceLayout || desired != activePedals || nextPostSplitIdx != dualIndPostSplitIndex) {
            dualIndPostSplitIndex = nextPostSplitIdx;
            activePedals = desired;
            for (int i = 0; i < kMaxPedalSlots; ++i) {
                auto* p = slotPool[static_cast<size_t>(i)].get();
                if (p == nullptr) continue;
                bool visible = false;
                for (auto* ap : activePedals) {
                    if (ap == p) { visible = true; break; }
                }
                p->setVisible(visible);
                if (visible) p->syncFromProcessor(false);
            }
            resized();
            repaint();
        }
    }

    void openAddPedalPopupMenu(juce::Rectangle<int> anchor) {
        const auto mode = processor.getRoutingMode();
        const juce::String laneSuffix = (isParallelRouting(mode) && activeLaneFilter >= 0)
            ? (" -> " + laneDisplayName(activeLaneFilter, mode))
            : "";
        juce::PopupMenu m;
        m.addSectionHeader("Add New Pedal" + laneSuffix + " (" + juce::String(countPedalsForFilter(-1, mode)) + "/" + juce::String(kMaxPedalSlots) + ")");
        m.addItem(999, "NAM - Neural NAM Pedal (.nam Overdrive / Fuzz / Preamp)");
        m.addSeparator();

        const ModuleId cats[8] = {
            ModuleId::Overdrive, ModuleId::Compressor, ModuleId::Wah, ModuleId::NoiseGate,
            ModuleId::GraphicEq, ModuleId::StereoChorus, ModuleId::StereoDelay, ModuleId::Reverb
        };
        const char* catTitles[8] = {
            "DST - Overdrive / Distortion / Fuzz / NAM (34)",
            "PRE - Compressor / Boost / Pitch / Filter (20)",
            "WAH - Wah Pedals (5)",
            "NR - Noise Gates (2)",
            "EQ - Graphic Equalizers (6)",
            "MOD - Chorus / Flanger / Phaser / Tremolo (24)",
            "DLY - Delay Echoes (19)",
            "RVB - Reverbs (13)"
        };

        for (int c = 0; c < 8; ++c) {
            juce::PopupMenu sub;
            const auto models = gp200::modelsForModule(cats[c]);
            for (size_t i = 0; i < models.size(); ++i) {
                const int itemId = 1000 + static_cast<int>(cats[c]) * 100 + static_cast<int>(i);
                sub.addItem(itemId, juce::String::fromUTF8(models[i].name)
                                        + "   (" + juce::String::fromUTF8(models[i].description) + ")");
            }
            m.addSubMenu(catTitles[c], sub);
        }

        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(anchor)),
                        [this](int res) {
                            if (res == 999) {
                                processor.addPedalSlot(ModuleId::Overdrive, kNamPedalModelIndex, -1, getEffectiveInsertLane(ModuleId::Overdrive));
                                syncOrderFromChain(true);
                            } else if (res >= 1000) {
                                const auto cat = static_cast<ModuleId>((res - 1000) / 100);
                                const int modIdx = (res - 1000) % 100;
                                processor.addPedalSlot(cat, modIdx, -1, getEffectiveInsertLane(cat));
                                syncOrderFromChain(true);
                            }
                        });
    }

    void openSlotContextMenu(int orderIdx, juce::Rectangle<int> anchor) {
        if (orderIdx < 0 || orderIdx >= static_cast<int>(activePedals.size())) return;
        auto* p = activePedals[static_cast<size_t>(orderIdx)];
        if (p == nullptr) return;
        const int sId = p->getSlotId();

        juce::PopupMenu m;
        m.addSectionHeader("Slot #" + juce::String(orderIdx + 1) + ": " + p->getActiveModelName());
        if (isNamPedalSlotModel(p->getCategory(), p->getModelIndex())) {
            m.addItem(30, "Load .nam Pedal Capture from Disk...");
            m.addItem(32, "Browse TONE3000 Cloud Pedals...");
            if (p->hasNamLoaded()) {
                m.addItem(31, "Clear .nam Capture");
            }
            m.addSeparator();
        }
        m.addItem(1, p->isEngaged() ? "Bypass (Turn OFF)" : "Turn ON");
        if (orderIdx > 0) m.addItem(2, "Move Left");
        if (orderIdx + 1 < static_cast<int>(activePedals.size())) m.addItem(3, "Move Right");
        if (static_cast<int>(processor.getActivePedalSlotIds().size()) < kMaxPedalSlots) {
            m.addItem(4, "Duplicate This Pedal (" + p->getActiveModelName() + ")");
        }
        const auto mode = processor.getRoutingMode();
        if (isParallelRouting(mode)) {
            const auto curLane = processor.getPedalSlotBus(sId);
            juce::PopupMenu laneSub;
            if (mode == RoutingMode::DualIndependent) {
                laneSub.addItem(21, "Chain A — Pre-Amp (Before Amp A)", true, curLane == BusLane::Pre || curLane == BusLane::PathA);
                laneSub.addItem(25, "Chain A — Post-Amp (After Cab A)", true, curLane == BusLane::PostA || curLane == BusLane::PathC);
                laneSub.addItem(22, "Chain B — Pre-Amp (Before Amp B)", true, curLane == BusLane::PathB);
                laneSub.addItem(23, "Chain B — Post-Amp (After Cab B)", true, curLane == BusLane::Post);
            } else if (mode == RoutingMode::TripleParallel) {
                laneSub.addItem(20, "Pre (Before Split)",   true, curLane == BusLane::Pre);
                laneSub.addItem(21, "Path A (Top Lane)",    true, curLane == BusLane::PathA);
                laneSub.addItem(22, "Path B (Middle Lane)", true, curLane == BusLane::PathB);
                laneSub.addItem(24, "Path C (Bottom Lane)", true, curLane == BusLane::PathC);
                laneSub.addItem(23, "Post (After Mix)",     true, curLane == BusLane::Post || curLane == BusLane::PostA);
            } else {
                laneSub.addItem(20, "Pre (Before Split)",  true, curLane == BusLane::Pre);
                laneSub.addItem(21, "Path A (Upper Lane)", true, curLane == BusLane::PathA);
                laneSub.addItem(22, "Path B (Lower Lane)", true, curLane == BusLane::PathB || curLane == BusLane::PathC);
                laneSub.addItem(23, "Post (After Mix)",    true, curLane == BusLane::Post || curLane == BusLane::PostA);
            }
            m.addSubMenu("Move to Signal Lane", laneSub);
        }
        m.addSeparator();
        m.addItem(5, "Remove Pedal from Board");

        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(anchor)),
                        [this, sId](int res) {
                            const auto snap = processor.getPedalSlot(sId);
                            if (res == 1) {
                                processor.setPedalSlotBypass(sId, !snap.bypassed);
                            } else if (res == 2) {
                                stepStompPedal(sId, -1);
                            } else if (res == 3) {
                                stepStompPedal(sId, +1);
                            } else if (res == 4) {
                                processor.addPedalSlot(snap.category, snap.modelIndex, -1, static_cast<int>(processor.getPedalSlotBus(sId)));
                            } else if (res >= 20 && res <= 25) {
                                processor.setPedalSlotBus(sId, static_cast<BusLane>(res - 20));
                            } else if (res == 30) {
                                if (loadSlotNamCb) loadSlotNamCb(sId);
                                return;
                            } else if (res == 32) {
                                if (browseSlotNamCloudCb) browseSlotNamCloudCb(sId);
                                return;
                            } else if (res == 31) {
                                processor.clearSlotNamModel(sId);
                            } else if (res == 5) {
                                processor.removePedalSlot(sId);
                            }
                            syncOrderFromChain(true);
                        });
    }

    int findClosestOrderIndex(juce::Point<int> pt) const noexcept {
        const int n = static_cast<int>(activePedals.size());
        if (n <= 0) return 0;
        const auto grp = pedalGroupRect.isEmpty() ? getLocalBounds().reduced(12, 10) : pedalGroupRect;
        if (grp.getWidth() <= 0) return 0;
        const float relX = static_cast<float>(pt.x - grp.getX()) / static_cast<float>(grp.getWidth());
        return std::clamp(static_cast<int>(relX * static_cast<float>(n)), 0, n - 1);
    }

    int findInsertionOrderIndex(juce::Point<int> pt) const noexcept {
        const int n = static_cast<int>(activePedals.size());
        if (n <= 0) return 0;
        for (int i = 0; i < n; ++i) {
            const auto b = activePedals[static_cast<size_t>(i)]->getBounds();
            if (pt.x < b.getCentreX()) {
                return i;
            }
        }
        return n;
    }

    float virtualSlotCenterX(int orderIdx) const noexcept {
        const int n = std::max(1, static_cast<int>(activePedals.size()));
        const auto grp = pedalGroupRect.isEmpty() ? getLocalBounds().reduced(12, 10) : pedalGroupRect;
        const float zoneW = static_cast<float>(std::max(100, grp.getWidth())) / static_cast<float>(n);
        return static_cast<float>(grp.getX()) + (static_cast<float>(orderIdx) + 0.5f) * zoneW;
    }

    float virtualSlotWidth() const noexcept {
        const int n = std::max(1, static_cast<int>(activePedals.size()));
        const auto grp = pedalGroupRect.isEmpty() ? getLocalBounds().reduced(12, 10) : pedalGroupRect;
        return static_cast<float>(std::max(100, grp.getWidth())) / static_cast<float>(n);
    }

    void beginPedalDragBySlotId(int sId, juce::Point<int> boardPt) {
        for (int i = 0; i < static_cast<int>(activePedals.size()); ++i) {
            if (activePedals[static_cast<size_t>(i)] != nullptr &&
                activePedals[static_cast<size_t>(i)]->getSlotId() == sId) {
                beginSlotDrag(i, boardPt);
                return;
            }
        }
    }

    void beginSlotDrag(int orderIdx, juce::Point<int> boardPt) {
        dragCandidateIdx = orderIdx;
        draggedSlotId = (orderIdx >= 0 && orderIdx < static_cast<int>(activePedals.size()) &&
                         activePedals[static_cast<size_t>(orderIdx)] != nullptr)
                            ? activePedals[static_cast<size_t>(orderIdx)]->getSlotId()
                            : -1;
        if (draggedSlotId >= 0) {
            selectedSlotId = draggedSlotId;
            for (auto* p : activePedals) {
                if (p != nullptr) p->setSelectedOnBoard(p->getSlotId() == selectedSlotId);
            }
        }
        dragStartPos = boardPt;
        dragCurrentPos = boardPt;
        isDraggingPedal = false;
        dropHoverIdx = -1;
        repaint();
    }

    bool isPointOutsideForRemoval(juce::Point<int> boardPt) const noexcept {
        // Trigger removal when dragged into the top Trash Banner (y < 36) or outside board boundaries
        return boardPt.y < 36 || boardPt.x > getWidth() - 10 || boardPt.x < -16 || boardPt.y > getHeight() + 16;
    }

    void updatePedalDrag(juce::Point<int> boardPt) {
        if (dragCandidateIdx < 0 || draggedSlotId < 0) return;
        dragCurrentPos = boardPt;
        if (!isDraggingPedal && boardPt.getDistanceFrom(dragStartPos) > 6) {
            isDraggingPedal = true;
        }
        if (isDraggingPedal) {
            if (isPointOutsideForRemoval(boardPt)) {
                dropHoverIdx = -1;
                repaint();
                return;
            }
            const int n = static_cast<int>(activePedals.size());
            const int candidateTarget = findClosestOrderIndex(boardPt);
            dropHoverIdx = candidateTarget;

            if (candidateTarget >= 0 && candidateTarget < n && candidateTarget != dragCandidateIdx) {
                const float centerDist = std::abs(static_cast<float>(boardPt.x) - virtualSlotCenterX(candidateTarget));
                if (centerDist <= virtualSlotWidth() * 0.36f) {
                    const int targetSlotId = activePedals[static_cast<size_t>(candidateTarget)]->getSlotId();
                    processor.movePedalSlotToOtherSlot(draggedSlotId, targetSlotId);
                    dragCandidateIdx = candidateTarget;
                    syncOrderFromChain(true);
                }
            }
            repaint();
        }
    }

    void endPedalDrag(juce::Point<int> boardPt) {
        if (isDraggingPedal && dragCandidateIdx >= 0 && draggedSlotId >= 0) {
            const int sId = draggedSlotId;
            const bool removeByDragOut = isPointOutsideForRemoval(boardPt);
            const int targetIdx = findClosestOrderIndex(boardPt);
            const int targetSlotId = (targetIdx >= 0 && targetIdx < static_cast<int>(activePedals.size()) &&
                                      activePedals[static_cast<size_t>(targetIdx)] != nullptr)
                                         ? activePedals[static_cast<size_t>(targetIdx)]->getSlotId()
                                         : -1;

            isDraggingPedal = false;
            dragCandidateIdx = -1;
            draggedSlotId = -1;
            dropHoverIdx = -1;

            if (removeByDragOut) {
                processor.removePedalSlot(sId);
            } else if (targetSlotId >= 0 && targetSlotId != sId) {
                processor.movePedalSlotToOtherSlot(sId, targetSlotId);
            }
            syncOrderFromChain(true);
            repaint();
            return;
        }
        isDraggingPedal = false;
        dragCandidateIdx = -1;
        draggedSlotId = -1;
        dropHoverIdx = -1;
        repaint();
    }

    void stepStompPedal(int sId, int delta) {
        int curOrderIdx = -1;
        const int n = static_cast<int>(activePedals.size());
        for (int i = 0; i < n; ++i) {
            if (activePedals[static_cast<size_t>(i)] != nullptr &&
                activePedals[static_cast<size_t>(i)]->getSlotId() == sId) {
                curOrderIdx = i;
                break;
            }
        }
        if (curOrderIdx >= 0) {
            const int targetIdx = std::clamp(curOrderIdx + delta, 0, n - 1);
            if (targetIdx != curOrderIdx && activePedals[static_cast<size_t>(targetIdx)] != nullptr) {
                processor.movePedalSlotToOtherSlot(sId, activePedals[static_cast<size_t>(targetIdx)]->getSlotId());
                syncOrderFromChain(true);
            }
        }
    }

    PedalBoardAudioProcessor& processor;
    std::array<std::unique_ptr<DynamicSlotPedal>, kMaxPedalSlots> slotPool {};
    std::vector<DynamicSlotPedal*> activePedals;
    std::array<juce::Rectangle<int>, kMaxPedalSlots> slotBarRects {};
    juce::Rectangle<int> addPedalStageRect, addPedalBarRect;
    juce::Rectangle<int> pedalGroupRect;

    // Per-lane StompBoard switcher state (visible in Dual Split, Triple Parallel & Dual Independent)
    int activeLaneFilter = -1; // -1 = ALL, 0 = PRE, 1 = PATH A, 2 = PATH B, 4 = PATH C, 3 = POST
    RoutingMode lastSyncedRoutingMode = RoutingMode::Serial;
    int laneTabCount = 0;
    std::array<int, 6> laneTabFilterIds { -1, 0, 1, 2, 4, 3 };
    std::array<juce::Rectangle<int>, 6> laneTabRects {};
    juce::Rectangle<int> laneTrackRect, laneAddBtnRect;
    int dualIndPostSplitIndex = -1;
    juce::Rectangle<int> dualIndAmpDividerRect;
    int hoveredLaneTab = -1;
    bool hoveredLaneAddBtn = false;
    std::function<void(int)> laneChangedCb;
    std::function<void(int)> loadSlotNamCb;
    std::function<void(int)> browseSlotNamCloudCb;

    int dragCandidateIdx = -1;
    int draggedSlotId = -1;
    int selectedSlotId = 3; // Default selected pedal slot
    int dropHoverIdx = -1;
    bool isDraggingPedal = false;
    juce::Point<int> dragStartPos {};
    juce::Point<int> dragCurrentPos {};

    bool sidebarDragActive = false;
    juce::Point<int> sidebarDragPt {};
    ModuleId sidebarDragCat = ModuleId::Overdrive;
    juce::String sidebarDragModelName;
    juce::Colour sidebarDragColour;
};

} // namespace pedalboard
