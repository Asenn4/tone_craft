#pragma once

#include "../engine/ModuleId.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "MidiBindableControl.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard {

// Interactive Signal Flow Strip + Routing Mode selector (Serial / Dual Parallel A/B) + Parallel Mixer.
// Accepts Drag-and-Drop directly onto PRE / PATH A / PATH B / POST lanes and reflects independent Amp B + Cab B on Path B.
class RoutingAndMixerBar : public juce::Component,
                           public juce::DragAndDropTarget {
public:
    explicit RoutingAndMixerBar(PedalBoardAudioProcessor& proc) : processor(proc) {
        routingBox.addItem("Serial Chain", 1);
        routingBox.addItem("Dual Parallel A/B", 2);
        addAndMakeVisible(routingBox);
        routingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.getAPVTS(), params::kRoutingMode, routingBox
        );

        configureSmallKnob(pathALvl, "Path A Level (dB)", params::kPathALevel, juce::Colour(0xfff59e0b), 0.0, attALvl);
        configureSmallKnob(pathAPan, "Path A Pan",        params::kPathAPan,   juce::Colour(0xfff59e0b), 0.0, attAPan);
        configureSmallToggle(pathAMute, "MUTE A", params::kPathAMute, juce::Colour(0xffdc2626), attAMute);

        configureSmallKnob(pathBLvl, "Path B Level (dB)", params::kPathBLevel, juce::Colour(0xff38bdf8), -4.0, attBLvl);
        configureSmallKnob(pathBPan, "Path B Pan",        params::kPathBPan,   juce::Colour(0xff38bdf8), 0.0,  attBPan);
        configureSmallToggle(pathBMute, "MUTE B", params::kPathBMute, juce::Colour(0xffdc2626), attBMute);
        configureSmallToggle(pathBPhase, "PHASE", params::kPathBPhase, juce::Colour(0xff8b5cf6), attBPhase);
    }

    static juce::Colour accentFor(ModuleId id) noexcept {
        switch (id) {
            case ModuleId::NoiseGate:    return juce::Colour(0xff14b8a6);
            case ModuleId::Compressor:   return juce::Colour(0xff06b6d4);
            case ModuleId::Wah:          return juce::Colour(0xffec4899);
            case ModuleId::Overdrive:    return juce::Colour(0xff22c55e);
            case ModuleId::AmpHead:      return juce::Colour(0xfff59e0b);
            case ModuleId::CabinetIR:    return juce::Colour(0xffea580c);
            case ModuleId::GraphicEq:    return juce::Colour(0xffeab308);
            case ModuleId::StereoChorus: return juce::Colour(0xff6366f1);
            case ModuleId::StereoDelay:  return juce::Colour(0xff3b82f6);
            case ModuleId::Reverb:       return juce::Colour(0xffa855f7);
            default:                     return juce::Colour(0xff94a3b8);
        }
    }

    static juce::Colour laneColour(BusLane lane) noexcept {
        switch (lane) {
            case BusLane::Pre:   return juce::Colour(0xff14b8a6);
            case BusLane::PathA: return juce::Colour(0xfff59e0b);
            case BusLane::PathB: return juce::Colour(0xff38bdf8);
            case BusLane::Post:  return juce::Colour(0xffa855f7);
            default:             return juce::Colour(0xff64748b);
        }
    }

    void updateMixerVisibility() {
        const bool parallel = (processor.getRoutingMode() == RoutingMode::DualParallel);
        pathALvl.setVisible(parallel);
        pathAPan.setVisible(parallel);
        pathAMute.setVisible(parallel);
        pathBLvl.setVisible(parallel);
        pathBPan.setVisible(parallel);
        pathBMute.setVisible(parallel);
        pathBPhase.setVisible(parallel);
        repaint();
    }

    bool isInterestedInDragSource(const SourceDetails& details) override {
        return processor.getRoutingMode() == RoutingMode::DualParallel
               && details.description.toString().startsWith("pedal:");
    }

    void itemDragMove(const SourceDetails& details) override {
        hoveredLane = hitTestLane(details.localPosition);
        repaint();
    }

    void itemDragExit(const SourceDetails&) override {
        hoveredLane = -1;
        repaint();
    }

    void itemDropped(const SourceDetails& details) override {
        const int laneIdx = hitTestLane(details.localPosition);
        hoveredLane = -1;
        const auto desc = details.description.toString();
        if (laneIdx >= 0 && desc.startsWith("pedal:")) {
            ModuleId modId {};
            if (moduleFromKey(desc.fromFirstOccurrenceOf("pedal:", false, false), modId)) {
                processor.setModuleBusLane(modId, static_cast<BusLane>(laneIdx));
            }
        }
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0f131a));
        g.fillRect(bounds);
        g.setColour(juce::Colour(0xff252c3a));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

        const bool parallel = (processor.getRoutingMode() == RoutingMode::DualParallel);
        auto flowArea = flowRect.toFloat();

        if (!parallel) {
            drawSerialFlow(g, flowArea);
        } else {
            drawParallelFlow(g, flowArea);
        }
    }

    void resized() override {
        auto area = getLocalBounds().reduced(10, 4);
        routingBox.setBounds(area.removeFromLeft(142).reduced(0, 6));
        area.removeFromLeft(8);

        auto mixerArea = area.removeFromRight(368);
        pathALvl.setBounds(mixerArea.removeFromLeft(50));
        pathAPan.setBounds(mixerArea.removeFromLeft(50));
        pathAMute.setBounds(mixerArea.removeFromLeft(50).reduced(2, 7));
        mixerArea.removeFromLeft(8);
        pathBLvl.setBounds(mixerArea.removeFromLeft(50));
        pathBPan.setBounds(mixerArea.removeFromLeft(50));
        pathBMute.setBounds(mixerArea.removeFromLeft(50).reduced(2, 7));
        pathBPhase.setBounds(mixerArea.removeFromLeft(52).reduced(2, 7));

        area.removeFromRight(8);
        flowRect = area;
        computeLaneRects();
        updateMixerVisibility();
    }

private:
    void computeLaneRects() {
        auto r = flowRect;
        const int preW = r.getWidth() * 21 / 100;
        const int postW = r.getWidth() * 21 / 100;
        laneRects[0] = r.removeFromLeft(preW).reduced(2, 2);
        laneRects[3] = r.removeFromRight(postW).reduced(2, 2);
        auto mid = r.reduced(4, 1);
        const int halfH = mid.getHeight() / 2;
        laneRects[1] = mid.removeFromTop(halfH).reduced(0, 1);
        laneRects[2] = mid.reduced(0, 1);
    }

    int hitTestLane(juce::Point<int> pt) const noexcept {
        for (int i = 0; i < 4; ++i) {
            if (laneRects[static_cast<size_t>(i)].contains(pt)) {
                return i;
            }
        }
        return -1;
    }

    void drawSerialFlow(juce::Graphics& g, juce::Rectangle<float> area) const {
        const auto order = processor.getChainOrder();
        const float ioW = 34.0f;
        const float arrowW = 10.0f;
        const float pillW = (area.getWidth() - ioW * 2.0f - arrowW * static_cast<float>(kNumModules + 1))
                            / static_cast<float>(kNumModules);
        auto row = area.reduced(0.0f, 7.0f);

        auto drawPill = [&](juce::Rectangle<float> r, const juce::String& text, juce::Colour col, bool active) {
            g.setColour(active ? col.withAlpha(0.24f) : juce::Colour(0xff1a1f28));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(active ? col : juce::Colour(0xff333c4d));
            g.drawRoundedRectangle(r, 4.0f, 1.0f);
            g.setColour(active ? juce::Colours::white : juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText(text, r, juce::Justification::centred);
        };

        auto drawArrow = [&](juce::Rectangle<float> r) {
            g.setColour(juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(">", r, juce::Justification::centred);
        };

        drawPill(row.removeFromLeft(ioW), "IN", juce::Colour(0xff38bdf8), true);
        drawArrow(row.removeFromLeft(arrowW));

        for (int i = 0; i < kNumModules; ++i) {
            const auto mod = order[static_cast<size_t>(i)];
            drawPill(row.removeFromLeft(pillW), moduleShortTag(mod), accentFor(mod), !isModuleBypassed(mod));
            drawArrow(row.removeFromLeft(arrowW));
        }

        drawPill(row.removeFromLeft(ioW), "OUT", juce::Colour(0xff22c55e), true);
    }

    void drawParallelFlow(juce::Graphics& g, juce::Rectangle<float>) const {
        const auto order = processor.getChainOrder();
        const auto buses = processor.getBusAssignments();

        const char* labels[4] = { "PRE:", "PATH A:", "PATH B:", "POST:" };
        for (int laneIdx = 0; laneIdx < 4; ++laneIdx) {
            const auto lane = static_cast<BusLane>(laneIdx);
            const auto r = laneRects[static_cast<size_t>(laneIdx)].toFloat();
            const auto col = laneColour(lane);
            const bool isDropTarget = (hoveredLane == laneIdx);

            g.setColour(isDropTarget ? col.withAlpha(0.28f) : juce::Colour(0xff141923));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(isDropTarget ? col : col.withAlpha(0.55f));
            g.drawRoundedRectangle(r, 4.0f, isDropTarget ? 1.8f : 1.0f);

            juce::StringArray tags;
            if (lane == BusLane::PathB) {
                if (buses[static_cast<size_t>(ModuleId::AmpHead)] != BusLane::PathB) {
                    tags.add(isParamBypassed(params::kAmpBBypass) ? "(AMP B)" : "AMP B");
                }
                if (buses[static_cast<size_t>(ModuleId::CabinetIR)] != BusLane::PathB) {
                    tags.add(isParamBypassed(params::kCabBBypass) ? "(CAB B)" : "CAB B");
                }
            }
            for (const auto mod : order) {
                if (buses[static_cast<size_t>(mod)] == lane) {
                    juce::String tag = moduleShortTag(mod);
                    if (lane == BusLane::PathA && mod == ModuleId::AmpHead) tag = "AMP A";
                    if (lane == BusLane::PathA && mod == ModuleId::CabinetIR) tag = "CAB A";
                    if (lane == BusLane::PathB && mod == ModuleId::AmpHead) tag = "AMP B";
                    if (lane == BusLane::PathB && mod == ModuleId::CabinetIR) tag = "CAB B";
                    const bool byp = (lane == BusLane::PathB && mod == ModuleId::AmpHead)
                                         ? isParamBypassed(params::kAmpBBypass)
                                         : ((lane == BusLane::PathB && mod == ModuleId::CabinetIR)
                                                ? isParamBypassed(params::kCabBBypass)
                                                : isModuleBypassed(mod));
                    tags.add(byp ? ("(" + tag + ")") : tag);
                }
            }
            const juce::String summary = juce::String(labels[laneIdx]) + " "
                                       + (tags.isEmpty() ? "—" : tags.joinIntoString(" > "));
            g.setColour(juce::Colour(0xffe2e8f0));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(summary, r.reduced(5.0f, 0.0f), juce::Justification::centredLeft, true);
        }
    }

    bool isParamBypassed(const char* paramId) const noexcept {
        if (auto* raw = processor.getAPVTS().getRawParameterValue(paramId)) {
            return raw->load(std::memory_order_relaxed) > 0.5f;
        }
        return false;
    }

    bool isModuleBypassed(ModuleId id) const noexcept {
        switch (id) {
            case ModuleId::NoiseGate:    return isParamBypassed(params::kGateBypass);
            case ModuleId::Compressor:   return isParamBypassed(params::kCompBypass);
            case ModuleId::Wah:          return isParamBypassed(params::kWahBypass);
            case ModuleId::Overdrive:    return isParamBypassed(params::kOdBypass);
            case ModuleId::AmpHead:      return isParamBypassed(params::kAmpBypass);
            case ModuleId::CabinetIR:    return isParamBypassed(params::kCabBypass);
            case ModuleId::GraphicEq:    return isParamBypassed(params::kEqBypass);
            case ModuleId::StereoChorus: return isParamBypassed(params::kChorusBypass);
            case ModuleId::StereoDelay:  return isParamBypassed(params::kDelayBypass);
            case ModuleId::Reverb:       return isParamBypassed(params::kReverbBypass);
            default: return false;
        }
    }

    void configureSmallKnob(MidiBindableSlider& s,
                            const juce::String& tooltip,
                            const char* paramId,
                            juce::Colour accent,
                            double defaultVal,
                            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 11);
        s.setTooltip(tooltip);
        s.setColour(juce::Slider::rotarySliderFillColourId, accent);
        s.setDoubleClickReturnValue(true, defaultVal);
        s.configureMidiLearn(&processor.getMidiLearnManager(), paramId);
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getAPVTS(), paramId, s
        );
    }

    void configureSmallToggle(MidiBindableButton& b,
                              const juce::String& text,
                              const char* paramId,
                              juce::Colour onCol,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att) {
        b.setButtonText(text);
        b.setClickingTogglesState(true);
        b.setColour(juce::TextButton::buttonOnColourId, onCol);
        b.configureMidiLearn(&processor.getMidiLearnManager(), paramId);
        addAndMakeVisible(b);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.getAPVTS(), paramId, b
        );
    }

    PedalBoardAudioProcessor& processor;
    juce::ComboBox routingBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> routingAttachment;

    MidiBindableSlider pathALvl, pathAPan, pathBLvl, pathBPan;
    MidiBindableButton pathAMute, pathBMute, pathBPhase;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attALvl, attAPan, attBLvl, attBPan;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attAMute, attBMute, attBPhase;

    juce::Rectangle<int> flowRect;
    std::array<juce::Rectangle<int>, 4> laneRects;
    int hoveredLane = -1;
};

} // namespace pedalboard
