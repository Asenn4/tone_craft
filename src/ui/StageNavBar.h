#pragma once

#include "../engine/ModuleId.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "SignalChainStrip.h"

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard {

// Flagship Architectural Stage Navigation Bar:
// - Flat studio underline tabs with bespoke hardware vector icons (zero bulky 3D buttons!)
// - Direct 1-click Segmented Routing toggle (SERIAL | DUAL A/B)
// - Clean vector Gear Library drawer toggle (zero ASCII arrows, zero AI-slop helper walls of text!)
class StageNavBar : public juce::Component,
                    public juce::TooltipClient {
public:
    StageNavBar(PedalBoardAudioProcessor& proc,
                std::function<void(StageViewTab)> onTabSelected,
                std::function<void()> onToggleSidebar,
                std::function<void()> onOpenCloud = {})
        : processor(proc),
          tabCb(std::move(onTabSelected)),
          sidebarCb(std::move(onToggleSidebar)),
          cloudCb(std::move(onOpenCloud)) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    juce::String getTooltip() override {
        switch (hoveredZone) {
            case 0:  return "Stompboard Pedal View (1)";
            case 1:  return "Amp Head & Neural NAM View (2)";
            case 2:  return "Cabinet & IR Room (3)";
            case 3:  return "Post-Amp Studio Rack & Console Mixer (4)";
            case 10: return "Serial Chain (1 Path)";
            case 11: return "Dual Split A/B (Shared Pre -> Path A & B -> Shared Post)";
            case 12: return "Triple Parallel A/B/C (Shared Pre -> Path A, B & C -> MIX -> Shared Post)";
            case 13: return "Dual Independent Rigs (Pre/Post Chain A || Pre/Post Chain B)";
            case 20: return sidebarOpen ? "Hide Gear Library Sidebar (G)" : "Show Gear Library Sidebar (G)";
            case 21: return "TONE3000 Cloud Browser (T) — Search & load .nam Amps, Pedals & .wav IRs";
            case 30: return processor.isUsingHostDawTempo()
                                ? "Host DAW Tempo (active) — Used by all SYNC-enabled Delays, Mods & Filters"
                                : "Master Tempo (BPM) — Drag up/down or scroll to adjust, double-click for 120 BPM";
            case 31: return "Tap Tempo (B) — Click or press B repeatedly to set Master BPM";
            default: return {};
        }
    }

    void setActiveTab(StageViewTab tab) noexcept {
        if (activeTab != tab) {
            activeTab = tab;
            repaint();
        }
    }

    void setSidebarOpen(bool open) noexcept {
        if (sidebarOpen != open) {
            sidebarOpen = open;
            repaint();
        }
    }

    void setCloudConnected(bool connected) noexcept {
        if (cloudConnected != connected) {
            cloudConnected = connected;
            repaint();
        }
    }

    void refreshLiveState() {
        const int count = static_cast<int>(processor.getActivePedalSlotIds().size());
        const auto mode = processor.getRoutingMode();
        const float bpm = processor.getEffectiveBpm();
        const bool dawSync = processor.isUsingHostDawTempo();
        const int64_t tapMs = processor.getLastTapTempoMs();
        const bool tapLitNow = (tapMs > 0 && (static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes()) - tapMs) < 170);

        if (count != lastPedalCount || mode != lastRoutingMode
            || std::abs(bpm - lastDisplayedBpm) > 0.05f
            || dawSync != lastDawSync
            || tapLitNow != lastTapLit) {
            lastPedalCount = count;
            lastRoutingMode = mode;
            lastDisplayedBpm = bpm;
            lastDawSync = dawSync;
            lastTapLit = tapLitNow;
            repaint();
        }
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const int prev = hoveredZone;
        hoveredZone = hitTestZone(e.getPosition());
        if (hoveredZone != prev) repaint();
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoveredZone != -1) {
            hoveredZone = -1;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const int z = hitTestZone(e.getPosition());
        if (z >= 0 && z < 4) {
            if (tabCb) tabCb(static_cast<StageViewTab>(z));
            return;
        }
        if (z >= 10 && z <= 13) {
            setRoutingMode(static_cast<RoutingMode>(z - 10));
            return;
        }
        if (z == 20) {
            if (sidebarCb) sidebarCb();
            return;
        }
        if (z == 21) {
            if (cloudCb) cloudCb();
            return;
        }
        if (z == 30) {
            draggingBpm = true;
            dragStartY = e.y;
            dragStartBpm = processor.getManualBpm();
            return;
        }
        if (z == 31) {
            processor.triggerTapTempo();
            repaint();
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (draggingBpm) {
            const float deltaBpm = static_cast<float>(dragStartY - e.y) * 0.45f;
            processor.setManualBpm(dragStartBpm + deltaBpm);
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        draggingBpm = false;
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override {
        if (hitTestZone(e.getPosition()) == 30) {
            processor.setManualBpm(120.0f);
            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        const int z = hitTestZone(e.getPosition());
        if (z == 30 || z == 31) {
            const float step = e.mods.isShiftDown() ? 5.0f : 1.0f;
            const float delta = (wheel.deltaY > 0.0f ? step : (wheel.deltaY < 0.0f ? -step : 0.0f));
            if (std::abs(delta) > 0.01f) {
                processor.setManualBpm(processor.getManualBpm() + delta);
                repaint();
            }
            return;
        }
        Component::mouseWheelMove(e, wheel);
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Matte studio navigation bar
        g.setColour(juce::Colour(0xff10141c));
        g.fillRect(bounds);
        g.setColour(juce::Colour(0xff202736));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

        const char* labels[4] = { "STOMPBOARD", "AMP / NAM", "CABINET", "RACK & MIXER" };

        // 1. Flat Architectural Underline Tabs with Vector Hardware Icons
        for (int i = 0; i < 4; ++i) {
            const auto r = tabRects[static_cast<size_t>(i)].toFloat();
            const bool sel = (static_cast<int>(activeTab) == i);
            const bool hov = (hoveredZone == i);

            if (sel) {
                g.setColour(juce::Colour(0xff181f2c));
                g.fillRect(r.withTrimmedBottom(1.0f));
                // Razor-sharp bottom active accent indicator bar
                g.setColour(juce::Colour(0xff3b82f6));
                g.fillRect(r.getX(), r.getBottom() - 2.2f, r.getWidth(), 2.2f);
            } else if (hov) {
                g.setColour(juce::Colours::white.withAlpha(0.035f));
                g.fillRect(r.withTrimmedBottom(1.0f));
            }

            const auto fg = sel ? juce::Colour(0xfff8fafc)
                                : juce::Colour(hov ? 0xffcbd5e1 : 0xff7e8a9f);

            const bool showCount = (i == 0 && lastPedalCount > 0 && r.getWidth() >= 118.0f);
            const float iconW = 14.0f;
            const float badgeW = showCount ? 20.0f : 0.0f;
            const float gap = 6.0f;

            juce::Font f(juce::FontOptions(r.getWidth() < 112.0f ? 9.5f : 10.4f, juce::Font::bold));
            g.setFont(f);
            const float textW = std::min(r.getWidth() - iconW - badgeW - 18.0f,
                                         juce::GlyphArrangement::getStringWidth(f, labels[i]) + 2.0f);
            const float totalContentW = iconW + gap + textW + (showCount ? (5.0f + badgeW) : 0.0f);
            float curX = r.getCentreX() - totalContentW * 0.5f;

            drawTabGlyph(g, { curX + iconW * 0.5f, r.getCentreY() }, i,
                         sel ? juce::Colour(0xff60a5fa) : fg);
            curX += iconW + gap;

            g.setColour(fg);
            g.drawText(labels[i],
                       juce::Rectangle<float>(curX, r.getY(), textW, r.getHeight()),
                       juce::Justification::centredLeft, true);

            if (showCount) {
                curX += textW + 5.0f;
                auto pill = juce::Rectangle<float>(curX, r.getCentreY() - 7.5f, 18.0f, 15.0f);
                g.setColour(sel ? juce::Colour(0xff2563eb).withAlpha(0.28f) : juce::Colour(0xff1b212e));
                g.fillRoundedRectangle(pill, 4.0f);
                g.setColour(sel ? juce::Colour(0xff93c5fd) : juce::Colour(0xff7c889e));
                g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
                g.drawText(juce::String(lastPedalCount), pill, juce::Justification::centred);
            }
        }

        // 2a. Master Tempo BPM + TAP Capsule (Synced to DAW PlayHead in VST3, Tap/Drag/Scroll in Standalone!)
        if (!tempoCapsuleRect.isEmpty()) {
            auto tr = tempoCapsuleRect.toFloat();
            const bool bpmHov = (hoveredZone == 30 || draggingBpm);
            const bool tapHov = (hoveredZone == 31);
            const float curBpm = processor.getEffectiveBpm();
            const bool dawSync = processor.isUsingHostDawTempo();
            const int64_t tapMs = processor.getLastTapTempoMs();
            const bool tapFlash = (tapMs > 0 && (static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes()) - tapMs) < 170);

            g.setColour(juce::Colour(0xff090c12));
            g.fillRoundedRectangle(tr, 5.0f);
            g.setColour(tapFlash ? juce::Colour(0xfff59e0b).withAlpha(0.75f)
                                 : (dawSync ? juce::Colour(0xff38bdf8).withAlpha(0.45f)
                                            : juce::Colour((bpmHov || tapHov) ? 0xff3b465c : 0xff232b3a)));
            g.drawRoundedRectangle(tr, 5.0f, 0.9f);

            // BPM readout half
            auto br = bpmDisplayRect.toFloat();
            if (bpmHov) {
                g.setColour(juce::Colours::white.withAlpha(0.045f));
                g.fillRoundedRectangle(br.reduced(1.5f), 3.6f);
            }
            const float dotR = 2.3f;
            const auto dotCol = tapFlash ? juce::Colour(0xfff59e0b)
                                         : (dawSync ? juce::Colour(0xff38bdf8) : juce::Colour(0xff22c55e));
            g.setColour(dotCol);
            g.fillEllipse(br.getX() + 7.5f - dotR, br.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);

            const bool isWhole = std::abs(curBpm - std::round(curBpm)) < 0.05f;
            const juce::String bpmStr = juce::String(curBpm, isWhole ? 0 : 1) + (dawSync ? " DAW" : " BPM");
            g.setColour(bpmHov ? juce::Colours::white : juce::Colour(0xffe2e8f0));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(bpmStr, br.withTrimmedLeft(13.0f).withTrimmedRight(3.0f), juce::Justification::centred, true);

            // Vertical hairline separator
            g.setColour(juce::Colour(0xff1e2533));
            g.drawVerticalLine(tapBtnRect.getX(), tr.getY() + 4.0f, tr.getBottom() - 4.0f);

            // TAP button half
            auto tapR = tapBtnRect.toFloat().reduced(1.5f);
            if (tapFlash) {
                g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.28f));
                g.fillRoundedRectangle(tapR, 3.6f);
            } else if (tapHov) {
                g.setColour(juce::Colours::white.withAlpha(0.06f));
                g.fillRoundedRectangle(tapR, 3.6f);
            }
            g.setColour(tapFlash ? juce::Colour(0xfffbbf24) : (tapHov ? juce::Colours::white : juce::Colour(0xff94a3b8)));
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
            g.drawText("TAP", tapR, juce::Justification::centred, false);
        }

        // 2b. Direct 1-Click 4-Topology Routing Switch (1x | A/B | 3x | A||B)
        const int curModeIdx = static_cast<int>(processor.getRoutingMode());
        auto segTrack = routingTrackRect.toFloat();
        g.setColour(juce::Colour(0xff090c12));
        g.fillRoundedRectangle(segTrack, 5.0f);
        g.setColour(juce::Colour(0xff232b3a));
        g.drawRoundedRectangle(segTrack, 5.0f, 0.9f);

        const char* segNames[4] = { "1x", "A/B", "3x", "A||B" };
        const juce::Colour segCols[4] = {
            juce::Colour(0xfff59e0b), juce::Colour(0xff38bdf8), juce::Colour(0xffa855f7), juce::Colour(0xff22c55e)
        };
        for (int s = 0; s < 4; ++s) {
            auto r = routingSegRects[static_cast<size_t>(s)].toFloat().reduced(1.5f);
            const bool active = (curModeIdx == s);
            const bool hov    = (hoveredZone == 10 + s);
            if (active) {
                g.setColour(juce::Colour(0xff1e2636));
                g.fillRoundedRectangle(r, 3.6f);
                g.setColour(segCols[s].withAlpha(0.55f));
                g.drawRoundedRectangle(r, 3.6f, 0.85f);
            } else if (hov) {
                g.setColour(juce::Colours::white.withAlpha(0.045f));
                g.fillRoundedRectangle(r, 3.6f);
            }

            const float dotR = 2.0f;
            const float dotX = r.getX() + 6.5f;
            g.setColour(active ? segCols[s] : juce::Colour(0xff3e4759));
            g.fillEllipse(dotX - dotR, r.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);

            g.setColour(active ? juce::Colour(0xfff1f5f9) : juce::Colour(hov ? 0xffb8c2d4 : 0xff6d788d));
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
            g.drawText(segNames[s], r.withTrimmedLeft(10.0f), juce::Justification::centred, true);
        }

        // Hairline divider before TONE3000 Cloud & Gear Drawer buttons
        g.setColour(juce::Colour(0xff212836));
        g.drawVerticalLine(rightDividerX, 7.0f, bounds.getHeight() - 7.0f);

        // 3a. TONE3000 Cloud Browser Pill Button
        if (!cloudBtnRect.isEmpty()) {
            auto cr = cloudBtnRect.toFloat();
            const bool cloudHov = (hoveredZone == 21);
            g.setColour(cloudHov ? juce::Colour(0xff162638)
                                 : (cloudConnected ? juce::Colour(0xff0f2130) : juce::Colour(0xff121620)));
            g.fillRoundedRectangle(cr, 5.0f);
            g.setColour(cloudHov ? juce::Colour(0xff22d3ee).withAlpha(0.85f)
                                 : (cloudConnected ? juce::Colour(0xff0891b2).withAlpha(0.70f) : juce::Colour(0xff263144)));
            g.drawRoundedRectangle(cr, 5.0f, 0.9f);

            const float dotR = 2.6f;
            const float dotX = cr.getX() + 9.0f;
            g.setColour(cloudConnected ? juce::Colour(0xff22d3ee) : juce::Colour(0xff64748b));
            g.fillEllipse(dotX - dotR, cr.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);

            g.setColour(cloudHov ? juce::Colours::white
                                 : (cloudConnected ? juce::Colour(0xff67e8f9) : juce::Colour(0xff94a3b8)));
            g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
            g.drawText("TONE3000", cr.withTrimmedLeft(15.0f).withTrimmedRight(4.0f), juce::Justification::centred, true);
        }

        // 3b. Clean Vector Gear Library Drawer Toggle
        auto gr = gearToggleRect.toFloat();
        const bool gearHov = (hoveredZone == 20);
        g.setColour(sidebarOpen ? juce::Colour(0xff1a2232) : (gearHov ? juce::Colour(0xff181e2b) : juce::Colour(0xff121620)));
        g.fillRoundedRectangle(gr, 5.0f);
        g.setColour(sidebarOpen ? juce::Colour(0xff3b82f6).withAlpha(0.65f)
                                : juce::Colour(gearHov ? 0xff3b465c : 0xff232b3a));
        g.drawRoundedRectangle(gr, 5.0f, 0.9f);

        const auto gearFg = sidebarOpen ? juce::Colour(0xfff1f5f9) : juce::Colour(gearHov ? 0xffcbd5e1 : 0xff8491a6);
        // Clean right-sidebar panel icon
        auto ic = juce::Rectangle<float>(gr.getX() + 9.0f, gr.getCentreY() - 5.0f, 12.0f, 10.0f);
        g.setColour(gearFg);
        g.drawRoundedRectangle(ic, 1.8f, 1.2f);
        if (sidebarOpen) {
            g.setColour(juce::Colour(0xff60a5fa));
            g.fillRect(ic.getRight() - 4.2f, ic.getY() + 1.2f, 3.0f, ic.getHeight() - 2.4f);
        } else {
            g.drawVerticalLine(static_cast<int>(ic.getRight() - 4.0f), ic.getY() + 1.0f, ic.getBottom() - 1.0f);
        }

        g.setColour(gearFg);
        g.setFont(juce::Font(juce::FontOptions(9.6f, juce::Font::bold)));
        g.drawText("GEAR", gr.withTrimmedLeft(23.0f).withTrimmedRight(5.0f), juce::Justification::centred, true);
    }

    void resized() override {
        auto area = getLocalBounds();
        const bool compact = (area.getWidth() < 1100);

        // Left: 4 flush architectural stage tabs
        auto leftArea = area;
        leftArea.removeFromLeft(compact ? 6 : 10);
        const int tabW = std::clamp(area.getWidth() * 11 / 100, 104, 138);
        for (int i = 0; i < 4; ++i) {
            tabRects[static_cast<size_t>(i)] = leftArea.removeFromLeft(tabW);
        }

        // Right: Gear drawer toggle + TONE3000 Cloud pill + 1-click Segmented Routing toggle
        auto rightArea = area.reduced(compact ? 8 : 10, 4);
        const int gearW = compact ? 64 : 72;
        gearToggleRect = rightArea.removeFromRight(gearW);
        rightArea.removeFromRight(6);
        const int cloudW = compact ? 80 : 90;
        cloudBtnRect = rightArea.removeFromRight(cloudW);
        rightArea.removeFromRight(8);
        rightDividerX = rightArea.getRight();
        rightArea.removeFromRight(9);

        const int segTotalW = compact ? 168 : 188;
        routingTrackRect = rightArea.removeFromRight(segTotalW);
        const int segW = routingTrackRect.getWidth() / 4;
        auto segCursor = routingTrackRect;
        for (int s = 0; s < 4; ++s) {
            routingSegRects[static_cast<size_t>(s)] = (s == 3) ? segCursor : segCursor.removeFromLeft(segW);
        }

        // Tempo BPM + TAP Capsule cleanly positioned left of Routing switch (only if space fits cleanly!)
        const int leftEdge = tabRects[3].getRight() + 10;
        const int tempoW = compact ? 112 : 126;
        rightArea.removeFromRight(8);
        if (rightArea.getRight() - tempoW >= leftEdge) {
            tempoCapsuleRect = rightArea.removeFromRight(tempoW);
            auto tc = tempoCapsuleRect;
            const int tapW = compact ? 34 : 38;
            tapBtnRect = tc.removeFromRight(tapW);
            bpmDisplayRect = tc;
        } else {
            tempoCapsuleRect = {};
            bpmDisplayRect = {};
            tapBtnRect = {};
        }
    }

private:
    int hitTestZone(juce::Point<int> pt) const noexcept {
        for (int i = 0; i < 4; ++i) {
            if (tabRects[static_cast<size_t>(i)].contains(pt)) return i;
        }
        for (int s = 0; s < 4; ++s) {
            if (routingSegRects[static_cast<size_t>(s)].contains(pt)) return 10 + s;
        }
        if (gearToggleRect.contains(pt)) return 20;
        if (cloudBtnRect.contains(pt)) return 21;
        if (bpmDisplayRect.contains(pt)) return 30;
        if (tapBtnRect.contains(pt)) return 31;
        return -1;
    }

    void setRoutingMode(RoutingMode mode) {
        if (auto* p = processor.getAPVTS().getParameter(params::kRoutingMode)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(static_cast<int>(mode))));
            p->endChangeGesture();
        }
        lastRoutingMode = mode;
        repaint();
    }

    static void drawTabGlyph(juce::Graphics& g, juce::Point<float> c, int tabIdx, juce::Colour col) {
        g.setColour(col);
        if (tabIdx == 0) {
            // Stompbox pedal silhouette
            auto r = juce::Rectangle<float>(c.x - 4.5f, c.y - 6.0f, 9.0f, 12.0f);
            g.drawRoundedRectangle(r, 1.8f, 1.25f);
            g.fillEllipse(c.x - 2.6f, c.y - 3.6f, 2.0f, 2.0f);
            g.fillEllipse(c.x + 0.6f, c.y - 3.6f, 2.0f, 2.0f);
            g.drawEllipse(c.x - 1.8f, c.y + 1.0f, 3.6f, 3.6f, 1.15f);
        } else if (tabIdx == 1) {
            // Amp head silhouette
            auto r = juce::Rectangle<float>(c.x - 6.2f, c.y - 3.6f, 12.4f, 7.8f);
            g.drawRoundedRectangle(r, 1.6f, 1.25f);
            g.drawLine(c.x - 2.6f, r.getY() - 1.6f, c.x + 2.6f, r.getY() - 1.6f, 1.25f);
            g.drawLine(r.getX() + 1.0f, c.y + 0.6f, r.getRight() - 1.0f, c.y + 0.6f, 1.0f);
        } else if (tabIdx == 2) {
            // Speaker cabinet cone silhouette
            auto r = juce::Rectangle<float>(c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f);
            g.drawRoundedRectangle(r, 1.8f, 1.25f);
            g.drawEllipse(c.x - 3.2f, c.y - 3.2f, 6.4f, 6.4f, 1.15f);
            g.fillEllipse(c.x - 1.1f, c.y - 1.1f, 2.2f, 2.2f);
        } else {
            // 3 vertical console faders silhouette
            const float xs[3] = { -4.0f, 0.0f, 4.0f };
            const float ys[3] = { -1.5f, 2.0f, -2.5f };
            for (int i = 0; i < 3; ++i) {
                g.drawLine(c.x + xs[i], c.y - 5.2f, c.x + xs[i], c.y + 5.2f, 1.15f);
                g.fillRoundedRectangle(c.x + xs[i] - 1.6f, c.y + ys[i] - 1.3f, 3.2f, 2.6f, 0.8f);
            }
        }
    }

    PedalBoardAudioProcessor& processor;
    std::function<void(StageViewTab)> tabCb;
    std::function<void()> sidebarCb;
    std::function<void()> cloudCb;

    StageViewTab activeTab = StageViewTab::Stomp;
    bool sidebarOpen = true;
    bool cloudConnected = false;
    int lastPedalCount = -1;
    RoutingMode lastRoutingMode = RoutingMode::Serial;
    float lastDisplayedBpm = 120.0f;
    bool lastDawSync = false;
    bool lastTapLit = false;
    bool draggingBpm = false;
    int dragStartY = 0;
    float dragStartBpm = 120.0f;
    int hoveredZone = -1;

    std::array<juce::Rectangle<int>, 4> tabRects {};
    juce::Rectangle<int> tempoCapsuleRect;
    juce::Rectangle<int> bpmDisplayRect;
    juce::Rectangle<int> tapBtnRect;
    juce::Rectangle<int> routingTrackRect;
    std::array<juce::Rectangle<int>, 4> routingSegRects {};
    juce::Rectangle<int> cloudBtnRect;
    juce::Rectangle<int> gearToggleRect;
    int rightDividerX = 900;
};

} // namespace pedalboard
