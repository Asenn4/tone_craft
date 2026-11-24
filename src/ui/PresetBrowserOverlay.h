#pragma once

#include "../plugin/PluginProcessor.h"
#include "../presets/PresetManager.h"

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace pedalboard {

// AmpliTube-5-inspired native slide-in Preset Browser with live search, Style/Source tag filters,
// metadata badges (DUAL A+B, NAM, FACTORY/USER), 1-click audition/load, and quick tagged save.
class PresetBrowserOverlay : public juce::Component,
                             private juce::ListBoxModel {
public:
    PresetBrowserOverlay(PedalBoardAudioProcessor& proc, std::function<void()> onClose)
        : processor(proc),
          closeCb(std::move(onClose)) {
        searchBox.setTextToShowWhenEmpty("Search name, style, character...", juce::Colour(0xff64748b));
        searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0f131a));
        searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff334155));
        searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        searchBox.onTextChange = [this] { applyFilters(); };
        addAndMakeVisible(searchBox);

        closeBtn.setButtonText("CLOSE");
        closeBtn.onClick = [this] { if (closeCb) closeCb(); };
        addAndMakeVisible(closeBtn);

        refreshBtn.setButtonText("REFRESH");
        refreshBtn.onClick = [this] { reloadLibrary(); };
        addAndMakeVisible(refreshBtn);

        // Source chips: All, Factory, User
        const char* srcLabels[3] = { "All", "Factory", "User" };
        for (int i = 0; i < 3; ++i) {
            auto btn = std::make_unique<juce::TextButton>(srcLabels[i]);
            btn->onClick = [this, i] {
                sourceFilter = i;
                updateFilterButtonStyles();
                applyFilters();
            };
            addAndMakeVisible(*btn);
            sourceButtons.push_back(std::move(btn));
        }

        // Style tag chips
        styleTags = { "All", "Clean", "Funk", "Rock", "Metal", "Fusion", "Lead", "Custom" };
        for (int i = 0; i < styleTags.size(); ++i) {
            auto btn = std::make_unique<juce::TextButton>(styleTags[i]);
            btn->onClick = [this, tag = styleTags[i]] {
                selectedStyle = tag;
                updateFilterButtonStyles();
                applyFilters();
            };
            addAndMakeVisible(*btn);
            styleButtons.push_back(std::move(btn));
        }

        presetList.setModel(this);
        presetList.setRowHeight(52);
        presetList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d1118));
        presetList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff273042));
        presetList.setOutlineThickness(1);
        addAndMakeVisible(presetList);

        // Quick tagged save panel at bottom
        saveNameBox.setTextToShowWhenEmpty("New preset name...", juce::Colour(0xff64748b));
        saveNameBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0f131a));
        saveNameBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff334155));
        saveNameBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        addAndMakeVisible(saveNameBox);

        for (int i = 1; i < styleTags.size(); ++i) {
            saveStyleBox.addItem(styleTags[i], i);
        }
        saveStyleBox.setSelectedId(3, juce::dontSendNotification); // Rock
        addAndMakeVisible(saveStyleBox);

        saveCharBox.addItemList({ "Clean", "Crunch", "Hi-Gain", "Dual Rig", "Ambient", "Auto-Wah", "User" }, 1);
        saveCharBox.setSelectedId(2, juce::dontSendNotification);
        addAndMakeVisible(saveCharBox);

        quickSaveBtn.setButtonText("SAVE TO LIBRARY");
        quickSaveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff15803d));
        quickSaveBtn.onClick = [this] { handleQuickSave(); };
        addAndMakeVisible(quickSaveBtn);

        openFolderBtn.setButtonText("PRESET FOLDER");
        openFolderBtn.onClick = [this] {
            processor.getPresetManager().getUserPresetDirectory().revealToUser();
        };
        addAndMakeVisible(openFolderBtn);

        updateFilterButtonStyles();
        reloadLibrary();
    }

    void reloadLibrary() {
        allPresets = processor.getPresetManager().scanAllPresets();
        applyFilters();
    }

    int getNumRows() override {
        return static_cast<int>(filteredPresets.size());
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(filteredPresets.size())) {
            return;
        }
        const auto& p = filteredPresets[static_cast<size_t>(rowNumber)];
        const bool isCurrent = (p.name == processor.getActivePresetName());

        auto row = juce::Rectangle<float>(3.0f, 2.0f, static_cast<float>(width - 6), static_cast<float>(height - 4));
        if (isCurrent) {
            g.setColour(juce::Colour(0xff1e293b));
        } else if (rowIsSelected) {
            g.setColour(juce::Colour(0xff172030));
        } else {
            g.setColour(juce::Colour(0xff111620));
        }
        g.fillRoundedRectangle(row, 6.0f);

        g.setColour(isCurrent ? juce::Colour(0xfff59e0b) : juce::Colour(0xff252e3f));
        g.drawRoundedRectangle(row, 6.0f, isCurrent ? 1.5f : 0.8f);

        auto inner = row.reduced(10.0f, 5.0f);
        auto topRow = inner.removeFromTop(20.0f);

        // Right-aligned metadata pills
        auto drawPill = [&](const juce::String& txt, juce::Colour col) {
            const float w = std::max(44.0f, static_cast<float>(txt.length()) * 6.2f + 10.0f);
            auto pr = topRow.removeFromRight(w).reduced(1.5f, 2.0f);
            g.setColour(col.withAlpha(0.22f));
            g.fillRoundedRectangle(pr, 3.5f);
            g.setColour(col);
            g.drawRoundedRectangle(pr, 3.5f, 0.8f);
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.drawText(txt, pr, juce::Justification::centred);
        };

        drawPill(p.isFactory ? "FACTORY" : "USER", p.isFactory ? juce::Colour(0xff64748b) : juce::Colour(0xff22c55e));
        if (p.usesNam()) {
            drawPill("NAM", juce::Colour(0xff38bdf8));
        }
        if (p.isDualParallel()) {
            drawPill("DUAL A+B", juce::Colour(0xffa855f7));
        }
        drawPill(p.style.toUpperCase(), styleColour(p.style));

        // Preset name
        g.setColour(isCurrent ? juce::Colour(0xfffbbf24) : juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(p.name, topRow, juce::Justification::centredLeft, true);

        // Subtitle: Character + description
        const juce::String sub = p.character
                               + (p.description.isNotEmpty() ? ("  |  " + p.description) : "");
        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::plain)));
        g.drawText(sub, inner, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override {
        loadRow(row);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override {
        loadRow(row);
        if (closeCb) closeCb();
    }

    void mouseDown(const juce::MouseEvent& e) override {
        // Click on dark dimmed backdrop closes drawer
        if (!drawerRect.contains(e.getPosition())) {
            if (closeCb) closeCb();
        }
    }

    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(getLocalBounds());

        const auto d = drawerRect.toFloat();
        g.setColour(juce::Colour(0xff141923));
        g.fillRoundedRectangle(d, 8.0f);
        g.setColour(juce::Colour(0xff334155));
        g.drawRoundedRectangle(d, 8.0f, 1.4f);

        auto header = juce::Rectangle<int>(drawerRect.getX() + 14, drawerRect.getY() + 10, 260, 24);
        g.setColour(juce::Colour(0xfff59e0b));
        g.setFont(juce::Font(juce::FontOptions(15.5f, juce::Font::bold)));
        g.drawText("PRESET BROWSER", header, juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText("QUICK SAVE CURRENT RIG WITH TAGS",
                   drawerRect.getX() + 14, saveSectionTop, drawerRect.getWidth() - 28, 16,
                   juce::Justification::centredLeft);
    }

    void resized() override {
        const int drawerW = std::min(520, getWidth() - 60);
        drawerRect = getLocalBounds().removeFromLeft(drawerW).reduced(12, 12);

        auto area = drawerRect.reduced(14, 12);

        // Header buttons
        auto topBar = area.removeFromTop(26);
        closeBtn.setBounds(topBar.removeFromRight(62));
        topBar.removeFromRight(6);
        refreshBtn.setBounds(topBar.removeFromRight(72));

        area.removeFromTop(8);
        searchBox.setBounds(area.removeFromTop(26));
        area.removeFromTop(8);

        // Source chips
        auto srcRow = area.removeFromTop(24);
        const int srcW = srcRow.getWidth() / 3;
        for (size_t i = 0; i < sourceButtons.size(); ++i) {
            sourceButtons[i]->setBounds(srcRow.removeFromLeft(srcW).reduced(2, 0));
        }

        area.removeFromTop(6);

        // Style chips (2 rows of 4)
        auto stRow1 = area.removeFromTop(24);
        auto stRow2 = area.removeFromTop(24);
        const int stW = stRow1.getWidth() / 4;
        for (size_t i = 0; i < styleButtons.size(); ++i) {
            auto& targetRow = (i < 4) ? stRow1 : stRow2;
            styleButtons[i]->setBounds(targetRow.removeFromLeft(stW).reduced(2, 1));
        }

        area.removeFromTop(8);

        // Bottom quick save section
        auto saveArea = area.removeFromBottom(92);
        saveSectionTop = saveArea.getY();
        saveArea.removeFromTop(18);
        saveNameBox.setBounds(saveArea.removeFromTop(24));
        saveArea.removeFromTop(5);
        auto tagRow = saveArea.removeFromTop(24);
        saveStyleBox.setBounds(tagRow.removeFromLeft(tagRow.getWidth() / 2).reduced(0, 0));
        tagRow.removeFromLeft(6);
        saveCharBox.setBounds(tagRow);
        saveArea.removeFromTop(5);
        auto btnRow = saveArea.removeFromTop(22);
        openFolderBtn.setBounds(btnRow.removeFromRight(115));
        btnRow.removeFromRight(6);
        quickSaveBtn.setBounds(btnRow);

        area.removeFromBottom(8);
        presetList.setBounds(area);
    }

private:
    static juce::Colour styleColour(const juce::String& st) noexcept {
        if (st == "Clean")  return juce::Colour(0xff38bdf8);
        if (st == "Funk")   return juce::Colour(0xffec4899);
        if (st == "Rock")   return juce::Colour(0xfff59e0b);
        if (st == "Metal")  return juce::Colour(0xffef4444);
        if (st == "Fusion") return juce::Colour(0xff14b8a6);
        if (st == "Lead")   return juce::Colour(0xffa855f7);
        return juce::Colour(0xff94a3b8);
    }

    void updateFilterButtonStyles() {
        for (int i = 0; i < static_cast<int>(sourceButtons.size()); ++i) {
            sourceButtons[static_cast<size_t>(i)]->setColour(
                juce::TextButton::buttonColourId,
                (i == sourceFilter) ? juce::Colour(0xff2563eb) : juce::Colour(0xff1e2430)
            );
        }
        for (int i = 0; i < static_cast<int>(styleButtons.size()); ++i) {
            const bool active = (styleTags[i] == selectedStyle);
            styleButtons[static_cast<size_t>(i)]->setColour(
                juce::TextButton::buttonColourId,
                active ? styleColour(styleTags[i]).withMultipliedBrightness(0.75f) : juce::Colour(0xff1e2430)
            );
        }
    }

    void applyFilters() {
        const auto query = searchBox.getText().trim().toLowerCase();
        filteredPresets.clear();

        for (const auto& p : allPresets) {
            if (sourceFilter == 1 && !p.isFactory) continue;
            if (sourceFilter == 2 && p.isFactory) continue;
            if (selectedStyle != "All" && !p.style.equalsIgnoreCase(selectedStyle)) continue;

            if (query.isNotEmpty()) {
                const auto haystack = (p.name + " " + p.style + " " + p.character + " " + p.description).toLowerCase();
                if (!haystack.contains(query)) continue;
            }
            filteredPresets.push_back(p);
        }
        presetList.updateContent();
        presetList.repaint();
    }

    void loadRow(int row) {
        if (row < 0 || row >= static_cast<int>(filteredPresets.size())) {
            return;
        }
        processor.applyPresetData(filteredPresets[static_cast<size_t>(row)]);
        presetList.repaint();
    }

    void handleQuickSave() {
        auto name = saveNameBox.getText().trim();
        if (name.isEmpty()) {
            name = "Custom_" + juce::Time::getCurrentTime().formatted("%H%M%S");
        }
        const auto safeName = juce::File::createLegalFileName(name);
        const auto file = processor.getPresetManager().getUserPresetDirectory().getChildFile(safeName + ".pbpreset");
        const auto style = saveStyleBox.getText();
        const auto ch = saveCharBox.getText();
        const auto desc = isParallelRouting(processor.getRoutingMode())
                              ? "User Dual Parallel Rig preset"
                              : "User Serial Pedalboard preset";

        if (processor.saveUserPreset(file, name, style, ch, desc)) {
            saveNameBox.clear();
            reloadLibrary();
        }
    }

    PedalBoardAudioProcessor& processor;
    std::function<void()> closeCb;

    juce::Rectangle<int> drawerRect;
    int saveSectionTop = 0;

    juce::TextEditor searchBox;
    juce::TextButton closeBtn;
    juce::TextButton refreshBtn;

    int sourceFilter = 0; // 0 = All, 1 = Factory, 2 = User
    juce::String selectedStyle { "All" };
    juce::StringArray styleTags;
    std::vector<std::unique_ptr<juce::TextButton>> sourceButtons;
    std::vector<std::unique_ptr<juce::TextButton>> styleButtons;

    juce::ListBox presetList;
    std::vector<PresetData> allPresets;
    std::vector<PresetData> filteredPresets;

    juce::TextEditor saveNameBox;
    juce::ComboBox saveStyleBox;
    juce::ComboBox saveCharBox;
    juce::TextButton quickSaveBtn;
    juce::TextButton openFolderBtn;
};

} // namespace pedalboard
