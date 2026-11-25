#pragma once

#include "../engine/Gp200Catalogue.h"
#include "../engine/ModelPackRegistry.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "MidiBindableControl.h"
#include "ValetonTheme.h"

#include <array>
#include <cmath>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace pedalboard {

class CabFilterCurveDisplay : public juce::Component {
public:
    void setFilterParams(float lowHz, float highHz, bool bypassed, juce::Colour accentCol) noexcept {
        lowCutHz = lowHz;
        highCutHz = highHz;
        isBypassed = bypassed;
        accent = accentCol;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xff0d1017));
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(juce::Colour(0xff2a3140));
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        auto plot = area.reduced(10.0f, 14.0f);
        const float minF = 20.0f;
        const float maxF = 20000.0f;
        const float logMin = std::log10(minF);
        const float logSpan = std::log10(maxF) - logMin;

        auto freqToX = [&](float hz) {
            return plot.getX() + ((std::log10(std::clamp(hz, minF, maxF)) - logMin) / logSpan) * plot.getWidth();
        };

        const float gridFreqs[6] = { 50.0f, 100.0f, 250.0f, 1000.0f, 4000.0f, 10000.0f };
        const char* gridLabels[6] = { "50", "100", "250", "1k", "4k", "10k" };
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
        for (int i = 0; i < 6; ++i) {
            const float gx = freqToX(gridFreqs[i]);
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.drawVerticalLine(static_cast<int>(gx), plot.getY(), plot.getBottom());
            g.setColour(juce::Colour(0xff64748b));
            g.drawText(gridLabels[i], static_cast<int>(gx) - 14, static_cast<int>(plot.getBottom()) + 1, 28, 12,
                       juce::Justification::centred);
        }

        const float zeroY = plot.getY() + plot.getHeight() * 0.25f;
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawHorizontalLine(static_cast<int>(zeroY), plot.getX(), plot.getRight());

        juce::Path curve;
        juce::Path fill;
        const int steps = 96;
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float hz = std::pow(10.0f, logMin + t * logSpan);
            const float x = plot.getX() + t * plot.getWidth();

            float magDb = 0.0f;
            if (!isBypassed) {
                const float rHp = lowCutHz / hz;
                const float magHp = 1.0f / std::sqrt(1.0f + rHp * rHp * rHp * rHp);
                const float rLp = hz / highCutHz;
                const float magLp = 1.0f / std::sqrt(1.0f + rLp * rLp * rLp * rLp);
                magDb = 20.0f * std::log10(std::max(1e-4f, magHp * magLp));
            }

            const float yNorm = std::clamp((6.0f - magDb) / 30.0f, 0.0f, 1.0f);
            const float y = plot.getY() + yNorm * plot.getHeight();

            if (i == 0) {
                curve.startNewSubPath(x, y);
                fill.startNewSubPath(x, plot.getBottom());
                fill.lineTo(x, y);
            } else {
                curve.lineTo(x, y);
                fill.lineTo(x, y);
            }
        }
        fill.lineTo(plot.getRight(), plot.getBottom());
        fill.closeSubPath();

        const auto lineCol = isBypassed ? juce::Colour(0xff64748b) : accent;
        g.setColour(lineCol.withAlpha(0.18f));
        g.fillPath(fill);
        g.setColour(lineCol);
        g.strokePath(curve, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText("CAB FILTER RESPONSE  (HPF " + juce::String(static_cast<int>(lowCutHz)) + " Hz  |  LPF "
                   + juce::String(highCutHz / 1000.0f, 1) + " kHz)",
                   area.reduced(8.0f, 2.0f).withHeight(13.0f), juce::Justification::centredLeft);
    }

private:
    float lowCutHz = 85.0f;
    float highCutHz = 7200.0f;
    bool isBypassed = false;
    juce::Colour accent { 0xffd97706 };
};

// Hero View 3: All 71 GP-200 Cabinet Models from Daftar_Effect_GP-200.xlsx (1x6" .. 8x10" + Acoustic + User IR)
// with dynamic speaker cone layout, ref/215612 CAB styling, Filter Curve & AmpliTube 64-frame PBR Knobs!
class CabRoomView : public juce::Component {
public:
    CabRoomView(PedalBoardAudioProcessor& proc,
                std::function<void(int)> onLoadIr,
                std::function<void(int)> onResetIr)
        : processor(proc),
          loadIrCb(std::move(onLoadIr)),
          resetIrCb(std::move(onResetIr)) {
        cabTabA.setButtonText("CAB A");
        cabTabA.onClick = [this] { selectRig(0); };
        addAndMakeVisible(cabTabA);

        cabTabB.setButtonText("CAB B");
        cabTabB.onClick = [this] { selectRig(1); };
        addAndMakeVisible(cabTabB);

        cabTabC.setButtonText("CAB C");
        cabTabC.onClick = [this] { selectRig(2); };
        addAndMakeVisible(cabTabC);

        modeFactoryBtn.setButtonText("FACTORY CAB");
        modeFactoryBtn.onClick = [this] { setUserIrMode(false); };
        addAndMakeVisible(modeFactoryBtn);

        modeUserIrBtn.setButtonText("USER IR");
        modeUserIrBtn.onClick = [this] { setUserIrMode(true); };
        addAndMakeVisible(modeUserIrBtn);

        // Populate ONLY the 70 built-in Factory Cabinets (0..69); User IR (70) has its own dedicated mode!
        const auto cabNames = gp200::getModelNameArray(ModuleId::CabinetIR);
        const int factoryCount = std::min(70, cabNames.size());
        for (int i = 0; i < factoryCount; ++i) {
            cabModelBox.addItem(cabNames[i], i + 1);
        }
        cabModelBox.onChange = [this] {
            if (!ignoreModelBoxCb && cabModelBox.getSelectedItemIndex() >= 0) {
                const int chosen = std::clamp(cabModelBox.getSelectedItemIndex(), 0, 69);
                lastFactoryCab[static_cast<size_t>(activeRig)] = chosen;
                setCabModelParam(chosen);
                refreshAll();
            }
        };
        addAndMakeVisible(cabModelBox);

        bypassBtn.setButtonText("ENGAGED");
        bypassBtn.setClickingTogglesState(true);
        bypassBtn.onStateChange = [this] { refreshAll(); };
        addAndMakeVisible(bypassBtn);

        loadIrBtn.setButtonText("LOAD .WAV / .AIFF...");
        loadIrBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
        loadIrBtn.onClick = [this] { if (loadIrCb) loadIrCb(activeRig); };
        addAndMakeVisible(loadIrBtn);

        cloudIrBtn.setButtonText("TONE3000");
        cloudIrBtn.setTooltip("Browse & load Cabinet IRs (.wav) from TONE3000 Cloud");
        cloudIrBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f766e));
        cloudIrBtn.onClick = [this] { if (browseCloudIrCb) browseCloudIrCb(activeRig); };
        addAndMakeVisible(cloudIrBtn);

        resetIrBtn.setButtonText("CLEAR");
        resetIrBtn.onClick = [this] {
            if (resetIrCb) resetIrCb(activeRig);
            refreshAll();
        };
        addAndMakeVisible(resetIrBtn);

        findPackIrBtn.setButtonText("FIND IR");
        findPackIrBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f766e));
        findPackIrBtn.onClick = [this] {
            const int m = getCurrentModelIndex();
            const auto& e = gp200::getModelEntry(ModuleId::CabinetIR, m);
            const auto info = ModelPackRegistry::getExcelPackInfo(e.excelNo);
            const juce::String q = (info.searchQuery != nullptr && info.searchQuery[0] != '\0')
                ? juce::String::fromUTF8(info.searchQuery)
                : juce::String::fromUTF8(e.description);
            const juce::String ext = (info.externalFreeUrl != nullptr) ? juce::String::fromUTF8(info.externalFreeUrl) : juce::String();
            if (findPackIrCb) findPackIrCb(activeRig, q, ext);
        };
        addAndMakeVisible(findPackIrBtn);

        addAndMakeVisible(filterCurve);

        const char* names[3] = { "LOW CUT", "HI CUT", "VOLUME" };
        const char* suffixes[3] = { " Hz", " Hz", " dB" };
        for (int i = 0; i < 3; ++i) {
            knobs[static_cast<size_t>(i)].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knobs[static_cast<size_t>(i)].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 15);
            knobs[static_cast<size_t>(i)].setTextValueSuffix(suffixes[i]);
            knobs[static_cast<size_t>(i)].getProperties().set("knobStyle", "chrome_knurled");
            knobs[static_cast<size_t>(i)].getProperties().set("showTicks", true);
            addAndMakeVisible(knobs[static_cast<size_t>(i)]);

            knobLabels[static_cast<size_t>(i)].setText(names[i], juce::dontSendNotification);
            knobLabels[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
            knobLabels[static_cast<size_t>(i)].setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            knobLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
            addAndMakeVisible(knobLabels[static_cast<size_t>(i)]);
        }

        rebindRig(0);
    }

    void selectRig(int rigIndex) {
        const int maxRigs = activeRigCountForMode(processor.getRoutingMode());
        rigIndex = std::clamp(rigIndex, 0, maxRigs - 1);
        if (activeRig == rigIndex) return;
        rebindRig(rigIndex);
        refreshAll();
    }

    int getSelectedRig() const noexcept { return activeRig; }

    void setBrowseCloudIrCallback(std::function<void(int)> cb) {
        browseCloudIrCb = std::move(cb);
    }

    void setFindPackOnCloudCallback(std::function<void(int, const juce::String&, const juce::String&)> cb) {
        findPackIrCb = std::move(cb);
    }

    void updateLive() {
        syncRoutingMode(false);
        const int curM = getCurrentModelIndex();
        if (curM != lastObservedCabModel) {
            refreshAll();
            return;
        }
        const bool isUserIr = (curM >= 70);
        const auto accent = isUserIr
            ? juce::Colour(0xff06b6d4)
            : ((activeRig == 2) ? juce::Colour(0xffa855f7)
                                : ((activeRig == 1) ? juce::Colour(0xff38bdf8) : juce::Colour(0xff3b82f6)));
        filterCurve.setFilterParams(static_cast<float>(knobs[0].getValue()),
                                    static_cast<float>(knobs[1].getValue()),
                                    bypassBtn.getToggleState(),
                                    accent);
    }

    void refreshAll() {
        syncRoutingMode(true);
        const int m = getCurrentModelIndex();
        const bool isUserIr = (m >= 70);
        const bool modeSwitched = (lastObservedCabModel < 0) || ((lastObservedCabModel >= 70) != isUserIr);
        lastObservedCabModel = m;

        if (!isUserIr) {
            lastFactoryCab[static_cast<size_t>(activeRig)] = m;
            ignoreModelBoxCb = true;
            if (cabModelBox.getSelectedItemIndex() != m) {
                cabModelBox.setSelectedItemIndex(m, juce::dontSendNotification);
            }
            ignoreModelBoxCb = false;
        }

        if (modeSwitched) {
            resized();
        }

        cabTabA.setColour(juce::TextButton::buttonColourId, activeRig == 0 ? valeton::pillOnBlue() : juce::Colour(0xff232730));
        cabTabB.setColour(juce::TextButton::buttonColourId, activeRig == 1 ? juce::Colour(0xff0284c7) : juce::Colour(0xff232730));
        cabTabC.setColour(juce::TextButton::buttonColourId, activeRig == 2 ? juce::Colour(0xff7c3aed) : juce::Colour(0xff232730));

        const bool bypassed = bypassBtn.getToggleState();
        const bool active = !bypassed;
        bypassBtn.setButtonText(bypassed ? "BYPASSED" : "ENGAGED");
        bypassBtn.getProperties().set("bypassed", bypassed);
        bypassBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15803d));
        bypassBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff232834));
        bypassBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff8c96a6));

        modeFactoryBtn.getProperties().set("bypassed", bypassed);
        modeFactoryBtn.setColour(juce::TextButton::buttonColourId,
                                 !isUserIr ? (active ? valeton::pillOnBlue() : juce::Colour(0xff2c3240)) : juce::Colour(0xff191e29));
        modeFactoryBtn.setColour(juce::TextButton::textColourOffId,
                                 (!isUserIr && active) ? juce::Colours::white : juce::Colour(0xff8491a6));
        modeFactoryBtn.repaint();

        modeUserIrBtn.getProperties().set("bypassed", bypassed);
        modeUserIrBtn.setColour(juce::TextButton::buttonColourId,
                                isUserIr ? (active ? juce::Colour(0xff0891b2) : juce::Colour(0xff2c3240)) : juce::Colour(0xff191e29));
        modeUserIrBtn.setColour(juce::TextButton::textColourOffId,
                                (isUserIr && active) ? juce::Colours::white : juce::Colour(0xff8491a6));
        modeUserIrBtn.repaint();

        loadIrBtn.getProperties().set("bypassed", bypassed);
        loadIrBtn.setColour(juce::TextButton::buttonColourId,
                            active ? juce::Colour(0xff0284c7) : juce::Colour(0xff262c38));
        loadIrBtn.setColour(juce::TextButton::textColourOffId,
                            active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
        loadIrBtn.repaint();

        cloudIrBtn.getProperties().set("bypassed", bypassed);
        cloudIrBtn.setColour(juce::TextButton::buttonColourId,
                             active ? juce::Colour(0xff0f766e) : juce::Colour(0xff232934));
        cloudIrBtn.setColour(juce::TextButton::textColourOffId,
                             active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
        cloudIrBtn.repaint();

        if (!isUserIr) {
            const auto& curEntry = gp200::getModelEntry(ModuleId::CabinetIR, m);
            const auto packHint = ModelPackRegistry::getExpectedFilenameHint(ModuleId::CabinetIR, m);
            const auto packInfo = ModelPackRegistry::getExcelPackInfo(curEntry.excelNo);
            const bool isExt = (packInfo.externalFreeUrl != nullptr && packInfo.externalFreeUrl[0] != '\0');
            findPackIrBtn.setTooltip(
                (isExt ? ("Open free IR source (" + juce::String::fromUTF8(packInfo.externalFreeUrl) + ")")
                       : ("Find real .wav IR on TONE3000 (\"" + juce::String::fromUTF8(packInfo.searchQuery) + "\")"))
                + ". Save as " + packHint + " in ModelPacks to auto-bind!"
            );
        }
        findPackIrBtn.getProperties().set("bypassed", bypassed);
        findPackIrBtn.setColour(juce::TextButton::buttonColourId,
                                active ? juce::Colour(0xff0f766e) : juce::Colour(0xff232934));
        findPackIrBtn.setColour(juce::TextButton::textColourOffId,
                                active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
        findPackIrBtn.repaint();

        const bool hasIr = processor.getSignalChain().getCabinet(activeRig).hasCustomIrFile();
        resetIrBtn.setEnabled(hasIr);
        resetIrBtn.setAlpha(hasIr ? 1.0f : 0.45f);
        resetIrBtn.repaint();

        for (int i = 0; i < 3; ++i) {
            knobs[static_cast<size_t>(i)].getProperties().set("bypassed", bypassed);
            knobs[static_cast<size_t>(i)].setColour(juce::Slider::textBoxTextColourId,
                                                    active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
            knobLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId,
                                                         active ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff5a6373));
            knobs[static_cast<size_t>(i)].repaint();
        }

        updateLive();
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        valeton::drawCleanStageBackdrop(g, bounds);

        const int m = getCurrentModelIndex();
        const bool isUserIr = (m >= 70);
        const auto& cabRef = processor.getSignalChain().getCabinet(activeRig);
        const bool hasCustomIr = cabRef.hasCustomIrFile();
        const auto customIrName = cabRef.getCustomIrName();
        const auto& entry = gp200::getModelEntry(ModuleId::CabinetIR, m);
        const juce::String sub(entry.subType);
        const bool active = !bypassBtn.getToggleState();
        const auto accent = isUserIr
            ? (hasCustomIr ? juce::Colour(0xff06b6d4) : juce::Colour(0xff475569))
            : juce::Colour(entry.chassisArgb);

        // Determine speaker cone grid from GP-200 cabinet type (1x12 studio baffle for User IR)
        int cols = 2, rows = 2;
        if (isUserIr || sub.containsIgnoreCase("1 x ") || sub.containsIgnoreCase("Acoustic")) {
            cols = 1; rows = 1;
        } else if (sub.containsIgnoreCase("2 x ")) {
            cols = 2; rows = 1;
        } else if (sub.containsIgnoreCase("8 x 10")) {
            cols = 2; rows = 4;
        }

        auto leftStage = leftStageRect.isEmpty()
                             ? bounds.withWidth(bounds.getWidth() * 0.48f).reduced(18.0f, 14.0f)
                             : leftStageRect.toFloat().reduced(8.0f, 4.0f);
        const float cabH = (rows == 1 && cols == 2) ? std::min(265.0f, leftStage.getHeight() * 0.72f)
                                                    : std::min(345.0f, leftStage.getHeight() * 0.92f);
        const float cabW = (rows == 1 && cols == 2) ? std::min(cabH * 1.48f, leftStage.getWidth())
                                                    : std::min(cabH * 1.02f, leftStage.getWidth());
        auto cabRect = juce::Rectangle<float>(leftStage.getCentreX() - cabW * 0.5f,
                                              leftStage.getCentreY() - cabH * 0.5f,
                                              cabW, cabH);

        g.setColour(juce::Colours::black.withAlpha(0.50f));
        g.fillRoundedRectangle(cabRect.translated(0.0f, 5.0f), 10.0f);
        g.setColour(juce::Colour(active ? 0xff1c1f27 : 0xff14161c));
        g.fillRoundedRectangle(cabRect, 10.0f);
        g.setColour(juce::Colour(0xff343a48));
        g.drawRoundedRectangle(cabRect, 10.0f, 1.2f);

        // 4 Metallic Silver Corner Brackets (exact ref/215612 CAB silhouette!)
        const auto cornerCol = juce::Colour(active ? 0xffd1d5db : 0xff525a6a);
        const float cs = 16.0f;
        g.setColour(cornerCol);
        g.fillRoundedRectangle(cabRect.getX(), cabRect.getY(), cs, cs, 3.0f);
        g.fillRoundedRectangle(cabRect.getRight() - cs, cabRect.getY(), cs, cs, 3.0f);
        g.fillRoundedRectangle(cabRect.getX(), cabRect.getBottom() - cs, cs, cs, 3.0f);
        g.fillRoundedRectangle(cabRect.getRight() - cs, cabRect.getBottom() - cs, cs, cs, 3.0f);

        // Crisp Inner Piping Frame (zero color highlight when cab is bypassed!)
        auto baffle = cabRect.reduced(16.0f, 16.0f);
        g.setColour(juce::Colour(0xff101318));
        g.fillRoundedRectangle(baffle, 5.0f);
        g.setColour(active ? accent.withAlpha(0.90f) : juce::Colour(0xff2a303d));
        g.drawRoundedRectangle(baffle, 5.0f, active ? 2.2f : 1.2f);

        const float cellW = baffle.getWidth() / static_cast<float>(cols);
        const float cellH = baffle.getHeight() / static_cast<float>(rows);
        const float coneR = std::min(cellW, cellH) * 0.40f;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const float cx = baffle.getX() + (static_cast<float>(c) + 0.5f) * cellW;
                const float cy = baffle.getY() + (static_cast<float>(r) + 0.5f) * cellH;

                g.setColour(juce::Colour(0xff1b202b));
                g.fillEllipse(cx - coneR, cy - coneR, coneR * 2.0f, coneR * 2.0f);
                g.setColour(juce::Colour(0xff343c4d));
                g.drawEllipse(cx - coneR, cy - coneR, coneR * 2.0f, coneR * 2.0f, 1.5f);

                g.setColour(active ? accent.withAlpha(0.24f) : juce::Colour(0xff252b38));
                g.drawEllipse(cx - coneR * 0.74f, cy - coneR * 0.74f, coneR * 1.48f, coneR * 1.48f, 1.2f);
                g.drawEllipse(cx - coneR * 0.52f, cy - coneR * 0.52f, coneR * 1.04f, coneR * 1.04f, 1.0f);

                const float capR = coneR * 0.30f;
                juce::ColourGradient dome(juce::Colour(0xff384050), cx - capR * 0.4f, cy - capR * 0.4f,
                                          juce::Colour(0xff0d1016), cx + capR * 0.8f, cy + capR * 0.8f, true);
                g.setGradientFill(dome);
                g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
            }
        }

        // Top nameplate inside baffle showing active Factory Cab or User IR status
        const float badgeW = std::min(210.0f, std::max(90.0f, baffle.getWidth() - 12.0f));
        auto cabBadge = juce::Rectangle<float>(baffle.getCentreX() - badgeW * 0.5f, baffle.getY() + 8.0f, badgeW, 20.0f);
        g.setColour(juce::Colour(0xff161a22).withAlpha(0.92f));
        g.fillRoundedRectangle(cabBadge, 3.5f);
        g.setColour(active ? accent : juce::Colour(0xff64748b));
        g.setFont(juce::Font(juce::FontOptions(badgeW < 140.0f ? 8.8f : 10.0f, juce::Font::bold)));
        const juce::String badgeStr = isUserIr
            ? (juce::String("USER IR  |  ") + (hasCustomIr ? customIrName : juce::String("EMPTY")))
            : (juce::String::fromUTF8(entry.name) + "  |  " + sub);
        g.drawText(badgeStr, cabBadge, juce::Justification::centred, true);

        // Right Control Panel Chassis
        auto rightPanel = controlPanelRect.toFloat();
        valeton::drawCleanChassis(g, rightPanel, accent, active, 10.0f, 0.0f);

        auto irLcd = irDisplayRect.toFloat();
        g.setColour(juce::Colour(0xff0c1017));
        g.fillRoundedRectangle(irLcd, 5.0f);
        g.setColour(active ? accent.withAlpha(0.70f) : juce::Colour(0xff2a303d));
        g.drawRoundedRectangle(irLcd, 5.0f, 1.0f);

        if (!isUserIr) {
            const bool hasPackIr = cabRef.isUsingPackIr();
            g.setColour(active ? (hasPackIr ? juce::Colour(0xff22d3ee) : juce::Colour(0xff60a5fa)) : juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
            g.drawText(hasPackIr
                           ? (juce::String("REAL CAB IR PACK ACTIVE  |  ") + juce::String::fromUTF8(entry.subType))
                           : (juce::String("FACTORY CABINET  |  ") + juce::String::fromUTF8(entry.subType)),
                       irLcd.reduced(8.0f, 3.0f).withHeight(13.0f), juce::Justification::centredLeft, true);

            g.setColour(active ? juce::Colours::white : juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(irLcd.getHeight() < 40.0f ? 10.5f : 11.8f, juce::Font::bold)));
            const juce::String cabTitle = hasPackIr
                ? (juce::String::fromUTF8(entry.name) + "   —   " + cabRef.getPackIrName())
                : (juce::String::fromUTF8(entry.name) + "   —   " + juce::String::fromUTF8(entry.description));
            g.drawText(cabTitle,
                       irLcd.reduced(8.0f, 3.0f).withTop(irLcd.getY() + 16.0f), juce::Justification::centredLeft, true);
        } else {
            g.setColour((active && hasCustomIr) ? juce::Colour(0xff38bdf8) : juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
            g.drawText(hasCustomIr ? "CUSTOM CONVOLUTION IR ACTIVE" : "USER IMPULSE RESPONSE (.WAV / .AIFF)",
                       irLcd.reduced(8.0f, 3.0f).withHeight(13.0f), juce::Justification::centredLeft, true);

            g.setColour(active ? (hasCustomIr ? juce::Colours::white : juce::Colour(0xff94a3b8))
                               : juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(irLcd.getHeight() < 40.0f ? 10.5f : 12.2f, juce::Font::bold)));
            g.drawText(hasCustomIr ? customIrName : "No User IR Loaded",
                       irLcd.reduced(8.0f, 3.0f).withTop(irLcd.getY() + 16.0f), juce::Justification::centredLeft, true);
        }
    }

    void resized() override {
        const bool compactH = (getHeight() < 390);
        const bool compactW = (getWidth() < 920);
        auto area = getLocalBounds().reduced(compactW ? 10 : 16, compactH ? 10 : 16);

        const int leftPct = compactW ? 43 : 49;
        leftStageRect = area.removeFromLeft(area.getWidth() * leftPct / 100);
        area.removeFromLeft(compactW ? 8 : 10);
        controlPanelRect = area;

        auto inner = controlPanelRect.reduced(compactW ? 10 : 14, compactH ? 8 : 12);
        const int rowH = compactH ? 23 : 26;
        const int vGap = compactH ? 6 : 9;

        const int rigCount = activeRigCountForMode(processor.getRoutingMode());
        const bool isUserIr = (getCurrentModelIndex() >= 70);

        cabTabA.setButtonText("CAB A");
        cabTabB.setButtonText("CAB B");
        cabTabC.setButtonText("CAB C");
        cabTabA.setVisible(rigCount >= 2);
        cabTabB.setVisible(rigCount >= 2);
        cabTabC.setVisible(rigCount >= 3);

        // Row 1: Rig tabs (if parallel) + Dedicated Source Switch [FACTORY CAB | USER IR] + ENGAGED/BYPASSED
        const bool singleTopRow = (rigCount == 1) || (inner.getWidth() >= (rigCount == 3 ? 470 : 410));
        if (singleTopRow) {
            auto topRow = inner.removeFromTop(rowH);
            if (rigCount >= 2) {
                const int tabW = (inner.getWidth() < 490) ? 54 : 62;
                cabTabA.setBounds(topRow.removeFromLeft(tabW));
                topRow.removeFromLeft(4);
                cabTabB.setBounds(topRow.removeFromLeft(tabW));
                if (rigCount >= 3) {
                    topRow.removeFromLeft(4);
                    cabTabC.setBounds(topRow.removeFromLeft(tabW));
                }
                topRow.removeFromLeft(8);
            }
            bypassBtn.setBounds(topRow.removeFromRight(82));
            topRow.removeFromRight(6);

            const int modeW = std::min(108, (topRow.getWidth() - 4) / 2);
            modeFactoryBtn.setButtonText(modeW < 88 ? "FACTORY" : "FACTORY CAB");
            modeUserIrBtn.setButtonText("USER IR");
            modeFactoryBtn.setBounds(topRow.removeFromLeft(modeW));
            topRow.removeFromLeft(4);
            modeUserIrBtn.setBounds(topRow.removeFromLeft(modeW));
        } else {
            auto row1 = inner.removeFromTop(rowH);
            const int tabGaps = (rigCount == 3) ? 14 : 10;
            const int tabW = (row1.getWidth() - 82 - tabGaps) / rigCount;
            cabTabA.setBounds(row1.removeFromLeft(tabW));
            row1.removeFromLeft(4);
            cabTabB.setBounds(row1.removeFromLeft(tabW));
            if (rigCount >= 3) {
                row1.removeFromLeft(4);
                cabTabC.setBounds(row1.removeFromLeft(tabW));
            }
            row1.removeFromLeft(6);
            bypassBtn.setBounds(row1);

            inner.removeFromTop(5);
            auto row1b = inner.removeFromTop(rowH);
            const int halfW = (row1b.getWidth() - 4) / 2;
            modeFactoryBtn.setButtonText("FACTORY CAB");
            modeUserIrBtn.setButtonText("USER IR");
            modeFactoryBtn.setBounds(row1b.removeFromLeft(halfW));
            row1b.removeFromLeft(4);
            modeUserIrBtn.setBounds(row1b);
        }

        // Mode-specific section: FACTORY CAB shows clean Model Dropdown + Find IR + Spec Card; USER IR shows OLED Card + Load/Cloud/Clear buttons!
        cabModelBox.setVisible(!isUserIr);
        findPackIrBtn.setVisible(!isUserIr);
        loadIrBtn.setVisible(isUserIr);
        cloudIrBtn.setVisible(isUserIr);
        resetIrBtn.setVisible(isUserIr);

        inner.removeFromTop(vGap);
        if (!isUserIr) {
            auto modelRow = inner.removeFromTop(rowH);
            const int findW = (modelRow.getWidth() < 320) ? 58 : 74;
            findPackIrBtn.setButtonText(findW < 68 ? "IR" : "FIND IR");
            findPackIrBtn.setBounds(modelRow.removeFromRight(findW));
            modelRow.removeFromRight(5);
            cabModelBox.setBounds(modelRow);
            inner.removeFromTop(compactH ? 5 : 7);
            irDisplayRect = inner.removeFromTop(compactH ? 38 : 44);
        } else {
            irDisplayRect = inner.removeFromTop(compactH ? 38 : 44);
            inner.removeFromTop(compactH ? 5 : 7);
            auto irBtnRow = inner.removeFromTop(rowH);
            const int resetW = (irBtnRow.getWidth() < 340) ? 54 : 66;
            const int cloudW = (irBtnRow.getWidth() < 340) ? 74 : 92;
            resetIrBtn.setBounds(irBtnRow.removeFromRight(resetW));
            irBtnRow.removeFromRight(5);
            cloudIrBtn.setButtonText(cloudW < 82 ? "T3K IR" : "TONE3000");
            cloudIrBtn.setBounds(irBtnRow.removeFromRight(cloudW));
            irBtnRow.removeFromRight(5);
            loadIrBtn.setButtonText(irBtnRow.getWidth() < 165 ? "LOAD .WAV..." : "LOAD USER IR (.WAV)...");
            loadIrBtn.setBounds(irBtnRow);
        }

        inner.removeFromTop(vGap);

        // Reserve guaranteed room for the 3 bottom knobs, give the rest to filter curve
        const int knobsH = std::clamp(inner.getHeight() * 44 / 100, 84, 134);
        auto knobsArea = inner.removeFromBottom(knobsH);
        inner.removeFromBottom(compactH ? 5 : 8);
        filterCurve.setBounds(inner);

        const int kw = knobsArea.getWidth() / 3;
        const int tbW = std::clamp(kw - 6, 48, 68);
        const int tbH = compactH ? 13 : 15;
        for (int i = 0; i < 3; ++i) {
            auto cell = knobsArea.removeFromLeft(kw).reduced(compactW ? 3 : 6, 1);
            knobLabels[static_cast<size_t>(i)].setBounds(cell.removeFromTop(14));
            knobs[static_cast<size_t>(i)].setTextBoxStyle(juce::Slider::TextBoxBelow, false, tbW, tbH);
            knobs[static_cast<size_t>(i)].setBounds(cell);
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

    void setCabModelParam(int modelIdx) {
        modelIdx = std::clamp(modelIdx, 0, 70);
        const char* mid = (activeRig == 2) ? params::kCabCModel : ((activeRig == 1) ? params::kCabBModel : params::kCabModel);
        if (auto* param = processor.getAPVTS().getParameter(mid)) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(modelIdx)));
            param->endChangeGesture();
        }
    }

    void setUserIrMode(bool enableUserIr) {
        if (enableUserIr) {
            const int cur = getCurrentModelIndex();
            if (cur < 70) {
                lastFactoryCab[static_cast<size_t>(activeRig)] = cur;
            }
            processor.activateSavedCabinetIR(activeRig);
            setCabModelParam(70);
        } else {
            const int target = std::clamp(lastFactoryCab[static_cast<size_t>(activeRig)], 0, 69);
            setCabModelParam(target);
        }
        refreshAll();
    }

    int getCurrentModelIndex() const noexcept {
        const char* mid = (activeRig == 2) ? params::kCabCModel : ((activeRig == 1) ? params::kCabBModel : params::kCabModel);
        if (auto* raw = processor.getAPVTS().getRawParameterValue(mid)) {
            return std::clamp(static_cast<int>(std::lround(raw->load(std::memory_order_relaxed))), 0, 70);
        }
        return 7;
    }

    void rebindRig(int rigIndex) {
        activeRig = std::clamp(rigIndex, 0, 2);
        const auto accent = (activeRig == 2) ? juce::Colour(0xffa855f7)
                                             : ((activeRig == 1) ? juce::Colour(0xff38bdf8) : juce::Colour(0xff3b82f6));

        bypassAtt.reset();
        for (auto& a : knobAtts) a.reset();

        const char* bypassId = (activeRig == 2) ? params::kCabCBypass : ((activeRig == 1) ? params::kCabBBypass : params::kCabBypass);
        bypassBtn.configureMidiLearn(&processor.getMidiLearnManager(), bypassId);
        bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.getAPVTS(), bypassId, bypassBtn
        );

        const char* pIds[3] = {
            (activeRig == 2) ? params::kCabCLowCut  : ((activeRig == 1) ? params::kCabBLowCut  : params::kCabLowCut),
            (activeRig == 2) ? params::kCabCHighCut : ((activeRig == 1) ? params::kCabBHighCut : params::kCabHighCut),
            (activeRig == 2) ? params::kCabCLevel   : ((activeRig == 1) ? params::kCabBLevel   : params::kCabLevel)
        };

        for (int i = 0; i < 3; ++i) {
            auto& s = knobs[static_cast<size_t>(i)];
            s.setColour(juce::Slider::rotarySliderFillColourId, accent);
            s.configureMidiLearn(&processor.getMidiLearnManager(), pIds[i]);
            if (auto* p = processor.getAPVTS().getParameter(pIds[i])) {
                s.setDoubleClickReturnValue(true, static_cast<double>(p->convertFrom0to1(p->getDefaultValue())));
            }
            knobAtts[static_cast<size_t>(i)] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), pIds[i], s
            );
        }

        lastObservedCabModel = -1;
        refreshAll();
    }

    PedalBoardAudioProcessor& processor;
    std::function<void(int)> loadIrCb;
    std::function<void(int)> resetIrCb;
    std::function<void(int)> browseCloudIrCb;
    std::function<void(int, const juce::String&, const juce::String&)> findPackIrCb;
    int activeRig = 0;
    RoutingMode lastRoutingMode = RoutingMode::Serial;
    std::array<int, 3> lastFactoryCab { 7, 7, 7 };
    int lastObservedCabModel = -1;
    bool ignoreModelBoxCb = false;

    juce::Rectangle<int> leftStageRect;
    juce::Rectangle<int> controlPanelRect;
    juce::Rectangle<int> irDisplayRect;

    juce::TextButton cabTabA, cabTabB, cabTabC;
    juce::TextButton modeFactoryBtn, modeUserIrBtn;
    juce::ComboBox cabModelBox;
    MidiBindableButton bypassBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    juce::TextButton loadIrBtn, cloudIrBtn, resetIrBtn, findPackIrBtn;
    CabFilterCurveDisplay filterCurve;

    std::array<MidiBindableSlider, 3> knobs;
    std::array<juce::Label, 3> knobLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 3> knobAtts;
};

} // namespace pedalboard
