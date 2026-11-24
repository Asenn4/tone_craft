#pragma once

#include "../engine/ModuleId.h"
#include "../plugin/PluginProcessor.h"
#include "MidiBindableControl.h"
#include "RoutingAndMixerBar.h"

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace pedalboard {

struct KnobSpec {
    juce::String paramId;
    juce::String label;
    juce::String suffix;
    bool verticalFader = false;
};

struct RigBindConfig {
    juce::String bypassParamId;
    juce::String choiceParamId;
    std::vector<juce::String> knobParamIds;
};

// Hardware pedal / module card rendered 100% natively in JUCE.
// Supports Mouse Drag-and-Drop reordering, Bus Lane switching (PRE/A/B/POST),
// Dual Rig A/B tab switching (for Amp A/B and Cab A/B), right-click MIDI Learn,
// double-click reset to default, GR meter, NAM loader, and IR loader.
class PedalCard : public juce::Component,
                  public juce::DragAndDropTarget {
public:
    using MoveCallback = std::function<void(ModuleId, int)>;
    using DropReorderCallback = std::function<void(ModuleId, int)>;

    PedalCard(PedalBoardAudioProcessor& proc,
              ModuleId id,
              juce::Colour accentColour,
              const char* bypassParamId,
              std::vector<KnobSpec> knobSpecs,
              MoveCallback onMove,
              DropReorderCallback onDropReorder)
        : processor(proc),
          moduleId(id),
          accent(accentColour),
          bypassId(bypassParamId),
          dropReorderCb(std::move(onDropReorder)) {
        busBtn.setButtonText("A");
        busBtn.setTooltip("Assign bus lane in Dual Parallel mode (PRE / A / B / POST)");
        busBtn.onClick = [this] {
            processor.cycleModuleBusLane(moduleId);
            refreshDynamicLabels();
        };
        addAndMakeVisible(busBtn);

        moveLeftBtn.setButtonText("<");
        moveLeftBtn.setTooltip("Move earlier in signal chain (or drag pedal header)");
        moveLeftBtn.onClick = [this, onMove] {
            if (onMove) onMove(moduleId, -1);
        };
        addAndMakeVisible(moveLeftBtn);

        moveRightBtn.setButtonText(">");
        moveRightBtn.setTooltip("Move later in signal chain (or drag pedal header)");
        moveRightBtn.onClick = [this, onMove] {
            if (onMove) onMove(moduleId, +1);
        };
        addAndMakeVisible(moveRightBtn);

        bypassBtn.setClickingTogglesState(true);
        bypassBtn.setButtonText("ENGAGED");
        bypassBtn.onStateChange = [this] { updateBypassVisuals(); };
        addAndMakeVisible(bypassBtn);

        rigAConfig.bypassParamId = bypassId;
        for (const auto& spec : knobSpecs) {
            auto item = std::make_unique<KnobControl>();
            item->isVertical = spec.verticalFader;
            if (spec.verticalFader) {
                item->slider.setSliderStyle(juce::Slider::LinearVertical);
                item->slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                item->slider.setColour(juce::Slider::trackColourId, accent);
            } else {
                item->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                item->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 13);
                item->slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
                if (spec.suffix.isNotEmpty()) {
                    item->slider.setTextValueSuffix(" " + spec.suffix);
                }
            }
            addAndMakeVisible(item->slider);

            item->label.setText(spec.label, juce::dontSendNotification);
            item->label.setJustificationType(juce::Justification::centred);
            item->label.setFont(juce::Font(juce::FontOptions(spec.verticalFader ? 9.0f : 10.0f, juce::Font::bold)));
            item->label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
            addAndMakeVisible(item->label);

            rigAConfig.knobParamIds.push_back(spec.paramId);
            knobs.push_back(std::move(item));
        }

        rebindToRig(0);
    }

    void addChoiceControl(const char* paramId, const juce::StringArray& choices) {
        choiceBox = std::make_unique<juce::ComboBox>();
        for (int i = 0; i < choices.size(); ++i) {
            choiceBox->addItem(choices[i], i + 1);
        }
        addAndMakeVisible(*choiceBox);
        rigAConfig.choiceParamId = paramId;
        choiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.getAPVTS(), paramId, *choiceBox
        );
    }

    void enableDualRigTabs(const juce::String& labelA,
                           const juce::String& labelB,
                           RigBindConfig rigB) {
        hasDualRig = true;
        rigBConfig = std::move(rigB);

        rigTabA = std::make_unique<juce::TextButton>(labelA);
        rigTabA->onClick = [this] { selectRigTab(0); };
        addAndMakeVisible(*rigTabA);

        rigTabB = std::make_unique<juce::TextButton>(labelB);
        rigTabB->onClick = [this] { selectRigTab(1); };
        addAndMakeVisible(*rigTabB);

        updateRigTabVisuals();
    }

    int getSelectedRigIndex() const noexcept { return activeRigIndex; }

    void addToggleControl(const char* paramId, const juce::String& text) {
        extraToggle = std::make_unique<MidiBindableButton>(text);
        extraToggle->setClickingTogglesState(true);
        extraToggle->setColour(juce::TextButton::buttonOnColourId, accent);
        extraToggle->configureMidiLearn(&processor.getMidiLearnManager(), paramId);
        addAndMakeVisible(*extraToggle);
        extraToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.getAPVTS(), paramId, *extraToggle
        );
    }

    void enableGainReductionMeter() {
        showGrMeter = true;
    }

    void setGainReductionDb(float grDb) noexcept {
        if (std::abs(grDb - currentGrDb) > 0.1f) {
            currentGrDb = grDb;
            repaint();
        }
    }

    void addNamControls(std::function<void(int)> onLoadNam, std::function<void(int)> onClearNam) {
        fileStatusLabel = std::make_unique<juce::Label>();
        fileStatusLabel->setJustificationType(juce::Justification::centredLeft);
        fileStatusLabel->setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
        fileStatusLabel->setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
        fileStatusLabel->setColour(juce::Label::backgroundColourId, juce::Colour(0xff0d1117));
        addAndMakeVisible(*fileStatusLabel);

        fileLoadBtn = std::make_unique<juce::TextButton>("Load .nam");
        fileLoadBtn->setTooltip("Load Neural Amp Modeler (.nam) capture for active Rig A/B");
        fileLoadBtn->onClick = [this, cb = std::move(onLoadNam)] { if (cb) cb(activeRigIndex); };
        addAndMakeVisible(*fileLoadBtn);

        fileResetBtn = std::make_unique<juce::TextButton>("X");
        fileResetBtn->setTooltip("Clear loaded .nam model on active Rig A/B");
        fileResetBtn->onClick = [this, cb = std::move(onClearNam)] { if (cb) cb(activeRigIndex); };
        addAndMakeVisible(*fileResetBtn);

        isNamCard = true;
        refreshDynamicLabels();
    }

    void addCabinetIrControls(std::function<void(int)> onLoadIr, std::function<void(int)> onResetIr) {
        fileStatusLabel = std::make_unique<juce::Label>();
        fileStatusLabel->setJustificationType(juce::Justification::centredLeft);
        fileStatusLabel->setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
        fileStatusLabel->setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
        fileStatusLabel->setColour(juce::Label::backgroundColourId, juce::Colour(0xff0d1117));
        addAndMakeVisible(*fileStatusLabel);

        fileLoadBtn = std::make_unique<juce::TextButton>("Load IR");
        fileLoadBtn->setTooltip("Load custom .wav/.aiff IR for active Cab A/B");
        fileLoadBtn->onClick = [this, cb = std::move(onLoadIr)] { if (cb) cb(activeRigIndex); };
        addAndMakeVisible(*fileLoadBtn);

        fileResetBtn = std::make_unique<juce::TextButton>("Reset");
        fileResetBtn->setTooltip("Reset to built-in synthetic cab (4x12 for Cab A / 2x12 for Cab B)");
        fileResetBtn->onClick = [this, cb = std::move(onResetIr)] { if (cb) cb(activeRigIndex); };
        addAndMakeVisible(*fileResetBtn);

        isIrCard = true;
        refreshDynamicLabels();
    }

    void refreshDynamicLabels() {
        if (fileStatusLabel != nullptr) {
            if (isIrCard) {
                fileStatusLabel->setText(" " + processor.getSignalChain().getCabinet(activeRigIndex).getDisplayName(),
                                         juce::dontSendNotification);
            } else if (isNamCard) {
                fileStatusLabel->setText(" " + processor.getSignalChain().getAmpHead(activeRigIndex).getNamRunner().getDisplayName(),
                                         juce::dontSendNotification);
            }
        }

        const bool parallel = (processor.getRoutingMode() == RoutingMode::DualParallel);
        const auto lane = processor.getBusAssignments()[static_cast<size_t>(moduleId)];
        busBtn.setVisible(parallel);
        busBtn.setButtonText(busLaneShortLabel(lane));
        busBtn.setColour(juce::TextButton::buttonColourId, RoutingAndMixerBar::laneColour(lane).withMultipliedBrightness(0.65f));

        if (hasDualRig) {
            updateRigTabVisuals();
        }

        updateBypassVisuals();
        for (auto& k : knobs) {
            k->slider.repaint();
        }
        bypassBtn.repaint();
        if (extraToggle != nullptr) extraToggle->repaint();
    }

    void setOrderIndex(int index, int total) {
        slotIndex = index;
        moveLeftBtn.setEnabled(index > 0);
        moveRightBtn.setEnabled(index < total - 1);
        repaint();
    }

    ModuleId getModuleId() const noexcept { return moduleId; }
    bool isBypassed() const noexcept { return bypassBtn.getToggleState(); }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (e.getDistanceFromDragStart() > 7 && e.getMouseDownY() <= 38) {
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
                if (!container->isDragAndDropActive()) {
                    const auto snapshot = createComponentSnapshot(getLocalBounds()).rescaled(
                        std::max(120, getWidth() / 2), std::max(100, getHeight() / 2)
                    );
                    container->startDragging("pedal:" + juce::String(moduleKey(moduleId)), this,
                                             juce::ScaledImage(snapshot), true);
                }
            }
        }
    }

    bool isInterestedInDragSource(const SourceDetails& details) override {
        const auto desc = details.description.toString();
        return desc.startsWith("pedal:") && desc != ("pedal:" + juce::String(moduleKey(moduleId)));
    }

    void itemDragEnter(const SourceDetails&) override {
        dragHighlight = true;
        repaint();
    }

    void itemDragExit(const SourceDetails&) override {
        dragHighlight = false;
        repaint();
    }

    void itemDropped(const SourceDetails& details) override {
        dragHighlight = false;
        const auto desc = details.description.toString();
        if (desc.startsWith("pedal:")) {
            ModuleId draggedId {};
            if (moduleFromKey(desc.fromFirstOccurrenceOf("pedal:", false, false), draggedId) && dropReorderCb) {
                dropReorderCb(draggedId, slotIndex);
            }
        }
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat().reduced(2.5f);
        const bool active = !isBypassed();
        const auto effectiveAccent = (hasDualRig && activeRigIndex == 1) ? juce::Colour(0xff38bdf8) : accent;

        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 3.0f), 9.0f);

        const auto topCol = active ? effectiveAccent.withMultipliedSaturation(0.32f).withMultipliedBrightness(0.26f)
                                   : juce::Colour(0xff191c23);
        const auto botCol = juce::Colour(0xff12151c);
        juce::ColourGradient chassisGrad(topCol, 0.0f, bounds.getY(), botCol, 0.0f, bounds.getBottom(), false);
        g.setGradientFill(chassisGrad);
        g.fillRoundedRectangle(bounds, 9.0f);

        auto headerBar = bounds.withHeight(28.0f);
        g.setColour(active ? effectiveAccent.withAlpha(0.24f) : juce::Colour(0xff202530));
        g.fillRoundedRectangle(headerBar, 9.0f);
        g.fillRect(headerBar.withTop(headerBar.getCentreY()));

        if (dragHighlight) {
            g.setColour(juce::Colour(0xff38bdf8));
            g.drawRoundedRectangle(bounds, 9.0f, 2.8f);
        } else {
            g.setColour(active ? effectiveAccent.withAlpha(0.75f) : juce::Colour(0xff2d3444));
            g.drawRoundedRectangle(bounds, 9.0f, active ? 1.5f : 1.0f);
        }

        // Corner screws
        g.setColour(juce::Colours::white.withAlpha(0.13f));
        const float sr = 2.0f;
        g.fillEllipse(bounds.getX() + 6.0f, bounds.getY() + 6.0f, sr * 2.0f, sr * 2.0f);
        g.fillEllipse(bounds.getRight() - 6.0f - sr * 2.0f, bounds.getY() + 6.0f, sr * 2.0f, sr * 2.0f);
        g.fillEllipse(bounds.getX() + 6.0f, bounds.getBottom() - 6.0f - sr * 2.0f, sr * 2.0f, sr * 2.0f);
        g.fillEllipse(bounds.getRight() - 6.0f - sr * 2.0f, bounds.getBottom() - 6.0f - sr * 2.0f, sr * 2.0f, sr * 2.0f);

        const int rightReserve = busBtn.isVisible() ? 94 : 58;
        auto titleRect = juce::Rectangle<int>(static_cast<int>(bounds.getX()) + 12,
                                              static_cast<int>(bounds.getY()) + 4,
                                              static_cast<int>(bounds.getWidth()) - rightReserve,
                                              20);
        g.setColour(active ? juce::Colours::white : juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        g.drawText(juce::String(slotIndex + 1) + ". " + moduleDisplayName(moduleId),
                   titleRect, juce::Justification::centredLeft, true);

        if (showGrMeter) {
            auto grRect = juce::Rectangle<float>(bounds.getX() + 12.0f, bounds.getY() + 32.0f,
                                                 bounds.getWidth() - 24.0f, 9.0f);
            g.setColour(juce::Colour(0xff0b0e14));
            g.fillRoundedRectangle(grRect, 3.0f);
            const float normGr = std::clamp(currentGrDb / 24.0f, 0.0f, 1.0f);
            if (normGr > 0.01f) {
                g.setColour(juce::Colour(0xfff59e0b));
                g.fillRoundedRectangle(grRect.withWidth(grRect.getWidth() * normGr), 3.0f);
            }
            g.setColour(juce::Colour(0xff94a3b8));
            g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            g.drawText("GR -" + juce::String(currentGrDb, 1) + " dB", grRect, juce::Justification::centred);
        }

        const float ledCx = bounds.getX() + 19.0f;
        const float ledCy = bounds.getBottom() - 16.0f;
        if (active) {
            g.setColour(effectiveAccent.withAlpha(0.32f));
            g.fillEllipse(ledCx - 7.5f, ledCy - 7.5f, 15.0f, 15.0f);
            g.setColour(effectiveAccent.brighter(0.35f));
            g.fillEllipse(ledCx - 4.0f, ledCy - 4.0f, 8.0f, 8.0f);
        } else {
            g.setColour(juce::Colour(0xff2c3340));
            g.fillEllipse(ledCx - 4.0f, ledCy - 4.0f, 8.0f, 8.0f);
        }
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8, 5);

        auto header = area.removeFromTop(22);
        moveRightBtn.setBounds(header.removeFromRight(20).reduced(1));
        moveLeftBtn.setBounds(header.removeFromRight(20).reduced(1));
        header.removeFromRight(2);
        busBtn.setBounds(header.removeFromRight(36).reduced(1));

        if (showGrMeter) {
            area.removeFromTop(14);
        }

        auto footer = area.removeFromBottom(24);
        footer.removeFromLeft(22);
        bypassBtn.setBounds(footer.reduced(12, 1));

        area.removeFromTop(4);
        area.removeFromBottom(3);

        if (hasDualRig && rigTabA != nullptr && rigTabB != nullptr) {
            auto tabRow = area.removeFromTop(20).reduced(4, 0);
            const int halfW = tabRow.getWidth() / 2;
            rigTabA->setBounds(tabRow.removeFromLeft(halfW).reduced(1, 0));
            rigTabB->setBounds(tabRow.reduced(1, 0));
            area.removeFromTop(3);
        }

        if (choiceBox != nullptr) {
            choiceBox->setBounds(area.removeFromTop(20).reduced(4, 0));
            area.removeFromTop(3);
        }

        if (fileStatusLabel != nullptr && fileLoadBtn != nullptr && fileResetBtn != nullptr) {
            auto row = area.removeFromBottom(20).reduced(2, 0);
            fileResetBtn->setBounds(row.removeFromRight(isIrCard ? 40 : 22));
            row.removeFromRight(3);
            fileLoadBtn->setBounds(row.removeFromRight(isNamCard ? 60 : 52));
            row.removeFromRight(4);
            fileStatusLabel->setBounds(row);
            area.removeFromBottom(3);
        } else if (extraToggle != nullptr) {
            extraToggle->setBounds(area.removeFromBottom(20).reduced(18, 0));
            area.removeFromBottom(3);
        }

        const int count = static_cast<int>(knobs.size());
        if (count <= 0) {
            return;
        }

        if (knobs[0]->isVertical) {
            const int faderW = area.getWidth() / count;
            for (int i = 0; i < count; ++i) {
                auto col = juce::Rectangle<int>(area.getX() + i * faderW, area.getY(), faderW, area.getHeight()).reduced(1, 0);
                knobs[static_cast<size_t>(i)]->label.setBounds(col.removeFromBottom(13));
                knobs[static_cast<size_t>(i)]->slider.setBounds(col);
            }
            return;
        }

        const int cols = (count <= 3) ? count : (count == 4 ? 2 : 3);
        const int rows = (count + cols - 1) / cols;
        const int cellW = area.getWidth() / cols;
        const int cellH = area.getHeight() / rows;

        for (int i = 0; i < count; ++i) {
            const int r = i / cols;
            const int c = i % cols;
            auto cell = juce::Rectangle<int>(area.getX() + c * cellW,
                                             area.getY() + r * cellH,
                                             cellW,
                                             cellH).reduced(1, 1);
            knobs[static_cast<size_t>(i)]->label.setBounds(cell.removeFromTop(12));
            knobs[static_cast<size_t>(i)]->slider.setBounds(cell);
        }
    }

private:
    struct KnobControl {
        bool isVertical = false;
        MidiBindableSlider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void selectRigTab(int rigIndex) {
        if (!hasDualRig || activeRigIndex == rigIndex) {
            return;
        }
        rebindToRig(rigIndex);
        refreshDynamicLabels();
    }

    void rebindToRig(int rigIndex) {
        activeRigIndex = rigIndex;
        const auto& cfg = (rigIndex == 1 && hasDualRig) ? rigBConfig : rigAConfig;
        const auto effectiveAccent = (hasDualRig && rigIndex == 1) ? juce::Colour(0xff38bdf8) : accent;

        // Release old attachments before binding to new parameter IDs
        bypassAttachment.reset();
        choiceAttachment.reset();
        for (auto& k : knobs) {
            k->attachment.reset();
        }

        bypassBtn.configureMidiLearn(&processor.getMidiLearnManager(), cfg.bypassParamId);
        bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.getAPVTS(), cfg.bypassParamId, bypassBtn
        );

        if (choiceBox != nullptr && cfg.choiceParamId.isNotEmpty()) {
            choiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.getAPVTS(), cfg.choiceParamId, *choiceBox
            );
        }

        for (size_t i = 0; i < knobs.size() && i < cfg.knobParamIds.size(); ++i) {
            const auto& pid = cfg.knobParamIds[i];
            auto& k = knobs[i];
            if (k->isVertical) {
                k->slider.setColour(juce::Slider::trackColourId, effectiveAccent);
            } else {
                k->slider.setColour(juce::Slider::rotarySliderFillColourId, effectiveAccent);
            }
            k->slider.configureMidiLearn(&processor.getMidiLearnManager(), pid);
            if (auto* param = processor.getAPVTS().getParameter(pid)) {
                const double defVal = static_cast<double>(param->convertFrom0to1(param->getDefaultValue()));
                k->slider.setDoubleClickReturnValue(true, defVal);
            }
            k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.getAPVTS(), pid, k->slider
            );
        }

        updateRigTabVisuals();
        updateBypassVisuals();
    }

    void updateRigTabVisuals() {
        if (!hasDualRig || rigTabA == nullptr || rigTabB == nullptr) return;
        const bool isDual = (processor.getRoutingMode() == RoutingMode::DualParallel);
        rigTabA->setColour(juce::TextButton::buttonColourId,
                           activeRigIndex == 0 ? accent.withMultipliedBrightness(0.65f) : juce::Colour(0xff1e2430));
        rigTabB->setColour(juce::TextButton::buttonColourId,
                           activeRigIndex == 1 ? juce::Colour(0xff0284c7) : juce::Colour(0xff1e2430));
        rigTabB->setAlpha(isDual || activeRigIndex == 1 ? 1.0f : 0.65f);
    }

    void updateBypassVisuals() {
        const bool bypassed = bypassBtn.getToggleState();
        const auto effectiveAccent = (hasDualRig && activeRigIndex == 1) ? juce::Colour(0xff38bdf8) : accent;
        bypassBtn.setButtonText(bypassed ? "BYPASSED" : "ENGAGED");
        bypassBtn.setColour(juce::TextButton::buttonColourId, effectiveAccent.withMultipliedBrightness(0.52f));
        bypassBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff232732));
        repaint();
    }

    PedalBoardAudioProcessor& processor;
    ModuleId moduleId;
    juce::Colour accent;
    const char* bypassId;
    DropReorderCallback dropReorderCb;
    int slotIndex = 0;
    bool dragHighlight = false;
    bool showGrMeter = false;
    float currentGrDb = 0.0f;
    bool isIrCard = false;
    bool isNamCard = false;

    bool hasDualRig = false;
    int activeRigIndex = 0;
    RigBindConfig rigAConfig;
    RigBindConfig rigBConfig;
    std::unique_ptr<juce::TextButton> rigTabA;
    std::unique_ptr<juce::TextButton> rigTabB;

    juce::TextButton busBtn;
    juce::TextButton moveLeftBtn;
    juce::TextButton moveRightBtn;
    MidiBindableButton bypassBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    std::vector<std::unique_ptr<KnobControl>> knobs;

    std::unique_ptr<juce::ComboBox> choiceBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> choiceAttachment;

    std::unique_ptr<MidiBindableButton> extraToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> extraToggleAttachment;

    std::unique_ptr<juce::Label> fileStatusLabel;
    std::unique_ptr<juce::TextButton> fileLoadBtn;
    std::unique_ptr<juce::TextButton> fileResetBtn;
};

} // namespace pedalboard
