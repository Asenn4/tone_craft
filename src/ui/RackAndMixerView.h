#pragma once

#include "../engine/Gp200Catalogue.h"
#include "../engine/Gp200ParamSpecs.h"
#include "../engine/ModuleId.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "HardwarePrimitives.h"
#include "MeterAndTunerBar.h"
#include "MidiBindableControl.h"
#include "ValetonTheme.h"

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace pedalboard {

// Authentic GP-200 Per-Model Studio FX Rack Card (MOD: 24, DLY: 19, RVB: 13)
// Dynamically rebuilds its exact knobs, mode selectors, and toggles to match Daftar_Effect_GP-200.xlsx!
class StudioRackUnit : public juce::Component {
public:
    StudioRackUnit(PedalBoardAudioProcessor& proc,
                   int rackSlotIndex,
                   ModuleId modId)
        : processor(proc),
          rackIdx(rackSlotIndex),
          moduleId(modId),
          accent(valeton::colourFor(modId)) {
        powerBtn.setButtonText("ACTIVE");
        powerBtn.setClickingTogglesState(true);
        powerBtn.onClick = [this] {
            if (!ignoreCallbacks) {
                processor.setRackSlotBypass(rackIdx, powerBtn.getToggleState());
                updatePowerStyle();
            }
        };
        addAndMakeVisible(powerBtn);

        const auto names = gp200::getModelNameArray(moduleId);
        for (int i = 0; i < names.size(); ++i) {
            modelBox.addItem(names[i], i + 1);
        }
        modelBox.onChange = [this] {
            if (!ignoreCallbacks && modelBox.getSelectedItemIndex() >= 0) {
                processor.setRackSlotModel(rackIdx, modelBox.getSelectedItemIndex());
                syncFromProcessor(false);
            }
        };
        addAndMakeVisible(modelBox);

        syncFromProcessor(true);
    }

    ModuleId getModuleId() const noexcept { return moduleId; }
    void setStepOrderCallback(std::function<void(ModuleId, int)> cb) { stepOrderCb = std::move(cb); }

    void syncFromProcessor(bool forceRebuild) {
        const auto snap = processor.getRackSlot(rackIdx);
        if (!snap.occupied) return;

        const bool modelChanged = (snap.modelIndex != currentModelIndex);
        currentModelIndex = snap.modelIndex;

        if (forceRebuild || modelChanged) {
            rebuildControlsForModel(currentModelIndex);
        }

        ignoreCallbacks = true;
        if (modelBox.getSelectedItemIndex() != currentModelIndex) {
            modelBox.setSelectedItemIndex(currentModelIndex, juce::dontSendNotification);
        }
        powerBtn.setToggleState(snap.bypassed, juce::dontSendNotification);

        for (auto& k : knobs) {
            k->slider.setValue(snap.params[static_cast<size_t>(k->paramIdx)], juce::dontSendNotification);
            k->slider.updateText();
        }
        for (auto& t : toggles) {
            t->btn.setToggleState(snap.params[static_cast<size_t>(t->paramIdx)] > 0.5f, juce::dontSendNotification);
        }
        for (auto& c : choices) {
            const int chosen = std::clamp(static_cast<int>(std::lround(snap.params[static_cast<size_t>(c->paramIdx)])),
                                          0, std::max(0, c->numChoices - 1));
            if (c->useButtons) {
                for (int b = 0; b < static_cast<int>(c->segButtons.size()); ++b) {
                    c->segButtons[static_cast<size_t>(b)]->setToggleState(b == chosen, juce::dontSendNotification);
                }
            } else if (c->combo != nullptr && c->combo->getSelectedItemIndex() != chosen) {
                c->combo->setSelectedItemIndex(chosen, juce::dontSendNotification);
            }
        }
        ignoreCallbacks = false;

        updatePowerStyle();
    }

    void refreshVisuals() {
        syncFromProcessor(false);
        for (auto& k : knobs) k->slider.repaint();
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f, 3.0f);
        const bool active = !powerBtn.getToggleState();
        const auto& entry = gp200::getModelEntry(moduleId, currentModelIndex);

        valeton::drawCleanChassis(g, bounds, accent, active, 7.0f, 0.0f);

        g.setColour(active ? accent : juce::Colour(0xff2c313c));
        g.fillRoundedRectangle(bounds.getX() + 4.0f, bounds.getY() + 6.0f, 4.5f, bounds.getHeight() - 12.0f, 2.0f);

        if (!iconRect.isEmpty()) {
            valeton::drawStylizedPedalIcon(g, iconRect, moduleId, active,
                                           active ? juce::Colours::transparentBlack : juce::Colour(0xff343a47));
        }

        if (!descRect.isEmpty()) {
            g.setColour(active ? accent.brighter(0.28f) : juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(8.6f, juce::Font::bold)));
            g.drawFittedText(juce::String::fromUTF8(entry.subType) + "  |  " + juce::String::fromUTF8(entry.description),
                             descRect.toNearestInt(), juce::Justification::centredLeft, 2, 0.88f);
        }

        hw::drawJewelLamp(g, jewelX, jewelY, 4.4f, active ? accent : juce::Colour(0xff3a4252), active);

        if (moduleId == ModuleId::StereoDelay && !lcdRect.isEmpty() && timeParamIdx >= 0) {
            const auto snap = processor.getRackSlot(rackIdx);
            const float rawMs = snap.params[static_cast<size_t>(timeParamIdx)];
            const bool synced = (timeSyncParamIdx >= 0 && snap.params[static_cast<size_t>(timeSyncParamIdx)] > 0.5f);
            const juce::String mainReadout = synced
                ? juce::String(tempo_sync::delayDivisionLabel(rawMs, timeMinMs, timeMaxMs))
                : (juce::String(static_cast<int>(std::lround(rawMs))) + " ms");
            const juce::String subReadout = synced
                ? (juce::String(static_cast<int>(std::lround(tempo_sync::syncedDelayMs(rawMs, timeMinMs, timeMaxMs, processor.getEffectiveBpm())))) + " ms  SYNC")
                : juce::String::fromUTF8(entry.name).toUpperCase();

            g.setColour(juce::Colour(0xff0b0f16));
            g.fillRoundedRectangle(lcdRect, 6.0f);
            g.setColour(active ? (synced ? juce::Colour(0xff38bdf8).withAlpha(0.75f) : accent.withAlpha(0.65f)) : juce::Colour(0xff262c38));
            g.drawRoundedRectangle(lcdRect, 6.0f, 1.0f);

            auto lcdInner = lcdRect.reduced(4.0f, 5.0f);
            g.setColour(active ? (synced ? juce::Colour(0xff38bdf8) : juce::Colour(0xfffbbf24)) : juce::Colour(0xff475569));
            g.setFont(juce::Font(juce::FontOptions(lcdRect.getWidth() < 92.0f ? 14.5f : 16.8f, juce::Font::bold)));
            g.drawText(mainReadout, lcdInner.removeFromTop(lcdInner.getHeight() * 0.60f), juce::Justification::centred);
            g.setColour(active ? accent.brighter(0.15f) : juce::Colour(0xff475569));
            g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            g.drawText(subReadout, lcdInner, juce::Justification::centred, true);
        }
    }

    void resized() override {
        auto face = getLocalBounds().reduced(10, 6);
        const int w = face.getWidth();
        const int h = face.getHeight();
        const bool showIcon = (w >= 500);
        const int leftW = showIcon ? std::clamp(w * 28 / 100, 174, 210)
                                   : std::clamp(w * 29 / 100, 126, 158);

        auto leftCol = face.removeFromLeft(leftW);
        const int ctrlH = (h < 92) ? 20 : 23;

        if (showIcon) {
            const float iconW = 40.0f;
            const float iconH = std::clamp(static_cast<float>(h - 28), 38.0f, 52.0f);
            iconRect = juce::Rectangle<float>(static_cast<float>(leftCol.getX() + 3),
                                              static_cast<float>(leftCol.getY() + 3),
                                              iconW, iconH);
            jewelX = iconRect.getCentreX();
            jewelY = static_cast<float>(leftCol.getBottom() - ctrlH / 2);

            auto rightOfIcon = leftCol.withTrimmedLeft(48);
            modelBox.setBounds(rightOfIcon.removeFromTop(ctrlH).withTrimmedRight(4));
            powerBtn.setBounds(rightOfIcon.removeFromBottom(ctrlH).withTrimmedRight(10));
            descRect = rightOfIcon.reduced(2, 3).toFloat();
        } else {
            iconRect = {};
            modelBox.setBounds(leftCol.removeFromTop(ctrlH).withTrimmedLeft(4).withTrimmedRight(4));
            auto botRow = leftCol.removeFromBottom(ctrlH).withTrimmedLeft(4).withTrimmedRight(4);
            jewelX = static_cast<float>(botRow.getX() + 7);
            jewelY = static_cast<float>(botRow.getCentreY());
            powerBtn.setBounds(botRow.withTrimmedLeft(18));
            descRect = leftCol.withTrimmedLeft(4).reduced(0, 2).toFloat();
        }

        // Delay LCD only when there is generous room & not crowded by >6 knobs (e.g. Tape Delay S)
        const bool showLcd = (moduleId == ModuleId::StereoDelay && timeParamIdx >= 0 && w >= 590 && knobs.size() <= 5);
        if (showLcd) {
            const int lcdW = std::clamp(w * 14 / 100, 84, 102);
            const int lcdH = std::clamp(h * 56 / 100, 48, 60);
            auto lcdCol = face.removeFromLeft(lcdW + 8);
            lcdRect = lcdCol.withSizeKeepingCentre(lcdW, lcdH).toFloat();
        } else {
            lcdRect = {};
        }

        // Special full-width showcase when the model ONLY has button choices (e.g., C-Chorus 4-Mode Dimension C!)
        if (knobs.empty() && !choices.empty() && choices[0]->useButtons) {
            auto& ch = *choices[0];
            auto centerBox = face.reduced(16, std::max(8, (h - 56) / 2));
            ch.label.setBounds(centerBox.removeFromTop(16));
            centerBox.removeFromTop(4);
            const int n = static_cast<int>(ch.segButtons.size());
            const int gap = 8;
            const int bw = std::min(115, (centerBox.getWidth() - gap * (n - 1)) / std::max(1, n));
            const int totalW = bw * n + gap * (n - 1);
            auto row = juce::Rectangle<int>(centerBox.getCentreX() - totalW / 2, centerBox.getY(), totalW, std::min(32, centerBox.getHeight()));
            for (int i = 0; i < n; ++i) {
                ch.segButtons[static_cast<size_t>(i)]->setBounds(row.removeFromLeft(bw));
                if (i + 1 < n) row.removeFromLeft(gap);
            }
            return;
        }

        // Right-side column for Choices & Toggles (SYNC, TRAIL, MODE, COLOR, CURVE)
        const int auxCount = static_cast<int>(toggles.size() + choices.size());
        if (auxCount > 0) {
            const int auxW = std::clamp(w * 15 / 100, 72, 114);
            auto auxCol = face.removeFromRight(auxW).reduced(3, 2);
            const int itemH = std::min(24, std::max(17, (auxCol.getHeight() - (auxCount - 1) * 4) / std::max(1, auxCount)));
            const int totalAuxH = auxCount * itemH + (auxCount - 1) * 4;
            int y = auxCol.getCentreY() - totalAuxH / 2;

            for (auto& c : choices) {
                if (c->useButtons) {
                    const int n = static_cast<int>(c->segButtons.size());
                    auto row = juce::Rectangle<int>(auxCol.getX(), y, auxCol.getWidth(), itemH);
                    const int bw = row.getWidth() / std::max(1, n);
                    for (int b = 0; b < n; ++b) {
                        c->segButtons[static_cast<size_t>(b)]->setBounds(row.removeFromLeft(bw).reduced(1, 0));
                    }
                } else if (c->combo != nullptr) {
                    c->label.setBounds(0, 0, 0, 0);
                    c->combo->setBounds(auxCol.getX(), y, auxCol.getWidth(), itemH);
                }
                y += itemH + 4;
            }
            for (auto& t : toggles) {
                t->btn.setBounds(auxCol.getX(), y, auxCol.getWidth(), itemH);
                y += itemH + 4;
            }
        }

        // Knobs area: 1 row for <= 5 knobs, crisp 2-row grid for 6..10 knobs (e.g. Tape Delay S, M-Chorus, Studio Reverb)
        const int count = static_cast<int>(knobs.size());
        if (count <= 0) return;

        const int rows = (count <= 5) ? 1 : 2;
        const int cols = (count + rows - 1) / rows;
        const int cellW = std::max(38, face.getWidth() / cols);
        const int cellH = std::max(34, face.getHeight() / rows);
        const int lblH  = (rows == 2 || h < 92) ? 11 : 13;
        const bool tinyCell = (cellH < 52 || cellW < 44);
        const int tbW = std::clamp(cellW - 4, 34, 58);

        for (int i = 0; i < count; ++i) {
            const int r = i / cols;
            const int c = i % cols;
            auto cell = juce::Rectangle<int>(face.getX() + c * cellW, face.getY() + r * cellH, cellW, cellH).reduced(2, 1);
            knobs[static_cast<size_t>(i)]->label.setBounds(cell.removeFromTop(lblH));
            auto& sl = knobs[static_cast<size_t>(i)]->slider;
            if (tinyCell) {
                sl.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                sl.setPopupDisplayEnabled(true, false, this);
            } else {
                sl.setTextBoxStyle(juce::Slider::TextBoxBelow, false, tbW, (cellH < 72) ? 11 : 13);
                sl.setPopupDisplayEnabled(false, false, nullptr);
            }
            sl.setBounds(cell);
        }
    }

private:
    struct KnobItem {
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

    void rebuildControlsForModel(int modelIdx) {
        knobs.clear();
        toggles.clear();
        choices.clear();
        timeParamIdx = -1;
        timeSyncParamIdx = -1;
        timeMinMs = 30.0f;
        timeMaxMs = 1500.0f;

        const auto specs = getSlotParamSpecs(moduleId, modelIdx);
        const auto& entry = gp200::getModelEntry(moduleId, modelIdx);
        accent = juce::Colour(entry.chassisArgb);

        for (size_t i = 0; i < specs.size(); ++i) {
            const auto& sp = specs[i];
            const int pIdx = static_cast<int>(i);

            if (juce::String(sp.label).equalsIgnoreCase("TIME")) {
                timeParamIdx = pIdx;
                timeMinMs = sp.minVal;
                timeMaxMs = sp.maxVal;
                bool dummyIsDly = false;
                timeSyncParamIdx = findSyncToggleIndexForParam(specs, pIdx, dummyIsDly);
            }

            if (sp.isToggle()) {
                auto t = std::make_unique<ToggleItem>();
                t->paramIdx = pIdx;
                t->btn.setButtonText(sp.label);
                t->btn.setClickingTogglesState(true);
                auto* rawBtn = &t->btn;
                t->btn.onClick = [this, pIdx, rawBtn] {
                    if (!ignoreCallbacks) {
                        processor.setRackSlotParam(rackIdx, pIdx, rawBtn->getToggleState() ? 1.0f : 0.0f);
                        for (auto& k : knobs) k->slider.updateText();
                        repaint();
                    }
                };
                addAndMakeVisible(t->btn);
                toggles.push_back(std::move(t));
            } else if (sp.isChoice()) {
                auto c = std::make_unique<ChoiceItem>();
                c->paramIdx = pIdx;
                c->numChoices = sp.numChoices;
                c->useButtons = (sp.numChoices <= 4 && specs.size() <= 3);
                c->label.setText(sp.label, juce::dontSendNotification);
                c->label.setJustificationType(juce::Justification::centred);
                c->label.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
                c->label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
                addAndMakeVisible(c->label);

                if (c->useButtons) {
                    for (int b = 0; b < sp.numChoices; ++b) {
                        auto sb = std::make_unique<juce::TextButton>(sp.choices[static_cast<size_t>(b)]);
                        sb->setClickingTogglesState(false);
                        sb->setColour(juce::TextButton::buttonOnColourId, accent.darker(0.15f));
                        sb->onClick = [this, pIdx, b] {
                            if (!ignoreCallbacks) {
                                processor.setRackSlotParam(rackIdx, pIdx, static_cast<float>(b));
                                syncFromProcessor(false);
                            }
                        };
                        addAndMakeVisible(*sb);
                        c->segButtons.push_back(std::move(sb));
                    }
                } else {
                    c->combo = std::make_unique<juce::ComboBox>();
                    for (int b = 0; b < sp.numChoices; ++b) {
                        c->combo->addItem(juce::String(sp.label) + ": " + sp.choices[static_cast<size_t>(b)], b + 1);
                    }
                    auto* rawCombo = c->combo.get();
                    c->combo->onChange = [this, pIdx, rawCombo] {
                        if (!ignoreCallbacks && rawCombo->getSelectedItemIndex() >= 0) {
                            processor.setRackSlotParam(rackIdx, pIdx, static_cast<float>(rawCombo->getSelectedItemIndex()));
                        }
                    };
                    addAndMakeVisible(*c->combo);
                }
                choices.push_back(std::move(c));
            } else {
                auto item = std::make_unique<KnobItem>();
                item->paramIdx = pIdx;
                item->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                item->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 13);
                item->slider.setRange(sp.minVal, sp.maxVal, sp.step);
                item->slider.setValue(sp.defaultVal, juce::dontSendNotification);
                item->slider.setDoubleClickReturnValue(true, sp.defaultVal);
                item->slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
                item->slider.getProperties().set("knobStyle", "chrome_knurled");
                item->slider.getProperties().set("showTicks", true);
                if (sp.suffix != nullptr && sp.suffix[0] != '\0') {
                    item->slider.setTextValueSuffix(juce::String(" ") + sp.suffix);
                }
                bool isDelaySync = false;
                const int syncIdx = findSyncToggleIndexForParam(specs, pIdx, isDelaySync);
                if (syncIdx >= 0) {
                    const float minV = sp.minVal;
                    const float maxV = sp.maxVal;
                    const juce::String sfx = (sp.suffix != nullptr && sp.suffix[0] != '\0')
                                                 ? (juce::String(" ") + sp.suffix)
                                                 : juce::String();
                    item->slider.textFromValueFunction = [this, syncIdx, isDelaySync, minV, maxV, sfx](double v) -> juce::String {
                        const auto cur = processor.getRackSlot(rackIdx);
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
                auto* rawSl = &item->slider;
                item->slider.onValueChange = [this, pIdx, rawSl] {
                    if (!ignoreCallbacks) {
                        processor.setRackSlotParam(rackIdx, pIdx, static_cast<float>(rawSl->getValue()));
                        if (pIdx == timeParamIdx) repaint();
                    }
                };
                addAndMakeVisible(item->slider);

                item->label.setText(sp.label, juce::dontSendNotification);
                item->label.setJustificationType(juce::Justification::centred);
                item->label.setFont(juce::Font(juce::FontOptions(9.4f, juce::Font::bold)));
                item->label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
                addAndMakeVisible(item->label);

                knobs.push_back(std::move(item));
            }
        }

        resized();
    }

    void updatePowerStyle() {
        const bool bypassed = powerBtn.getToggleState();
        const bool active = !bypassed;
        powerBtn.setButtonText(bypassed ? "BYPASS" : "ACTIVE");
        powerBtn.getProperties().set("bypassed", bypassed);
        powerBtn.setColour(juce::TextButton::buttonColourId, accent.withMultipliedBrightness(0.55f));
        powerBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff232834));
        powerBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff8c96a6));

        for (auto& k : knobs) {
            k->slider.getProperties().set("bypassed", bypassed);
            k->slider.setColour(juce::Slider::textBoxTextColourId,
                                active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
            k->slider.setAlpha(active ? 1.0f : 0.72f);
            k->label.setAlpha(active ? 1.0f : 0.55f);
            k->slider.repaint();
        }
        for (auto& t : toggles) {
            t->btn.getProperties().set("bypassed", bypassed);
            t->btn.setAlpha(active ? 1.0f : 0.65f);
            t->btn.setColour(juce::TextButton::buttonOnColourId,
                             active ? accent.darker(0.18f) : juce::Colour(0xff2c3240));
            t->btn.setColour(juce::TextButton::textColourOnId,
                             active ? juce::Colours::white : juce::Colour(0xff64748b));
            t->btn.setColour(juce::TextButton::textColourOffId,
                             active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff5a6373));
            t->btn.repaint();
        }
        for (auto& c : choices) {
            c->label.setAlpha(active ? 1.0f : 0.55f);
            if (c->combo != nullptr) c->combo->setAlpha(active ? 1.0f : 0.60f);
            for (auto& sb : c->segButtons) {
                sb->getProperties().set("bypassed", bypassed);
                sb->setAlpha(active ? 1.0f : 0.65f);
                sb->setColour(juce::TextButton::buttonOnColourId,
                              active ? accent.darker(0.15f) : juce::Colour(0xff2c3240));
                sb->setColour(juce::TextButton::textColourOnId,
                              active ? juce::Colours::white : juce::Colour(0xff64748b));
                sb->setColour(juce::TextButton::textColourOffId,
                              active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff5a6373));
                sb->repaint();
            }
        }
        repaint();
    }

    PedalBoardAudioProcessor& processor;
    int rackIdx = 0;
    ModuleId moduleId;
    int currentModelIndex = -1;
    int timeParamIdx = -1;
    int timeSyncParamIdx = -1;
    float timeMinMs = 30.0f;
    float timeMaxMs = 1500.0f;
    bool ignoreCallbacks = false;
    juce::Colour accent;
    juce::ComboBox modelBox;
    MidiBindableButton powerBtn;
    std::vector<std::unique_ptr<KnobItem>> knobs;
    std::vector<std::unique_ptr<ToggleItem>> toggles;
    std::vector<std::unique_ptr<ChoiceItem>> choices;
    juce::Rectangle<float> iconRect, descRect, lcdRect;
    float jewelX = 24.0f, jewelY = 70.0f;
    std::function<void(ModuleId, int)> stepOrderCb;
};

class RackAndMixerView : public juce::Component {
public:
    explicit RackAndMixerView(PedalBoardAudioProcessor& proc)
        : processor(proc),
          masterMeter("OUT") {
        chorusUnit = std::make_unique<StudioRackUnit>(processor, 0, ModuleId::StereoChorus);
        delayUnit  = std::make_unique<StudioRackUnit>(processor, 1, ModuleId::StereoDelay);
        reverbUnit = std::make_unique<StudioRackUnit>(processor, 2, ModuleId::Reverb);

        allRackUnits = { chorusUnit.get(), delayUnit.get(), reverbUnit.get() };
        for (auto* u : allRackUnits) {
            u->setStepOrderCallback([this](ModuleId mod, int delta) { stepRackUnit(mod, delta); });
            addAndMakeVisible(*u);
        }

        configurePan(panA, panALabel, "PAN A", params::kPathAPan, juce::Colour(0xfff59e0b), "", attPanA);
        configurePan(panB, panBLabel, "PAN B", params::kPathBPan, juce::Colour(0xff38bdf8), "", attPanB);
        configurePan(panC, panCLabel, "PAN C", params::kPathCPan, juce::Colour(0xffa855f7), "", attPanC);
        configurePan(inTrim, inTrimLabel, "IN TRIM", params::kInputGain, juce::Colour(0xff22c55e), " dB", attInTrim);

        configureConsoleFader(faderA, params::kPathALevel, juce::Colour(0xfff59e0b), 0.0, attFaderA);
        configureConsoleFader(faderB, params::kPathBLevel, juce::Colour(0xff38bdf8), -4.0, attFaderB);
        configureConsoleFader(faderC, params::kPathCLevel, juce::Colour(0xffa855f7), -6.0, attFaderC);
        configureConsoleFader(faderMaster, params::kOutputGain, juce::Colour(0xff22c55e), 0.0, attFaderMaster);

        configureToggle(muteA, "MUTE A", params::kPathAMute, juce::Colour(0xffdc2626), attMuteA);
        configureToggle(muteB, "MUTE B", params::kPathBMute, juce::Colour(0xffdc2626), attMuteB);
        configureToggle(phaseB, "PHASE", params::kPathBPhase, juce::Colour(0xff8b5cf6), attPhaseB);
        configureToggle(muteC, "MUTE C", params::kPathCMute, juce::Colour(0xffdc2626), attMuteC);
        configureToggle(phaseC, "PHASE", params::kPathCPhase, juce::Colour(0xff8b5cf6), attPhaseC);

        addAndMakeVisible(masterMeter);
        syncOrderFromChain(true);
        updateMixerStripDimming();
    }

    void updateLive() {
        syncOrderFromChain(false);
        const auto curMode = processor.getRoutingMode();
        if (curMode != lastRoutingMode) {
            lastRoutingMode = curMode;
            resized();
            repaint();
        }
        updateMixerStripDimming();
        const auto& chain = processor.getSignalChain();
        masterMeter.setLevels(chain.getOutputPeakL(), chain.getOutputPeakR());
        delayUnit->repaint();
    }

    void refreshAll() {
        lastRoutingMode = processor.getRoutingMode();
        syncOrderFromChain(true);
        updateMixerStripDimming();
        chorusUnit->refreshVisuals();
        delayUnit->refreshVisuals();
        reverbUnit->refreshVisuals();
        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        valeton::drawCleanStageBackdrop(g, bounds);

        g.setColour(juce::Colour(0xff13161e));
        g.fillRoundedRectangle(rackFrameRect.toFloat(), 8.0f);
        g.setColour(juce::Colour(0xff282e3c));
        g.drawRoundedRectangle(rackFrameRect.toFloat(), 8.0f, 1.0f);

        auto mixR = mixerFrameRect.toFloat();
        const auto mode = processor.getRoutingMode();
        const bool isSerial  = (mode == RoutingMode::Serial);
        const bool isTriple  = (mode == RoutingMode::TripleParallel);
        const bool isDualInd = (mode == RoutingMode::DualIndependent);
        const auto headerAccent = isTriple ? juce::Colour(0xffa855f7)
                                           : (isDualInd ? juce::Colour(0xff22c55e)
                                                        : (isSerial ? juce::Colour(0xfff59e0b) : juce::Colour(0xff38bdf8)));
        valeton::drawCleanChassis(g, mixR, headerAccent, true, 8.0f, 0.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
        g.drawText("CONSOLE MIXER", mixR.reduced(14.0f, 9.0f).withHeight(18.0f), juce::Justification::centredLeft);

        g.setColour(headerAccent);
        g.setFont(juce::Font(juce::FontOptions(mixR.getWidth() < 260.0f ? 8.0f : 8.8f, juce::Font::bold)));
        const juce::String subTxt = isSerial
            ? "SERIAL  |  MAIN + MASTER"
            : (juce::String(routingModeDisplayName(mode)) + " ACTIVE");
        g.drawText(subTxt,
                   mixR.reduced(12.0f, 8.0f).withTop(mixR.getY() + 25.0f).withHeight(14.0f),
                   juce::Justification::centredLeft, true);

        const int numStrips = isSerial ? 2 : (isTriple ? 4 : 3);

        const char* stripTitles2[2] = { "MAIN", "MASTER" };
        const juce::Colour stripCols2[2] = { juce::Colour(0xfff59e0b), juce::Colour(0xff22c55e) };

        const char* stripTitles3[3] = {
            isDualInd ? "CHAIN A" : "PATH A",
            isDualInd ? "CHAIN B" : "PATH B",
            "MASTER"
        };
        const juce::Colour stripCols3[3] = { juce::Colour(0xfff59e0b), juce::Colour(0xff38bdf8), juce::Colour(0xff22c55e) };

        const char* stripTitles4[4] = { "PATH A", "PATH B", "PATH C", "MASTER" };
        const juce::Colour stripCols4[4] = { juce::Colour(0xfff59e0b), juce::Colour(0xff38bdf8), juce::Colour(0xffa855f7), juce::Colour(0xff22c55e) };

        for (int i = 0; i < numStrips; ++i) {
            const auto sr = stripRects[static_cast<size_t>(i)].toFloat();
            const auto col = isSerial ? stripCols2[i] : (isTriple ? stripCols4[i] : stripCols3[i]);
            const char* title = isSerial ? stripTitles2[i] : (isTriple ? stripTitles4[i] : stripTitles3[i]);
            g.setColour(juce::Colour(0xff10131a));
            g.fillRoundedRectangle(sr, 5.0f);
            g.setColour(col.withAlpha(0.55f));
            g.drawRoundedRectangle(sr, 5.0f, 1.0f);

            g.setColour(col);
            g.setFont(juce::Font(juce::FontOptions(sr.getWidth() < 76.0f ? 8.4f : 9.8f, juce::Font::bold)));
            g.drawText(title, sr.withTop(sr.getBottom() - 18.0f), juce::Justification::centred);
        }
    }

    void resized() override {
        const bool compactW = (getWidth() < 900);
        const bool compactH = (getHeight() < 390);
        const auto mode     = processor.getRoutingMode();
        const bool isSerial = (mode == RoutingMode::Serial);
        const bool isTriple = (mode == RoutingMode::TripleParallel);
        auto area = getLocalBounds().reduced(compactW ? 10 : 14, compactH ? 8 : 12);

        const int mixerW = isSerial
            ? std::clamp(area.getWidth() * 24 / 100, 196, 290)
            : (isTriple ? std::clamp(area.getWidth() * 38 / 100, 304, 464)
                        : std::clamp(area.getWidth() * 32 / 100, 250, 410));
        mixerFrameRect = area.removeFromRight(mixerW);
        area.removeFromRight(compactW ? 8 : 10);
        rackFrameRect = area;

        auto rackInner = rackFrameRect.reduced(compactW ? 6 : 8, compactH ? 6 : 8);
        const int unitGap = compactH ? 4 : 6;
        const int unitH = (rackInner.getHeight() - unitGap * 2) / 3;
        for (size_t i = 0; i < 3; ++i) {
            if (orderedUnits[i] != nullptr) {
                orderedUnits[i]->setBounds((i < 2) ? rackInner.removeFromTop(unitH) : rackInner);
            }
            if (i < 2) rackInner.removeFromTop(unitGap);
        }

        auto mixInner = mixerFrameRect.reduced(compactW ? 8 : 10, compactH ? 8 : 10);
        mixInner.removeFromTop(34);
        const int numStrips = isSerial ? 2 : (isTriple ? 4 : 3);
        const int stripGap = (isTriple || compactW) ? 4 : 7;
        const int stripW = (mixInner.getWidth() - stripGap * (numStrips - 1)) / numStrips;

        for (int i = 0; i < numStrips; ++i) {
            stripRects[static_cast<size_t>(i)] = (i + 1 == numStrips) ? mixInner : mixInner.removeFromLeft(stripW);
            if (i + 1 < numStrips) mixInner.removeFromLeft(stripGap);
        }

        auto layoutStrip = [compactH](juce::Rectangle<int> r, juce::Label& topLbl, juce::Slider& topKnob, juce::Slider& fader,
                                      juce::Component* btn1, juce::Component* btn2, juce::Component* meter) {
            auto in = r.reduced(3, 5);
            in.removeFromBottom(16);
            const int lblH  = 12;
            const int knobH = compactH ? 42 : 50;
            const int btnH  = compactH ? 18 : 21;
            const int gapY  = compactH ? 3 : 4;
            const int tbW   = std::clamp(in.getWidth() - 4, 34, 54);

            topLbl.setBounds(in.removeFromTop(lblH));
            topKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, tbW, 12);
            topKnob.setBounds(in.removeFromTop(knobH).reduced(2, 0));
            in.removeFromTop(gapY);

            auto slot1 = in.removeFromTop(btnH);
            if (btn1 != nullptr) btn1->setBounds(slot1.reduced(1, 0));
            in.removeFromTop(gapY);

            auto slot2 = in.removeFromTop(btnH);
            if (btn2 != nullptr) btn2->setBounds(slot2.reduced(1, 0));
            in.removeFromTop(gapY);

            if (meter != nullptr) {
                const int meterW = (in.getWidth() < 70) ? 18 : 24;
                meter->setBounds(in.removeFromRight(meterW).reduced(0, 4));
                in.removeFromRight(2);
            }
            fader.setTextBoxStyle(juce::Slider::TextBoxBelow, false, std::clamp(in.getWidth(), 32, 54), compactH ? 12 : 14);
            fader.setBounds(in);
        };

        const bool showB = !isSerial;
        panALabel.setText(isSerial ? "PAN" : "PAN A", juce::dontSendNotification);
        muteA.setButtonText(isSerial ? "MUTE" : "MUTE A");

        panBLabel.setVisible(showB);
        panB.setVisible(showB);
        faderB.setVisible(showB);
        muteB.setVisible(showB);
        phaseB.setVisible(showB);

        panCLabel.setVisible(isTriple);
        panC.setVisible(isTriple);
        faderC.setVisible(isTriple);
        muteC.setVisible(isTriple);
        phaseC.setVisible(isTriple);

        layoutStrip(stripRects[0], panALabel, panA, faderA, &muteA, nullptr, nullptr);
        if (isSerial) {
            layoutStrip(stripRects[1], inTrimLabel, inTrim, faderMaster, nullptr, nullptr, &masterMeter);
        } else if (isTriple) {
            layoutStrip(stripRects[1], panBLabel, panB, faderB, &muteB, &phaseB, nullptr);
            layoutStrip(stripRects[2], panCLabel, panC, faderC, &muteC, &phaseC, nullptr);
            layoutStrip(stripRects[3], inTrimLabel, inTrim, faderMaster, nullptr, nullptr, &masterMeter);
        } else {
            layoutStrip(stripRects[1], panBLabel, panB, faderB, &muteB, &phaseB, nullptr);
            layoutStrip(stripRects[2], inTrimLabel, inTrim, faderMaster, nullptr, nullptr, &masterMeter);
        }
    }

private:
    void updateMixerStripDimming() {
        panBLabel.setAlpha(1.0f);
        panB.setAlpha(1.0f);
        muteB.setAlpha(1.0f);
        phaseB.setAlpha(1.0f);
        faderB.setAlpha(1.0f);
        panCLabel.setAlpha(1.0f);
        panC.setAlpha(1.0f);
        muteC.setAlpha(1.0f);
        phaseC.setAlpha(1.0f);
        faderC.setAlpha(1.0f);
    }

    void configurePan(MidiBindableSlider& s, juce::Label& l, const char* title, const char* pid,
                      juce::Colour accent, const char* suffix,
                      std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 12);
        if (suffix != nullptr && suffix[0] != '\0') {
            s.setTextValueSuffix(suffix);
        }
        s.setColour(juce::Slider::rotarySliderFillColourId, accent);
        s.getProperties().set("knobStyle", "chrome_knurled");
        s.getProperties().set("showTicks", true);
        s.setDoubleClickReturnValue(true, 0.0);
        s.configureMidiLearn(&processor.getMidiLearnManager(), pid);
        addAndMakeVisible(s);

        l.setText(title, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
        l.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(l);

        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), pid, s);
    }

    void configureConsoleFader(MidiBindableSlider& s, const char* pid, juce::Colour accent, double defVal,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 14);
        s.setTextValueSuffix(" dB");
        s.setColour(juce::Slider::trackColourId, accent);
        s.getProperties().set("faderStyle", "console");
        s.setDoubleClickReturnValue(true, defVal);
        s.configureMidiLearn(&processor.getMidiLearnManager(), pid);
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), pid, s);
    }

    void configureToggle(MidiBindableButton& b, const char* text, const char* pid, juce::Colour onCol,
                         std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att) {
        b.setButtonText(text);
        b.setClickingTogglesState(true);
        b.setColour(juce::TextButton::buttonOnColourId, onCol);
        b.configureMidiLearn(&processor.getMidiLearnManager(), pid);
        addAndMakeVisible(b);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.getAPVTS(), pid, b);
    }

    StudioRackUnit* unitForModule(ModuleId id) const noexcept {
        switch (id) {
            case ModuleId::StereoChorus: return chorusUnit.get();
            case ModuleId::StereoDelay:  return delayUnit.get();
            case ModuleId::Reverb:       return reverbUnit.get();
            default:                     return nullptr;
        }
    }

    void syncOrderFromChain(bool forceLayout) {
        const auto chainOrder = processor.getChainOrder();
        std::array<StudioRackUnit*, 3> desired {};
        size_t idx = 0;
        for (const auto mod : chainOrder) {
            if (auto* u = unitForModule(mod)) {
                if (idx < 3) desired[idx++] = u;
            }
        }
        for (auto* u : allRackUnits) {
            bool present = false;
            for (size_t j = 0; j < idx; ++j) {
                if (desired[j] == u) { present = true; break; }
            }
            if (!present && idx < 3) desired[idx++] = u;
        }
        if (forceLayout || desired != orderedUnits) {
            orderedUnits = desired;
            resized();
            repaint();
        }
    }

    void stepRackUnit(ModuleId mod, int delta) {
        auto order = processor.getChainOrder();
        std::vector<int> rackChainPositions;
        std::vector<ModuleId> rackSequence;
        int fromRackSlot = -1;

        for (int i = 0; i < kNumModules; ++i) {
            if (unitForModule(order[static_cast<size_t>(i)]) != nullptr) {
                if (order[static_cast<size_t>(i)] == mod) {
                    fromRackSlot = static_cast<int>(rackSequence.size());
                }
                rackChainPositions.push_back(i);
                rackSequence.push_back(order[static_cast<size_t>(i)]);
            }
        }

        if (fromRackSlot >= 0 && static_cast<int>(rackSequence.size()) == 3) {
            const int targetRackSlot = std::clamp(fromRackSlot + delta, 0, 2);
            if (targetRackSlot == fromRackSlot) return;
            rackSequence.erase(rackSequence.begin() + fromRackSlot);
            rackSequence.insert(rackSequence.begin() + targetRackSlot, mod);

            for (size_t i = 0; i < 3; ++i) {
                order[static_cast<size_t>(rackChainPositions[i])] = rackSequence[i];
            }
            processor.setChainOrder(order);
            syncOrderFromChain(true);
        }
    }

    PedalBoardAudioProcessor& processor;
    RoutingMode lastRoutingMode = RoutingMode::Serial;
    juce::Rectangle<int> rackFrameRect, mixerFrameRect;
    std::array<juce::Rectangle<int>, 4> stripRects;

    std::unique_ptr<StudioRackUnit> chorusUnit, delayUnit, reverbUnit;
    std::array<StudioRackUnit*, 3> allRackUnits {};
    std::array<StudioRackUnit*, 3> orderedUnits {};

    juce::Label panALabel, panBLabel, panCLabel, inTrimLabel;
    MidiBindableSlider panA, panB, panC, inTrim;
    MidiBindableSlider faderA, faderB, faderC, faderMaster;
    MidiBindableButton muteA, muteB, phaseB, muteC, phaseC;
    LevelMeterComponent masterMeter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attPanA, attPanB, attPanC, attInTrim;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attFaderA, attFaderB, attFaderC, attFaderMaster;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attMuteA, attMuteB, attPhaseB, attMuteC, attPhaseC;
};

} // namespace pedalboard
