#pragma once

#include "../engine/MidiLearnManager.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard {

inline void showMidiLearnMenu(MidiLearnManager& mgr, const juce::String& paramId, juce::Component* target) {
    juce::PopupMenu menu;
    const int currentCc = mgr.getCcForParam(paramId);
    const bool isLearningThis = mgr.isLearnArmed() && mgr.getLearningParamId() == paramId;

    menu.addItem(1, isLearningThis ? "Cancel MIDI Learn" : "MIDI Learn (move CC controller)...");
    if (currentCc >= 0) {
        menu.addItem(2, "Clear MIDI CC (CC #" + juce::String(currentCc) + ")");
    }
    menu.addSeparator();
    menu.addItem(3, "Clear All MIDI CC Mappings");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
                       [&mgr, paramId, isLearningThis](int result) {
                           if (result == 1) {
                               if (isLearningThis) mgr.cancelLearn();
                               else mgr.armLearn(paramId);
                           } else if (result == 2) {
                               mgr.clearMappingForParam(paramId);
                           } else if (result == 3) {
                               mgr.clearAllMappings();
                           }
                       });
}

inline void paintMidiBadge(juce::Graphics& g,
                           juce::Rectangle<int> bounds,
                           const MidiLearnManager* mgr,
                           const juce::String& paramId) {
    if (mgr == nullptr || paramId.isEmpty()) {
        return;
    }

    const bool learningThis = mgr->isLearnArmed() && mgr->getLearningParamId() == paramId;
    const int cc = mgr->getCcForParam(paramId);
    if (!learningThis && cc < 0) {
        return;
    }

    const juce::String text = learningThis ? "LEARN" : ("CC" + juce::String(cc));
    auto pill = juce::Rectangle<float>(
        static_cast<float>(bounds.getRight() - 36),
        static_cast<float>(bounds.getY() + 1),
        35.0f,
        13.0f
    );
    g.setColour(learningThis ? juce::Colour(0xfff59e0b) : juce::Colour(0xff2563eb));
    g.fillRoundedRectangle(pill, 3.5f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.drawText(text, pill, juce::Justification::centred);
}

// Rotary / Linear slider with right-click MIDI Learn & visual CC badge.
class MidiBindableSlider : public juce::Slider {
public:
    void configureMidiLearn(MidiLearnManager* manager, const juce::String& parameterId) {
        midiMgr = manager;
        paramId = parameterId;
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (midiMgr != nullptr && paramId.isNotEmpty()) {
            if (e.mods.isPopupMenu()) {
                showMidiLearnMenu(*midiMgr, paramId, this);
                return;
            }
            if (midiMgr->isLearnArmed() && midiMgr->getLearningParamId().isEmpty()) {
                midiMgr->armLearn(paramId);
                repaint();
                return;
            }
        }
        juce::Slider::mouseDown(e);
    }

    void paintOverChildren(juce::Graphics& g) override {
        paintMidiBadge(g, getLocalBounds(), midiMgr, paramId);
    }

private:
    MidiLearnManager* midiMgr = nullptr;
    juce::String paramId;
};

// Button / Footswitch with right-click MIDI Learn & visual CC badge.
class MidiBindableButton : public juce::TextButton {
public:
    using juce::TextButton::TextButton;

    void configureMidiLearn(MidiLearnManager* manager, const juce::String& parameterId) {
        midiMgr = manager;
        paramId = parameterId;
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (midiMgr != nullptr && paramId.isNotEmpty()) {
            if (e.mods.isPopupMenu()) {
                showMidiLearnMenu(*midiMgr, paramId, this);
                return;
            }
            if (midiMgr->isLearnArmed() && midiMgr->getLearningParamId().isEmpty()) {
                midiMgr->armLearn(paramId);
                repaint();
                return;
            }
        }
        juce::TextButton::mouseDown(e);
    }

    void paintOverChildren(juce::Graphics& g) override {
        paintMidiBadge(g, getLocalBounds(), midiMgr, paramId);
    }

private:
    MidiLearnManager* midiMgr = nullptr;
    juce::String paramId;
};

} // namespace pedalboard
