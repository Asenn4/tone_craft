#pragma once

#include "../engine/Gp200Catalogue.h"
#include "../engine/ModuleId.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "SignalChainStrip.h"
#include "ValetonTheme.h"

#include <algorithm>
#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace pedalboard {

// Right-Side Gear Browser Sidebar populated with ALL 265 models from Daftar_Effect_GP-200.xlsx!
// - Search by name, hardware model description (e.g. "TS-808", "Rat", "Mark IV", "Rectifier", "Marshall", "VOX"), or type
// - Filter by 4 Category Tabs (Stomp | Amp | Cab | Rack) or Active-Only
// - Toggle between TYPE grouping (OD, Fuzz, Distortion, Clean, Hi Gain, 4x12, etc.) and MODULE grouping (PRE, WAH, DST, AMP, CAB, EQ, MOD, DLY, RVB)
// - Bottom info footer showing the hovered/selected model's original hardware & parameter specs from Excel!
class GearSidebar : public juce::Component {
public:
    enum class Category : int {
        All   = -1,
        Stomp = 0,
        Amp   = 1,
        Cab   = 2,
        Rack  = 3
    };

    using SelectGearCallback = std::function<void(StageViewTab /*tab*/, int /*subIndex*/)>;
    using SidebarDragCallback = std::function<void(bool /*active*/,
                                                   bool /*dropped*/,
                                                   ModuleId /*moduleId*/,
                                                   int /*modelIndex*/,
                                                   const juce::String& /*displayName*/,
                                                   juce::Colour /*accentCol*/,
                                                   juce::Point<int> /*screenPos*/)>;

    GearSidebar(PedalBoardAudioProcessor& proc, SelectGearCallback onSelectGear)
        : processor(proc),
          selectGearCb(std::move(onSelectGear)) {
        searchBox.setTextToShowWhenEmpty("Search 266 models & NAM Pedal...", juce::Colour(0xff6b7280));
        searchBox.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::italic)));
        searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff101318));
        searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff282e3c));
        searchBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff3b82f6));
        searchBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe5e7eb));
        searchBox.setIndents(26, 5);
        searchBox.onTextChange = [this] { rebuildFilteredGrid(); };
        addAndMakeVisible(searchBox);

        gridContent = std::make_unique<GridContent>(*this);
        viewport.setViewedComponent(gridContent.get(), false);
        viewport.setScrollBarsShown(true, false);
        viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff3a4252));
        addAndMakeVisible(viewport);

        buildFullGp200Catalogue();
        rebuildFilteredGrid();
    }

    void setSidebarDragCallback(SidebarDragCallback cb) {
        sidebarDragCb = std::move(cb);
    }

    void setInsertLaneProvider(std::function<int(ModuleId)> provider) {
        insertLaneProvider = std::move(provider);
    }

    void refreshLiveState() {
        if (gridContent != nullptr) {
            gridContent->repaint();
        }
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff161920));
        g.fillRect(bounds);

        g.setColour(juce::Colour(0xff2a303d));
        g.drawVerticalLine(0, 0.0f, bounds.getHeight());

        if (!searchRect.isEmpty()) {
            const float cx = static_cast<float>(searchRect.getX()) + 13.0f;
            const float cy = static_cast<float>(searchRect.getCentreY()) - 1.0f;
            g.setColour(juce::Colour(0xff7b8394));
            g.drawEllipse(cx - 5.0f, cy - 5.0f, 9.0f, 9.0f, 1.5f);
            g.drawLine(cx + 2.8f, cy + 2.8f, cx + 6.5f, cy + 6.5f, 1.6f);
        }

        drawFilterToggle(g, activeFilterRect.toFloat(), onlyActiveFilter);

        for (int i = 0; i < 4; ++i) {
            const bool selected = (activeCategory == static_cast<Category>(i));
            const auto r = catTabRects[static_cast<size_t>(i)].toFloat();
            g.setColour(selected ? valeton::pillOnBlue() : juce::Colour(0xff252a35));
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(selected ? juce::Colour(0xff60a5fa) : juce::Colour(0xff353c4b));
            g.drawRoundedRectangle(r, 3.0f, selected ? 1.2f : 0.7f);
            drawCategoryGlyph(g, r, i, selected ? juce::Colours::white : juce::Colour(0xffd1d5db));
        }

        auto drawModeBtn = [&](juce::Rectangle<int> ri, const juce::String& label, bool active, bool isTagIcon) {
            auto r = ri.toFloat();
            g.setColour(active ? juce::Colour(0xff0f1218) : juce::Colour(0xff252a35));
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(active ? juce::Colour(0xff3b82f6).withAlpha(0.75f) : juce::Colour(0xff343a47));
            g.drawRoundedRectangle(r, 3.0f, 0.8f);

            const auto col = active ? juce::Colour(0xff60a5fa) : juce::Colour(0xffd1d5db);
            const float gx = r.getX() + 14.0f;
            const float gy = r.getCentreY();
            g.setColour(col);
            if (isTagIcon) {
                juce::Path tag;
                tag.startNewSubPath(gx - 5.0f, gy - 4.0f);
                tag.lineTo(gx, gy - 4.0f);
                tag.lineTo(gx + 5.5f, gy + 1.5f);
                tag.lineTo(gx + 1.5f, gy + 5.5f);
                tag.lineTo(gx - 5.0f, gy - 1.0f);
                tag.closeSubPath();
                g.strokePath(tag, juce::PathStrokeType(1.3f));
                g.fillEllipse(gx - 3.2f, gy - 2.4f, 2.0f, 2.0f);
            } else {
                g.drawRoundedRectangle(gx - 5.0f, gy - 4.5f, 10.0f, 9.0f, 1.5f, 1.3f);
                g.drawLine(gx - 5.0f, gy - 1.5f, gx + 5.0f, gy - 1.5f, 1.2f);
            }

            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(label, r.withTrimmedLeft(22.0f), juce::Justification::centred);
        };
        drawModeBtn(typeBtnRect, "TYPE", !groupByModule, true);
        drawModeBtn(rigBtnRect,  "MODULE", groupByModule, false);

        // Bottom Excel Spec Footer (shows original hardware & parameter info for hovered model!)
        if (!footerRect.isEmpty()) {
            auto fr = footerRect.toFloat();
            g.setColour(juce::Colour(0xff101319));
            g.fillRoundedRectangle(fr, 4.0f);
            g.setColour(juce::Colour(0xff282f3d));
            g.drawRoundedRectangle(fr, 4.0f, 0.8f);

            g.setColour(juce::Colour(0xff60a5fa));
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText(hoveredTitle.isNotEmpty() ? hoveredTitle : "GP-200 & NAM LIBRARY (266 MODELS)",
                       fr.reduced(6.0f, 3.0f).withHeight(13.0f), juce::Justification::centredLeft, true);

            g.setColour(juce::Colour(0xff9ca3af));
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
            g.drawText(hoveredDesc.isNotEmpty() ? hoveredDesc : "Click any model to load it into its module & open stage.",
                       fr.reduced(6.0f, 3.0f).withTop(fr.getY() + 16.0f), juce::Justification::centredLeft, true);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto pos = e.getPosition();

        if (activeFilterRect.contains(pos)) {
            onlyActiveFilter = !onlyActiveFilter;
            rebuildFilteredGrid();
            repaint();
            return;
        }

        for (int i = 0; i < 4; ++i) {
            if (catTabRects[static_cast<size_t>(i)].contains(pos)) {
                const auto clicked = static_cast<Category>(i);
                activeCategory = (activeCategory == clicked) ? Category::All : clicked;
                rebuildFilteredGrid();
                repaint();
                return;
            }
        }

        if (typeBtnRect.contains(pos)) {
            groupByModule = false;
            rebuildFilteredGrid();
            repaint();
            return;
        }
        if (rigBtnRect.contains(pos)) {
            groupByModule = true;
            rebuildFilteredGrid();
            repaint();
            return;
        }
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8, 8);

        auto searchRow = area.removeFromTop(28);
        activeFilterRect = searchRow.removeFromRight(30);
        searchRow.removeFromRight(6);
        searchRect = searchRow;
        searchBox.setBounds(searchRect);

        area.removeFromTop(6);

        auto tabRow = area.removeFromTop(28);
        const int tabGap = 4;
        const int tabW = (tabRow.getWidth() - tabGap * 3) / 4;
        for (int i = 0; i < 4; ++i) {
            catTabRects[static_cast<size_t>(i)] = tabRow.removeFromLeft(tabW);
            if (i < 3) tabRow.removeFromLeft(tabGap);
        }

        area.removeFromTop(5);

        auto modeRow = area.removeFromTop(26);
        const int halfW = (modeRow.getWidth() - 4) / 2;
        typeBtnRect = modeRow.removeFromLeft(halfW);
        modeRow.removeFromLeft(4);
        rigBtnRect = modeRow;

        footerRect = area.removeFromBottom(34);
        area.removeFromBottom(5);
        area.removeFromTop(6);
        viewport.setBounds(area);
        layoutGridContent();
    }

private:
    struct GearEntry {
        int uid = 0;
        juce::String displayName;
        juce::String codeBadge;
        juce::String typeGroup;
        int typeSortRank = 0; // 0 = Effects/Stomps, 1 = Amps, 2 = Cabinets
        bool hideInTypeMode = false;
        juce::String moduleGroup;
        juce::String description;
        juce::String searchKeywords;
        juce::String paramsHint;
        Category category = Category::Stomp;
        StageViewTab targetTab = StageViewTab::Stomp;
        ModuleId moduleId = ModuleId::Overdrive;
        int modelIndex = 0;
        const char* bypassParam = nullptr;
        juce::Colour accentCol = juce::Colours::transparentBlack;
    };

    struct SectionRow {
        juce::String title;
        std::vector<const GearEntry*> items;
        juce::Rectangle<int> headerRect;
        std::vector<juce::Rectangle<int>> itemRects;
    };

    class GridContent : public juce::Component {
    public:
        explicit GridContent(GearSidebar& p) : owner(p) {}

        void paint(juce::Graphics& g) override { owner.paintGridSections(g); }
        void mouseDown(const juce::MouseEvent& e) override { owner.handleGridMouseDown(e); }
        void mouseDrag(const juce::MouseEvent& e) override { owner.handleGridMouseDrag(e); }
        void mouseUp(const juce::MouseEvent& e) override { owner.handleGridMouseUp(e); }
        void mouseMove(const juce::MouseEvent& e) override { owner.handleGridHover(e.getPosition()); }
        void mouseExit(const juce::MouseEvent&) override { owner.clearGridHover(); }

    private:
        GearSidebar& owner;
    };

    static juce::String computeCleanTypeGroup(ModuleId mod, const gp200::ModelEntry& m, int& outRank) {
        const juce::String sub = juce::String::fromUTF8(m.subType).trim();
        outRank = 0;

        if (mod == ModuleId::AmpHead) {
            outRank = 1;
            if (sub.equalsIgnoreCase("Clean"))       return "Amp - Clean";
            if (sub.equalsIgnoreCase("Drive"))       return "Amp - Drive";
            if (sub.equalsIgnoreCase("Hi Gain"))     return "Amp - Hi-Gain";
            if (sub.equalsIgnoreCase("Bass"))        return "Amp - Bass";
            if (sub.containsIgnoreCase("Acoustic"))  return "Amp - Acoustic";
            return "Amp - Neural NAM";
        }
        if (mod == ModuleId::CabinetIR) {
            outRank = 2;
            if (sub.containsIgnoreCase("bass"))      return "Cab - Bass";
            if (sub.containsIgnoreCase("Acoustic"))  return "Cab - Acoustic";
            if (sub.containsIgnoreCase("User IR"))   return "Cab - User IR";
            if (sub.containsIgnoreCase("1 x 12"))    return "Cab - 1x12";
            if (sub.containsIgnoreCase("2 x 12") || sub.containsIgnoreCase("2 x 10")) return "Cab - 2x10 / 2x12";
            if (sub.containsIgnoreCase("4 x 12") || sub.containsIgnoreCase("4 x 10")) return "Cab - 4x10 / 4x12";
            return "Cab - Small (1x6 - 1x10)";
        }

        if (mod == ModuleId::NoiseGate) return "Noise Gate";
        if (mod == ModuleId::Wah)       return "Wah";
        if (mod == ModuleId::GraphicEq) return "Equalizer (EQ)";
        if (mod == ModuleId::StereoDelay) return "Delay";
        if (mod == ModuleId::Reverb)    return "Reverb";

        if (mod == ModuleId::Overdrive) {
            if (sub.containsIgnoreCase("Neural NAM")) return "Neural NAM Pedal";
            if (sub.equalsIgnoreCase("OD"))           return "Overdrive";
            if (sub.equalsIgnoreCase("Distortion"))   return "Distortion";
            if (sub.equalsIgnoreCase("Fuzz"))         return "Fuzz";
            if (sub.containsIgnoreCase("Bass"))       return "Bass Drive & Preamp";
            if (sub.equalsIgnoreCase("Boost"))        return "Boost";
            return "Overdrive";
        }
        if (mod == ModuleId::Compressor) {
            if (sub.equalsIgnoreCase("Comp"))         return "Compressor";
            if (sub.equalsIgnoreCase("Boost"))        return "Boost";
            if (sub.equalsIgnoreCase("Acoustic"))     return "Acoustic Sim";
            if (sub.containsIgnoreCase("Filter"))     return "Filter & Auto-Wah";
            if (sub.equalsIgnoreCase("Pitch"))        return "Pitch & Whammy";
            return "Special FX";
        }
        if (mod == ModuleId::StereoChorus) {
            if (sub.equalsIgnoreCase("Chorus"))       return "Chorus";
            if (sub.equalsIgnoreCase("Flanger"))      return "Flanger";
            if (sub.equalsIgnoreCase("Vibrato"))      return "Vibrato";
            if (sub.equalsIgnoreCase("Phaser"))       return "Phaser & Vibe";
            if (sub.equalsIgnoreCase("Tremolo"))      return "Tremolo";
            if (sub.equalsIgnoreCase("Pitch"))        return "Pitch & Whammy";
            return "Special FX";
        }
        return sub;
    }

    void buildFullGp200Catalogue() {
        catalogue.clear();
        int uidCounter = 1;

        auto appendModuleModels = [&](ModuleId mod, Category cat, StageViewTab tab, const char* bypassId, const char* modGroupTitle) {
            const auto list = gp200::modelsForModule(mod);
            const auto code = juce::String(valeton::gpCodeFor(mod));
            for (size_t i = 0; i < list.size(); ++i) {
                const auto& m = list[i];
                GearEntry ge;
                ge.uid = uidCounter++;
                ge.displayName = juce::String::fromUTF8(m.name);
                ge.codeBadge   = code;
                ge.typeGroup   = computeCleanTypeGroup(mod, m, ge.typeSortRank);
                // Deduplicate the 7 identical Boost models in TYPE mode (keep them in PRE, still shown in both PRE & DST in MODULE mode!)
                ge.hideInTypeMode = (mod == ModuleId::Overdrive && juce::String::fromUTF8(m.subType).equalsIgnoreCase("Boost"));
                ge.moduleGroup = modGroupTitle;
                ge.description = juce::String::fromUTF8(m.description);
                ge.paramsHint  = juce::String::fromUTF8(m.paramsHint);
                ge.searchKeywords = ge.displayName + " " + ge.description + " " + ge.typeGroup + " " + code;
                if (mod == ModuleId::Compressor && i == 17) {
                    ge.searchKeywords += " whammy expression pitch bend digitech";
                }
                if (isNamPedalSlotModel(mod, static_cast<int>(i))) {
                    ge.codeBadge = "NAM";
                    ge.searchKeywords += " nam neural pedal tone3000 capture overdrive distortion fuzz preamp";
                }
                ge.category    = cat;
                ge.targetTab   = tab;
                ge.moduleId    = mod;
                ge.modelIndex  = static_cast<int>(i);
                ge.bypassParam = bypassId;
                ge.accentCol   = (m.chassisArgb != 0u) ? juce::Colour(m.chassisArgb) : valeton::colourFor(mod);
                catalogue.push_back(std::move(ge));
            }
        };

        // True GP-200 Signal-Flow order: NR -> PRE -> WAH -> DST -> AMP -> CAB -> EQ -> MOD -> DLY -> RVB
        appendModuleModels(ModuleId::NoiseGate,    Category::Stomp, StageViewTab::Stomp, params::kGateBypass,   "NR - Noise Gate (2)");
        appendModuleModels(ModuleId::Compressor,   Category::Stomp, StageViewTab::Stomp, params::kCompBypass,   "PRE - Comp / Boost / Pitch (20)");
        appendModuleModels(ModuleId::Wah,          Category::Stomp, StageViewTab::Stomp, params::kWahBypass,    "WAH - Wah Pedals (5)");
        appendModuleModels(ModuleId::Overdrive,    Category::Stomp, StageViewTab::Stomp, params::kOdBypass,     "DST - Drive / Fuzz / Dist / NAM (34)");
        appendModuleModels(ModuleId::AmpHead,      Category::Amp,   StageViewTab::Amp,   params::kAmpBypass,    "AMP - Amplifiers & NAM (72)");
        appendModuleModels(ModuleId::CabinetIR,    Category::Cab,   StageViewTab::Cab,   params::kCabBypass,    "CAB - Speaker Cabinets (71)");
        appendModuleModels(ModuleId::GraphicEq,    Category::Stomp, StageViewTab::Stomp, params::kEqBypass,     "EQ - Equalizers (6)");
        appendModuleModels(ModuleId::StereoChorus, Category::Rack,  StageViewTab::Rack,  params::kChorusBypass, "MOD - Modulation (24)");
        appendModuleModels(ModuleId::StereoDelay,  Category::Rack,  StageViewTab::Rack,  params::kDelayBypass,  "DLY - Delay Echoes (19)");
        appendModuleModels(ModuleId::Reverb,       Category::Rack,  StageViewTab::Rack,  params::kReverbBypass, "RVB - Reverbs (13)");
    }

    int getCurrentModelIndex(ModuleId mod, int rigIdx = 0) const noexcept {
        if (auto* raw = processor.getAPVTS().getRawParameterValue(params::modelParamFor(mod, rigIdx))) {
            return static_cast<int>(std::lround(raw->load(std::memory_order_relaxed)));
        }
        return 0;
    }

    bool isModuleEngaged(const GearEntry& e) const noexcept {
        const auto bs = processor.getDynamicBoardState();
        for (int i = 0; i < kMaxPedalSlots; ++i) {
            const auto& sl = bs.slots[static_cast<size_t>(i)];
            if (sl.occupied && sl.category == e.moduleId && sl.modelIndex == e.modelIndex) {
                if (!sl.bypassed) return true;
            }
        }
        if (e.bypassParam == nullptr) return true;
        if (auto* raw = processor.getAPVTS().getRawParameterValue(e.bypassParam)) {
            return raw->load(std::memory_order_relaxed) <= 0.5f;
        }
        return true;
    }

    bool isEntryCurrentlySelectedModel(const GearEntry& e) const noexcept {
        // Check all 8 dynamic StompBoard pedal slots first (highlights every stacked pedal!)
        const auto bs = processor.getDynamicBoardState();
        for (int i = 0; i < kMaxPedalSlots; ++i) {
            const auto& sl = bs.slots[static_cast<size_t>(i)];
            if (sl.occupied && sl.category == e.moduleId && sl.modelIndex == e.modelIndex) {
                return true;
            }
        }
        // Check core Amp A/B, Cab A/B, and Studio Rack units
        if (e.moduleId == ModuleId::AmpHead || e.moduleId == ModuleId::CabinetIR) {
            if (getCurrentModelIndex(e.moduleId, 0) == e.modelIndex) return true;
            if (isParallelRouting(processor.getRoutingMode()) &&
                getCurrentModelIndex(e.moduleId, 1) == e.modelIndex) return true;
            return false;
        }
        if (e.moduleId == ModuleId::StereoChorus || e.moduleId == ModuleId::StereoDelay || e.moduleId == ModuleId::Reverb) {
            return getCurrentModelIndex(e.moduleId, 0) == e.modelIndex;
        }
        return false;
    }

    void rebuildFilteredGrid() {
        sections.clear();
        const auto query = searchBox.getText().trim().toLowerCase();

        struct Bucket {
            juce::String title;
            int rank = 0;
            std::vector<const GearEntry*> items;
        };
        std::vector<Bucket> buckets;

        for (const auto& entry : catalogue) {
            if (!groupByModule && entry.hideInTypeMode) {
                continue;
            }
            if (activeCategory != Category::All && entry.category != activeCategory) {
                continue;
            }
            if (onlyActiveFilter && (!isEntryCurrentlySelectedModel(entry) || !isModuleEngaged(entry))) {
                continue;
            }
            if (query.isNotEmpty() && !entry.searchKeywords.toLowerCase().contains(query)) {
                continue;
            }

            const auto& secTitle = groupByModule ? entry.moduleGroup : entry.typeGroup;
            Bucket* target = nullptr;
            for (auto& b : buckets) {
                if (b.title == secTitle) {
                    target = &b;
                    break;
                }
            }
            if (target == nullptr) {
                buckets.push_back({ secTitle, groupByModule ? 0 : entry.typeSortRank, {} });
                target = &buckets.back();
            }
            target->items.push_back(&entry);
        }

        // In TYPE mode: sort section groups cleanly by rank (Effects A-Z, then Amps A-Z, then Cabs A-Z)
        if (!groupByModule) {
            std::sort(buckets.begin(), buckets.end(), [](const Bucket& a, const Bucket& b) {
                if (a.rank != b.rank) return a.rank < b.rank;
                return a.title.compareNatural(b.title) < 0;
            });
        }

        // Sort items alphabetically (A-Z) inside every section & append live item count badge!
        for (auto& b : buckets) {
            std::sort(b.items.begin(), b.items.end(), [](const GearEntry* a, const GearEntry* b) {
                return a->displayName.compareNatural(b->displayName) < 0;
            });
            const juce::String headerTitle = groupByModule
                ? b.title
                : (b.title + " (" + juce::String(b.items.size()) + ")");
            sections.push_back({ headerTitle, std::move(b.items), {}, {} });
        }

        layoutGridContent();
        if (gridContent != nullptr) {
            gridContent->repaint();
        }
    }

    void layoutGridContent() {
        if (gridContent == nullptr) return;
        const int w = std::max(180, viewport.getWidth() - (viewport.isVerticalScrollBarShown() ? 10 : 0));
        constexpr int kCols = 3;
        const int cellW = w / kCols;
        const int cellH = 72;
        const int headerH = 24;

        int y = 0;
        for (auto& sec : sections) {
            sec.headerRect = juce::Rectangle<int>(0, y, w, headerH);
            y += headerH + 5;
            sec.itemRects.clear();

            const bool collapsed = collapsedSections.contains(sec.title);
            if (!collapsed) {
                const int n = static_cast<int>(sec.items.size());
                for (int i = 0; i < n; ++i) {
                    const int row = i / kCols;
                    const int col = i % kCols;
                    sec.itemRects.emplace_back(col * cellW, y + row * cellH, cellW, cellH);
                }
                const int rows = (n + kCols - 1) / kCols;
                y += rows * cellH + 8;
            } else {
                y += 4;
            }
        }

        gridContent->setSize(w, std::max(viewport.getHeight(), y + 8));
    }

    void paintGridSections(juce::Graphics& g) {
        g.fillAll(juce::Colour(0xff161920));

        for (const auto& sec : sections) {
            const bool collapsed = collapsedSections.contains(sec.title);

            auto hr = sec.headerRect.toFloat().reduced(1.0f, 1.0f);
            g.setColour(juce::Colour(0xff232833));
            g.fillRoundedRectangle(hr, 3.0f);
            g.setColour(juce::Colour(0xffd4d8e0));
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::italic)));
            g.drawText(sec.title, hr.reduced(18.0f, 0.0f), juce::Justification::centred);

            const float cx = hr.getRight() - 11.0f;
            const float cy = hr.getCentreY();
            juce::Path chev;
            if (collapsed) {
                chev.startNewSubPath(cx - 2.5f, cy - 3.5f);
                chev.lineTo(cx + 2.5f, cy);
                chev.lineTo(cx - 2.5f, cy + 3.5f);
            } else {
                chev.startNewSubPath(cx - 3.5f, cy - 2.0f);
                chev.lineTo(cx, cy + 2.5f);
                chev.lineTo(cx + 3.5f, cy - 2.0f);
            }
            g.setColour(juce::Colour(0xffb5becd));
            g.strokePath(chev, juce::PathStrokeType(1.3f));

            if (collapsed) continue;

            for (size_t i = 0; i < sec.items.size() && i < sec.itemRects.size(); ++i) {
                const auto* item = sec.items[i];
                const auto cell = sec.itemRects[i].toFloat().reduced(3.0f, 2.5f);
                const bool engaged = isModuleEngaged(*item);
                const bool isCurrentModel = isEntryCurrentlySelectedModel(*item);
                const bool isHovered = (hoveredUid == item->uid);

                // Modern dark-glass card backdrop for every gear item
                if (isCurrentModel) {
                    juce::ColourGradient cardGrad(juce::Colour(0xff1e3a8a).withAlpha(0.65f), cell.getX(), cell.getY(),
                                                  juce::Colour(0xff172554).withAlpha(0.65f), cell.getX(), cell.getBottom(), false);
                    g.setGradientFill(cardGrad);
                    g.fillRoundedRectangle(cell, 6.0f);
                    g.setColour(juce::Colour(0xff60a5fa).withAlpha(0.90f));
                    g.drawRoundedRectangle(cell, 6.0f, 1.2f);
                } else {
                    g.setColour(isHovered ? juce::Colour(0xff232a3a) : juce::Colour(0xff1a1f2b));
                    g.fillRoundedRectangle(cell, 6.0f);
                    g.setColour(isHovered ? item->accentCol.withAlpha(0.75f) : juce::Colour(0xff283042));
                    g.drawRoundedRectangle(cell, 6.0f, isHovered ? 1.1f : 0.8f);
                }

                auto iconBox = cell.withTrimmedBottom(15.0f).reduced(3.0f, 2.0f);
                valeton::drawStylizedPedalIcon(g, iconBox, item->moduleId, engaged && isCurrentModel ? true : isHovered || isCurrentModel,
                                               item->accentCol, item->codeBadge);

                // Subtle '+' drag/add badge in top-right corner when hovered
                if (isHovered) {
                    auto plusBadge = juce::Rectangle<float>(cell.getRight() - 14.0f, cell.getY() + 2.0f, 12.0f, 12.0f);
                    g.setColour(juce::Colour(0xff2563eb));
                    g.fillRoundedRectangle(plusBadge, 3.0f);
                    g.setColour(juce::Colours::white);
                    g.drawLine(plusBadge.getCentreX() - 3.0f, plusBadge.getCentreY(), plusBadge.getCentreX() + 3.0f, plusBadge.getCentreY(), 1.3f);
                    g.drawLine(plusBadge.getCentreX(), plusBadge.getCentreY() - 3.0f, plusBadge.getCentreX(), plusBadge.getCentreY() + 3.0f, 1.3f);
                }

                g.setColour((isCurrentModel || isHovered) ? juce::Colours::white : juce::Colour(0xffcbd5e1));
                g.setFont(juce::Font(juce::FontOptions(9.2f, (isCurrentModel || isHovered) ? juce::Font::bold : juce::Font::plain)));
                g.drawText(item->displayName, cell.withTop(cell.getBottom() - 15.0f).reduced(2.0f, 0.0f), juce::Justification::centred, true);
            }
        }
    }

    void handleGridMouseDown(const juce::MouseEvent& e) {
        draggedItem = nullptr;
        isDraggingFromGrid = false;
        const auto pos = e.getPosition();

        for (const auto& sec : sections) {
            if (sec.headerRect.contains(pos)) {
                if (collapsedSections.contains(sec.title)) {
                    collapsedSections.removeString(sec.title);
                } else {
                    collapsedSections.add(sec.title);
                }
                layoutGridContent();
                gridContent->repaint();
                return;
            }

            if (collapsedSections.contains(sec.title)) continue;
            for (size_t i = 0; i < sec.items.size() && i < sec.itemRects.size(); ++i) {
                if (!sec.itemRects[i].contains(pos)) continue;
                const auto* item = sec.items[i];

                if (e.mods.isPopupMenu()) {
                    openSidebarItemContextMenu(*item, sec.itemRects[i]);
                    return;
                }

                draggedItem = item;
                return;
            }
        }
    }

    void handleGridMouseDrag(const juce::MouseEvent& e) {
        if (draggedItem == nullptr) return;
        if (!isDraggingFromGrid && e.getDistanceFromDragStart() > 5) {
            isDraggingFromGrid = true;
        }
        if (isDraggingFromGrid && sidebarDragCb) {
            sidebarDragCb(true, false, draggedItem->moduleId, draggedItem->modelIndex,
                          draggedItem->displayName, draggedItem->accentCol, e.getScreenPosition());
        }
    }

    void handleGridMouseUp(const juce::MouseEvent& e) {
        if (draggedItem == nullptr) return;
        const auto* item = draggedItem;
        const bool wasDragged = isDraggingFromGrid;
        draggedItem = nullptr;
        isDraggingFromGrid = false;

        if (wasDragged) {
            if (sidebarDragCb) {
                sidebarDragCb(false, true, item->moduleId, item->modelIndex,
                              item->displayName, item->accentCol, e.getScreenPosition());
            }
            return;
        }

        applySidebarItemClick(*item);
    }

    void openSidebarItemContextMenu(const GearEntry& item, juce::Rectangle<int> cellRect) {
        juce::PopupMenu m;
        m.addSectionHeader(item.codeBadge + ": " + item.displayName);
        if (item.moduleId != ModuleId::AmpHead && item.moduleId != ModuleId::CabinetIR) {
            if (static_cast<int>(processor.getActivePedalSlotIds().size()) < kMaxPedalSlots) {
                m.addItem(1, "+ Add as New Pedal Slot on StompBoard");
            }
        }
        m.addItem(2, "Load into Primary " + item.codeBadge + " Module");

        auto screenArea = gridContent->localAreaToGlobal(cellRect);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screenArea),
                        [this, item](int res) {
                            if (res == 1) {
                                const int lane = insertLaneProvider ? insertLaneProvider(item.moduleId) : -1;
                                processor.addPedalSlot(item.moduleId, item.modelIndex, -1, lane);
                                if (selectGearCb) selectGearCb(StageViewTab::Stomp, -2);
                            } else if (res == 2) {
                                applySidebarItemClick(item);
                            }
                        });
    }

    void applySidebarItemClick(const GearEntry& item) {
        const auto activeSlots = processor.getActivePedalSlotIds();
        if (isNamPedalSlotModel(item.moduleId, item.modelIndex)) {
            int existingNamSlot = -1;
            for (const int sId : activeSlots) {
                const auto snap = processor.getPedalSlot(sId);
                if (snap.occupied && isNamPedalSlotModel(snap.category, snap.modelIndex)) {
                    existingNamSlot = sId;
                    break;
                }
            }
            if (existingNamSlot >= 0) {
                processor.setPedalSlotBypass(existingNamSlot, false);
            } else if (static_cast<int>(activeSlots.size()) < kMaxPedalSlots) {
                const int lane = insertLaneProvider ? insertLaneProvider(item.moduleId) : -1;
                processor.addPedalSlot(item.moduleId, item.modelIndex, -1, lane);
            } else {
                for (const int sId : activeSlots) {
                    const auto snap = processor.getPedalSlot(sId);
                    if (snap.occupied && snap.category == item.moduleId) {
                        processor.setPedalSlotModel(sId, item.modelIndex);
                        processor.setPedalSlotBypass(sId, false);
                        break;
                    }
                }
            }
            if (selectGearCb) {
                selectGearCb(StageViewTab::Stomp, -2);
            }
            gridContent->repaint();
            repaint();
            return;
        }

        bool updatedSlot = false;
        for (const int sId : activeSlots) {
            const auto snap = processor.getPedalSlot(sId);
            if (snap.occupied && snap.category == item.moduleId && !isNamPedalSlotModel(snap.category, snap.modelIndex)) {
                processor.setPedalSlotModel(sId, item.modelIndex);
                processor.setPedalSlotBypass(sId, false);
                updatedSlot = true;
                break;
            }
        }
        if (!updatedSlot && item.targetTab == StageViewTab::Stomp &&
            item.moduleId != ModuleId::AmpHead && item.moduleId != ModuleId::CabinetIR &&
            static_cast<int>(activeSlots.size()) < kMaxPedalSlots) {
            const int lane = insertLaneProvider ? insertLaneProvider(item.moduleId) : -1;
            processor.addPedalSlot(item.moduleId, item.modelIndex, -1, lane);
        }

        if (auto* p = processor.getAPVTS().getParameter(params::modelParamFor(item.moduleId, 0))) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(item.modelIndex)));
            p->endChangeGesture();
        }

        if (item.moduleId == ModuleId::AmpHead) {
            if (auto* cp = processor.getAPVTS().getParameter(params::kAmpChannel)) {
                const int cat = AmpHead::categoryForModel(item.modelIndex);
                cp->beginChangeGesture();
                cp->setValueNotifyingHost(cp->convertTo0to1(static_cast<float>(cat)));
                cp->endChangeGesture();
            }
        }

        if (item.bypassParam != nullptr) {
            if (auto* bp = processor.getAPVTS().getParameter(item.bypassParam)) {
                if (bp->getValue() > 0.5f) {
                    bp->beginChangeGesture();
                    bp->setValueNotifyingHost(0.0f);
                    bp->endChangeGesture();
                }
            }
        }

        if (selectGearCb) {
            selectGearCb(item.targetTab, item.targetTab == StageViewTab::Stomp ? -2 : 0);
        }
        gridContent->repaint();
        repaint();
    }

    void handleGridClick(const juce::MouseEvent& e) {
        const auto pos = e.getPosition();
        for (const auto& sec : sections) {
            if (sec.headerRect.contains(pos)) {
                if (collapsedSections.contains(sec.title)) {
                    collapsedSections.removeString(sec.title);
                } else {
                    collapsedSections.add(sec.title);
                }
                layoutGridContent();
                gridContent->repaint();
                return;
            }

            if (collapsedSections.contains(sec.title)) continue;
            for (size_t i = 0; i < sec.items.size() && i < sec.itemRects.size(); ++i) {
                if (!sec.itemRects[i].contains(pos)) continue;
                const auto* item = sec.items[i];

                if (e.mods.isPopupMenu() && item->bypassParam != nullptr) {
                    if (auto* p = processor.getAPVTS().getParameter(item->bypassParam)) {
                        const float next = (p->getValue() > 0.5f) ? 0.0f : 1.0f;
                        p->beginChangeGesture();
                        p->setValueNotifyingHost(next);
                        p->endChangeGesture();
                    }
                    gridContent->repaint();
                    return;
                }

                // 1. Load the selected GP-200 model into its module parameter!
                if (auto* p = processor.getAPVTS().getParameter(params::modelParamFor(item->moduleId, 0))) {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(item->modelIndex)));
                    p->endChangeGesture();
                }

                // Keep Amp channel category synced when choosing an Amp model
                if (item->moduleId == ModuleId::AmpHead) {
                    if (auto* cp = processor.getAPVTS().getParameter(params::kAmpChannel)) {
                        const int cat = AmpHead::categoryForModel(item->modelIndex);
                        cp->beginChangeGesture();
                        cp->setValueNotifyingHost(cp->convertTo0to1(static_cast<float>(cat)));
                        cp->endChangeGesture();
                    }
                }

                // Auto-engage module when selecting a model from the Gear Sidebar
                if (item->bypassParam != nullptr) {
                    if (auto* bp = processor.getAPVTS().getParameter(item->bypassParam)) {
                        if (bp->getValue() > 0.5f) {
                            bp->beginChangeGesture();
                            bp->setValueNotifyingHost(0.0f);
                            bp->endChangeGesture();
                        }
                    }
                }

                if (selectGearCb) {
                    selectGearCb(item->targetTab, 0);
                }
                gridContent->repaint();
                repaint();
                return;
            }
        }
    }

    void handleGridHover(juce::Point<int> pos) {
        int foundUid = 0;
        juce::String title, desc;
        for (const auto& sec : sections) {
            if (collapsedSections.contains(sec.title)) continue;
            for (size_t i = 0; i < sec.items.size() && i < sec.itemRects.size(); ++i) {
                if (sec.itemRects[i].contains(pos)) {
                    const auto* it = sec.items[i];
                    foundUid = it->uid;
                    title = it->codeBadge + ": " + it->displayName + " (" + it->typeGroup.fromFirstOccurrenceOf(" - ", false, false) + ")";
                    desc  = it->description;
                    break;
                }
            }
            if (foundUid != 0) break;
        }
        if (foundUid != hoveredUid) {
            hoveredUid = foundUid;
            hoveredTitle = title;
            hoveredDesc = desc;
            gridContent->repaint();
            repaint();
        }
    }

    void clearGridHover() {
        if (hoveredUid != 0) {
            hoveredUid = 0;
            hoveredTitle.clear();
            hoveredDesc.clear();
            gridContent->repaint();
            repaint();
        }
    }

    static void drawFilterToggle(juce::Graphics& g, juce::Rectangle<float> r, bool active) {
        g.setColour(active ? valeton::pillOnBlue() : juce::Colour(0xff252a35));
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(active ? juce::Colour(0xff60a5fa) : juce::Colour(0xff353c4b));
        g.drawRoundedRectangle(r, 3.0f, 1.0f);

        const auto cx = r.getCentreX();
        const auto cy = r.getCentreY();
        g.setColour(active ? juce::Colours::white : juce::Colour(0xffd1d5db));
        juce::Path arc;
        arc.addCentredArc(cx, cy, 5.5f, 5.5f, 0.0f, 0.55f, 5.73f, true);
        g.strokePath(arc, juce::PathStrokeType(1.6f));
        g.drawLine(cx, cy - 6.8f, cx, cy - 1.2f, 1.6f);
    }

    static void drawCategoryGlyph(juce::Graphics& g, juce::Rectangle<float> r, int idx, juce::Colour col) {
        g.setColour(col);
        const float cx = r.getCentreX();
        const float cy = r.getCentreY();
        if (idx == 0) {
            auto box = juce::Rectangle<float>(cx - 6.0f, cy - 8.5f, 12.0f, 17.0f);
            g.drawRoundedRectangle(box, 1.8f, 1.5f);
            g.fillEllipse(cx - 3.2f, cy - 6.0f, 2.5f, 2.5f);
            g.fillEllipse(cx + 0.8f, cy - 6.0f, 2.5f, 2.5f);
            g.fillRoundedRectangle(cx - 4.0f, cy + 1.0f, 8.0f, 5.5f, 1.0f);
        } else if (idx == 1) {
            auto box = juce::Rectangle<float>(cx - 10.0f, cy - 5.5f, 20.0f, 12.0f);
            g.fillRoundedRectangle(box, 1.8f);
            g.drawRoundedRectangle(cx - 4.5f, cy - 8.5f, 9.0f, 3.5f, 1.2f, 1.3f);
            g.setColour(juce::Colour(0xff161920));
            for (int k = 0; k < 4; ++k) {
                g.fillEllipse(cx - 7.5f + static_cast<float>(k) * 4.5f, cy - 3.5f, 2.2f, 2.2f);
            }
        } else if (idx == 2) {
            auto box = juce::Rectangle<float>(cx - 8.0f, cy - 7.5f, 16.0f, 15.0f);
            g.drawRoundedRectangle(box, 1.8f, 1.5f);
            g.drawEllipse(cx - 4.8f, cy - 4.8f, 9.6f, 9.6f, 1.4f);
            g.fillEllipse(cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);
        } else {
            g.fillRoundedRectangle(cx - 8.0f, cy - 7.5f, 6.5f, 15.0f, 1.2f);
            g.fillRoundedRectangle(cx + 1.5f, cy - 7.5f, 6.5f, 15.0f, 1.2f);
            g.setColour(juce::Colour(0xff161920));
            for (int d = 0; d < 3; ++d) {
                const float dy = cy - 4.5f + static_cast<float>(d) * 4.0f;
                g.fillEllipse(cx - 6.0f, dy, 2.4f, 2.4f);
                g.fillEllipse(cx + 3.5f, dy, 2.4f, 2.4f);
            }
        }
    }

    PedalBoardAudioProcessor& processor;
    SelectGearCallback selectGearCb;
    SidebarDragCallback sidebarDragCb;
    std::function<int(ModuleId)> insertLaneProvider;
    const GearEntry* draggedItem = nullptr;
    bool isDraggingFromGrid = false;

    juce::TextEditor searchBox;
    juce::Rectangle<int> searchRect, activeFilterRect, footerRect;
    std::array<juce::Rectangle<int>, 4> catTabRects;
    juce::Rectangle<int> typeBtnRect, rigBtnRect;

    Category activeCategory = Category::All;
    bool onlyActiveFilter = false;
    bool groupByModule = false;

    std::vector<GearEntry> catalogue;
    std::vector<SectionRow> sections;
    juce::StringArray collapsedSections;
    int hoveredUid = 0;
    juce::String hoveredTitle, hoveredDesc;

    juce::Viewport viewport;
    std::unique_ptr<GridContent> gridContent;
};

} // namespace pedalboard
