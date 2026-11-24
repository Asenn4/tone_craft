#pragma once

#include "../engine/Gp200Catalogue.h"
#include "../engine/ModuleId.h"
#include "../engine/PedalSlot.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "ValetonTheme.h"

#include <algorithm>
#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

namespace pedalboard {

enum class StageViewTab : int {
    Stomp = 0,
    Amp   = 1,
    Cab   = 2,
    Rack  = 3
};

// Interactive Multi-Topology Signal Chain Strip supporting all 4 AmpliTube-style routing modes:
//   0 [->]  Serial Chain        : IN -> Pedals -> AMP A -> CAB A -> MOD -> DLY -> RVB -> OUT
//   1 [-<]  Dual Split A/B      : IN -> Pre -< (Path A: AMP A + CAB A || Path B: AMP B + CAB B) >- MIX -> Post -> OUT
//   2 [-<=] Triple Parallel     : IN -> Pre -<= (Path A: AMP A + CAB A || Path B: AMP B + CAB B || Parallel Wet FX) => MIX -> OUT
//   3 [==]  Dual Independent    : IN A -> A Pedals -> AMP A -> CAB A -> MOD -> OUT A
//                                 IN B -> B Pedals -> AMP B -> CAB B -> DLY -> RVB -> OUT B
class SignalChainStrip : public juce::Component,
                         public juce::TooltipClient {
public:
    using FocusCallback = std::function<void(StageViewTab, int /*rigIndex*/)>;

    SignalChainStrip(PedalBoardAudioProcessor& proc, FocusCallback onFocusView)
        : processor(proc),
          focusCb(std::move(onFocusView)) {}

    juce::String getTooltip() override {
        switch (hoveredRouteBtn) {
            case 0: return "Serial Chain (1 Path): Single linear signal flow";
            case 1: return "Dual Split A/B (1-to-2): Shared Pre -> Path A & Path B -> Shared Post FX";
            case 2: return "Triple Parallel A/B/C (1-to-3): Shared Pre -> Path A, B & C (3 Amps/Cabs) -> MIX -> Shared Post";
            case 3: return "Dual Independent Rigs (A || B): Two full separate chains with Pre/Post per chain";
            default: break;
        }
        if (hoveredMixNode) {
            return "Console Mixer — Click to adjust Path A / B / C levels, pan, phase & master";
        }
        return {};
    }

    void setActiveTab(StageViewTab tab) noexcept {
        if (activeTab != tab) {
            activeTab = tab;
            repaint();
        }
    }

    void setSelectedModule(ModuleId mod) noexcept {
        juce::ignoreUnused(mod);
        repaint();
    }

    void setHighlightedStompLane(int laneFilter) noexcept {
        if (highlightedStompLane != laneFilter) {
            highlightedStompLane = laneFilter;
            repaint();
        }
    }

    static StageViewTab tabForModule(ModuleId id) noexcept {
        switch (id) {
            case ModuleId::NoiseGate:
            case ModuleId::Compressor:
            case ModuleId::Wah:
            case ModuleId::Overdrive:
            case ModuleId::GraphicEq:
                return StageViewTab::Stomp;
            case ModuleId::AmpHead:
                return StageViewTab::Amp;
            case ModuleId::CabinetIR:
                return StageViewTab::Cab;
            case ModuleId::StereoChorus:
            case ModuleId::StereoDelay:
            case ModuleId::Reverb:
            default:
                return StageViewTab::Rack;
        }
    }

    static StageViewTab tabForNode(const ChainNode& nd) noexcept {
        switch (nd.kind) {
            case ChainNodeKind::PedalSlot:  return StageViewTab::Stomp;
            case ChainNodeKind::AmpHead:    return StageViewTab::Amp;
            case ChainNodeKind::CabinetIR:  return StageViewTab::Cab;
            case ChainNodeKind::RackMod:
            case ChainNodeKind::RackDelay:
            case ChainNodeKind::RackReverb:
            default:                        return StageViewTab::Rack;
        }
    }

    static juce::Colour laneColour(BusLane lane) noexcept {
        switch (lane) {
            case BusLane::Pre:   return juce::Colour(0xff14b8a6);
            case BusLane::PathA: return juce::Colour(0xfff59e0b);
            case BusLane::PathB: return juce::Colour(0xff38bdf8);
            case BusLane::PathC: return juce::Colour(0xffa855f7);
            case BusLane::Post:
            case BusLane::PostA: return juce::Colour(0xff22c55e);
            default:             return juce::Colour(0xff64748b);
        }
    }

    void setSidebarDragHover(bool active, juce::Point<int> localPt, ModuleId cat, const juce::String& modelName) {
        sidebarDragActive = active;
        sidebarDragPt = localPt;
        sidebarDragCat = cat;
        sidebarDragModelName = modelName;
        repaint();
    }

    bool handleSidebarDrop(juce::Point<int> localPt, ModuleId cat, int modelIndex) {
        sidebarDragActive = false;
        const auto mode = processor.getRoutingMode();
        computeLayout();

        // Dropping an Amp or Cab onto Path B or Path C loads Rig B or Rig C directly!
        int dropRig = 0;
        if (mode == RoutingMode::TripleParallel && laneCZoneRect.contains(localPt)) {
            dropRig = 2;
        } else if (isParallelRouting(mode) && laneBZoneRect.contains(localPt)) {
            dropRig = 1;
        }
        if (cat == ModuleId::AmpHead) {
            setCoreModelIndex(ModuleId::AmpHead, modelIndex, dropRig);
            if (focusCb) focusCb(StageViewTab::Amp, dropRig);
            repaint();
            return true;
        }
        if (cat == ModuleId::CabinetIR) {
            setCoreModelIndex(ModuleId::CabinetIR, modelIndex, dropRig);
            if (focusCb) focusCb(StageViewTab::Cab, dropRig);
            repaint();
            return true;
        }

        BusLane targetLane = BusLane::Pre;
        if (mode == RoutingMode::DualIndependent) {
            if (!postAZoneRect.isEmpty() && postAZoneRect.contains(localPt))      targetLane = BusLane::PostA;
            else if (!preAZoneRect.isEmpty() && preAZoneRect.contains(localPt))   targetLane = BusLane::PathA;
            else if (!postBZoneRect.isEmpty() && postBZoneRect.contains(localPt)) targetLane = BusLane::Post;
            else if (!preBZoneRect.isEmpty() && preBZoneRect.contains(localPt))   targetLane = BusLane::PathB;
            else if (laneBZoneRect.contains(localPt))                             targetLane = BusLane::PathB;
            else                                                                  targetLane = BusLane::PathA;
        } else if (mode == RoutingMode::TripleParallel) {
            if (laneCZoneRect.contains(localPt))      targetLane = BusLane::PathC;
            else if (laneBZoneRect.contains(localPt)) targetLane = BusLane::PathB;
            else if (laneAZoneRect.contains(localPt)) targetLane = BusLane::PathA;
            else if (postZoneRect.contains(localPt))  targetLane = BusLane::Post;
            else                                      targetLane = BusLane::Pre;
        } else if (isParallelRouting(mode)) {
            if (laneBZoneRect.contains(localPt))      targetLane = BusLane::PathB;
            else if (laneAZoneRect.contains(localPt)) targetLane = BusLane::PathA;
            else if (postZoneRect.contains(localPt))  targetLane = BusLane::Post;
            else                                      targetLane = BusLane::Pre;
        }

        const int insIdx = (mode == RoutingMode::Serial)
            ? findInsertionChainIndex(localPt)
            : static_cast<int>(cachedNodes.size());
        const int newSlot = processor.addPedalSlotAtChainIndex(cat, modelIndex, insIdx, targetLane);
        if (newSlot >= 0) {
            selectedChainIndex = insIdx;
            if (focusCb) focusCb(StageViewTab::Stomp, isParallelRouting(mode) ? static_cast<int>(targetLane) : -2);
        }
        repaint();
        return true;
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto pos = e.getPosition();
        int nextRoute = -1;
        for (int i = 0; i < 4; ++i) {
            if (routeBtnRects[static_cast<size_t>(i)].contains(pos)) {
                nextRoute = i;
                break;
            }
        }
        const bool nextMix = !mixBadgeRect.isEmpty() && mixBadgeRect.contains(pos);
        if (nextRoute != hoveredRouteBtn || nextMix != hoveredMixNode) {
            hoveredRouteBtn = nextRoute;
            hoveredMixNode = nextMix;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoveredRouteBtn != -1 || hoveredMixNode) {
            hoveredRouteBtn = -1;
            hoveredMixNode = false;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        dragCandidateSlot = -1;
        pillClickCandidateSlot = -1;
        isDraggingNode = false;
        dropHoverSlot = -1;
        const auto pos = e.getPosition();
        dragStartPos = pos;
        dragCurrentPos = pos;

        computeLayout();

        // Left 4 AmpliTube-style routing topology buttons (ALL 4 ACTIVE!)
        for (int i = 0; i < 4; ++i) {
            if (!routeBtnRects[static_cast<size_t>(i)].contains(pos)) continue;
            setRoutingModeParam(static_cast<RoutingMode>(i));
            repaint();
            return;
        }

        // Click MIX summing node in parallel views -> jump straight to Console Mixer!
        if (!mixBadgeRect.isEmpty() && mixBadgeRect.contains(pos)) {
            if (focusCb) focusCb(StageViewTab::Rack, 0);
            return;
        }

        const auto mode = processor.getRoutingMode();

        // Multi-Lane Parallel Modes (Dual Split, Triple Parallel, Dual Independent)
        if (isParallelRouting(mode)) {
            for (const auto& vn : parallelNodes) {
                if (!vn.bounds.contains(pos)) continue;

                // Right-click -> full GP-200 model + lane assignment context menu
                if (e.mods.isPopupMenu()) {
                    openParallelNodeMenu(vn, vn.bounds);
                    return;
                }

                // Left click on power toggle pill/dot -> toggle bypass ON/OFF!
                if (vn.bypassRect.contains(pos)) {
                    toggleParallelNodeBypass(vn);
                    repaint();
                    return;
                }

                // Left click on dropdown chevron area -> open model picker menu
                if (vn.menuRect.contains(pos)) {
                    openParallelNodeMenu(vn, vn.bounds);
                    return;
                }

                // Left click on node body -> focus exact hardware stage & Rig/Lane!
                if (vn.chainIndex >= 0) selectedChainIndex = vn.chainIndex;
                if (focusCb) {
                    const int subIdx = (vn.kind == ChainNodeKind::PedalSlot)
                        ? static_cast<int>(vn.lane)
                        : vn.rigIndex;
                    focusCb(tabForModule(vn.mod), subIdx);
                }
                repaint();
                return;
            }

            // Clicking an empty spot on a specific lane zone (Pre / Path A / Path B / Path C / Post) switches StompBoard to that lane!
            if (focusCb) {
                if (!preZoneRect.isEmpty() && preZoneRect.contains(pos))        { focusCb(StageViewTab::Stomp, static_cast<int>(BusLane::Pre)); return; }
                if (!laneAZoneRect.isEmpty() && laneAZoneRect.contains(pos))    { focusCb(StageViewTab::Stomp, static_cast<int>(BusLane::PathA)); return; }
                if (!laneBZoneRect.isEmpty() && laneBZoneRect.contains(pos))    { focusCb(StageViewTab::Stomp, static_cast<int>(BusLane::PathB)); return; }
                if (!laneCZoneRect.isEmpty() && laneCZoneRect.contains(pos))    { focusCb(StageViewTab::Stomp, static_cast<int>(BusLane::PathC)); return; }
                if (!postZoneRect.isEmpty() && postZoneRect.contains(pos))      { focusCb(StageViewTab::Stomp, static_cast<int>(BusLane::Post)); return; }
            }
            return;
        }

        // Mode 0: Serial 3-Row Classic Strip
        const int numNodes = static_cast<int>(cachedNodes.size());
        for (int slot = 0; slot < numNodes; ++slot) {
            const auto& nd = cachedNodes[static_cast<size_t>(slot)];

            if (pillRects[static_cast<size_t>(slot)].contains(pos)) {
                selectedChainIndex = slot;
                pillClickCandidateSlot = slot;
                dragCandidateSlot = slot;
                repaint();
                return;
            }

            if (dropdownRects[static_cast<size_t>(slot)].contains(pos)) {
                selectedChainIndex = slot;
                openSerialNodeDropdownMenu(nd, slot, dropdownRects[static_cast<size_t>(slot)]);
                repaint();
                return;
            }

            if (iconRects[static_cast<size_t>(slot)].contains(pos)) {
                if (e.mods.isPopupMenu()) {
                    openSerialNodeDropdownMenu(nd, slot, iconRects[static_cast<size_t>(slot)]);
                    repaint();
                    return;
                }
                selectedChainIndex = slot;
                dragCandidateSlot = slot;
                if (focusCb) {
                    focusCb(tabForNode(nd), 0);
                }
                repaint();
                return;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (processor.getRoutingMode() != RoutingMode::Serial || dragCandidateSlot < 0) return;
        dragCurrentPos = e.getPosition();

        if (!isDraggingNode && e.getDistanceFromDragStart() > 5) {
            isDraggingNode = true;
            pillClickCandidateSlot = -1;
        }

        if (isDraggingNode) {
            computeLayout();
            const int numNodes = static_cast<int>(cachedNodes.size());
            const bool canRemoveNode = (dragCandidateSlot >= 0 && dragCandidateSlot < numNodes &&
                                        cachedNodes[static_cast<size_t>(dragCandidateSlot)].kind == ChainNodeKind::PedalSlot);
            if (canRemoveNode && (dragCurrentPos.y < -24 || dragCurrentPos.y > getHeight() + 24)) {
                dropHoverSlot = -2;
                repaint();
                return;
            }
            const int candidateTarget = findClosestSlot(dragCurrentPos);
            dropHoverSlot = candidateTarget;

            if (candidateTarget >= 0 && candidateTarget < numNodes && candidateTarget != dragCandidateSlot) {
                const auto targetRect = iconRects[static_cast<size_t>(candidateTarget)];
                const int distFromCenter = std::abs(dragCurrentPos.x - targetRect.getCentreX());
                if (distFromCenter <= std::max(10, targetRect.getWidth() * 38 / 100)) {
                    processor.moveDynamicChainNode(dragCandidateSlot, candidateTarget);
                    dragCandidateSlot = candidateTarget;
                    selectedChainIndex = candidateTarget;
                    computeLayout();
                }
            }
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override {
        if (processor.getRoutingMode() != RoutingMode::Serial) return;
        computeLayout();
        const int numNodes = static_cast<int>(cachedNodes.size());

        if (isDraggingNode && dragCandidateSlot >= 0 && dragCandidateSlot < numNodes) {
            const auto& draggedNode = cachedNodes[static_cast<size_t>(dragCandidateSlot)];
            if (draggedNode.kind == ChainNodeKind::PedalSlot &&
                (e.getPosition().y < -24 || e.getPosition().y > getHeight() + 24)) {
                processor.removePedalSlot(draggedNode.slotId);
            } else {
                const int targetSlot = findClosestSlot(e.getPosition());
                if (targetSlot >= 0 && targetSlot < numNodes && targetSlot != dragCandidateSlot) {
                    processor.moveDynamicChainNode(dragCandidateSlot, targetSlot);
                    selectedChainIndex = targetSlot;
                }
            }
            isDraggingNode = false;
            dragCandidateSlot = -1;
            dropHoverSlot = -1;
            pillClickCandidateSlot = -1;
            repaint();
            return;
        }

        if (pillClickCandidateSlot >= 0 && pillClickCandidateSlot < numNodes && e.getDistanceFromDragStart() <= 5) {
            toggleSerialNodeBypass(cachedNodes[static_cast<size_t>(pillClickCandidateSlot)]);
        }

        isDraggingNode = false;
        pillClickCandidateSlot = -1;
        dragCandidateSlot = -1;
        dropHoverSlot = -1;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        juce::ColourGradient bg(juce::Colour(0xff1e222c), 0.0f, 0.0f,
                                juce::Colour(0xff151820), 0.0f, bounds.getHeight(), false);
        g.setGradientFill(bg);
        g.fillRect(bounds);

        g.setColour(juce::Colour(0xff2a303e));
        g.drawHorizontalLine(0, 0.0f, bounds.getWidth());
        g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

        const auto mode = processor.getRoutingMode();
        computeLayout();

        // 1. Left Column: Exact 4 AmpliTube-style Routing Topology Buttons
        g.setColour(juce::Colour(0xff10131a));
        g.fillRect(0.0f, 1.0f, 40.0f, bounds.getHeight() - 2.0f);
        g.setColour(juce::Colour(0xff252b38));
        g.drawVerticalLine(40, 1.0f, bounds.getHeight() - 1.0f);

        for (int i = 0; i < 4; ++i) {
            const auto r = routeBtnRects[static_cast<size_t>(i)].toFloat();
            const bool active = (static_cast<int>(mode) == i);
            const bool hov    = (hoveredRouteBtn == i);

            g.setColour(active ? juce::Colour(0xff11141b)
                               : (hov ? juce::Colour(0xff343944) : juce::Colour(0xff292d36)));
            g.fillRoundedRectangle(r, 4.0f);

            g.setColour(active ? juce::Colour(0xfff59e0b).withAlpha(0.55f)
                               : juce::Colour(hov ? 0xff4b5366 : 0xff363b47));
            g.drawRoundedRectangle(r, 4.0f, active ? 1.0f : 0.8f);

            drawTopologyIcon(g, r, i, active ? juce::Colour(0xfff59e0b) : juce::Colour(0xffe2e8f0));
        }

        // 2. Render either Serial 1-lane view OR true Multi-Lane Branching Parallel Topology!
        if (isParallelRouting(mode)) {
            paintParallelDiagram(g, mode);
        } else {
            paintSerialStrip(g);
        }
    }

    void resized() override {
        computeLayout();
    }

private:
    struct VisualNode {
        juce::Rectangle<int> bounds;
        juce::Rectangle<int> bypassRect;
        juce::Rectangle<int> iconRect;
        juce::Rectangle<int> menuRect;
        ChainNodeKind kind = ChainNodeKind::AmpHead;
        int chainIndex = -1; // >= 0 if mapped to cachedNodes[chainIndex]
        int slotId = -1;
        int rigIndex = 0;    // 0 = Path A, 1 = Path B
        ModuleId mod = ModuleId::AmpHead;
        int modelIdx = 0;
        bool active = true;
        BusLane lane = BusLane::PathA;
        juce::Colour iconAccentCol { juce::Colours::transparentBlack };
        juce::String badgeText;
        juce::String modelName;
    };

    void paintSerialStrip(juce::Graphics& g) {
        const int numNodes = static_cast<int>(cachedNodes.size());

        if (numNodes > 0) {
            const float wireY = static_cast<float>(iconRects[0].getCentreY());
            const float leftEdge  = static_cast<float>(chainAreaRect.getX()) + 4.0f;
            const float rightEdge = static_cast<float>(chainAreaRect.getRight()) - 4.0f;
            const float wireX0 = std::min(leftEdge + 22.0f, static_cast<float>(iconRects[0].getX()) - 6.0f);
            const float wireX1 = std::max(rightEdge - 26.0f, static_cast<float>(iconRects[static_cast<size_t>(numNodes - 1)].getRight()) + 6.0f);

            g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.20f));
            g.drawLine(wireX0, wireY, wireX1, wireY, 5.0f);
            g.setColour(juce::Colour(0xff60a5fa).withAlpha(0.76f));
            g.drawLine(wireX0, wireY, wireX1, wireY, 2.0f);

            if (iconRects[0].getX() - chainAreaRect.getX() >= 28) {
                drawJackPill(g, { leftEdge, wireY - 8.5f, 22.0f, 17.0f }, "IN", juce::Colour(0xff38bdf8));
            }
            if (chainAreaRect.getRight() - iconRects[static_cast<size_t>(numNodes - 1)].getRight() >= 32) {
                drawJackPill(g, { rightEdge - 26.0f, wireY - 8.5f, 26.0f, 17.0f }, "OUT", juce::Colour(0xff22c55e));
            }
        }

        for (int i = 0; i < numNodes; ++i) {
            const auto& nd = cachedNodes[static_cast<size_t>(i)];
            ModuleId mod = ModuleId::Overdrive;
            int modelIdx = 0;
            bool active = true;
            juce::Colour customCol = juce::Colours::transparentBlack;
            resolveNodeDisplayInfo(nd, mod, modelIdx, active, customCol);
            const auto& entry = gp200::getModelEntry(mod, modelIdx);
            const bool isNamPedal = (nd.kind == ChainNodeKind::PedalSlot && isNamPedalSlotModel(mod, modelIdx));
            const juce::String pillCode = isNamPedal ? juce::String("NAM") : juce::String(valeton::gpCodeFor(mod));
            const bool isSelected = (i == selectedChainIndex);
            const bool isStageMatch = (tabForNode(nd) == activeTab);

            // Row 1: Top ON/OFF Pill
            auto pr = pillRects[static_cast<size_t>(i)].toFloat();
            if (active) {
                juce::ColourGradient pillGrad(juce::Colour(0xff3b82f6), pr.getX(), pr.getY(),
                                              juce::Colour(0xff1d4ed8), pr.getX(), pr.getBottom(), false);
                g.setGradientFill(pillGrad);
            } else {
                g.setColour(valeton::pillOffSlate());
            }
            g.fillRoundedRectangle(pr, 4.5f);
            g.setColour(active ? juce::Colour(0xff60a5fa).withAlpha(0.75f) : juce::Colour(0xff4c5262));
            g.drawRoundedRectangle(pr, 4.5f, 0.8f);
            g.setColour(active ? juce::Colours::white : juce::Colour(0xff9aa3b2));
            g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
            g.drawText(pillCode, pr, juce::Justification::centred);

            // Row 2: Stylized Mini Pedal Icon (zero color highlight when bypassed!)
            auto ir = iconRects[static_cast<size_t>(i)].toFloat();
            if (isSelected) {
                g.setColour(active ? valeton::selectHaloBlue().withAlpha(0.58f)
                                   : juce::Colour(0xff2a303c).withAlpha(0.72f));
                g.fillRoundedRectangle(ir.reduced(2.0f, 0.0f), 6.0f);
                g.setColour(active ? juce::Colour(0xff60a5fa).withAlpha(0.75f)
                                   : juce::Colour(0xff475569).withAlpha(0.65f));
                g.drawRoundedRectangle(ir.reduced(2.0f, 0.0f), 6.0f, 1.0f);
            } else if (isStageMatch && active) {
                g.setColour(valeton::selectHaloBlue().withAlpha(0.16f));
                g.fillRoundedRectangle(ir.reduced(2.0f, 0.0f), 6.0f);
            }

            const float maxIconW = (mod == ModuleId::AmpHead)
                ? (ir.getHeight() * 1.18f)
                : ((mod == ModuleId::CabinetIR) ? (ir.getHeight() * 0.98f) : (ir.getHeight() * 0.84f));
            const float iconW = std::min(ir.getWidth() - 6.0f, maxIconW);
            const auto iconBox = ir.withSizeKeepingCentre(iconW, std::max(24.0f, ir.getHeight() - 4.0f));
            valeton::drawStylizedPedalIcon(g, iconBox, mod, active,
                                           active ? customCol : juce::Colour(0xff343a47),
                                           pillCode);

            if (dropHoverSlot == i) {
                g.setColour(juce::Colour(0xff38bdf8).withAlpha(0.22f));
                g.fillRoundedRectangle(ir.expanded(1.5f), 6.0f);
                g.setColour(juce::Colour(0xff38bdf8));
                g.drawRoundedRectangle(ir.expanded(1.5f), 6.0f, 2.2f);
                g.drawRoundedRectangle(pr.expanded(1.5f), 4.5f, 1.8f);
            }

            // Row 3: Model Dropdown Box
            auto dr = dropdownRects[static_cast<size_t>(i)].toFloat();
            g.setColour(juce::Colour(0xff262a35));
            g.fillRoundedRectangle(dr, 3.0f);
            g.setColour(juce::Colour(0xff3f4656));
            g.drawRoundedRectangle(dr, 3.0f, 0.8f);

            juce::String displayLabel = juce::String::fromUTF8(entry.name);
            if (isNamPedal) {
                const auto& runner = processor.getSignalChain().getSlotNamRunner(nd.slotId);
                if (runner.hasLoadedModel()) {
                    displayLabel = runner.getDisplayName();
                }
            }
            g.setColour(active ? juce::Colour(0xffe5e9f2) : juce::Colour(0xff7f8a9c));
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::plain)));
            g.drawText(displayLabel, dr.reduced(4.0f, 0.0f).withTrimmedRight(10.0f),
                       juce::Justification::centredLeft, true);

            const float ax = dr.getRight() - 6.5f;
            const float ay = dr.getCentreY();
            juce::Path tri;
            tri.addTriangle(ax - 2.8f, ay - 1.8f, ax + 2.8f, ay - 1.8f, ax, ay + 2.4f);
            g.setColour(juce::Colour(0xff94a3b8));
            g.fillPath(tri);
        }

        // Sidebar drag insertion marker
        if (sidebarDragActive) {
            const int insIdx = findInsertionChainIndex(sidebarDragPt);
            float lineX = 48.0f;
            if (numNodes > 0) {
                if (insIdx <= 0) lineX = static_cast<float>(iconRects[0].getX() - 3);
                else if (insIdx >= numNodes) lineX = static_cast<float>(iconRects[static_cast<size_t>(numNodes - 1)].getRight() + 3);
                else lineX = 0.5f * static_cast<float>(iconRects[static_cast<size_t>(insIdx - 1)].getRight()
                                                       + iconRects[static_cast<size_t>(insIdx)].getX());
            }
            g.setColour(juce::Colour(0xff38bdf8));
            g.drawLine(lineX, 6.0f, lineX, static_cast<float>(getHeight()) - 6.0f, 3.0f);
        }

        // Floating drag ghost
        if (isDraggingNode && dragCandidateSlot >= 0 && dragCandidateSlot < numNodes) {
            ModuleId draggedMod = ModuleId::Overdrive;
            int mIdx = 0;
            bool act = true;
            juce::Colour cCol = juce::Colours::transparentBlack;
            resolveNodeDisplayInfo(cachedNodes[static_cast<size_t>(dragCandidateSlot)], draggedMod, mIdx, act, cCol);

            const auto ghostRect = juce::Rectangle<float>(
                static_cast<float>(dragCurrentPos.x) - 34.0f,
                static_cast<float>(dragCurrentPos.y) - 36.0f,
                68.0f, 72.0f
            );
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRoundedRectangle(ghostRect.translated(0.0f, 4.0f), 7.0f);

            const bool removeOutside = (dropHoverSlot == -2);
            g.setColour(removeOutside ? juce::Colour(0xff7f1d1d).withAlpha(0.95f)
                                      : juce::Colour(0xff172033).withAlpha(0.95f));
            g.fillRoundedRectangle(ghostRect, 7.0f);
            g.setColour(removeOutside ? juce::Colour(0xffef4444) : juce::Colour(0xff38bdf8));
            g.drawRoundedRectangle(ghostRect, 7.0f, 1.8f);

            const auto& draggedNd = cachedNodes[static_cast<size_t>(dragCandidateSlot)];
            const bool draggedIsNam = (draggedNd.kind == ChainNodeKind::PedalSlot && isNamPedalSlotModel(draggedMod, mIdx));
            const juce::String draggedCode = draggedIsNam ? juce::String("NAM") : juce::String(valeton::gpCodeFor(draggedMod));

            auto iconBox = ghostRect.withTrimmedBottom(18.0f).reduced(6.0f, 4.0f);
            valeton::drawStylizedPedalIcon(g, iconBox, draggedMod, true, cCol, draggedCode);

            const juce::String targetTxt = removeOutside
                ? "REMOVE"
                : ((dropHoverSlot >= 0)
                       ? (draggedCode + " -> #" + juce::String(dropHoverSlot + 1))
                       : draggedCode);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
            g.drawText(targetTxt, ghostRect.withTop(ghostRect.getBottom() - 17.0f), juce::Justification::centred);
        }
    }

    void paintParallelDiagram(juce::Graphics& g, RoutingMode mode) {
        const auto colPre  = juce::Colour(0xff94a3b8);
        const auto colA    = juce::Colour(0xfff59e0b);
        const auto colB    = juce::Colour(0xff38bdf8);
        const auto colC    = juce::Colour(0xffa855f7);
        const auto colPost = juce::Colour(0xff22c55e);

        auto drawWire = [&](float x0, float y0, float x1, float y1, juce::Colour col) {
            g.setColour(col.withAlpha(0.18f));
            g.drawLine(x0, y0, x1, y1, 4.6f);
            g.setColour(col.withAlpha(0.80f));
            g.drawLine(x0, y0, x1, y1, 1.8f);
        };

        auto drawBezierBranch = [&](float x0, float y0, float x1, float y1, juce::Colour col) {
            juce::Path p;
            p.startNewSubPath(x0, y0);
            const float mx = (x0 + x1) * 0.5f;
            p.cubicTo(mx, y0, mx, y1, x1, y1);
            g.setColour(col.withAlpha(0.18f));
            g.strokePath(p, juce::PathStrokeType(4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(col.withAlpha(0.82f));
            g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        // Subtle highlight of whichever StompBoard lane is currently selected when viewing STOMPBOARD!
        if (!sidebarDragActive && activeTab == StageViewTab::Stomp && highlightedStompLane >= 0) {
            auto paintActiveLaneGlow = [&](juce::Rectangle<int> zr, juce::Colour c) {
                if (zr.isEmpty()) return;
                g.setColour(c.withAlpha(0.065f));
                g.fillRoundedRectangle(zr.toFloat().reduced(2.0f, 1.0f), 6.0f);
                g.setColour(c.withAlpha(0.26f));
                g.drawRoundedRectangle(zr.toFloat().reduced(2.0f, 1.0f), 6.0f, 0.9f);
            };
            if (highlightedStompLane == static_cast<int>(BusLane::Pre))   paintActiveLaneGlow(preZoneRect,   colPre);
            if (highlightedStompLane == static_cast<int>(BusLane::PathA)) paintActiveLaneGlow(laneAZoneRect, colA);
            if (highlightedStompLane == static_cast<int>(BusLane::PathB)) paintActiveLaneGlow(laneBZoneRect, colB);
            if (highlightedStompLane == static_cast<int>(BusLane::PathC)) paintActiveLaneGlow(laneCZoneRect, colC);
            if (highlightedStompLane == static_cast<int>(BusLane::Post))  paintActiveLaneGlow(postZoneRect,  colPost);
        }

        // Subtle lane drop-zone highlight while dragging from GearSidebar!
        if (sidebarDragActive) {
            auto highlightZone = [&](juce::Rectangle<int> zr, juce::Colour c, const char* lbl) {
                if (!zr.isEmpty() && zr.contains(sidebarDragPt)) {
                    g.setColour(c.withAlpha(0.13f));
                    g.fillRoundedRectangle(zr.toFloat().reduced(2.0f), 6.0f);
                    g.setColour(c.withAlpha(0.72f));
                    g.drawRoundedRectangle(zr.toFloat().reduced(2.0f), 6.0f, 1.3f);
                    g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
                    g.drawText(juce::String("+ DROP TO ") + lbl, zr.toFloat().reduced(6.0f, 2.0f).withHeight(12.0f),
                               juce::Justification::centredRight);
                }
            };
            if (mode == RoutingMode::DualIndependent) {
                if (sidebarDragCat == ModuleId::AmpHead || sidebarDragCat == ModuleId::CabinetIR) {
                    highlightZone(laneAZoneRect, colA, "CHAIN A RIG");
                    highlightZone(laneBZoneRect, colB, "CHAIN B RIG");
                } else {
                    highlightZone(preAZoneRect,  colA, "CHAIN A (PRE)");
                    highlightZone(postAZoneRect, colA, "CHAIN A (POST)");
                    highlightZone(preBZoneRect,  colB, "CHAIN B (PRE)");
                    highlightZone(postBZoneRect, colB, "CHAIN B (POST)");
                }
            } else {
                highlightZone(preZoneRect,   colPre,  "PRE");
                highlightZone(laneAZoneRect, colA,    "PATH A");
                highlightZone(laneBZoneRect, colB,    "PATH B");
                if (mode == RoutingMode::TripleParallel) {
                    highlightZone(laneCZoneRect, colC, "PATH C");
                }
                highlightZone(postZoneRect,  colPost, "POST");
            }
        }

        // Draw exact routing topology cables matching the active icon!
        if (mode == RoutingMode::DualIndependent) {
            // Mode 3 [==]: Two completely independent parallel horizontal lanes (NO split, NO merge!)
            const float xIn  = static_cast<float>(chainAreaRect.getX()) + 30.0f;
            const float xOut = static_cast<float>(chainAreaRect.getRight()) - 36.0f;

            drawWire(xIn, laneAY, xOut, laneAY, colA);
            drawWire(xIn, laneBY, xOut, laneBY, colB);

            drawJackPill(g, { static_cast<float>(chainAreaRect.getX()) + 2.0f, laneAY - 8.5f, 28.0f, 17.0f }, "IN A", colA);
            drawJackPill(g, { static_cast<float>(chainAreaRect.getX()) + 2.0f, laneBY - 8.5f, 28.0f, 17.0f }, "IN B", colB);
            drawJackPill(g, { xOut + 2.0f, laneAY - 8.5f, 34.0f, 17.0f }, "OUT A", colA);
            drawJackPill(g, { xOut + 2.0f, laneBY - 8.5f, 34.0f, 17.0f }, "OUT B", colB);
        } else if (mode == RoutingMode::DualParallel) {
            // Mode 1 [-<]: Shared Pre -> Y-Split -> Path A & Path B -> Y-Merge (MIX) -> Shared Post -> OUT
            const float xIn  = static_cast<float>(chainAreaRect.getX()) + 26.0f;
            const float xOut = static_cast<float>(chainAreaRect.getRight()) - 30.0f;

            drawWire(xIn, centerY, splitX0, centerY, colPre);
            drawBezierBranch(splitX0, centerY, splitX1, laneAY, colA);
            drawBezierBranch(splitX0, centerY, splitX1, laneBY, colB);
            drawWire(splitX1, laneAY, mergeX0, laneAY, colA);
            drawWire(splitX1, laneBY, mergeX0, laneBY, colB);
            drawBezierBranch(mergeX0, laneAY, mergeX1, centerY, colA);
            drawBezierBranch(mergeX0, laneBY, mergeX1, centerY, colB);
            drawWire(mergeX1, centerY, xOut, centerY, colPost);

            // Split junction dot
            g.setColour(juce::Colour(0xffe2e8f0));
            g.fillEllipse(splitX0 - 3.0f, centerY - 3.0f, 6.0f, 6.0f);

            drawJackPill(g, { static_cast<float>(chainAreaRect.getX()) + 2.0f, centerY - 8.5f, 24.0f, 17.0f }, "IN", juce::Colour(0xff38bdf8));
            drawJackPill(g, { xOut + 2.0f, centerY - 8.5f, 28.0f, 17.0f }, "OUT", colPost);
        } else if (mode == RoutingMode::TripleParallel) {
            // Mode 2 [-<=]: Shared Pre -> 3-Way Trident Split -> Path A (Top) / Path B (Mid) / Parallel FX (Bot) -> MIX -> OUT
            const float xIn  = static_cast<float>(chainAreaRect.getX()) + 26.0f;
            const float xOut = static_cast<float>(chainAreaRect.getRight()) - 30.0f;

            drawWire(xIn, centerY, splitX0, centerY, colPre);
            drawBezierBranch(splitX0, centerY, splitX1, laneAY, colA);
            drawWire(splitX0, centerY, splitX1, laneBY, colB);
            drawBezierBranch(splitX0, centerY, splitX1, laneCY, colC);

            drawWire(splitX1, laneAY, mergeX0, laneAY, colA);
            drawWire(splitX1, laneBY, mergeX0, laneBY, colB);
            drawWire(splitX1, laneCY, mergeX0, laneCY, colC);

            drawBezierBranch(mergeX0, laneAY, mergeX1, centerY, colA);
            drawWire(mergeX0, laneBY, mergeX1, centerY, colB);
            drawBezierBranch(mergeX0, laneCY, mergeX1, centerY, colC);
            drawWire(mergeX1, centerY, xOut, centerY, colPost);

            g.setColour(juce::Colour(0xffe2e8f0));
            g.fillEllipse(splitX0 - 3.0f, centerY - 3.0f, 6.0f, 6.0f);

            drawJackPill(g, { static_cast<float>(chainAreaRect.getX()) + 2.0f, centerY - 8.5f, 24.0f, 17.0f }, "IN", juce::Colour(0xff38bdf8));
            drawJackPill(g, { xOut + 2.0f, centerY - 8.5f, 28.0f, 17.0f }, "OUT", colPost);
        }

        // Draw MIX summing pill (clickable -> opens Console Mixer!)
        if (!mixBadgeRect.isEmpty()) {
            auto mr = mixBadgeRect.toFloat();
            g.setColour(hoveredMixNode ? juce::Colour(0xff1e293b) : juce::Colour(0xff111622));
            g.fillRoundedRectangle(mr, 5.0f);
            g.setColour(hoveredMixNode ? juce::Colour(0xfff59e0b) : juce::Colour(0xff475569));
            g.drawRoundedRectangle(mr, 5.0f, 1.1f);
            g.setColour(hoveredMixNode ? juce::Colours::white : juce::Colour(0xffcbd5e1));
            g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
            g.drawText("MIX", mr, juce::Justification::centred);
        }

        // Draw all interactive multi-lane hardware node cards using exact Valeton stylized mini pedal/amp/cab assets!
        for (const auto& vn : parallelNodes) {
            paintStylizedParallelNode(g, vn, mode == RoutingMode::TripleParallel);
        }
    }

    void paintStylizedParallelNode(juce::Graphics& g, const VisualNode& vn, bool isTriple) {
        const bool active = vn.active;
        const bool isFocusedTab = (tabForModule(vn.mod) == activeTab);

        // 1. Top ON/OFF Pill (click to toggle bypass — zero color highlight when OFF!)
        auto pr = vn.bypassRect.toFloat();
        if (active) {
            juce::ColourGradient pillGrad(juce::Colour(0xff3b82f6), pr.getX(), pr.getY(),
                                          juce::Colour(0xff1d4ed8), pr.getX(), pr.getBottom(), false);
            g.setGradientFill(pillGrad);
        } else {
            g.setColour(valeton::pillOffSlate());
        }
        g.fillRoundedRectangle(pr, 3.8f);
        g.setColour(active ? juce::Colour(0xff60a5fa).withAlpha(0.75f) : juce::Colour(0xff4c5262));
        g.drawRoundedRectangle(pr, 3.8f, 0.75f);
        g.setColour(active ? juce::Colours::white : juce::Colour(0xff9aa3b2));
        g.setFont(juce::Font(juce::FontOptions(isTriple ? 7.4f : 8.2f, juce::Font::bold)));
        g.drawText(vn.badgeText, pr, juce::Justification::centred, true);

        // 2. Middle Stylized Mini Pedal / Amp / Cab Hardware Asset (riding directly on the signal cable!)
        auto ir = vn.iconRect.toFloat();
        if (isFocusedTab && active) {
            g.setColour(valeton::selectHaloBlue().withAlpha(0.16f));
            g.fillRoundedRectangle(ir.reduced(2.0f, 0.0f), 5.0f);
        }

        const float maxIconW = (vn.mod == ModuleId::AmpHead)
            ? (ir.getHeight() * 1.24f)
            : ((vn.mod == ModuleId::CabinetIR) ? (ir.getHeight() * 1.02f) : (ir.getHeight() * 0.88f));
        const float iconW = std::min(ir.getWidth() - 4.0f, maxIconW);
        const auto iconBox = ir.withSizeKeepingCentre(iconW, std::max(20.0f, ir.getHeight() - 2.0f));
        const auto iconCol = active ? vn.iconAccentCol : juce::Colour(0xff343a47);
        const juce::String iconCode = (vn.kind == ChainNodeKind::PedalSlot && isNamPedalSlotModel(vn.mod, vn.modelIdx))
            ? juce::String("NAM")
            : juce::String(valeton::gpCodeFor(vn.mod));
        valeton::drawStylizedPedalIcon(g, iconBox, vn.mod, active, iconCol, iconCode);

        // 3. Bottom Compact Model Dropdown Box (click to change GP-200 model or assign lane!)
        auto dr = vn.menuRect.toFloat();
        g.setColour(juce::Colour(0xff252934));
        g.fillRoundedRectangle(dr, 2.8f);
        g.setColour(juce::Colour(0xff3d4454));
        g.drawRoundedRectangle(dr, 2.8f, 0.75f);

        g.setColour(active ? juce::Colour(0xffe5e9f2) : juce::Colour(0xff7f8a9c));
        g.setFont(juce::Font(juce::FontOptions(isTriple ? 7.4f : 8.0f, juce::Font::plain)));
        g.drawText(vn.modelName, dr.reduced(3.5f, 0.0f).withTrimmedRight(9.0f),
                   juce::Justification::centredLeft, true);

        const float ax = dr.getRight() - 5.5f;
        const float ay = dr.getCentreY();
        juce::Path tri;
        tri.addTriangle(ax - 2.3f, ay - 1.4f, ax + 2.3f, ay - 1.4f, ax, ay + 1.9f);
        g.setColour(juce::Colour(0xff94a3b8));
        g.fillPath(tri);
    }

    static void drawJackPill(juce::Graphics& g, juce::Rectangle<float> r, const char* txt, juce::Colour col) {
        g.setColour(juce::Colour(0xff10141e));
        g.fillRoundedRectangle(r, 3.8f);
        g.setColour(col.withAlpha(0.65f));
        g.drawRoundedRectangle(r, 3.8f, 0.9f);
        g.setColour(col.brighter(0.30f));
        g.setFont(juce::Font(juce::FontOptions(7.4f, juce::Font::bold)));
        g.drawText(txt, r, juce::Justification::centred);
    }

    // Exact 4 AmpliTube-style routing topology vector glyphs matching user's screenshot!
    static void drawTopologyIcon(juce::Graphics& g, juce::Rectangle<float> r, int idx, juce::Colour col) {
        g.setColour(col);
        const float cx = r.getCentreX();
        const float cy = r.getCentreY();
        auto arrowHead = [&](float tx, float ty) {
            juce::Path ah;
            ah.addTriangle(tx - 3.4f, ty - 2.3f, tx - 3.4f, ty + 2.3f, tx + 0.6f, ty);
            g.fillPath(ah);
        };
        const juce::PathStrokeType stroke(1.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        if (idx == 0) {
            // 1. Single Straight Arrow (->)
            g.drawLine(cx - 7.5f, cy, cx + 4.8f, cy, 1.6f);
            arrowHead(cx + 7.2f, cy);
        } else if (idx == 1) {
            // 2. Smooth 1-to-2 Split Arrow (-<)
            juce::Path topP;
            topP.startNewSubPath(cx - 7.5f, cy);
            topP.lineTo(cx - 3.2f, cy);
            topP.cubicTo(cx - 0.5f, cy, cx - 1.0f, cy - 4.2f, cx + 2.2f, cy - 4.2f);
            topP.lineTo(cx + 4.8f, cy - 4.2f);
            g.strokePath(topP, stroke);

            juce::Path botP;
            botP.startNewSubPath(cx - 3.2f, cy);
            botP.cubicTo(cx - 0.5f, cy, cx - 1.0f, cy + 4.2f, cx + 2.2f, cy + 4.2f);
            botP.lineTo(cx + 4.8f, cy + 4.2f);
            g.strokePath(botP, stroke);

            arrowHead(cx + 7.2f, cy - 4.2f);
            arrowHead(cx + 7.2f, cy + 4.2f);
        } else if (idx == 2) {
            // 3. Smooth 1-to-3 Trident Split Arrow (-<=)
            g.drawLine(cx - 7.5f, cy, cx + 4.8f, cy, 1.5f);
            arrowHead(cx + 7.2f, cy);

            juce::Path topP;
            topP.startNewSubPath(cx - 3.5f, cy);
            topP.cubicTo(cx - 0.6f, cy, cx - 1.0f, cy - 5.2f, cx + 2.2f, cy - 5.2f);
            topP.lineTo(cx + 4.8f, cy - 5.2f);
            g.strokePath(topP, stroke);

            juce::Path botP;
            botP.startNewSubPath(cx - 3.5f, cy);
            botP.cubicTo(cx - 0.6f, cy, cx - 1.0f, cy + 5.2f, cx + 2.2f, cy + 5.2f);
            botP.lineTo(cx + 4.8f, cy + 5.2f);
            g.strokePath(botP, stroke);

            arrowHead(cx + 7.2f, cy - 5.2f);
            arrowHead(cx + 7.2f, cy + 5.2f);
        } else {
            // 4. Two Separate Parallel Horizontal Arrows (==>)
            g.drawLine(cx - 7.5f, cy - 3.2f, cx + 4.8f, cy - 3.2f, 1.6f);
            arrowHead(cx + 7.2f, cy - 3.2f);
            g.drawLine(cx - 7.5f, cy + 3.2f, cx + 4.8f, cy + 3.2f, 1.6f);
            arrowHead(cx + 7.2f, cy + 3.2f);
        }
    }

    VisualNode makeVisualNode(const ChainNode& nd, int chainIdx, int rigIdx, BusLane laneOverride, const juce::String& tagOverride = {}) const {
        VisualNode vn;
        vn.kind = nd.kind;
        vn.chainIndex = chainIdx;
        vn.slotId = nd.slotId;
        vn.rigIndex = rigIdx;
        vn.lane = laneOverride;

        if (nd.kind == ChainNodeKind::AmpHead && rigIdx == 1) {
            vn.mod = ModuleId::AmpHead;
            vn.modelIdx = getCoreModelIndex(ModuleId::AmpHead, 1);
            vn.active = !isParamBypassed(params::kAmpBBypass);
            vn.iconAccentCol = juce::Colour(0xff38bdf8); // Distinct sky-blue faceplate tint for AMP B!
        } else if (nd.kind == ChainNodeKind::CabinetIR && rigIdx == 1) {
            vn.mod = ModuleId::CabinetIR;
            vn.modelIdx = getCoreModelIndex(ModuleId::CabinetIR, 1);
            vn.active = !isParamBypassed(params::kCabBBypass);
            vn.iconAccentCol = juce::Colour(0xff38bdf8); // Distinct sky-blue piping for CAB B!
        } else if (nd.kind == ChainNodeKind::AmpHead && rigIdx == 2) {
            vn.mod = ModuleId::AmpHead;
            vn.modelIdx = getCoreModelIndex(ModuleId::AmpHead, 2);
            vn.active = !isParamBypassed(params::kAmpCBypass);
            vn.iconAccentCol = juce::Colour(0xffa855f7); // Distinct violet faceplate tint for AMP C!
        } else if (nd.kind == ChainNodeKind::CabinetIR && rigIdx == 2) {
            vn.mod = ModuleId::CabinetIR;
            vn.modelIdx = getCoreModelIndex(ModuleId::CabinetIR, 2);
            vn.active = !isParamBypassed(params::kCabCBypass);
            vn.iconAccentCol = juce::Colour(0xffa855f7); // Distinct violet piping for CAB C!
        } else {
            juce::Colour customCol;
            resolveNodeDisplayInfo(nd, vn.mod, vn.modelIdx, vn.active, customCol);
            vn.iconAccentCol = customCol; // Preserves authentic gold AMP A, white CAB A, and per-model pedal colors!
        }

        const auto& entry = gp200::getModelEntry(vn.mod, vn.modelIdx);
        const bool isNamPedal = (nd.kind == ChainNodeKind::PedalSlot && isNamPedalSlotModel(vn.mod, vn.modelIdx));
        if (isNamPedal) {
            const auto& runner = processor.getSignalChain().getSlotNamRunner(nd.slotId);
            vn.modelName = runner.hasLoadedModel() ? runner.getDisplayName() : juce::String::fromUTF8(entry.name);
            vn.badgeText = tagOverride.isNotEmpty() ? tagOverride : juce::String("NAM");
        } else {
            vn.modelName = juce::String::fromUTF8(entry.name);
            vn.badgeText = tagOverride.isNotEmpty() ? tagOverride : juce::String(valeton::gpCodeFor(vn.mod));
        }
        return vn;
    }

    void layoutParallelNodesRow(std::vector<VisualNode>& rowNodes, int xStart, int xEnd, int yCenter, int cardH, int maxCardW) {
        const int n = static_cast<int>(rowNodes.size());
        if (n <= 0 || xEnd <= xStart + 20) return;
        const int availW = xEnd - xStart;
        const int gap = (n >= 5) ? 4 : 6;
        const int cardW = std::clamp((availW - gap * (n - 1)) / n, 46, maxCardW);
        const int totalW = cardW * n + gap * (n - 1);
        int x = xStart + std::max(0, (availW - totalW) / 2);

        const int pillH = (cardH < 56) ? 11 : ((cardH < 72) ? 13 : 16);
        const int dropH = (cardH < 56) ? 12 : ((cardH < 72) ? 14 : 17);

        for (auto& vn : rowNodes) {
            auto col = juce::Rectangle<int>(x, yCenter - cardH / 2, cardW, cardH);
            vn.bounds = col;
            const int pillW = std::clamp(cardW - 8, 30, 56);
            vn.bypassRect = col.removeFromTop(pillH).withSizeKeepingCentre(pillW, pillH);
            vn.menuRect   = col.removeFromBottom(dropH).reduced(1, 0);
            col.reduce(0, 1);
            vn.iconRect   = col;
            parallelNodes.push_back(vn);
            x += cardW + gap;
        }
    }

    void computeLayout() {
        cachedNodes = processor.getDynamicChainNodes();
        const int numNodes = static_cast<int>(cachedNodes.size());
        const auto mode = processor.getRoutingMode();
        auto area = getLocalBounds();

        // Left 4 Routing Topology Buttons (fixed crisp square matte buttons, vertically centered — never stretches!)
        auto leftCol = area.removeFromLeft(40);
        const int btnGap = 5;
        const int btnSz  = std::clamp((leftCol.getHeight() - 12 - btnGap * 3) / 4, 22, 28);
        const int stackH = btnSz * 4 + btnGap * 3;
        int btnY = leftCol.getCentreY() - stackH / 2;
        const int btnX = leftCol.getCentreX() - btnSz / 2;
        for (int i = 0; i < 4; ++i) {
            routeBtnRects[static_cast<size_t>(i)] = juce::Rectangle<int>(btnX, btnY, btnSz, btnSz);
            btnY += btnSz + btnGap;
        }

        auto chain = area.reduced(8, 5);
        chainAreaRect = chain;
        parallelNodes.clear();
        mixBadgeRect = {};
        preZoneRect = {};
        laneAZoneRect = {};
        laneBZoneRect = {};
        laneCZoneRect = {};
        postZoneRect = {};
        preAZoneRect = {};
        postAZoneRect = {};
        preBZoneRect = {};
        postBZoneRect = {};

        // Multi-Lane Parallel Layout Computation (Modes 1, 2, 3)
        if (isParallelRouting(mode)) {
            std::vector<VisualNode> preList, pathAList, pathBList, pathCList, postList;

            int ampAIdx = -1, cabAIdx = -1, modIdx = -1, dlyIdx = -1, rvbIdx = -1;
            for (int i = 0; i < numNodes; ++i) {
                const auto& nd = cachedNodes[static_cast<size_t>(i)];
                if (nd.kind == ChainNodeKind::AmpHead)    ampAIdx = i;
                if (nd.kind == ChainNodeKind::CabinetIR)  cabAIdx = i;
                if (nd.kind == ChainNodeKind::RackMod)    modIdx = i;
                if (nd.kind == ChainNodeKind::RackDelay)  dlyIdx = i;
                if (nd.kind == ChainNodeKind::RackReverb) rvbIdx = i;
            }

            if (mode == RoutingMode::DualIndependent) {
                // Mode 3 [==]:
                //   Top Full Lane A:    IN A -> [Pre A Pedals] -> AMP A -> CAB A -> [Post A Pedals] -> MOD A -> OUT A
                //   Bottom Full Lane B: IN B -> [Pre B Pedals] -> AMP B -> CAB B -> [Post B Pedals] -> DLY B -> RVB B -> OUT B
                for (int i = 0; i < numNodes; ++i) {
                    const auto& nd = cachedNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && (nd.bus == BusLane::Pre || nd.bus == BusLane::PathA)) {
                        pathAList.push_back(makeVisualNode(nd, i, 0, BusLane::PathA));
                    }
                }
                pathAList.push_back(makeVisualNode({ ChainNodeKind::AmpHead, 0, BusLane::PathA }, ampAIdx, 0, BusLane::PathA, "AMP A"));
                pathAList.push_back(makeVisualNode({ ChainNodeKind::CabinetIR, 0, BusLane::PathA }, cabAIdx, 0, BusLane::PathA, "CAB A"));
                for (int i = 0; i < numNodes; ++i) {
                    const auto& nd = cachedNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && (nd.bus == BusLane::PostA || nd.bus == BusLane::PathC)) {
                        pathAList.push_back(makeVisualNode(nd, i, 0, BusLane::PostA));
                    }
                }
                if (modIdx >= 0) pathAList.push_back(makeVisualNode(cachedNodes[static_cast<size_t>(modIdx)], modIdx, 0, BusLane::PostA, "MOD A"));

                for (int i = 0; i < numNodes; ++i) {
                    const auto& nd = cachedNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::PathB) {
                        pathBList.push_back(makeVisualNode(nd, i, 1, BusLane::PathB));
                    }
                }
                pathBList.push_back(makeVisualNode({ ChainNodeKind::AmpHead, 0, BusLane::PathB }, -1, 1, BusLane::PathB, "AMP B"));
                pathBList.push_back(makeVisualNode({ ChainNodeKind::CabinetIR, 0, BusLane::PathB }, -1, 1, BusLane::PathB, "CAB B"));
                for (int i = 0; i < numNodes; ++i) {
                    const auto& nd = cachedNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot && nd.bus == BusLane::Post) {
                        pathBList.push_back(makeVisualNode(nd, i, 1, BusLane::Post));
                    }
                }
                if (dlyIdx >= 0) pathBList.push_back(makeVisualNode(cachedNodes[static_cast<size_t>(dlyIdx)], dlyIdx, 1, BusLane::Post, "DLY B"));
                if (rvbIdx >= 0) pathBList.push_back(makeVisualNode(cachedNodes[static_cast<size_t>(rvbIdx)], rvbIdx, 1, BusLane::Post, "RVB B"));

                const int halfH = chain.getHeight() / 2;
                laneAZoneRect = chain.withHeight(halfH);
                laneBZoneRect = chain.withTop(chain.getY() + halfH);
                laneAY = static_cast<float>(laneAZoneRect.getCentreY());
                laneBY = static_cast<float>(laneBZoneRect.getCentreY());
                centerY = static_cast<float>(chain.getCentreY());

                const int cardH = std::clamp(halfH - 5, 58, 74);
                const int xL = chain.getX() + 38;
                const int xR = chain.getRight() - 44;
                const size_t startA = parallelNodes.size();
                layoutParallelNodesRow(pathAList, xL, xR, static_cast<int>(laneAY), cardH, 86);
                const size_t endA = parallelNodes.size();
                layoutParallelNodesRow(pathBList, xL, xR, static_cast<int>(laneBY), cardH, 86);
                const size_t endB = parallelNodes.size();

                // Split Chain A & Chain B into exact Pre-Amp (left of CAB right edge) and Post-Amp (right of CAB) drop zones!
                int splitAX = laneAZoneRect.getCentreX();
                for (size_t k = startA; k < endA; ++k) {
                    if (parallelNodes[k].kind == ChainNodeKind::CabinetIR) {
                        splitAX = parallelNodes[k].bounds.getRight() + 3;
                        break;
                    }
                }
                int splitBX = laneBZoneRect.getCentreX();
                for (size_t k = endA; k < endB; ++k) {
                    if (parallelNodes[k].kind == ChainNodeKind::CabinetIR) {
                        splitBX = parallelNodes[k].bounds.getRight() + 3;
                        break;
                    }
                }
                splitAX = std::clamp(splitAX, laneAZoneRect.getX() + 40, laneAZoneRect.getRight() - 40);
                splitBX = std::clamp(splitBX, laneBZoneRect.getX() + 40, laneBZoneRect.getRight() - 40);
                preAZoneRect  = laneAZoneRect.withRight(splitAX);
                postAZoneRect = laneAZoneRect.withLeft(splitAX);
                preBZoneRect  = laneBZoneRect.withRight(splitBX);
                postBZoneRect = laneBZoneRect.withLeft(splitBX);
            } else {
                // Mode 1 (Dual Split A/B) & Mode 2 (Triple Parallel A/B/C + Shared Post)
                for (int i = 0; i < numNodes; ++i) {
                    const auto& nd = cachedNodes[static_cast<size_t>(i)];
                    if (nd.kind == ChainNodeKind::PedalSlot) {
                        if (nd.bus == BusLane::Pre) {
                            preList.push_back(makeVisualNode(nd, i, 0, BusLane::Pre));
                        } else if (nd.bus == BusLane::PathA) {
                            pathAList.push_back(makeVisualNode(nd, i, 0, BusLane::PathA));
                        } else if (nd.bus == BusLane::PathB) {
                            pathBList.push_back(makeVisualNode(nd, i, 1, BusLane::PathB));
                        } else if (nd.bus == BusLane::PathC) {
                            if (mode == RoutingMode::TripleParallel) {
                                pathCList.push_back(makeVisualNode(nd, i, 2, BusLane::PathC));
                            } else {
                                pathBList.push_back(makeVisualNode(nd, i, 1, BusLane::PathB));
                            }
                        } else {
                            postList.push_back(makeVisualNode(nd, i, 0, BusLane::Post));
                        }
                    }
                }
                pathAList.push_back(makeVisualNode({ ChainNodeKind::AmpHead, 0, BusLane::PathA }, ampAIdx, 0, BusLane::PathA, "AMP A"));
                pathAList.push_back(makeVisualNode({ ChainNodeKind::CabinetIR, 0, BusLane::PathA }, cabAIdx, 0, BusLane::PathA, "CAB A"));

                pathBList.push_back(makeVisualNode({ ChainNodeKind::AmpHead, 0, BusLane::PathB }, -1, 1, BusLane::PathB, "AMP B"));
                pathBList.push_back(makeVisualNode({ ChainNodeKind::CabinetIR, 0, BusLane::PathB }, -1, 1, BusLane::PathB, "CAB B"));

                if (mode == RoutingMode::TripleParallel) {
                    pathCList.push_back(makeVisualNode({ ChainNodeKind::AmpHead, 0, BusLane::PathC }, -1, 2, BusLane::PathC, "AMP C"));
                    pathCList.push_back(makeVisualNode({ ChainNodeKind::CabinetIR, 0, BusLane::PathC }, -1, 2, BusLane::PathC, "CAB C"));
                }

                if (modIdx >= 0) postList.push_back(makeVisualNode(cachedNodes[static_cast<size_t>(modIdx)], modIdx, 0, BusLane::Post, "MOD"));
                if (dlyIdx >= 0) postList.push_back(makeVisualNode(cachedNodes[static_cast<size_t>(dlyIdx)], dlyIdx, 0, BusLane::Post, "DLY"));
                if (rvbIdx >= 0) postList.push_back(makeVisualNode(cachedNodes[static_cast<size_t>(rvbIdx)], rvbIdx, 0, BusLane::Post, "RVB"));

                const int xIn  = chain.getX() + 30;
                const int xOut = chain.getRight() - 34;
                const int availW = std::max(240, xOut - xIn);
                centerY = static_cast<float>(chain.getCentreY());

                const int preW = preList.empty()
                    ? 36
                    : std::clamp(static_cast<int>(preList.size()) * 84 + 10, 82, availW * 25 / 100);
                const int curveW = (mode == RoutingMode::TripleParallel) ? 20 : 24;
                const int mixW = 28;

                if (mode == RoutingMode::DualParallel) {
                    const int postW = std::clamp(static_cast<int>(postList.size()) * 84 + 10, 156, availW * 33 / 100);
                    splitX0 = static_cast<float>(xIn + preW);
                    splitX1 = splitX0 + static_cast<float>(curveW);
                    mergeX1 = static_cast<float>(xOut - postW - mixW);
                    mergeX0 = mergeX1 - static_cast<float>(curveW);

                    const int halfH = chain.getHeight() / 2;
                    preZoneRect   = juce::Rectangle<int>(xIn, chain.getY(), preW, chain.getHeight());
                    laneAZoneRect = juce::Rectangle<int>(static_cast<int>(splitX1), chain.getY(), static_cast<int>(mergeX0 - splitX1), halfH);
                    laneBZoneRect = juce::Rectangle<int>(static_cast<int>(splitX1), chain.getY() + halfH, static_cast<int>(mergeX0 - splitX1), chain.getHeight() - halfH);
                    postZoneRect  = juce::Rectangle<int>(static_cast<int>(mergeX1) + mixW, chain.getY(), xOut - (static_cast<int>(mergeX1) + mixW), chain.getHeight());

                    laneAY = static_cast<float>(laneAZoneRect.getCentreY());
                    laneBY = static_cast<float>(laneBZoneRect.getCentreY());
                    mixBadgeRect = juce::Rectangle<int>(static_cast<int>(mergeX1) - 2, static_cast<int>(centerY) - 12, mixW, 24);

                    const int cardH = std::clamp(halfH - 5, 58, 74);
                    const int centerCardH = std::clamp(cardH + 10, 68, 84);
                    layoutParallelNodesRow(preList,   xIn + 4, static_cast<int>(splitX0) - 6, static_cast<int>(centerY), centerCardH, 84);
                    layoutParallelNodesRow(pathAList, static_cast<int>(splitX1) + 4, static_cast<int>(mergeX0) - 4, static_cast<int>(laneAY), cardH, 86);
                    layoutParallelNodesRow(pathBList, static_cast<int>(splitX1) + 4, static_cast<int>(mergeX0) - 4, static_cast<int>(laneBY), cardH, 86);
                    layoutParallelNodesRow(postList,  mixBadgeRect.getRight() + 6, xOut - 4, static_cast<int>(centerY), centerCardH, 84);
                } else {
                    // Mode 2: True Triple Parallel (Shared Pre -> Path A / Path B / Path C -> MIX -> Shared Post -> OUT)
                    const int postW = std::clamp(static_cast<int>(postList.size()) * 72 + 10, 146, availW * 31 / 100);
                    splitX0 = static_cast<float>(xIn + preW);
                    splitX1 = splitX0 + static_cast<float>(curveW);
                    mergeX1 = static_cast<float>(xOut - postW - mixW);
                    mergeX0 = mergeX1 - static_cast<float>(curveW);

                    const int rowH = chain.getHeight() / 3;
                    preZoneRect   = juce::Rectangle<int>(xIn, chain.getY(), preW, chain.getHeight());
                    laneAZoneRect = juce::Rectangle<int>(static_cast<int>(splitX1), chain.getY(), static_cast<int>(mergeX0 - splitX1), rowH);
                    laneBZoneRect = juce::Rectangle<int>(static_cast<int>(splitX1), chain.getY() + rowH, static_cast<int>(mergeX0 - splitX1), rowH);
                    laneCZoneRect = juce::Rectangle<int>(static_cast<int>(splitX1), chain.getY() + rowH * 2, static_cast<int>(mergeX0 - splitX1), chain.getHeight() - rowH * 2);
                    postZoneRect  = juce::Rectangle<int>(static_cast<int>(mergeX1) + mixW, chain.getY(), xOut - (static_cast<int>(mergeX1) + mixW), chain.getHeight());

                    laneAY = static_cast<float>(laneAZoneRect.getCentreY());
                    laneBY = static_cast<float>(laneBZoneRect.getCentreY());
                    laneCY = static_cast<float>(laneCZoneRect.getCentreY());
                    mixBadgeRect = juce::Rectangle<int>(static_cast<int>(mergeX1) - 2, static_cast<int>(centerY) - 13, mixW, 26);

                    const int cardH = std::clamp(rowH - 2, 44, 52);
                    const int sideCardH = std::clamp(cardH + 14, 58, 68);
                    layoutParallelNodesRow(preList,   xIn + 4, static_cast<int>(splitX0) - 6, static_cast<int>(centerY), sideCardH, 82);
                    layoutParallelNodesRow(pathAList, static_cast<int>(splitX1) + 4, static_cast<int>(mergeX0) - 4, static_cast<int>(laneAY), cardH, 84);
                    layoutParallelNodesRow(pathBList, static_cast<int>(splitX1) + 4, static_cast<int>(mergeX0) - 4, static_cast<int>(laneBY), cardH, 84);
                    layoutParallelNodesRow(pathCList, static_cast<int>(splitX1) + 4, static_cast<int>(mergeX0) - 4, static_cast<int>(laneCY), cardH, 84);
                    layoutParallelNodesRow(postList,  mixBadgeRect.getRight() + 6, xOut - 4, static_cast<int>(centerY), sideCardH, 80);
                }
            }
            return;
        }

        // Mode 0: Serial 1-Lane Proportional 3-Row Layout (vertically centered at balanced height inside fixed strip!)
        if (numNodes <= 0) return;

        const int gap = (numNodes >= 11 || chain.getWidth() < 760) ? 4 : 7;
        const int maxSlotW = 94;
        const int slotW = std::clamp((chain.getWidth() - gap * (numNodes - 1)) / numNodes, 36, maxSlotW);
        const int totalGroupW = slotW * numNodes + gap * (numNodes - 1);
        const int startX = chain.getX() + std::max(0, (chain.getWidth() - totalGroupW) / 2);

        const int serialRowH = std::min(chain.getHeight(), 100);
        const int serialRowY = chain.getCentreY() - serialRowH / 2;
        const int pillH = 16;
        const int dropH = 18;

        for (int i = 0; i < numNodes && i < 16; ++i) {
            auto col = juce::Rectangle<int>(startX + i * (slotW + gap), serialRowY, slotW, serialRowH);
            const int pillW = std::clamp(slotW - 10, 30, 62);
            pillRects[static_cast<size_t>(i)] = col.removeFromTop(pillH).withSizeKeepingCentre(pillW, pillH);
            dropdownRects[static_cast<size_t>(i)] = col.removeFromBottom(dropH).reduced(1, 0);
            col.reduce(0, 2);
            iconRects[static_cast<size_t>(i)] = col;
        }
    }

    void resolveNodeDisplayInfo(const ChainNode& nd, ModuleId& outMod, int& outModelIdx, bool& outActive, juce::Colour& outCustomCol) const noexcept {
        outCustomCol = juce::Colours::transparentBlack;
        switch (nd.kind) {
            case ChainNodeKind::PedalSlot: {
                const auto snap = processor.getPedalSlot(nd.slotId);
                outMod = snap.category;
                outModelIdx = snap.modelIndex;
                outActive = !snap.bypassed;
                const auto& entry = gp200::getModelEntry(outMod, outModelIdx);
                outCustomCol = (outMod == ModuleId::Overdrive) ? juce::Colour(entry.chassisArgb) : valeton::colourFor(outMod);
                break;
            }
            case ChainNodeKind::AmpHead:
                outMod = ModuleId::AmpHead;
                outModelIdx = getCoreModelIndex(ModuleId::AmpHead, 0);
                outActive = !isParamBypassed(params::kAmpBypass);
                break;
            case ChainNodeKind::CabinetIR:
                outMod = ModuleId::CabinetIR;
                outModelIdx = getCoreModelIndex(ModuleId::CabinetIR, 0);
                outActive = !isParamBypassed(params::kCabBypass);
                break;
            case ChainNodeKind::RackMod: {
                const auto rs = processor.getRackSlot(0);
                outMod = ModuleId::StereoChorus;
                outModelIdx = rs.occupied ? rs.modelIndex : getCoreModelIndex(ModuleId::StereoChorus, 0);
                outActive = rs.occupied ? !rs.bypassed : !isParamBypassed(params::kChorusBypass);
                break;
            }
            case ChainNodeKind::RackDelay: {
                const auto rs = processor.getRackSlot(1);
                outMod = ModuleId::StereoDelay;
                outModelIdx = rs.occupied ? rs.modelIndex : getCoreModelIndex(ModuleId::StereoDelay, 0);
                outActive = rs.occupied ? !rs.bypassed : !isParamBypassed(params::kDelayBypass);
                break;
            }
            case ChainNodeKind::RackReverb:
            default: {
                const auto rs = processor.getRackSlot(2);
                outMod = ModuleId::Reverb;
                outModelIdx = rs.occupied ? rs.modelIndex : getCoreModelIndex(ModuleId::Reverb, 0);
                outActive = rs.occupied ? !rs.bypassed : !isParamBypassed(params::kReverbBypass);
                break;
            }
        }
    }

    void toggleSerialNodeBypass(const ChainNode& nd) {
        switch (nd.kind) {
            case ChainNodeKind::PedalSlot: {
                const auto snap = processor.getPedalSlot(nd.slotId);
                processor.setPedalSlotBypass(nd.slotId, !snap.bypassed);
                break;
            }
            case ChainNodeKind::AmpHead:    toggleBypassParam(params::kAmpBypass); break;
            case ChainNodeKind::CabinetIR:  toggleBypassParam(params::kCabBypass); break;
            case ChainNodeKind::RackMod:    processor.setRackSlotBypass(0, !processor.getRackSlot(0).bypassed); break;
            case ChainNodeKind::RackDelay:  processor.setRackSlotBypass(1, !processor.getRackSlot(1).bypassed); break;
            case ChainNodeKind::RackReverb: processor.setRackSlotBypass(2, !processor.getRackSlot(2).bypassed); break;
            default: break;
        }
    }

    void toggleParallelNodeBypass(const VisualNode& vn) {
        if (vn.kind == ChainNodeKind::AmpHead && vn.rigIndex == 1) {
            toggleBypassParam(params::kAmpBBypass);
            return;
        }
        if (vn.kind == ChainNodeKind::CabinetIR && vn.rigIndex == 1) {
            toggleBypassParam(params::kCabBBypass);
            return;
        }
        if (vn.kind == ChainNodeKind::AmpHead && vn.rigIndex == 2) {
            toggleBypassParam(params::kAmpCBypass);
            return;
        }
        if (vn.kind == ChainNodeKind::CabinetIR && vn.rigIndex == 2) {
            toggleBypassParam(params::kCabCBypass);
            return;
        }
        toggleSerialNodeBypass({ vn.kind, vn.slotId, vn.lane });
    }

    void openParallelNodeMenu(const VisualNode& vn, juce::Rectangle<int> anchor) {
        juce::PopupMenu m;
        const auto models = gp200::modelsForModule(vn.mod);

        if (models.size() > 14) {
            std::vector<juce::String> subOrder;
            std::map<juce::String, juce::PopupMenu> subMenus;
            for (size_t i = 0; i < models.size(); ++i) {
                const juce::String st = juce::String::fromUTF8(models[i].subType);
                if (subMenus.find(st) == subMenus.end()) subOrder.push_back(st);
                const juce::String label = juce::String::fromUTF8(models[i].name)
                                         + "   (" + juce::String::fromUTF8(models[i].description) + ")";
                subMenus[st].addItem(100 + static_cast<int>(i), label, true, static_cast<int>(i) == vn.modelIdx);
            }
            m.addSectionHeader(vn.badgeText + " Models (" + juce::String(models.size()) + ")");
            for (const auto& st : subOrder) m.addSubMenu(st, subMenus[st]);
        } else {
            m.addSectionHeader(vn.badgeText + " Models (" + juce::String(models.size()) + ")");
            for (size_t i = 0; i < models.size(); ++i) {
                const juce::String label = juce::String::fromUTF8(models[i].name)
                                         + "   (" + juce::String::fromUTF8(models[i].description) + ")";
                m.addItem(100 + static_cast<int>(i), label, true, static_cast<int>(i) == vn.modelIdx);
            }
        }

        m.addSeparator();
        m.addItem(1, "Open Hardware Stage");
        m.addItem(2, vn.active ? "Bypass (Turn OFF)" : "Turn ON");

        if (vn.kind == ChainNodeKind::PedalSlot && vn.chainIndex >= 0) {
            m.addSeparator();
            const auto mode = processor.getRoutingMode();
            juce::PopupMenu laneSub;
            if (mode == RoutingMode::DualIndependent) {
                laneSub.addItem(21, "Chain A — Pre-Amp (Before Amp A)", true, vn.lane == BusLane::Pre || vn.lane == BusLane::PathA);
                laneSub.addItem(25, "Chain A — Post-Amp (After Cab A)", true, vn.lane == BusLane::PostA || vn.lane == BusLane::PathC);
                laneSub.addItem(22, "Chain B — Pre-Amp (Before Amp B)", true, vn.lane == BusLane::PathB);
                laneSub.addItem(23, "Chain B — Post-Amp (After Cab B)", true, vn.lane == BusLane::Post);
            } else if (mode == RoutingMode::TripleParallel) {
                laneSub.addItem(20, "Pre (Before Split)",   true, vn.lane == BusLane::Pre);
                laneSub.addItem(21, "Path A (Top Lane)",    true, vn.lane == BusLane::PathA);
                laneSub.addItem(22, "Path B (Middle Lane)", true, vn.lane == BusLane::PathB);
                laneSub.addItem(24, "Path C (Bottom Lane)", true, vn.lane == BusLane::PathC);
                laneSub.addItem(23, "Post (After Mix)",     true, vn.lane == BusLane::Post || vn.lane == BusLane::PostA);
            } else {
                laneSub.addItem(20, "Pre (Before Split)",  true, vn.lane == BusLane::Pre);
                laneSub.addItem(21, "Path A (Upper Lane)", true, vn.lane == BusLane::PathA);
                laneSub.addItem(22, "Path B (Lower Lane)", true, vn.lane == BusLane::PathB || vn.lane == BusLane::PathC);
                laneSub.addItem(23, "Post (After Mix)",    true, vn.lane == BusLane::Post || vn.lane == BusLane::PostA);
            }
            m.addSubMenu("Assign Signal Lane", laneSub);
            m.addItem(6, "Remove Pedal from Chain");
        }

        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(anchor)),
                        [this, vn, total = static_cast<int>(models.size())](int res) {
                            const int focusSubIdx = (vn.kind == ChainNodeKind::PedalSlot)
                                ? static_cast<int>(vn.lane)
                                : vn.rigIndex;
                            if (res >= 100 && res < 100 + total) {
                                const int chosen = res - 100;
                                if (vn.kind == ChainNodeKind::PedalSlot) {
                                    processor.setPedalSlotModel(vn.slotId, chosen);
                                } else if (vn.kind == ChainNodeKind::RackMod) {
                                    processor.setRackSlotModel(0, chosen);
                                } else if (vn.kind == ChainNodeKind::RackDelay) {
                                    processor.setRackSlotModel(1, chosen);
                                } else if (vn.kind == ChainNodeKind::RackReverb) {
                                    processor.setRackSlotModel(2, chosen);
                                } else {
                                    setCoreModelIndex(vn.mod, chosen, vn.rigIndex);
                                }
                                if (focusCb) focusCb(tabForModule(vn.mod), focusSubIdx);
                            } else if (res == 1 && focusCb) {
                                focusCb(tabForModule(vn.mod), focusSubIdx);
                            } else if (res == 2) {
                                toggleParallelNodeBypass(vn);
                            } else if (res >= 20 && res <= 25 && vn.chainIndex >= 0) {
                                const auto newBus = static_cast<BusLane>(res - 20);
                                processor.setDynamicChainNodeBus(vn.chainIndex, newBus);
                                if (focusCb && vn.kind == ChainNodeKind::PedalSlot) {
                                    focusCb(StageViewTab::Stomp, static_cast<int>(newBus));
                                }
                            } else if (res == 6 && vn.kind == ChainNodeKind::PedalSlot) {
                                processor.removePedalSlot(vn.slotId);
                            }
                            repaint();
                        });
    }

    void openSerialNodeDropdownMenu(ChainNode nd, int slotIdx, juce::Rectangle<int> anchor) {
        ModuleId mod = ModuleId::Overdrive;
        int curIdx = 0;
        bool active = true;
        juce::Colour cCol;
        resolveNodeDisplayInfo(nd, mod, curIdx, active, cCol);

        juce::PopupMenu m;
        const auto models = gp200::modelsForModule(mod);

        if (models.size() > 14) {
            std::vector<juce::String> subOrder;
            std::map<juce::String, juce::PopupMenu> subMenus;
            for (size_t i = 0; i < models.size(); ++i) {
                const juce::String st = juce::String::fromUTF8(models[i].subType);
                if (subMenus.find(st) == subMenus.end()) subOrder.push_back(st);
                const juce::String label = juce::String::fromUTF8(models[i].name)
                                         + "   (" + juce::String::fromUTF8(models[i].description) + ")";
                subMenus[st].addItem(100 + static_cast<int>(i), label, true, static_cast<int>(i) == curIdx);
            }
            m.addSectionHeader(juce::String(valeton::gpCodeFor(mod)) + " Models (" + juce::String(models.size()) + ")");
            for (const auto& st : subOrder) m.addSubMenu(st, subMenus[st]);
        } else {
            m.addSectionHeader(juce::String(valeton::gpCodeFor(mod)) + " Models (" + juce::String(models.size()) + ")");
            for (size_t i = 0; i < models.size(); ++i) {
                const juce::String label = juce::String::fromUTF8(models[i].name)
                                         + "   (" + juce::String::fromUTF8(models[i].description) + ")";
                m.addItem(100 + static_cast<int>(i), label, true, static_cast<int>(i) == curIdx);
            }
        }

        m.addSeparator();
        m.addItem(1, "Open Hardware Stage");
        m.addItem(2, active ? "Bypass (Turn OFF)" : "Turn ON");
        if (slotIdx > 0) m.addItem(3, "Move Left in Signal Chain");
        if (slotIdx + 1 < static_cast<int>(cachedNodes.size())) m.addItem(4, "Move Right in Signal Chain");
        if (nd.kind == ChainNodeKind::PedalSlot) {
            if (static_cast<int>(processor.getActivePedalSlotIds().size()) < kMaxPedalSlots) {
                m.addItem(5, "Duplicate This Pedal");
            }
            m.addItem(6, "Remove Pedal from Chain");
        }

        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(anchor)),
                        [this, nd, mod, curIdx, slotIdx, total = static_cast<int>(models.size())](int res) {
                            if (res >= 100 && res < 100 + total) {
                                const int chosen = res - 100;
                                if (nd.kind == ChainNodeKind::PedalSlot) {
                                    processor.setPedalSlotModel(nd.slotId, chosen);
                                } else if (nd.kind == ChainNodeKind::RackMod) {
                                    processor.setRackSlotModel(0, chosen);
                                } else if (nd.kind == ChainNodeKind::RackDelay) {
                                    processor.setRackSlotModel(1, chosen);
                                } else if (nd.kind == ChainNodeKind::RackReverb) {
                                    processor.setRackSlotModel(2, chosen);
                                } else {
                                    setCoreModelIndex(mod, chosen, 0);
                                }
                                if (focusCb) focusCb(tabForNode(nd), 0);
                            } else if (res == 1 && focusCb) {
                                focusCb(tabForNode(nd), 0);
                            } else if (res == 2) {
                                toggleSerialNodeBypass(nd);
                            } else if (res == 3 && slotIdx > 0) {
                                processor.moveDynamicChainNode(slotIdx, slotIdx - 1);
                            } else if (res == 4) {
                                processor.moveDynamicChainNode(slotIdx, slotIdx + 1);
                            } else if (res == 5 && nd.kind == ChainNodeKind::PedalSlot) {
                                processor.addPedalSlotAtChainIndex(mod, curIdx, slotIdx + 1);
                            } else if (res == 6 && nd.kind == ChainNodeKind::PedalSlot) {
                                processor.removePedalSlot(nd.slotId);
                            }
                            repaint();
                        });
    }

    int getCoreModelIndex(ModuleId mod, int rigIndex) const noexcept {
        if (auto* raw = processor.getAPVTS().getRawParameterValue(params::modelParamFor(mod, rigIndex))) {
            return static_cast<int>(std::lround(raw->load(std::memory_order_relaxed)));
        }
        return 0;
    }

    void setCoreModelIndex(ModuleId mod, int modelIdx, int rigIndex = 0) {
        const char* pid = params::modelParamFor(mod, rigIndex);
        if (auto* p = processor.getAPVTS().getParameter(pid)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(modelIdx)));
            p->endChangeGesture();
        }
        if (mod == ModuleId::AmpHead) {
            const char* chId = (rigIndex == 2) ? params::kAmpCChannel : ((rigIndex == 1) ? params::kAmpBChannel : params::kAmpChannel);
            if (auto* cp = processor.getAPVTS().getParameter(chId)) {
                const int cat = AmpHead::categoryForModel(modelIdx);
                cp->beginChangeGesture();
                cp->setValueNotifyingHost(cp->convertTo0to1(static_cast<float>(cat)));
                cp->endChangeGesture();
            }
        }
    }

    int findClosestSlot(juce::Point<int> pt) const noexcept {
        const int numNodes = static_cast<int>(cachedNodes.size());
        int best = -1;
        int bestDist = 1000000;
        for (int i = 0; i < numNodes; ++i) {
            const int d = std::abs(iconRects[static_cast<size_t>(i)].getCentreX() - pt.x);
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        return best;
    }

    int findInsertionChainIndex(juce::Point<int> pt) const noexcept {
        const int numNodes = static_cast<int>(cachedNodes.size());
        for (int i = 0; i < numNodes; ++i) {
            if (pt.x < iconRects[static_cast<size_t>(i)].getCentreX()) {
                return i;
            }
        }
        return numNodes;
    }

    void setRoutingModeParam(RoutingMode mode) {
        if (auto* p = processor.getAPVTS().getParameter(params::kRoutingMode)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(static_cast<int>(mode))));
            p->endChangeGesture();
        }
    }

    bool isParamBypassed(const char* pid) const noexcept {
        if (auto* raw = processor.getAPVTS().getRawParameterValue(pid)) {
            return raw->load(std::memory_order_relaxed) > 0.5f;
        }
        return false;
    }

    void toggleBypassParam(const char* pid) {
        if (auto* p = processor.getAPVTS().getParameter(pid)) {
            const float next = (p->getValue() > 0.5f) ? 0.0f : 1.0f;
            p->beginChangeGesture();
            p->setValueNotifyingHost(next);
            p->endChangeGesture();
        }
    }

    PedalBoardAudioProcessor& processor;
    FocusCallback focusCb;
    StageViewTab activeTab = StageViewTab::Stomp;
    int selectedChainIndex = 0;

    std::vector<ChainNode> cachedNodes;
    std::array<juce::Rectangle<int>, 4> routeBtnRects {};
    std::array<juce::Rectangle<int>, 16> pillRects {};
    std::array<juce::Rectangle<int>, 16> iconRects {};
    std::array<juce::Rectangle<int>, 16> dropdownRects {};
    juce::Rectangle<int> chainAreaRect;

    // Parallel multi-lane layout geometry
    std::vector<VisualNode> parallelNodes;
    juce::Rectangle<int> preZoneRect, laneAZoneRect, laneBZoneRect, laneCZoneRect, postZoneRect;
    juce::Rectangle<int> preAZoneRect, postAZoneRect, preBZoneRect, postBZoneRect;
    juce::Rectangle<int> mixBadgeRect;
    float centerY = 56.0f, laneAY = 30.0f, laneBY = 82.0f, laneCY = 110.0f;
    float splitX0 = 100.0f, splitX1 = 124.0f, mergeX0 = 600.0f, mergeX1 = 624.0f;

    int hoveredRouteBtn = -1;
    bool hoveredMixNode = false;
    int highlightedStompLane = -1;

    int dragCandidateSlot = -1;
    int pillClickCandidateSlot = -1;
    int dropHoverSlot = -1;
    bool isDraggingNode = false;
    juce::Point<int> dragStartPos {};
    juce::Point<int> dragCurrentPos {};

    bool sidebarDragActive = false;
    juce::Point<int> sidebarDragPt {};
    ModuleId sidebarDragCat = ModuleId::Overdrive;
    juce::String sidebarDragModelName;
};

} // namespace pedalboard
