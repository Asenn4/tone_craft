#pragma once

#include "../engine/Gp200Catalogue.h"
#include "../engine/Gp200ParamSpecs.h"
#include "../engine/ModelPackRegistry.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "HardwarePrimitives.h"
#include "MidiBindableControl.h"
#include "SkinManager.h"
#include "ValetonTheme.h"

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace pedalboard {

class ChromePowerToggle : public MidiBindableButton {
public:
    ChromePowerToggle() { setClickingTogglesState(true); }

    void paintButton(juce::Graphics& g, bool highlighted, bool) override {
        const auto bounds = getLocalBounds().toFloat();
        const bool isStandby = getToggleState();
        const auto& strip = SkinManager::instance().getImage(SkinAsset::ToggleChrome2);

        auto swArea = bounds.withTrimmedBottom(12.0f);
        const float sz = std::min(swArea.getWidth(), swArea.getHeight());
        auto dest = juce::Rectangle<float>(swArea.getCentreX() - sz * 0.5f,
                                           swArea.getCentreY() - sz * 0.5f,
                                           sz, sz);
        if (strip.isValid()) {
            SkinManager::drawSwitchFrame(g, strip, isStandby ? 1 : 0, 2, dest, highlighted ? 1.0f : 0.94f);
        } else {
            g.setColour(isStandby ? juce::Colour(0xff475569) : juce::Colour(0xffcbd5e1));
            g.fillEllipse(dest.reduced(6.0f));
        }

        g.setColour(isStandby ? juce::Colour(0xff64748b) : juce::Colour(0xfffbbf24));
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText(isStandby ? "STANDBY" : "POWER", bounds.withTop(bounds.getBottom() - 12.0f), juce::Justification::centred);
    }
};

// Hero View 2: All 71 GP-200 Amp Models (+ Neural NAM = 72 total) from Daftar_Effect_GP-200.xlsx
// Dynamically displays the EXACT authentic knobs & switches (Bright, Char Cool/Hot, Ampeg SVT 5-way Midrange,
// Dual Gain 1/2, Tone Cut, Input, Edge, AC Pre Parametric EQ) for each Amp model!
class AmpHeadView : public juce::Component {
public:
    AmpHeadView(PedalBoardAudioProcessor& proc,
                std::function<void(int)> onLoadNam,
                std::function<void(int)> onClearNam)
        : processor(proc),
          loadNamCb(std::move(onLoadNam)),
          clearNamCb(std::move(onClearNam)) {
        rigTabA.setButtonText("AMP A");
        rigTabA.onClick = [this] { selectRig(0); };
        addAndMakeVisible(rigTabA);

        rigTabB.setButtonText("AMP B");
        rigTabB.onClick = [this] { selectRig(1); };
        addAndMakeVisible(rigTabB);

        rigTabC.setButtonText("AMP C");
        rigTabC.onClick = [this] { selectRig(2); };
        addAndMakeVisible(rigTabC);

        modeAmpBtn.setButtonText("AMP MODELS");
        modeAmpBtn.onClick = [this] { setNamMode(false); };
        addAndMakeVisible(modeAmpBtn);

        modeNamBtn.setButtonText("NAM CAPTURE");
        modeNamBtn.onClick = [this] { setNamMode(true); };
        addAndMakeVisible(modeNamBtn);

        // Populate ONLY the 71 built-in DSP Amp models (0..70); Neural NAM (71) has its own dedicated mode!
        const auto ampNames = gp200::getModelNameArray(ModuleId::AmpHead);
        const int dspCount = std::min(71, ampNames.size());
        for (int i = 0; i < dspCount; ++i) {
            ampModelBox.addItem(ampNames[i], i + 1);
        }
        ampModelBox.onChange = [this] {
            if (!ignoreModelBoxCb && ampModelBox.getSelectedItemIndex() >= 0) {
                const int chosen = std::clamp(ampModelBox.getSelectedItemIndex(), 0, 70);
                lastDspModel[static_cast<size_t>(activeRig)] = chosen;
                setAmpModelParam(chosen);
                updateVisualTheme();
                repaint();
            }
        };
        addAndMakeVisible(ampModelBox);

        const char* chNames[3] = { "CLEAN", "DRIVE", "HI-GAIN" };
        for (int i = 0; i < 3; ++i) {
            channelBtns[static_cast<size_t>(i)].setButtonText(chNames[i]);
            channelBtns[static_cast<size_t>(i)].onClick = [this, i] { setQuickCategory(i); };
            addAndMakeVisible(channelBtns[static_cast<size_t>(i)]);
        }

        powerSwitch.onStateChange = [this] { updateVisualTheme(); };
        addAndMakeVisible(powerSwitch);

        loadNamBtn.setButtonText("LOAD .NAM CAPTURE...");
        loadNamBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
        loadNamBtn.onClick = [this] { if (loadNamCb) loadNamCb(activeRig); };
        addAndMakeVisible(loadNamBtn);

        cloudNamBtn.setButtonText("TONE3000");
        cloudNamBtn.setTooltip("Browse & load Neural Amp Modeler captures from TONE3000 Cloud");
        cloudNamBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f766e));
        cloudNamBtn.onClick = [this] { if (browseCloudNamCb) browseCloudNamCb(activeRig); };
        addAndMakeVisible(cloudNamBtn);

        clearNamBtn.setButtonText("CLEAR");
        clearNamBtn.onClick = [this] { if (clearNamCb) clearNamCb(activeRig); };
        addAndMakeVisible(clearNamBtn);

        findPackBtn.setButtonText("FIND .NAM");
        findPackBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f766e));
        findPackBtn.onClick = [this] {
            const int m = getCurrentModelIndex();
            const auto& e = gp200::getModelEntry(ModuleId::AmpHead, m);
            const auto info = ModelPackRegistry::getExcelPackInfo(e.excelNo);
            const juce::String q = (info.searchQuery != nullptr && info.searchQuery[0] != '\0')
                ? juce::String::fromUTF8(info.searchQuery)
                : juce::String::fromUTF8(e.description);
            if (findPackCloudCb) findPackCloudCb(activeRig, q);
        };
        addAndMakeVisible(findPackBtn);

        for (int i = 0; i < 7; ++i) {
            knobs[static_cast<size_t>(i)].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knobs[static_cast<size_t>(i)].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
            knobs[static_cast<size_t>(i)].getProperties().set("showTicks", true);
            addChildComponent(knobs[static_cast<size_t>(i)]);

            knobLabels[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
            knobLabels[static_cast<size_t>(i)].setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            addChildComponent(knobLabels[static_cast<size_t>(i)]);
        }

        switchLabel.setJustificationType(juce::Justification::centred);
        switchLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        switchLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
        addChildComponent(switchLabel);

        switchToggleBtn.setClickingTogglesState(true);
        switchToggleBtn.onClick = [this] {
            if (!ignoreSwitchCb) {
                setSwitchParamValue(switchToggleBtn.getToggleState() ? 1.0f : 0.0f);
                syncSwitchVisualText();
            }
        };
        addChildComponent(switchToggleBtn);

        switchCombo.onChange = [this] {
            if (!ignoreSwitchCb && switchCombo.getSelectedItemIndex() >= 0) {
                setSwitchParamValue(static_cast<float>(switchCombo.getSelectedItemIndex()));
            }
        };
        addChildComponent(switchCombo);

        rebindRig(0);
    }

    void selectRig(int rigIndex) {
        const int maxRigs = activeRigCountForMode(processor.getRoutingMode());
        rigIndex = std::clamp(rigIndex, 0, maxRigs - 1);
        if (activeRig == rigIndex) return;
        rebindRig(rigIndex);
        repaint();
    }

    int getSelectedRig() const noexcept { return activeRig; }

    void setBrowseCloudNamCallback(std::function<void(int)> cb) {
        browseCloudNamCb = std::move(cb);
    }

    void setFindPackOnCloudCallback(std::function<void(int, const juce::String&)> cb) {
        findPackCloudCb = std::move(cb);
    }

    void updateLive() {
        syncRoutingMode(false);
        const auto& chain = processor.getSignalChain();
        const float pk = std::max(chain.getOutputPeakL(), chain.getOutputPeakR());
        liveGlow = std::max(pk, liveGlow * 0.88f);

        const int m = getCurrentModelIndex();
        if (m != lastRenderedModel) {
            updateVisualTheme();
        }
        syncSwitchFromApvts();
        repaint();
    }

    void refreshAll() {
        syncRoutingMode(true);
        updateVisualTheme();
        syncSwitchFromApvts();
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        valeton::drawCleanStageBackdrop(g, area);

        const auto lay = computeLayoutGeometry();
        const auto headRect = lay.headRect;
        const int m = getCurrentModelIndex();
        const bool isNam = (m >= 71);
        const auto& entry = gp200::getModelEntry(ModuleId::AmpHead, m);
        const bool active = !powerSwitch.getToggleState();
        const auto& namRunner = processor.getSignalChain().getAmpHead(activeRig).getNamRunner();
        const bool hasNamFile = namRunner.hasLoadedModel();
        const auto namName = namRunner.getDisplayName();
        const auto accent = isNam ? juce::Colour(0xff06b6d4) : juce::Colour(entry.chassisArgb);

        valeton::drawCleanChassis(g, headRect, accent, active, 12.0f, lay.hdrH);

        auto hdr = headRect.withHeight(lay.hdrH);
        valeton::drawModuleGlyph(g, hdr.withWidth(36.0f).reduced(7.0f, 4.0f), ModuleId::AmpHead,
                                 active ? juce::Colour(0xff0b0d11) : juce::Colour(0xff94a3b8));

        g.setColour(active ? juce::Colour(0xff0b0d11) : juce::Colour(0xffcbd5e1));
        g.setFont(juce::Font(juce::FontOptions(lay.compact ? 11.5f : 13.0f, juce::Font::bold)));
        const juce::String hdrTitle = isNam
            ? (juce::String("NAM  |  ") + (hasNamFile ? namName : juce::String("Neural Amp Modeler Slot")))
            : (juce::String("AMP  |  ") + juce::String::fromUTF8(entry.name) + "   —   " + juce::String::fromUTF8(entry.description));
        g.drawText(hdrTitle,
                   hdr.withTrimmedLeft(36.0f).withTrimmedRight(144.0f),
                   juce::Justification::centredLeft, true);

        g.setFont(juce::Font(juce::FontOptions(lay.compact ? 9.0f : 10.0f, juce::Font::bold)));
        const auto rMode = processor.getRoutingMode();
        juce::String rigBadge = "SERIAL ACTIVE";
        if (rMode == RoutingMode::DualIndependent) {
            rigBadge = (activeRig == 1) ? "CHAIN B ACTIVE" : "CHAIN A ACTIVE";
        } else if (rMode == RoutingMode::TripleParallel) {
            rigBadge = (activeRig == 2) ? "PATH C ACTIVE" : ((activeRig == 1) ? "PATH B ACTIVE" : "PATH A ACTIVE");
        } else if (rMode == RoutingMode::DualParallel) {
            rigBadge = (activeRig == 1) ? "PATH B ACTIVE" : "PATH A ACTIVE";
        }
        g.drawText(rigBadge,
                   hdr.withTrimmedRight(14.0f), juce::Justification::centredRight);

        const auto upperBay = lay.upperBay;
        g.setColour(juce::Colour(0xff11141b));
        g.fillRoundedRectangle(upperBay, 7.0f);
        g.setColour(juce::Colour(0xff252b38));
        g.drawRoundedRectangle(upperBay, 7.0f, 1.0f);

        if (!isNam) {
            // ===== AMP MODELS MODE: Clean Tube Stage + Full-Width Hardware Crest (zero NAM clutter!) =====
            const float tubeH = std::clamp(lay.tubeZone.getHeight() - 4.0f, 34.0f, 68.0f);
            const float tubeY = lay.tubeZone.getBottom() - tubeH;
            const float tubeStep = lay.tubeZone.getWidth() * 0.25f;
            const float tubeW = std::clamp(tubeStep * 0.72f, 20.0f, 36.0f);
            float gainFactor = 0.5f;
            const char* gainPid = (activeRig == 2) ? params::kAmpCGain : ((activeRig == 1) ? params::kAmpBGain : params::kAmpGain);
            if (auto* raw = processor.getAPVTS().getRawParameterValue(gainPid)) {
                gainFactor = std::clamp(raw->load(std::memory_order_relaxed) / 10.0f, 0.0f, 1.0f);
            }
            const float tubeGlow = std::clamp(0.32f + gainFactor * 0.38f + liveGlow * 0.6f, 0.0f, 1.0f);

            for (int t = 0; t < 4; ++t) {
                const float tx = lay.tubeZone.getX() + (static_cast<float>(t) + 0.5f) * tubeStep - tubeW * 0.5f;
                hw::drawVacuumTube(g, juce::Rectangle<float>(tx, tubeY, tubeW, tubeH), tubeGlow, active);
            }

            const auto infoBox = lay.ampInfoFull;
            if (infoBox.getWidth() >= 80.0f) {
                const auto& ampRef = processor.getSignalChain().getAmpHead(activeRig);
                const bool hasPackNam = ampRef.isUsingPackNam();
                const float titleH = std::min(26.0f, infoBox.getHeight() * 0.44f);
                const float descH  = std::min(16.0f, infoBox.getHeight() * 0.28f);
                const float typeH  = std::max(12.0f, infoBox.getHeight() - titleH - descH);

                auto titleRow = infoBox.withHeight(titleH);
                if (hasPackNam && infoBox.getWidth() >= 190.0f) {
                    auto badge = titleRow.removeFromRight(76.0f).reduced(0.0f, 3.0f);
                    g.setColour(active ? juce::Colour(0xff0891b2).withAlpha(0.24f) : juce::Colour(0xff232834));
                    g.fillRoundedRectangle(badge, 4.0f);
                    g.setColour(active ? juce::Colour(0xff22d3ee) : juce::Colour(0xff64748b));
                    g.drawRoundedRectangle(badge, 4.0f, 1.0f);
                    g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
                    g.drawText("NAM PACK", badge, juce::Justification::centred);
                    titleRow.removeFromRight(6.0f);
                }

                g.setColour(active ? accent : juce::Colour(0xff64748b));
                g.setFont(juce::Font(juce::FontOptions(infoBox.getWidth() < 260.0f ? 16.0f : 20.5f,
                                                       juce::Font::bold | juce::Font::italic)));
                g.drawText(juce::String::fromUTF8(entry.name), titleRow,
                           juce::Justification::centredLeft, true);

                g.setColour(active ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff5a6373));
                g.setFont(juce::Font(juce::FontOptions(infoBox.getWidth() < 260.0f ? 10.0f : 11.2f, juce::Font::bold)));
                g.drawText(juce::String::fromUTF8(entry.description),
                           infoBox.withTop(infoBox.getY() + titleH).withHeight(descH),
                           juce::Justification::centredLeft, true);

                g.setColour(hasPackNam && active ? juce::Colour(0xff67e8f9) : juce::Colour(0xff64748b));
                g.setFont(juce::Font(juce::FontOptions(8.8f, hasPackNam ? juce::Font::bold : juce::Font::plain)));
                const juce::String subLine = hasPackNam
                    ? (juce::String("Neural Capture Active: ") + ampRef.getPackNamDisplayName())
                    : (juce::String("Type: ") + juce::String::fromUTF8(entry.subType) + "   |   " + juce::String::fromUTF8(entry.paramsHint));
                g.drawText(subLine,
                           infoBox.withTop(infoBox.getY() + titleH + descH).withHeight(typeH),
                           juce::Justification::centredLeft, true);
            }
        } else {
            // ===== NAM CAPTURE MODE: Dedicated Neural Amp Modeler Capture Deck =====
            const auto deck = lay.namDeckRect;
            g.setColour(juce::Colour(0xff0a0e16));
            g.fillRoundedRectangle(deck, 6.0f);
            g.setColour((active && hasNamFile) ? juce::Colour(0xff06b6d4).withAlpha(0.75f) : juce::Colour(0xff252e40));
            g.drawRoundedRectangle(deck, 6.0f, (active && hasNamFile) ? 1.3f : 1.0f);

            // Left crisp Neural Network Emblem
            const auto embRect = deck.withWidth(std::min(68.0f, deck.getHeight() + 8.0f)).reduced(10.0f, 8.0f);
            const auto nodeCol = (active && hasNamFile) ? juce::Colour(0xff38bdf8) : juce::Colour(0xff475569);
            const float lx = embRect.getX() + 6.0f;
            const float mx = embRect.getCentreX();
            const float rx = embRect.getRight() - 6.0f;
            const float cy = embRect.getCentreY();
            const float dy = std::min(14.0f, embRect.getHeight() * 0.34f);

            const juce::Point<float> inPts[2]  = { { lx, cy - dy * 0.65f }, { lx, cy + dy * 0.65f } };
            const juce::Point<float> midPts[3] = { { mx, cy - dy }, { mx, cy }, { mx, cy + dy } };
            const juce::Point<float> outPt     = { rx, cy };

            g.setColour(nodeCol.withAlpha(active && hasNamFile ? 0.50f : 0.25f));
            for (const auto& ip : inPts) {
                for (const auto& mp : midPts) g.drawLine(ip.x, ip.y, mp.x, mp.y, 1.1f);
            }
            for (const auto& mp : midPts) g.drawLine(mp.x, mp.y, outPt.x, outPt.y, 1.1f);

            g.setColour(nodeCol);
            for (const auto& ip : inPts)  g.fillEllipse(ip.x - 2.6f, ip.y - 2.6f, 5.2f, 5.2f);
            for (const auto& mp : midPts) g.fillEllipse(mp.x - 2.8f, mp.y - 2.8f, 5.6f, 5.6f);
            g.fillEllipse(outPt.x - 3.2f, outPt.y - 3.2f, 6.4f, 6.4f);

            // Middle NAM Status & Filename
            auto textBox = deck.withTrimmedLeft(embRect.getWidth() + 14.0f)
                               .withTrimmedRight(lay.namBtnColW + 14.0f)
                               .reduced(0.0f, 7.0f);
            if (textBox.getWidth() >= 80.0f) {
                g.setColour((active && hasNamFile) ? juce::Colour(0xff38bdf8) : juce::Colour(0xff64748b));
                g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
                g.drawText(hasNamFile ? "NEURAL AMP CAPTURE ACTIVE" : "NEURAL AMP MODELER (.NAM)",
                           textBox.withHeight(12.0f), juce::Justification::centredLeft, true);

                g.setColour(active ? (hasNamFile ? juce::Colours::white : juce::Colour(0xffcbd5e1))
                                   : juce::Colour(0xff64748b));
                g.setFont(juce::Font(juce::FontOptions(textBox.getWidth() < 240.0f ? 13.0f : 16.5f, juce::Font::bold)));
                g.drawText(hasNamFile ? namName : "No .nam Capture Loaded",
                           textBox.withTop(textBox.getY() + 14.0f).withHeight(22.0f),
                           juce::Justification::centredLeft, true);

                g.setColour(juce::Colour(0xff6b778c));
                g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::plain)));
                const juce::String tip = hasNamFile
                    ? "Post-NAM Tone Stack active below  |  Bypass CAB if this .nam is a full-rig capture"
                    : "Load a .nam amp capture file — Input Drive & 4-band Tone Stack stay active below";
                g.drawText(tip, textBox.withTop(textBox.getBottom() - 14.0f).withHeight(14.0f),
                           juce::Justification::centredLeft, true);
            }
        }

        const auto face = lay.face;
        g.setColour(juce::Colour(0xff141821));
        g.fillRoundedRectangle(face, 7.0f);
        g.setColour(active ? accent.withAlpha(0.45f) : juce::Colour(0xff262c38));
        g.drawRoundedRectangle(face, 7.0f, 1.1f);

        g.setColour(active ? accent.withAlpha(0.85f) : juce::Colour(0xff262c38));
        g.fillRoundedRectangle(face.getX() + 12.0f, face.getY() + 5.0f, face.getWidth() - 24.0f, 2.5f, 1.2f);

        const float lampR = (face.getHeight() < 135.0f) ? 6.5f : 8.5f;
        const float lampX = face.getRight() - (lay.compact ? 38.0f : 48.0f);
        const float lampY = face.getY() + std::min(26.0f, face.getHeight() * 0.20f);
        hw::drawJewelLamp(g, lampX, lampY, lampR, active ? accent : juce::Colour(0xff3a4252), active);
    }

    void resized() override {
        const auto lay = computeLayoutGeometry();
        auto upperBay = lay.upperBay.toNearestInt();

        const int topRowH = (upperBay.getHeight() < 120) ? 22 : 25;
        auto topSwitchRow = upperBay.reduced(10, 6).removeFromTop(topRowH);
        const bool narrowTop = (topSwitchRow.getWidth() < 800);
        const int rigCount = activeRigCountForMode(processor.getRoutingMode());
        const bool isNam = (getCurrentModelIndex() >= 71);

        rigTabA.setButtonText("AMP A");
        rigTabB.setButtonText("AMP B");
        rigTabC.setButtonText("AMP C");
        rigTabA.setVisible(rigCount >= 2);
        rigTabB.setVisible(rigCount >= 2);
        rigTabC.setVisible(rigCount >= 3);

        const int rigW = narrowTop ? 54 : 66;
        if (rigCount >= 2) {
            rigTabA.setBounds(topSwitchRow.removeFromLeft(rigW));
            topSwitchRow.removeFromLeft(4);
            rigTabB.setBounds(topSwitchRow.removeFromLeft(rigW));
            if (rigCount >= 3) {
                topSwitchRow.removeFromLeft(4);
                rigTabC.setBounds(topSwitchRow.removeFromLeft(rigW));
            }
            topSwitchRow.removeFromLeft(8);
        }

        // Dedicated Engine Switch: AMP MODELS | NAM CAPTURE
        const int modeW = narrowTop ? 82 : 96;
        modeAmpBtn.setButtonText(narrowTop ? "AMP MODEL" : "AMP MODELS");
        modeNamBtn.setButtonText(narrowTop ? "NAM" : "NAM CAPTURE");
        modeAmpBtn.setBounds(topSwitchRow.removeFromLeft(modeW));
        topSwitchRow.removeFromLeft(3);
        modeNamBtn.setBounds(topSwitchRow.removeFromLeft(modeW));
        topSwitchRow.removeFromLeft(8);

        // Mode-specific controls: DSP dropdown + Find .NAM + Clean/Drive/Hi-Gain ONLY in AMP MODELS mode; Load/Clear NAM ONLY in NAM mode!
        ampModelBox.setVisible(!isNam);
        findPackBtn.setVisible(!isNam);
        for (int i = 0; i < 3; ++i) {
            channelBtns[static_cast<size_t>(i)].setVisible(!isNam);
        }
        loadNamBtn.setVisible(isNam);
        cloudNamBtn.setVisible(isNam);
        clearNamBtn.setVisible(isNam);

        if (!isNam) {
            const int chGroupW = narrowTop ? 154 : 192;
            auto chGroup = topSwitchRow.removeFromRight(chGroupW);
            const int chW = chGroup.getWidth() / 3;
            for (int i = 0; i < 3; ++i) {
                channelBtns[static_cast<size_t>(i)].setBounds(chGroup.removeFromLeft(chW).reduced(2, 0));
            }
            topSwitchRow.removeFromRight(5);
            const int findW = narrowTop ? 58 : 74;
            findPackBtn.setButtonText(narrowTop ? "T3K" : "FIND .NAM");
            findPackBtn.setBounds(topSwitchRow.removeFromRight(findW));
            topSwitchRow.removeFromRight(5);
            ampModelBox.setBounds(topSwitchRow);
        } else {
            auto namBtns = lay.namDeckRect.toNearestInt().reduced(10, 8).removeFromRight(static_cast<int>(lay.namBtnColW));
            const int btnH = std::clamp(namBtns.getHeight() / 2 - 3, 20, 26);
            const int totalBtnH = btnH * 2 + 5;
            auto stacked = namBtns.withSizeKeepingCentre(namBtns.getWidth(), std::min(totalBtnH, namBtns.getHeight()));
            loadNamBtn.setButtonText(stacked.getWidth() < 145 ? "LOAD .NAM..." : "LOAD .NAM CAPTURE...");
            loadNamBtn.setBounds(stacked.removeFromTop(btnH));
            stacked.removeFromTop(5);
            auto botRow = stacked.removeFromTop(btnH);
            const int clrW = std::min(56, std::max(44, botRow.getWidth() * 34 / 100));
            clearNamBtn.setBounds(botRow.removeFromRight(clrW));
            botRow.removeFromRight(4);
            cloudNamBtn.setButtonText(botRow.getWidth() < 78 ? "T3K" : "TONE3000");
            cloudNamBtn.setBounds(botRow);
        }

        auto face = lay.face.toNearestInt().reduced(lay.compact ? 12 : 18, lay.compact ? 8 : 12);
        auto rightCol = face.removeFromRight(lay.compact ? 62 : 78);
        const int lampClearance = std::min(36, face.getHeight() * 28 / 100);
        rightCol.removeFromTop(lampClearance);
        const int swH = std::min(60, std::max(34, rightCol.getHeight()));
        powerSwitch.setBounds(rightCol.removeFromTop(swH).reduced(2, 0));
        face.removeFromRight(6);

        // Layout exact active knobs & optional switch (Bright / Char / Midrange) centered cleanly!
        const int totalItems = activeKnobCount + (hasActiveSwitch ? 1 : 0);
        if (totalItems <= 0) return;

        const int maxCellW = (totalItems <= 4) ? 138 : 118;
        const int cellW = std::min(maxCellW, face.getWidth() / totalItems);
        const int rowW  = cellW * totalItems;
        auto ctrlRow = juce::Rectangle<int>(face.getCentreX() - rowW / 2, face.getY(), rowW, face.getHeight());

        const int tbW = std::clamp(cellW - 8, 40, 62);
        const int tbH = (face.getHeight() < 115) ? 12 : 14;

        for (int i = 0; i < activeKnobCount; ++i) {
            auto cell = ctrlRow.removeFromLeft(cellW).reduced(lay.compact ? 3 : 5, 2);
            knobLabels[static_cast<size_t>(i)].setBounds(cell.removeFromTop(15));
            knobs[static_cast<size_t>(i)].setTextBoxStyle(juce::Slider::TextBoxBelow, false, tbW, tbH);
            knobs[static_cast<size_t>(i)].setBounds(cell);
        }

        if (hasActiveSwitch) {
            auto swCell = ctrlRow.removeFromLeft(cellW).reduced(4, 2);
            switchLabel.setBounds(swCell.removeFromTop(15));
            const int ctrlBtnW = std::min(92, swCell.getWidth() - 4);
            const int ctrlBtnH = std::min(28, std::max(22, swCell.getHeight() / 3));
            auto centered = swCell.withSizeKeepingCentre(ctrlBtnW, ctrlBtnH);
            if (activeSwitchSpec.numChoices <= 2) {
                switchToggleBtn.setBounds(centered);
            } else {
                switchCombo.setBounds(centered);
            }
        }
    }

private:
    static int activeRigCountForMode(RoutingMode m) noexcept {
        if (m == RoutingMode::TripleParallel) return 3;
        if (isParallelRouting(m))             return 2;
        return 1;
    }

    void syncRoutingMode(bool forceLayout) {
        const auto m = processor.getRoutingMode();
        const int maxRigs = activeRigCountForMode(m);
        const bool modeChanged = (m != lastRoutingMode);
        lastRoutingMode = m;
        if (activeRig >= maxRigs) {
            rebindRig(0);
            forceLayout = true;
        }
        if (modeChanged || forceLayout) {
            resized();
        }
    }

    struct LayoutGeometry {
        juce::Rectangle<float> headRect;
        juce::Rectangle<float> upperBay;
        juce::Rectangle<float> tubeZone;
        juce::Rectangle<float> ampInfoFull;
        juce::Rectangle<float> namDeckRect;
        float namBtnColW = 160.0f;
        juce::Rectangle<float> face;
        float hdrH = 34.0f;
        bool compact = false;
    };

    LayoutGeometry computeLayoutGeometry() const {
        LayoutGeometry g;
        const auto full = getLocalBounds().toFloat();
        g.compact = (full.getWidth() < 860.0f || full.getHeight() < 390.0f);

        const float padX = g.compact ? 10.0f : 20.0f;
        const float padY = g.compact ? 8.0f : 16.0f;
        auto b = full.reduced(padX, padY);
        const float maxW = std::min(1080.0f, b.getWidth());
        const float maxH = std::min(430.0f, b.getHeight());
        g.headRect = juce::Rectangle<float>(b.getCentreX() - maxW * 0.5f,
                                            b.getCentreY() - maxH * 0.5f,
                                            maxW, maxH);

        g.hdrH = (g.headRect.getHeight() < 340.0f) ? 28.0f : 34.0f;
        auto inner = g.headRect.reduced(g.compact ? 12.0f : 16.0f, g.compact ? 8.0f : 12.0f)
                               .withTrimmedTop(g.hdrH - 4.0f);
        g.upperBay = inner.withHeight(inner.getHeight() * 0.45f);

        const float topRowH = (g.upperBay.getHeight() < 120.0f) ? 22.0f : 25.0f;
        const float contentTop = g.upperBay.getY() + topRowH + 11.0f;
        const float contentH = std::max(46.0f, g.upperBay.getBottom() - 6.0f - contentTop);
        const float contentY = g.upperBay.getBottom() - 6.0f - contentH;
        const float contentW = std::max(100.0f, g.upperBay.getWidth() - 20.0f);
        const float x0 = g.upperBay.getX() + 10.0f;

        const float tubeW = std::clamp(contentW * 0.27f, 128.0f, 224.0f);
        g.tubeZone = juce::Rectangle<float>(x0, contentY, tubeW, contentH);

        const float infoX = g.tubeZone.getRight() + 14.0f;
        const float infoW = std::max(0.0f, g.upperBay.getRight() - 12.0f - infoX);
        const float infoH = std::min(62.0f, contentH);
        g.ampInfoFull = juce::Rectangle<float>(infoX, contentY + (contentH - infoH) * 0.5f, infoW, infoH);

        g.namDeckRect = juce::Rectangle<float>(x0, contentY, contentW, contentH);
        g.namBtnColW  = std::clamp(contentW * 0.22f, 126.0f, 174.0f);

        const float bayGap = g.compact ? 7.0f : 10.0f;
        g.face = inner.withTop(g.upperBay.getBottom() + bayGap);
        return g;
    }

    const char* paramIdForBind(AmpSlotBind bind, int rigIdx) const noexcept {
        if (rigIdx == 2) {
            switch (bind) {
                case AmpSlotBind::Gain:     return params::kAmpCGain;
                case AmpSlotBind::Bass:     return params::kAmpCBass;
                case AmpSlotBind::Mid:      return params::kAmpCMid;
                case AmpSlotBind::Treble:   return params::kAmpCTreble;
                case AmpSlotBind::Presence: return params::kAmpCPresence;
                case AmpSlotBind::Master:   return params::kAmpCMaster;
                case AmpSlotBind::Extra:    return params::kAmpCExtra;
                case AmpSlotBind::Switch:   return params::kAmpCSwitch;
                default:                    return params::kAmpCGain;
            }
        }
        const bool isB = (rigIdx == 1);
        switch (bind) {
            case AmpSlotBind::Gain:     return isB ? params::kAmpBGain     : params::kAmpGain;
            case AmpSlotBind::Bass:     return isB ? params::kAmpBBass     : params::kAmpBass;
            case AmpSlotBind::Mid:      return isB ? params::kAmpBMid      : params::kAmpMid;
            case AmpSlotBind::Treble:   return isB ? params::kAmpBTreble   : params::kAmpTreble;
            case AmpSlotBind::Presence: return isB ? params::kAmpBPresence : params::kAmpPresence;
            case AmpSlotBind::Master:   return isB ? params::kAmpBMaster   : params::kAmpMaster;
            case AmpSlotBind::Extra:    return isB ? params::kAmpBExtra    : params::kAmpExtra;
            case AmpSlotBind::Switch:   return isB ? params::kAmpBSwitch   : params::kAmpSwitch;
            default:                    return isB ? params::kAmpBGain     : params::kAmpGain;
        }
    }

    int getCurrentModelIndex() const noexcept {
        const char* mid = (activeRig == 2) ? params::kAmpCModel : ((activeRig == 1) ? params::kAmpBModel : params::kAmpModel);
        if (auto* raw = processor.getAPVTS().getRawParameterValue(mid)) {
            return std::clamp(static_cast<int>(std::lround(raw->load(std::memory_order_relaxed))), 0, 71);
        }
        return 39;
    }

    void setSwitchParamValue(float val) {
        const char* sid = (activeRig == 2) ? params::kAmpCSwitch : ((activeRig == 1) ? params::kAmpBSwitch : params::kAmpSwitch);
        if (auto* p = processor.getAPVTS().getParameter(sid)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(val));
            p->endChangeGesture();
        }
    }

    void syncSwitchVisualText() {
        if (!hasActiveSwitch || activeSwitchSpec.numChoices > 2) return;
        const bool on = switchToggleBtn.getToggleState();
        const char* txt = on ? activeSwitchSpec.choices[1] : activeSwitchSpec.choices[0];
        switchToggleBtn.setButtonText(txt != nullptr ? txt : (on ? "ON" : "OFF"));
    }

    void syncSwitchFromApvts() {
        if (!hasActiveSwitch) return;
        const char* sid = (activeRig == 2) ? params::kAmpCSwitch : ((activeRig == 1) ? params::kAmpBSwitch : params::kAmpSwitch);
        int cur = 0;
        if (auto* raw = processor.getAPVTS().getRawParameterValue(sid)) {
            cur = static_cast<int>(std::lround(raw->load(std::memory_order_relaxed)));
        }
        ignoreSwitchCb = true;
        if (activeSwitchSpec.numChoices <= 2) {
            switchToggleBtn.setToggleState(cur >= 1, juce::dontSendNotification);
            syncSwitchVisualText();
        } else {
            const int clamped = std::clamp(cur, 0, activeSwitchSpec.numChoices - 1);
            if (switchCombo.getSelectedItemIndex() != clamped) {
                switchCombo.setSelectedItemIndex(clamped, juce::dontSendNotification);
            }
        }
        ignoreSwitchCb = false;
    }

    void setAmpModelParam(int modelIdx) {
        modelIdx = std::clamp(modelIdx, 0, 71);
        const char* mid = (activeRig == 2) ? params::kAmpCModel : ((activeRig == 1) ? params::kAmpBModel : params::kAmpModel);
        if (auto* param = processor.getAPVTS().getParameter(mid)) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(modelIdx)));
            param->endChangeGesture();
        }
        const int catIdx = AmpHead::categoryForModel(modelIdx);
        const char* cid = (activeRig == 2) ? params::kAmpCChannel : ((activeRig == 1) ? params::kAmpBChannel : params::kAmpChannel);
        if (auto* cp = processor.getAPVTS().getParameter(cid)) {
            cp->beginChangeGesture();
            cp->setValueNotifyingHost(cp->convertTo0to1(static_cast<float>(catIdx)));
            cp->endChangeGesture();
        }
    }

    void setNamMode(bool enableNam) {
        if (enableNam) {
            const int cur = getCurrentModelIndex();
            if (cur < 71) {
                lastDspModel[static_cast<size_t>(activeRig)] = cur;
            }
            setAmpModelParam(71);
        } else {
            const int target = std::clamp(lastDspModel[static_cast<size_t>(activeRig)], 0, 70);
            setAmpModelParam(target);
        }
        updateVisualTheme();
        resized();
        repaint();
    }

    void setQuickCategory(int catIdx) {
        const int repModel = AmpHead::defaultModelForChannel(catIdx);
        if (repModel < 71) {
            lastDspModel[static_cast<size_t>(activeRig)] = repModel;
        }
        setAmpModelParam(repModel);
        updateVisualTheme();
        repaint();
    }

    void rebindRig(int rigIndex) {
        activeRig = std::clamp(rigIndex, 0, 2);

        powerAtt.reset();

        const char* bypassId = (activeRig == 2) ? params::kAmpCBypass : ((activeRig == 1) ? params::kAmpBBypass : params::kAmpBypass);
        powerSwitch.configureMidiLearn(&processor.getMidiLearnManager(), bypassId);
        powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.getAPVTS(), bypassId, powerSwitch
        );

        lastRenderedModel = -1;
        updateVisualTheme();
    }

    void rebuildControlsForModel(int m) {
        const auto specs = getAmpControlSpecs(m);

        for (auto& a : knobAtts) a.reset();
        activeKnobCount = 0;
        hasActiveSwitch = false;
        activeSwitchSpec = {};

        for (const auto& sp : specs) {
            if (sp.isSwitch) {
                hasActiveSwitch = true;
                activeSwitchSpec = sp;
                continue;
            }
            if (activeKnobCount >= 7) continue;
            const size_t idx = static_cast<size_t>(activeKnobCount);
            const char* pid = paramIdForBind(sp.bind, activeRig);

            knobLabels[idx].setText(sp.label, juce::dontSendNotification);
            knobLabels[idx].setVisible(true);

            auto& s = knobs[idx];
            s.configureMidiLearn(&processor.getMidiLearnManager(), pid);
            if (auto* p = processor.getAPVTS().getParameter(pid)) {
                s.setDoubleClickReturnValue(true, static_cast<double>(p->convertFrom0to1(p->getDefaultValue())));
            }
            knobAtts[idx] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), pid, s
            );
            s.setVisible(true);
            ++activeKnobCount;
        }

        for (int i = activeKnobCount; i < 7; ++i) {
            knobs[static_cast<size_t>(i)].setVisible(false);
            knobLabels[static_cast<size_t>(i)].setVisible(false);
        }

        switchLabel.setVisible(hasActiveSwitch);
        switchToggleBtn.setVisible(hasActiveSwitch && activeSwitchSpec.numChoices <= 2);
        switchCombo.setVisible(hasActiveSwitch && activeSwitchSpec.numChoices > 2);

        if (hasActiveSwitch) {
            switchLabel.setText(activeSwitchSpec.label, juce::dontSendNotification);
            if (activeSwitchSpec.numChoices > 2) {
                ignoreSwitchCb = true;
                switchCombo.clear(juce::dontSendNotification);
                for (int c = 0; c < activeSwitchSpec.numChoices; ++c) {
                    switchCombo.addItem(activeSwitchSpec.choices[static_cast<size_t>(c)], c + 1);
                }
                ignoreSwitchCb = false;
            }
            syncSwitchFromApvts();
        }

        resized();
    }

    void updateVisualTheme() {
        const int m = getCurrentModelIndex();
        const bool isNam = (m >= 71);
        const bool modelChanged = (m != lastRenderedModel);
        lastRenderedModel = m;

        if (!isNam) {
            lastDspModel[static_cast<size_t>(activeRig)] = m;
            ignoreModelBoxCb = true;
            if (ampModelBox.getSelectedItemIndex() != m) {
                ampModelBox.setSelectedItemIndex(m, juce::dontSendNotification);
            }
            ignoreModelBoxCb = false;
        }

        if (modelChanged) {
            rebuildControlsForModel(m);
        }

        const int cat = AmpHead::categoryForModel(m);
        const auto& entry = gp200::getModelEntry(ModuleId::AmpHead, m);
        const auto accent = isNam ? juce::Colour(0xff06b6d4) : juce::Colour(entry.chassisArgb);

        const bool active = !powerSwitch.getToggleState();

        rigTabA.setColour(juce::TextButton::buttonColourId, activeRig == 0 ? valeton::pillOnBlue() : juce::Colour(0xff1f2430));
        rigTabB.setColour(juce::TextButton::buttonColourId, activeRig == 1 ? juce::Colour(0xff0284c7) : juce::Colour(0xff1f2430));
        rigTabC.setColour(juce::TextButton::buttonColourId, activeRig == 2 ? juce::Colour(0xff7c3aed) : juce::Colour(0xff1f2430));

        modeAmpBtn.getProperties().set("bypassed", !active);
        modeAmpBtn.setColour(juce::TextButton::buttonColourId,
                             !isNam ? (active ? valeton::pillOnBlue() : juce::Colour(0xff2c3240)) : juce::Colour(0xff191e29));
        modeAmpBtn.setColour(juce::TextButton::textColourOffId,
                             (!isNam && active) ? juce::Colours::white : juce::Colour(0xff8491a6));
        modeAmpBtn.repaint();

        modeNamBtn.getProperties().set("bypassed", !active);
        modeNamBtn.setColour(juce::TextButton::buttonColourId,
                             isNam ? (active ? juce::Colour(0xff0891b2) : juce::Colour(0xff2c3240)) : juce::Colour(0xff191e29));
        modeNamBtn.setColour(juce::TextButton::textColourOffId,
                             (isNam && active) ? juce::Colours::white : juce::Colour(0xff8491a6));
        modeNamBtn.repaint();

        const juce::Colour chCols[3] = {
            juce::Colour(0xff0284c7), juce::Colour(0xffb45309), juce::Colour(0xffb91c1c)
        };
        for (int i = 0; i < 3; ++i) {
            channelBtns[static_cast<size_t>(i)].getProperties().set("bypassed", !active);
            channelBtns[static_cast<size_t>(i)].setColour(
                juce::TextButton::buttonColourId,
                (!isNam && i == cat) ? (active ? chCols[i] : juce::Colour(0xff2c3240)) : juce::Colour(0xff1b202b)
            );
            channelBtns[static_cast<size_t>(i)].setColour(
                juce::TextButton::textColourOffId,
                active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b)
            );
            channelBtns[static_cast<size_t>(i)].repaint();
        }
        loadNamBtn.getProperties().set("bypassed", !active);
        loadNamBtn.setColour(juce::TextButton::buttonColourId,
                             active ? juce::Colour(0xff0284c7) : juce::Colour(0xff262c38));
        loadNamBtn.setColour(juce::TextButton::textColourOffId,
                             active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
        loadNamBtn.repaint();

        cloudNamBtn.getProperties().set("bypassed", !active);
        cloudNamBtn.setColour(juce::TextButton::buttonColourId,
                              active ? juce::Colour(0xff0f766e) : juce::Colour(0xff232934));
        cloudNamBtn.setColour(juce::TextButton::textColourOffId,
                              active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
        cloudNamBtn.repaint();

        const auto& curEntry = gp200::getModelEntry(ModuleId::AmpHead, m);
        const auto packHint = ModelPackRegistry::getExpectedFilenameHint(ModuleId::AmpHead, m);
        const auto packInfo = ModelPackRegistry::getExcelPackInfo(curEntry.excelNo);
        findPackBtn.setTooltip(
            "Find real .nam capture on TONE3000 (\"" + juce::String::fromUTF8(packInfo.searchQuery)
            + "\"). Save as " + packHint + " in ModelPacks to auto-bind!"
        );
        findPackBtn.getProperties().set("bypassed", !active);
        findPackBtn.setColour(juce::TextButton::buttonColourId,
                              active ? juce::Colour(0xff0f766e) : juce::Colour(0xff232934));
        findPackBtn.setColour(juce::TextButton::textColourOffId,
                              active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
        findPackBtn.repaint();

        const bool hasNamFile = processor.getSignalChain().getAmpHead(activeRig).getNamRunner().hasLoadedModel();
        clearNamBtn.setEnabled(hasNamFile);
        clearNamBtn.setAlpha(hasNamFile ? 1.0f : 0.45f);
        clearNamBtn.repaint();

        const char* knobStyle = (cat == 0) ? "vintage_skirted"
                                           : ((cat == 1) ? "gold_plexi" : "chrome_knurled");

        for (int i = 0; i < activeKnobCount; ++i) {
            knobs[static_cast<size_t>(i)].getProperties().set("knobStyle", knobStyle);
            knobs[static_cast<size_t>(i)].getProperties().set("bypassed", !active);
            knobs[static_cast<size_t>(i)].setColour(juce::Slider::rotarySliderFillColourId, accent);
            knobs[static_cast<size_t>(i)].setColour(juce::Slider::textBoxTextColourId,
                                                    active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
            knobLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId,
                                                         active ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff5a6373));
            knobs[static_cast<size_t>(i)].repaint();
        }
        switchToggleBtn.getProperties().set("bypassed", !active);
        switchToggleBtn.setColour(juce::TextButton::buttonOnColourId,
                                  active ? accent.darker(0.15f) : juce::Colour(0xff2c3240));
        switchToggleBtn.setColour(juce::TextButton::textColourOnId,
                                  active ? juce::Colours::white : juce::Colour(0xff64748b));
        switchToggleBtn.setColour(juce::TextButton::textColourOffId,
                                  active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff5a6373));
        switchToggleBtn.repaint();
        powerSwitch.repaint();
    }

    PedalBoardAudioProcessor& processor;
    std::function<void(int)> loadNamCb;
    std::function<void(int)> clearNamCb;
    std::function<void(int)> browseCloudNamCb;
    std::function<void(int, const juce::String&)> findPackCloudCb;

    int activeRig = 0;
    RoutingMode lastRoutingMode = RoutingMode::Serial;
    std::array<int, 3> lastDspModel { 3, 3, 3 };
    int lastRenderedModel = -1;
    float liveGlow = 0.0f;
    int activeKnobCount = 0;
    bool hasActiveSwitch = false;
    bool ignoreSwitchCb = false;
    bool ignoreModelBoxCb = false;
    AmpControlSpec activeSwitchSpec {};

    juce::TextButton rigTabA, rigTabB, rigTabC;
    juce::TextButton modeAmpBtn, modeNamBtn;
    juce::ComboBox ampModelBox;
    std::array<juce::TextButton, 3> channelBtns;
    ChromePowerToggle powerSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;

    juce::TextButton loadNamBtn, cloudNamBtn, clearNamBtn, findPackBtn;

    std::array<MidiBindableSlider, 7> knobs;
    std::array<juce::Label, 7> knobLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> knobAtts;

    juce::Label switchLabel;
    juce::TextButton switchToggleBtn;
    juce::ComboBox switchCombo;
};

} // namespace pedalboard
