#pragma once

#include "../cloud/Tone3000Client.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "../plugin/StandaloneBridge.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace pedalboard {

namespace settings_ui {

inline const juce::Colour kBgCanvas      { 0xff0c0f16 };
inline const juce::Colour kBgRail        { 0xff090c12 };
inline const juce::Colour kBgCard        { 0xff121722 };
inline const juce::Colour kBgRecessed    { 0xff090c12 };
inline const juce::Colour kBorderCard    { 0xff202838 };
inline const juce::Colour kBorderSubtle  { 0xff19202e };
inline const juce::Colour kTextPrimary   { 0xfff1f5f9 };
inline const juce::Colour kTextSecondary { 0xff94a3b8 };
inline const juce::Colour kTextMuted     { 0xff64748b };
inline const juce::Colour kAccentCyan    { 0xff38bdf8 };
inline const juce::Colour kAccentGreen   { 0xff22c55e };
inline const juce::Colour kAccentAmber   { 0xfff59e0b };
inline const juce::Colour kAccentRed     { 0xffef4444 };

inline void drawCardFrame(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title = {}, const juce::String& subtitle = {}) {
    auto r = bounds.toFloat().reduced(0.5f);
    g.setColour(kBgCard);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(kBorderCard);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    if (title.isNotEmpty()) {
        auto hdr = bounds.reduced(14, 10).withHeight(subtitle.isNotEmpty() ? 32 : 18);
        g.setColour(kTextPrimary);
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        g.drawText(title, hdr.removeFromTop(16), juce::Justification::centredLeft, true);
        if (subtitle.isNotEmpty()) {
            g.setColour(kTextMuted);
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));
            g.drawText(subtitle, hdr, juce::Justification::centredLeft, true);
        }
    }
}

inline void drawOledTelemetryCard(juce::Graphics& g,
                                  juce::Rectangle<int> bounds,
                                  const juce::String& label,
                                  const juce::String& value,
                                  const juce::String& sub,
                                  juce::Colour accent,
                                  float barNorm = -1.0f) {
    auto r = bounds.toFloat().reduced(0.5f);
    g.setColour(kBgRecessed);
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(kBorderCard);
    g.drawRoundedRectangle(r, 7.0f, 1.0f);

    // Subtle top accent hairline
    g.setColour(accent.withAlpha(0.55f));
    g.fillRoundedRectangle(r.getX() + 10.0f, r.getY() + 1.0f, r.getWidth() - 20.0f, 2.0f, 1.0f);

    auto inner = bounds.reduced(10, 7);
    g.setColour(kTextMuted);
    g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
    g.drawText(label, inner.removeFromTop(12), juce::Justification::centredLeft, true);

    g.setColour(kTextPrimary);
    g.setFont(juce::Font(juce::FontOptions(15.5f, juce::Font::bold)));
    g.drawText(value, inner.removeFromTop(21), juce::Justification::centredLeft, true);

    if (barNorm >= 0.0f) {
        auto bar = inner.removeFromBottom(5).toFloat();
        g.setColour(juce::Colour(0xff1a2232));
        g.fillRoundedRectangle(bar, 2.5f);
        const float fillW = std::clamp(barNorm, 0.0f, 1.0f) * bar.getWidth();
        if (fillW > 1.0f) {
            g.setColour(accent);
            g.fillRoundedRectangle(bar.withWidth(fillW), 2.5f);
        }
    } else if (sub.isNotEmpty()) {
        g.setColour(kTextSecondary);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
        g.drawText(sub, inner, juce::Justification::centredLeft, true);
    }
}

// Clean matte hardware toggle switch (ZERO color highlight when OFF!)
class StudioSwitchPill : public juce::Component {
public:
    StudioSwitchPill(juce::Colour activeAccent = kAccentGreen,
                     juce::String onLabel = "ON",
                     juce::String offLabel = "OFF")
        : accent(activeAccent),
          onText(std::move(onLabel)),
          offText(std::move(offLabel)) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setState(bool newState, bool notify = false) {
        if (state != newState) {
            state = newState;
            repaint();
            if (notify && onToggle) {
                onToggle(state);
            }
        }
    }

    bool getState() const noexcept { return state; }
    void setAccentColour(juce::Colour col) { accent = col; repaint(); }

    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { hovered = false; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override {
        if (isEnabled() && getLocalBounds().contains(e.getPosition())) {
            state = !state;
            repaint();
            if (onToggle) onToggle(state);
        }
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        const float trackW = 42.0f;
        const float trackH = 22.0f;
        auto track = juce::Rectangle<float>(area.getRight() - trackW,
                                            area.getCentreY() - trackH * 0.5f,
                                            trackW, trackH);

        const float alpha = isEnabled() ? 1.0f : 0.45f;
        if (state) {
            g.setColour(accent.withAlpha((hovered ? 0.96f : 0.85f) * alpha));
            g.fillRoundedRectangle(track, trackH * 0.5f);
            g.setColour(accent.brighter(0.25f).withAlpha(0.75f * alpha));
            g.drawRoundedRectangle(track, trackH * 0.5f, 1.0f);
        } else {
            g.setColour(juce::Colour(hovered ? 0xff1e2636 : 0xff151b27).withAlpha(alpha));
            g.fillRoundedRectangle(track, trackH * 0.5f);
            g.setColour(juce::Colour(hovered ? 0xff3b465c : 0xff283244).withAlpha(alpha));
            g.drawRoundedRectangle(track, trackH * 0.5f, 1.0f);
        }

        const float knobD = trackH - 6.0f;
        const float knobX = state ? (track.getRight() - knobD - 3.0f) : (track.getX() + 3.0f);
        const float knobY = track.getCentreY() - knobD * 0.5f;
        g.setColour((state ? juce::Colours::white : juce::Colour(0xff94a3b8)).withAlpha(alpha));
        g.fillEllipse(knobX, knobY, knobD, knobD);

        auto textBox = area.withRight(track.getX() - 8.0f);
        if (textBox.getWidth() > 14.0f) {
            g.setColour((state ? accent.brighter(0.25f) : kTextMuted).withAlpha(alpha));
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText(state ? onText : offText, textBox, juce::Justification::centredRight, true);
        }
    }

    std::function<void(bool)> onToggle;

private:
    bool state = false;
    bool hovered = false;
    juce::Colour accent;
    juce::String onText;
    juce::String offText;
};

// Segmented matte-dark pill selector (e.g. Stereo | Mono Ch1 | Mono Ch2 | Sum Mono)
class SegmentedPillGroup : public juce::Component {
public:
    explicit SegmentedPillGroup(juce::StringArray items, juce::Colour activeCol = kAccentCyan)
        : labels(std::move(items)),
          accent(activeCol) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setSelectedIndex(int idx, bool notify = false) {
        const int clamped = std::clamp(idx, 0, std::max(0, labels.size() - 1));
        if (selectedIndex != clamped) {
            selectedIndex = clamped;
            repaint();
            if (notify && onChange) onChange(selectedIndex);
        }
    }

    int getSelectedIndex() const noexcept { return selectedIndex; }

    void mouseMove(const juce::MouseEvent& e) override {
        const int next = indexAtX(e.x);
        if (next != hoverIndex) {
            hoverIndex = next;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoverIndex != -1) {
            hoverIndex = -1;
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override {
        if (!isEnabled()) return;
        const int idx = indexAtX(e.x);
        if (idx >= 0 && idx < labels.size() && idx != selectedIndex) {
            selectedIndex = idx;
            repaint();
            if (onChange) onChange(selectedIndex);
        }
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(kBgRecessed);
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(kBorderCard);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);

        const int n = labels.size();
        if (n <= 0) return;
        const float cellW = area.getWidth() / static_cast<float>(n);

        for (int i = 0; i < n; ++i) {
            auto cell = juce::Rectangle<float>(area.getX() + static_cast<float>(i) * cellW,
                                               area.getY(), cellW, area.getHeight());
            const bool isSel = (i == selectedIndex);
            const bool isHov = (i == hoverIndex);

            if (isSel) {
                auto pill = cell.reduced(2.0f, 2.0f);
                g.setColour(accent.withAlpha(0.22f));
                g.fillRoundedRectangle(pill, 4.5f);
                g.setColour(accent.withAlpha(0.85f));
                g.drawRoundedRectangle(pill, 4.5f, 1.0f);
            } else if (isHov) {
                g.setColour(juce::Colours::white.withAlpha(0.05f));
                g.fillRoundedRectangle(cell.reduced(2.0f, 2.0f), 4.5f);
            }

            if (i > 0 && !isSel && (i - 1) != selectedIndex) {
                g.setColour(kBorderSubtle);
                g.drawVerticalLine(static_cast<int>(cell.getX()), area.getY() + 6.0f, area.getBottom() - 6.0f);
            }

            g.setColour(isSel ? juce::Colours::white : (isHov ? kTextPrimary : kTextSecondary));
            g.setFont(juce::Font(juce::FontOptions(10.0f, isSel ? juce::Font::bold : juce::Font::plain)));
            g.drawText(labels[i], cell.reduced(4.0f, 0.0f), juce::Justification::centred, true);
        }
    }

    std::function<void(int)> onChange;

private:
    int indexAtX(int x) const noexcept {
        if (labels.isEmpty() || getWidth() <= 0) return -1;
        return std::clamp(x * labels.size() / getWidth(), 0, labels.size() - 1);
    }

    juce::StringArray labels;
    juce::Colour accent;
    int selectedIndex = 0;
    int hoverIndex = -1;
};

// Sleek stereo horizontal LED peak meter with numeric dBFS readout
class HorizontalPeakBar : public juce::Component {
public:
    explicit HorizontalPeakBar(juce::String tagText) : tag(std::move(tagText)) {}

    void setPeaks(float newL, float newR) noexcept {
        lvlL = std::max(newL, lvlL * 0.84f);
        lvlR = std::max(newR, lvlR * 0.84f);
        holdPeak = std::max(std::max(newL, newR), holdPeak * 0.96f);
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        g.setColour(kBgRecessed);
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(kBorderSubtle);
        g.drawRoundedRectangle(r, 5.0f, 1.0f);

        auto inner = r.reduced(6.0f, 4.0f);
        g.setColour(kTextMuted);
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText(tag, inner.removeFromLeft(28.0f), juce::Justification::centredLeft);

        const float peakDb = juce::Decibels::gainToDecibels(holdPeak, -60.0f);
        const juce::String dbText = (peakDb <= -59.5f) ? "-inf dB" : (juce::String(peakDb, 1) + " dB");
        g.setColour(peakDb > -1.0f ? kAccentRed : (peakDb > -9.0f ? kAccentAmber : kTextSecondary));
        g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
        g.drawText(dbText, inner.removeFromRight(48.0f), juce::Justification::centredRight);

        inner.reduce(4.0f, 1.0f);
        const float gap = 2.0f;
        const float laneH = std::max(3.0f, (inner.getHeight() - gap) * 0.5f);
        drawLane(g, inner.withHeight(laneH), lvlL);
        drawLane(g, inner.withY(inner.getY() + laneH + gap).withHeight(laneH), lvlR);
    }

private:
    static void drawLane(juce::Graphics& g, juce::Rectangle<float> r, float peak) {
        g.setColour(juce::Colour(0xff151c28));
        g.fillRoundedRectangle(r, 1.5f);

        const float db = juce::Decibels::gainToDecibels(peak, -60.0f);
        const float norm = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
        const int segs = 24;
        const int lit = static_cast<int>(std::round(norm * static_cast<float>(segs)));
        const float pitch = r.getWidth() / static_cast<float>(segs);
        const float w = std::max(1.5f, pitch - 1.2f);
        for (int s = 0; s < lit; ++s) {
            const float f = static_cast<float>(s + 1) / static_cast<float>(segs);
            const juce::Colour col = (f > 0.88f) ? kAccentRed : ((f > 0.70f) ? kAccentAmber : kAccentGreen);
            g.setColour(col);
            g.fillRect(r.getX() + static_cast<float>(s) * pitch, r.getY(), w, r.getHeight());
        }
    }

    juce::String tag;
    float lvlL = 0.0f;
    float lvlR = 0.0f;
    float holdPeak = 0.0f;
};

// Clickable hardware patch-bay grid for Active Input / Output Audio Channels
class ChannelToggleGrid : public juce::Component {
public:
    struct PairItem {
        int bitA = 0;
        int bitB = -1; // -1 if single mono channel
        juce::String label;
        bool active = false;
    };

    ChannelToggleGrid() {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setChannels(const juce::StringArray& channelNames, const juce::BigInteger& activeMask) {
        items.clear();
        for (int i = 0; i < channelNames.size(); i += 2) {
            PairItem p;
            p.bitA = i;
            if (i + 1 < channelNames.size()) {
                p.bitB = i + 1;
                p.label = "CH " + juce::String(i + 1) + "+" + juce::String(i + 2) + "  " + channelNames[i];
                p.active = activeMask[i] || activeMask[i + 1];
            } else {
                p.bitB = -1;
                p.label = "CH " + juce::String(i + 1) + "  " + channelNames[i];
                p.active = activeMask[i];
            }
            items.push_back(std::move(p));
        }
        repaint();
    }

    void setEmptyMessage(const juce::String& msg) {
        emptyMsg = msg;
        repaint();
    }

    int getPreferredHeight(int availableWidth) const noexcept {
        if (items.empty()) return 38;
        const int innerW = std::max(80, availableWidth - 12);
        const int cols = (innerW >= 380 && items.size() > 1) ? 2 : 1;
        const int rows = (static_cast<int>(items.size()) + cols - 1) / cols;
        return std::max(38, 12 + rows * 26 + std::max(0, rows - 1) * 6);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const int idx = hitTestItem(e.getPosition());
        if (idx != hoverIdx) {
            hoverIdx = idx;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoverIdx != -1) {
            hoverIdx = -1;
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override {
        const int idx = hitTestItem(e.getPosition());
        if (idx >= 0 && idx < static_cast<int>(items.size())) {
            auto& it = items[static_cast<size_t>(idx)];
            it.active = !it.active;
            repaint();
            if (onTogglePair) {
                onTogglePair(it.bitA, it.bitB, it.active);
            }
        }
    }

    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(kBgRecessed);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(kBorderSubtle);
        g.drawRoundedRectangle(r, 6.0f, 1.0f);

        if (items.empty()) {
            g.setColour(kTextMuted);
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
            g.drawText(emptyMsg, getLocalBounds().reduced(10, 4), juce::Justification::centredLeft, true);
            return;
        }

        const auto rects = computeItemRects();
        for (size_t i = 0; i < items.size() && i < rects.size(); ++i) {
            const auto& it = items[i];
            const auto box = rects[i].toFloat();
            const bool hov = (static_cast<int>(i) == hoverIdx);

            g.setColour(it.active ? kAccentCyan.withAlpha(hov ? 0.24f : 0.16f)
                                  : juce::Colour(hov ? 0xff1a2232 : 0xff121722));
            g.fillRoundedRectangle(box, 5.0f);
            g.setColour(it.active ? kAccentCyan.withAlpha(0.75f)
                                  : juce::Colour(hov ? 0xff36435a : 0xff222b3c));
            g.drawRoundedRectangle(box, 5.0f, 1.0f);

            // Status LED dot
            g.setColour(it.active ? kAccentGreen : juce::Colour(0xff334155));
            g.fillEllipse(box.getX() + 8.0f, box.getCentreY() - 3.5f, 7.0f, 7.0f);

            g.setColour(it.active ? kTextPrimary : kTextSecondary);
            g.setFont(juce::Font(juce::FontOptions(9.8f, it.active ? juce::Font::bold : juce::Font::plain)));
            g.drawText(it.label, box.withTrimmedLeft(20.0f).withTrimmedRight(6.0f), juce::Justification::centredLeft, true);
        }
    }

    std::function<void(int bitA, int bitB, bool active)> onTogglePair;

private:
    std::vector<juce::Rectangle<int>> computeItemRects() const {
        std::vector<juce::Rectangle<int>> out;
        auto inner = getLocalBounds().reduced(6, 6);
        const int cols = (inner.getWidth() >= 380 && items.size() > 1) ? 2 : 1;
        const int gap = 6;
        const int cellW = std::max(80, (inner.getWidth() - (cols - 1) * gap) / cols);
        const int cellH = 26;
        for (size_t i = 0; i < items.size(); ++i) {
            const int row = static_cast<int>(i) / cols;
            const int col = static_cast<int>(i) % cols;
            out.emplace_back(inner.getX() + col * (cellW + gap),
                             inner.getY() + row * (cellH + gap),
                             cellW, cellH);
        }
        return out;
    }

    int hitTestItem(juce::Point<int> pt) const {
        const auto rects = computeItemRects();
        for (size_t i = 0; i < rects.size(); ++i) {
            if (rects[i].contains(pt)) return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<PairItem> items;
    juce::String emptyMsg { "No channels available" };
    int hoverIdx = -1;
};

// Left rail navigation tab button with custom vector icon
class SettingsNavButton : public juce::Button {
public:
    enum class Icon { AudioIo, InputDsp, MidiControl, StorageCloud };

    SettingsNavButton(Icon ic, const juce::String& titleText, const juce::String& subText)
        : juce::Button(titleText),
          icon(ic),
          title(titleText),
          subtitle(subText) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setActive(bool isActive) {
        if (active != isActive) {
            active = isActive;
            repaint();
        }
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override {
        auto b = getLocalBounds().toFloat().reduced(6.0f, 2.0f);
        if (active) {
            g.setColour(juce::Colour(0xff141d2e));
            g.fillRoundedRectangle(b, 6.0f);
            g.setColour(kAccentCyan.withAlpha(0.35f));
            g.drawRoundedRectangle(b, 6.0f, 1.0f);
            g.setColour(kAccentCyan);
            g.fillRoundedRectangle(b.getX() + 1.0f, b.getY() + 8.0f, 3.0f, b.getHeight() - 16.0f, 1.5f);
        } else if (hovered || down) {
            g.setColour(juce::Colours::white.withAlpha(down ? 0.08f : 0.04f));
            g.fillRoundedRectangle(b, 6.0f);
        }

        const auto fg = active ? kAccentCyan : (hovered ? kTextPrimary : kTextSecondary);
        drawIcon(g, { b.getX() + 20.0f, b.getCentreY() }, fg);

        auto txt = b.withTrimmedLeft(36.0f).withTrimmedRight(8.0f).reduced(0.0f, 7.0f);
        g.setColour(active ? kTextPrimary : (hovered ? kTextPrimary : kTextSecondary));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(title, txt.removeFromTop(15.0f), juce::Justification::centredLeft, true);

        g.setColour(active ? kTextSecondary : kTextMuted);
        g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::plain)));
        g.drawText(subtitle, txt, juce::Justification::centredLeft, true);
    }

private:
    void drawIcon(juce::Graphics& g, juce::Point<float> c, juce::Colour col) const {
        g.setColour(col);
        switch (icon) {
            case Icon::AudioIo: {
                const float ys[3] = { -4.5f, 0.0f, 4.5f };
                const float xs[3] = { -2.0f, 2.5f, -3.0f };
                for (int i = 0; i < 3; ++i) {
                    g.drawLine(c.x - 6.5f, c.y + ys[i], c.x + 6.5f, c.y + ys[i], 1.3f);
                    g.fillEllipse(c.x + xs[i] - 1.8f, c.y + ys[i] - 1.8f, 3.6f, 3.6f);
                }
                break;
            }
            case Icon::InputDsp: {
                juce::Path p;
                p.startNewSubPath(c.x - 7.0f, c.y);
                p.lineTo(c.x - 3.5f, c.y);
                p.lineTo(c.x - 1.5f, c.y - 5.5f);
                p.lineTo(c.x + 1.5f, c.y + 5.5f);
                p.lineTo(c.x + 3.5f, c.y);
                p.lineTo(c.x + 7.0f, c.y);
                g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                break;
            }
            case Icon::MidiControl: {
                g.drawEllipse(c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f, 1.3f);
                const float pts[5][2] = {
                    { -3.6f, 0.5f }, { -2.4f, -2.5f }, { 0.0f, -3.5f }, { 2.4f, -2.5f }, { 3.6f, 0.5f }
                };
                for (const auto& pt : pts) {
                    g.fillEllipse(c.x + pt[0] - 0.9f, c.y + pt[1] - 0.9f, 1.8f, 1.8f);
                }
                break;
            }
            case Icon::StorageCloud: {
                juce::Path p;
                p.startNewSubPath(c.x - 6.5f, c.y - 4.0f);
                p.lineTo(c.x - 1.8f, c.y - 4.0f);
                p.lineTo(c.x - 0.4f, c.y - 2.2f);
                p.lineTo(c.x + 6.5f, c.y - 2.2f);
                p.lineTo(c.x + 6.5f, c.y + 5.0f);
                p.lineTo(c.x - 6.5f, c.y + 5.0f);
                p.closeSubPath();
                g.strokePath(p, juce::PathStrokeType(1.35f));
                break;
            }
        }
    }

    Icon icon;
    juce::String title;
    juce::String subtitle;
    bool active = false;
};

} // namespace settings_ui

// ==============================================================================
// Main Content Component of the Flagship Studio Settings Window
// ==============================================================================
class StudioSettingsContent : public juce::Component,
                              private juce::Timer,
                              private juce::ChangeListener {
public:
    enum class Tab : int {
        AudioIo = 0,
        InputDsp = 1,
        MidiControl = 2,
        StorageCloud = 3
    };

    StudioSettingsContent(PedalBoardAudioProcessor& proc,
                          t3k::Tone3000Client* cloudClient,
                          std::function<void()> onOpenCloudBrowser = {})
        : processor(proc),
          toneClient(cloudClient),
          openCloudCb(std::move(onOpenCloudBrowser)),
          navAudioBtn(settings_ui::SettingsNavButton::Icon::AudioIo, "AUDIO I/O", "Driver, Channels & Latency"),
          navDspBtn(settings_ui::SettingsNavButton::Icon::InputDsp, "INPUT & DSP", "Routing, HPF, Tuner & NAM"),
          navMidiBtn(settings_ui::SettingsNavButton::Icon::MidiControl, "MIDI & CONTROL", "Devices, Exp & CC Map"),
          navStorageBtn(settings_ui::SettingsNavButton::Icon::StorageCloud, "STORAGE & CLOUD", "Folders, Cache & System") {
        host = queryStandaloneAudioHost();
        if (host.deviceManager != nullptr) {
            host.deviceManager->addChangeListener(this);
        }

        navAudioBtn.onClick   = [this] { selectTab(Tab::AudioIo); };
        navDspBtn.onClick     = [this] { selectTab(Tab::InputDsp); };
        navMidiBtn.onClick    = [this] { selectTab(Tab::MidiControl); };
        navStorageBtn.onClick = [this] { selectTab(Tab::StorageCloud); };
        addAndMakeVisible(navAudioBtn);
        addAndMakeVisible(navDspBtn);
        addAndMakeVisible(navMidiBtn);
        addAndMakeVisible(navStorageBtn);

        viewport.setScrollBarsShown(true, false);
        viewport.setScrollBarThickness(8);
        addAndMakeVisible(viewport);

        audioPage   = std::make_unique<AudioIoPage>(*this);
        dspPage     = std::make_unique<InputDspPage>(*this);
        midiPage    = std::make_unique<MidiControlPage>(*this);
        storagePage = std::make_unique<StorageCloudPage>(*this);

        selectTab(Tab::AudioIo);
    }

    ~StudioSettingsContent() override {
        stopTimer();
        if (host.deviceManager != nullptr) {
            host.deviceManager->removeChangeListener(this);
        }
        viewport.setViewedComponent(nullptr, false);
    }

    void onWindowShown() {
        if (!host.isAvailable()) {
            host = queryStandaloneAudioHost();
            if (host.deviceManager != nullptr) {
                host.deviceManager->addChangeListener(this);
            }
        }
        refreshAllPages(true);
        startTimerHz(20);
    }

    void onWindowHidden() {
        stopTimer();
    }

    void selectTab(Tab t) {
        activeTab = t;
        navAudioBtn.setActive(t == Tab::AudioIo);
        navDspBtn.setActive(t == Tab::InputDsp);
        navMidiBtn.setActive(t == Tab::MidiControl);
        navStorageBtn.setActive(t == Tab::StorageCloud);

        juce::Component* page = nullptr;
        switch (t) {
            case Tab::AudioIo:      page = audioPage.get();   break;
            case Tab::InputDsp:     page = dspPage.get();     break;
            case Tab::MidiControl:  page = midiPage.get();    break;
            case Tab::StorageCloud: page = storagePage.get(); break;
        }
        if (page != nullptr) {
            viewport.setViewedComponent(page, false);
            layoutActivePage();
            refreshAllPages(false);
        }
        repaint();
    }

    void paint(juce::Graphics& g) override {
        using namespace settings_ui;
        g.fillAll(kBgCanvas);

        // Left matte navigation rail
        auto rail = getLocalBounds().withWidth(kRailWidth).toFloat();
        g.setColour(kBgRail);
        g.fillRect(rail);
        g.setColour(kBorderCard);
        g.drawVerticalLine(kRailWidth - 1, 0.0f, static_cast<float>(getHeight()));

        // Rail Header
        auto hdr = rail.withHeight(66.0f).reduced(14.0f, 12.0f);
        g.setColour(kTextPrimary);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("STUDIO SETTINGS", hdr.removeFromTop(18.0f), juce::Justification::centredLeft);

        const bool isStandalone = (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
        auto badge = hdr.removeFromTop(17.0f).withWidth(isStandalone ? 104.0f : 92.0f).translated(0.0f, 4.0f);
        g.setColour(isStandalone ? kAccentCyan.withAlpha(0.14f) : juce::Colour(0xff1a2232));
        g.fillRoundedRectangle(badge, 4.0f);
        g.setColour(isStandalone ? kAccentCyan.withAlpha(0.55f) : kBorderCard);
        g.drawRoundedRectangle(badge, 4.0f, 0.8f);
        g.setColour(isStandalone ? kAccentCyan : kTextSecondary);
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText(isStandalone ? "STANDALONE APP" : "VST3 PLUGIN", badge, juce::Justification::centred);

        // Bottom live telemetry mini-card in rail
        if (!railStatusRect.isEmpty()) {
            auto sr = railStatusRect.toFloat();
            g.setColour(kBgCard);
            g.fillRoundedRectangle(sr, 7.0f);
            g.setColour(kBorderCard);
            g.drawRoundedRectangle(sr, 7.0f, 1.0f);

            auto in = sr.reduced(10.0f, 8.0f);
            const auto telemetry = readTelemetry();
            g.setColour(telemetry.running ? kAccentGreen : kAccentAmber);
            g.fillEllipse(in.getX(), in.getY() + 3.0f, 7.0f, 7.0f);
            g.setColour(kTextPrimary);
            g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
            g.drawText(telemetry.running ? "ENGINE ACTIVE" : "STANDBY",
                       in.removeFromTop(14.0f).withTrimmedLeft(12.0f), juce::Justification::centredLeft);

            g.setColour(kTextSecondary);
            g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::plain)));
            g.drawText(telemetry.sampleRateShort + "  |  " + juce::String(telemetry.bufferSamples) + " smp",
                       in.removeFromTop(15.0f), juce::Justification::centredLeft, true);
            g.drawText("RTL: " + telemetry.rtlText,
                       in.removeFromTop(14.0f), juce::Justification::centredLeft, true);
            g.setColour(kTextMuted);
            g.setFont(juce::Font(juce::FontOptions(8.6f, juce::Font::bold)));
            g.drawText(telemetry.driverTypeName.isNotEmpty() ? telemetry.driverTypeName : "Audio Engine",
                       in.removeFromTop(13.0f), juce::Justification::centredLeft, true);
        }
    }

    void resized() override {
        auto bounds = getLocalBounds();
        auto rail = bounds.removeFromLeft(kRailWidth);
        rail.removeFromTop(70);
        railStatusRect = rail.removeFromBottom(86).reduced(10, 10);

        const int btnH = 48;
        navAudioBtn.setBounds(rail.removeFromTop(btnH));
        navDspBtn.setBounds(rail.removeFromTop(btnH));
        navMidiBtn.setBounds(rail.removeFromTop(btnH));
        navStorageBtn.setBounds(rail.removeFromTop(btnH));

        viewport.setBounds(bounds);
        layoutActivePage();
    }

private:
    static constexpr int kRailWidth = 204;

    struct EngineTelemetry {
        bool running = false;
        double sampleRate = 48000.0;
        int bufferSamples = 512;
        int inputLatencySamples = 0;
        int outputLatencySamples = 0;
        int bitDepth = 32;
        float cpuPct = 0.0f;
        juce::String sampleRateShort;
        juce::String rtlText;
        juce::String driverTypeName;
        juce::String deviceName;
    };

    EngineTelemetry readTelemetry() const {
        EngineTelemetry t;
        t.cpuPct = processor.getDspCpuLoadPercent();
        if (host.deviceManager != nullptr) {
            t.driverTypeName = host.deviceManager->getCurrentAudioDeviceType();
            if (auto* dev = host.deviceManager->getCurrentAudioDevice()) {
                t.running              = dev->isOpen();
                t.sampleRate           = dev->getCurrentSampleRate();
                t.bufferSamples        = dev->getCurrentBufferSizeSamples();
                t.inputLatencySamples  = dev->getInputLatencyInSamples();
                t.outputLatencySamples = dev->getOutputLatencyInSamples();
                t.bitDepth             = dev->getCurrentBitDepth();
                t.deviceName           = dev->getName();
                t.cpuPct               = std::max(t.cpuPct, static_cast<float>(host.deviceManager->getCpuUsage() * 100.0));
            }
        } else {
            const double sr = processor.getSampleRate();
            t.running       = (sr > 1000.0);
            t.sampleRate    = (sr > 1000.0) ? sr : 48000.0;
            t.bufferSamples = processor.getCurrentAudioBlockSize();
            t.driverTypeName = "VST3 Host DAW";
        }

        t.sampleRateShort = juce::String(t.sampleRate / 1000.0, 1) + " kHz";
        if (t.sampleRate > 1000.0) {
            const double totalSmp = (t.inputLatencySamples + t.outputLatencySamples > 0)
                                        ? static_cast<double>(t.inputLatencySamples + t.outputLatencySamples)
                                        : static_cast<double>(t.bufferSamples * 2);
            const double rtlMs = (totalSmp / t.sampleRate) * 1000.0;
            t.rtlText = juce::String(rtlMs, 1) + " ms";
        } else {
            t.rtlText = "-- ms";
        }
        return t;
    }

    // =========================================================================
    // TAB 1: AUDIO I/O
    // =========================================================================
    class AudioIoPage : public juce::Component {
    public:
        explicit AudioIoPage(StudioSettingsContent& o)
            : owner(o),
              outPeakBar("OUT"),
              inPeakBar("IN"),
              muteInputSwitch(settings_ui::kAccentRed, "INPUT MUTED", "LIVE INPUT") {
            driverBox.onChange = [this] { handleDriverChanged(); };
            outputBox.onChange = [this] { handleDeviceSelectionChanged(false); };
            inputBox.onChange  = [this] { handleDeviceSelectionChanged(true); };
            srBox.onChange     = [this] { handleSampleRateOrBufferChanged(); };
            bufBox.onChange    = [this] { handleSampleRateOrBufferChanged(); };

            asioPanelBtn.setButtonText("CONTROL PANEL");
            asioPanelBtn.setTooltip("Open native hardware driver / ASIO control panel");
            asioPanelBtn.onClick = [this] {
                if (owner.host.deviceManager != nullptr) {
                    if (auto* dev = owner.host.deviceManager->getCurrentAudioDevice()) {
                        if (dev->hasControlPanel()) {
                            dev->showControlPanel();
                        }
                    }
                }
            };

            testToneBtn.setButtonText("TEST TONE");
            testToneBtn.setTooltip("Play a short sine verification tone through the selected output device");
            testToneBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f4c5c));
            testToneBtn.onClick = [this] {
                if (owner.host.deviceManager != nullptr) {
                    owner.host.deviceManager->playTestSound();
                }
            };

            rescanBtn.setButtonText("RESCAN");
            rescanBtn.setTooltip("Rescan connected USB / ASIO / WASAPI audio interfaces");
            rescanBtn.onClick = [this] { refreshFromDeviceManager(true); };

            defaultsBtn.setButtonText("RESET AUDIO");
            defaultsBtn.setTooltip("Reinitialize with default Windows audio devices");
            defaultsBtn.onClick = [this] {
                if (owner.host.deviceManager != nullptr) {
                    lastError = owner.host.deviceManager->initialiseWithDefaultDevices(2, 2);
                    if (owner.host.saveAudioState) owner.host.saveAudioState();
                    refreshFromDeviceManager(true);
                }
            };

            outChannelsGrid.onTogglePair = [this](int bA, int bB, bool active) { toggleChannels(false, bA, bB, active); };
            inChannelsGrid.onTogglePair  = [this](int bA, int bB, bool active) { toggleChannels(true, bA, bB, active); };

            muteInputSwitch.onToggle = [this](bool muted) {
                if (owner.host.muteInputValue != nullptr) {
                    owner.host.muteInputValue->setValue(muted);
                    if (owner.host.saveAudioState) owner.host.saveAudioState();
                }
                repaint();
            };

            for (juce::Component* c : std::initializer_list<juce::Component*>{
                     &driverBox, &outputBox, &inputBox, &srBox, &bufBox,
                     &asioPanelBtn, &testToneBtn, &rescanBtn, &defaultsBtn,
                     &outChannelsGrid, &inChannelsGrid, &outPeakBar, &inPeakBar, &muteInputSwitch }) {
                addAndMakeVisible(*c);
            }
        }

        int computePreferredHeight() const {
            if (!owner.host.isAvailable()) return 340;
            const int w = std::max(440, getWidth() - 64);
            const int deviceCardH = separateInOut ? 248 : 210;
            const int outH = outChannelsGrid.getPreferredHeight(w);
            const int inH  = inChannelsGrid.getPreferredHeight(w);
            const int chanCardH = 114 + outH + inH;
            return 16 + 66 + 14 + deviceCardH + 14 + chanCardH + 14 + 98 + 20;
        }

        void refreshFromDeviceManager(bool forceScan) {
            const bool standalone = owner.host.isAvailable();
            driverBox.setVisible(standalone);
            outputBox.setVisible(standalone);
            inputBox.setVisible(standalone);
            srBox.setVisible(standalone);
            bufBox.setVisible(standalone);
            asioPanelBtn.setVisible(standalone);
            testToneBtn.setVisible(standalone);
            rescanBtn.setVisible(standalone);
            defaultsBtn.setVisible(standalone);
            outChannelsGrid.setVisible(standalone);
            inChannelsGrid.setVisible(standalone);
            muteInputSwitch.setVisible(standalone && owner.host.muteInputValue != nullptr);

            if (!standalone) {
                resized();
                repaint();
                return;
            }

            auto* dm = owner.host.deviceManager;
            const auto& types = dm->getAvailableDeviceTypes();
            const auto currentTypeName = dm->getCurrentAudioDeviceType();

            driverBox.clear(juce::dontSendNotification);
            int selTypeId = 1;
            juce::AudioIODeviceType* currentType = nullptr;
            for (int i = 0; i < types.size(); ++i) {
                const auto name = types[i]->getTypeName();
                driverBox.addItem(name, i + 1);
                if (name == currentTypeName) {
                    selTypeId = i + 1;
                    currentType = types[i];
                }
            }
            if (currentType == nullptr && !types.isEmpty()) {
                currentType = types[0];
                selTypeId = 1;
            }
            if (!types.isEmpty()) {
                driverBox.setSelectedId(selTypeId, juce::dontSendNotification);
            }

            const auto setup = dm->getAudioDeviceSetup();
            separateInOut = (currentType == nullptr || currentType->hasSeparateInputsAndOutputs());

            outputBox.clear(juce::dontSendNotification);
            inputBox.clear(juce::dontSendNotification);
            if (currentType != nullptr) {
                if (forceScan) {
                    currentType->scanForDevices();
                }
                const auto outDevs = currentType->getDeviceNames(false);
                outputBox.addItem("<< None >>", 1);
                int selOut = 1;
                for (int i = 0; i < outDevs.size(); ++i) {
                    outputBox.addItem(outDevs[i], i + 2);
                    if (outDevs[i] == setup.outputDeviceName) selOut = i + 2;
                }
                outputBox.setSelectedId(selOut, juce::dontSendNotification);

                if (separateInOut) {
                    const auto inDevs = currentType->getDeviceNames(true);
                    inputBox.addItem("<< None >>", 1);
                    int selIn = 1;
                    for (int i = 0; i < inDevs.size(); ++i) {
                        inputBox.addItem(inDevs[i], i + 2);
                        if (inDevs[i] == setup.inputDeviceName) selIn = i + 2;
                    }
                    inputBox.setSelectedId(selIn, juce::dontSendNotification);
                }
            }
            inputBox.setVisible(separateInOut);

            // Sample Rate & Buffer Size from live active device
            srBox.clear(juce::dontSendNotification);
            bufBox.clear(juce::dontSendNotification);
            if (auto* dev = dm->getCurrentAudioDevice()) {
                const auto rates = dev->getAvailableSampleRates();
                const double curSr = dev->getCurrentSampleRate();
                int selSrId = 1;
                for (int i = 0; i < rates.size(); ++i) {
                    srBox.addItem(juce::String(static_cast<int>(std::lround(rates[i]))) + " Hz", i + 1);
                    if (std::abs(rates[i] - curSr) < 1.0) selSrId = i + 1;
                }
                if (!rates.isEmpty()) srBox.setSelectedId(selSrId, juce::dontSendNotification);
                srBox.setEnabled(!rates.isEmpty());

                const auto bufs = dev->getAvailableBufferSizes();
                const int curBuf = dev->getCurrentBufferSizeSamples();
                int selBufId = 1;
                for (int i = 0; i < bufs.size(); ++i) {
                    const double ms = (curSr > 1000.0) ? (static_cast<double>(bufs[i]) * 1000.0 / curSr) : 0.0;
                    bufBox.addItem(juce::String(bufs[i]) + " samples (" + juce::String(ms, 1) + " ms)", i + 1);
                    if (bufs[i] == curBuf) selBufId = i + 1;
                }
                if (!bufs.isEmpty()) bufBox.setSelectedId(selBufId, juce::dontSendNotification);
                bufBox.setEnabled(!bufs.isEmpty());

                asioPanelBtn.setEnabled(dev->hasControlPanel());
                testToneBtn.setEnabled(dev->getOutputChannelNames().size() > 0);
                outChannelsGrid.setChannels(dev->getOutputChannelNames(), dev->getActiveOutputChannels());
                inChannelsGrid.setChannels(dev->getInputChannelNames(), dev->getActiveInputChannels());
                outChannelsGrid.setEmptyMessage("No output channels on current device");
                inChannelsGrid.setEmptyMessage("No input channels on current device");
            } else {
                srBox.addItem("No Active Device", 1);
                srBox.setSelectedId(1, juce::dontSendNotification);
                srBox.setEnabled(false);
                bufBox.addItem("No Active Device", 1);
                bufBox.setSelectedId(1, juce::dontSendNotification);
                bufBox.setEnabled(false);
                asioPanelBtn.setEnabled(false);
                testToneBtn.setEnabled(false);
                outChannelsGrid.setChannels({}, {});
                inChannelsGrid.setChannels({}, {});
                outChannelsGrid.setEmptyMessage("Select an Output Device above");
                inChannelsGrid.setEmptyMessage("Select an Input Device above");
            }

            if (owner.host.muteInputValue != nullptr) {
                muteInputSwitch.setState(static_cast<bool>(owner.host.muteInputValue->getValue()), false);
            }

            resized();
            repaint();
        }

        void updateLive() {
            const auto& chain = owner.processor.getSignalChain();
            inPeakBar.setPeaks(chain.getInputPeakL(), chain.getInputPeakR());
            outPeakBar.setPeaks(chain.getOutputPeakL(), chain.getOutputPeakR());
            if (owner.host.muteInputValue != nullptr) {
                muteInputSwitch.setState(static_cast<bool>(owner.host.muteInputValue->getValue()), false);
            }
            repaint(telemetryRowRect);
        }

        void paint(juce::Graphics& g) override {
            using namespace settings_ui;
            g.fillAll(kBgCanvas);

            // Top 3 OLED Telemetry Cards (Sample Rate | Buffer Size | Round-Trip Latency)
            const auto t = owner.readTelemetry();
            const int gap = 10;
            const int cardW = (telemetryRowRect.getWidth() - 2 * gap) / 3;
            auto row = telemetryRowRect;

            const double blockMs = (t.sampleRate > 1000.0) ? (static_cast<double>(t.bufferSamples) * 1000.0 / t.sampleRate) : 0.0;
            drawOledTelemetryCard(g, row.removeFromLeft(cardW), "SAMPLE RATE",
                                  t.sampleRateShort,
                                  t.bitDepth > 0 ? (juce::String(t.bitDepth) + "-bit engine") : "32-bit float",
                                  kAccentCyan);
            row.removeFromLeft(gap);
            drawOledTelemetryCard(g, row.removeFromLeft(cardW), "BUFFER SIZE",
                                  juce::String(t.bufferSamples) + " smp",
                                  juce::String(blockMs, 1) + " ms block",
                                  kAccentCyan);
            row.removeFromLeft(gap);
            drawOledTelemetryCard(g, row, "ROUND-TRIP LATENCY",
                                  t.rtlText,
                                  (t.inputLatencySamples + t.outputLatencySamples > 0)
                                      ? ("In " + juce::String(t.inputLatencySamples) + " + Out " + juce::String(t.outputLatencySamples) + " smp")
                                      : "Estimated 2x buffer",
                                  kAccentGreen);

            if (!owner.host.isAvailable()) {
                drawCardFrame(g, deviceCardRect, "HOST DAW AUDIO ENGINE (VST3 MODE)",
                              "Audio driver, sample rate, and buffer size are managed directly by your Host DAW.");
                auto body = deviceCardRect.reduced(16, 50);
                g.setColour(kTextSecondary);
                g.setFont(juce::Font(juce::FontOptions(10.8f, juce::Font::plain)));
                g.drawFittedText(
                    "PedalBoard is running as a VST3 plugin inside your DAW. Change audio interface, ASIO buffer size, "
                    "and hardware I/O routing inside your DAW's Audio Preferences.\n\n"
                    "Tip: Use the INPUT & DSP tab to configure Mono Ch 1 / Ch 2 guitar routing, Input Sub-Rumble HPF, "
                    "Tuner A4 calibration, and NAM Loudness Normalization.",
                    body.removeFromTop(80), juce::Justification::topLeft, 5);
                return;
            }

            // Card 1: Audio Interface & Driver
            drawCardFrame(g, deviceCardRect, "AUDIO INTERFACE & DRIVER",
                          "Select WASAPI / ASIO driver, interface devices, sample rate, and low-latency buffer size.");

            auto drawFieldLabel = [&](juce::Rectangle<int> r, const juce::String& text) {
                g.setColour(kTextSecondary);
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
                g.drawText(text, r, juce::Justification::centredLeft, true);
            };
            drawFieldLabel(lblDriverRect, "Driver Architecture");
            drawFieldLabel(lblOutRect, separateInOut ? "Output Device" : "Audio Interface (In + Out)");
            if (separateInOut) {
                drawFieldLabel(lblInRect, "Input Device");
            }
            drawFieldLabel(lblSrRect, "Sample Rate");
            drawFieldLabel(lblBufRect, "Buffer Size");

            if (lastError.isNotEmpty() && !errorRect.isEmpty()) {
                auto er = errorRect.toFloat();
                g.setColour(kAccentRed.withAlpha(0.16f));
                g.fillRoundedRectangle(er, 5.0f);
                g.setColour(kAccentRed.withAlpha(0.65f));
                g.drawRoundedRectangle(er, 5.0f, 1.0f);
                g.setColour(juce::Colour(0xfffca5a5));
                g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
                g.drawText("Audio Error: " + lastError, errorRect.reduced(8, 2), juce::Justification::centredLeft, true);
            }

            // Card 2: Active Channels & Live Signal Monitor
            drawCardFrame(g, channelsCardRect, "ACTIVE CHANNELS & LIVE SIGNAL MONITOR",
                          "Enable or disable stereo channel pairs and verify real-time input / output audio levels.");
            drawFieldLabel(lblOutChRect, "Active Output Channels");
            drawFieldLabel(lblInChRect, "Active Input Channels");

            // Card 3: Feedback Protection
            drawCardFrame(g, feedbackCardRect, "FEEDBACK LOOP PROTECTION",
                          "Prevents microphone-to-speaker feedback on laptops. Switch to LIVE INPUT when playing guitar.");
            const bool isMuted = muteInputSwitch.getState();
            g.setColour(isMuted ? juce::Colour(0xfffca5a5) : kTextSecondary);
            g.setFont(juce::Font(juce::FontOptions(10.5f, isMuted ? juce::Font::bold : juce::Font::plain)));
            g.drawText(isMuted ? "Audio input is currently MUTED — toggle OFF to hear your guitar!"
                               : "Audio input is LIVE and routed into the PedalBoard DSP engine.",
                       feedbackTextRect, juce::Justification::centredLeft, true);
        }

        void resized() override {
            auto area = getLocalBounds().reduced(18, 16);
            telemetryRowRect = area.removeFromTop(66);
            area.removeFromTop(14);

            if (!owner.host.isAvailable()) {
                deviceCardRect = area.removeFromTop(210);
                auto meters = deviceCardRect.reduced(16, 14).removeFromBottom(32);
                const int half = (meters.getWidth() - 12) / 2;
                inPeakBar.setBounds(meters.removeFromLeft(half));
                meters.removeFromLeft(12);
                outPeakBar.setBounds(meters);
                return;
            }

            const int deviceCardH = separateInOut ? 248 : 210;
            deviceCardRect = area.removeFromTop(deviceCardH);
            {
                auto inner = deviceCardRect.reduced(14, 12);
                auto headerBtns = inner.removeFromTop(26);
                defaultsBtn.setBounds(headerBtns.removeFromRight(92));
                headerBtns.removeFromRight(6);
                rescanBtn.setBounds(headerBtns.removeFromRight(72));
                headerBtns.removeFromRight(6);
                asioPanelBtn.setBounds(headerBtns.removeFromRight(112));

                inner.removeFromTop(18); // subtitle clearance
                const int labelW = 148;
                const int rowH = 28;
                const int rowGap = 8;

                auto row1 = inner.removeFromTop(rowH);
                lblDriverRect = row1.removeFromLeft(labelW);
                driverBox.setBounds(row1);
                inner.removeFromTop(rowGap);

                auto row2 = inner.removeFromTop(rowH);
                lblOutRect = row2.removeFromLeft(labelW);
                testToneBtn.setBounds(row2.removeFromRight(88));
                row2.removeFromRight(8);
                outputBox.setBounds(row2);
                inner.removeFromTop(rowGap);

                if (separateInOut) {
                    auto row3 = inner.removeFromTop(rowH);
                    lblInRect = row3.removeFromLeft(labelW);
                    inputBox.setBounds(row3);
                    inner.removeFromTop(rowGap);
                } else {
                    lblInRect = {};
                }

                auto row4 = inner.removeFromTop(rowH);
                const int halfW = (row4.getWidth() - 14) / 2;
                auto leftHalf = row4.removeFromLeft(halfW);
                lblSrRect = leftHalf.removeFromLeft(90);
                srBox.setBounds(leftHalf);
                row4.removeFromLeft(14);
                lblBufRect = row4.removeFromLeft(84);
                bufBox.setBounds(row4);

                if (lastError.isNotEmpty()) {
                    inner.removeFromTop(8);
                    errorRect = inner.removeFromTop(24);
                } else {
                    errorRect = {};
                }
            }

            area.removeFromTop(14);
            const int innerGridW = std::max(300, area.getWidth() - 28);
            const int outGridH = outChannelsGrid.getPreferredHeight(innerGridW);
            const int inGridH  = inChannelsGrid.getPreferredHeight(innerGridW);
            channelsCardRect = area.removeFromTop(114 + outGridH + inGridH);
            {
                auto inner = channelsCardRect.reduced(14, 12);
                inner.removeFromTop(36);

                auto outRow = inner.removeFromTop(20);
                lblOutChRect = outRow.removeFromLeft(160);
                outPeakBar.setBounds(outRow.removeFromRight(std::min(260, outRow.getWidth())).expanded(0, 2));
                inner.removeFromTop(5);
                outChannelsGrid.setBounds(inner.removeFromTop(outGridH));

                inner.removeFromTop(10);
                auto inRow = inner.removeFromTop(20);
                lblInChRect = inRow.removeFromLeft(160);
                inPeakBar.setBounds(inRow.removeFromRight(std::min(260, inRow.getWidth())).expanded(0, 2));
                inner.removeFromTop(5);
                inChannelsGrid.setBounds(inner.removeFromTop(inGridH));
            }

            area.removeFromTop(14);
            feedbackCardRect = area.removeFromTop(98);
            {
                auto inner = feedbackCardRect.reduced(14, 12);
                inner.removeFromTop(38);
                muteInputSwitch.setBounds(inner.removeFromRight(156).withHeight(28));
                inner.removeFromRight(10);
                feedbackTextRect = inner.withHeight(28);
            }
        }

    private:
        void handleDriverChanged() {
            if (owner.host.deviceManager == nullptr) return;
            const auto chosen = driverBox.getText();
            if (chosen.isNotEmpty() && chosen != owner.host.deviceManager->getCurrentAudioDeviceType()) {
                owner.host.deviceManager->setCurrentAudioDeviceType(chosen, true);
                lastError.clear();
                if (owner.host.saveAudioState) owner.host.saveAudioState();
                refreshFromDeviceManager(true);
            }
        }

        void handleDeviceSelectionChanged(bool isInput) {
            if (owner.host.deviceManager == nullptr) return;
            auto setup = owner.host.deviceManager->getAudioDeviceSetup();
            if (!separateInOut) {
                const auto name = (outputBox.getSelectedId() <= 1) ? juce::String() : outputBox.getText();
                setup.outputDeviceName = name;
                setup.inputDeviceName  = name;
                setup.useDefaultOutputChannels = true;
                setup.useDefaultInputChannels  = true;
            } else if (isInput) {
                setup.inputDeviceName = (inputBox.getSelectedId() <= 1) ? juce::String() : inputBox.getText();
                setup.useDefaultInputChannels = true;
            } else {
                setup.outputDeviceName = (outputBox.getSelectedId() <= 1) ? juce::String() : outputBox.getText();
                setup.useDefaultOutputChannels = true;
            }
            lastError = owner.host.deviceManager->setAudioDeviceSetup(setup, true);
            if (lastError.isEmpty() && owner.host.saveAudioState) owner.host.saveAudioState();
            refreshFromDeviceManager(false);
        }

        void handleSampleRateOrBufferChanged() {
            if (owner.host.deviceManager == nullptr) return;
            auto* dev = owner.host.deviceManager->getCurrentAudioDevice();
            if (dev == nullptr) return;

            auto setup = owner.host.deviceManager->getAudioDeviceSetup();
            const auto rates = dev->getAvailableSampleRates();
            const int srIdx = srBox.getSelectedId() - 1;
            if (srIdx >= 0 && srIdx < rates.size()) {
                setup.sampleRate = rates[srIdx];
            }
            const auto bufs = dev->getAvailableBufferSizes();
            const int bufIdx = bufBox.getSelectedId() - 1;
            if (bufIdx >= 0 && bufIdx < bufs.size()) {
                setup.bufferSize = bufs[bufIdx];
            }
            lastError = owner.host.deviceManager->setAudioDeviceSetup(setup, true);
            if (lastError.isEmpty() && owner.host.saveAudioState) owner.host.saveAudioState();
            refreshFromDeviceManager(false);
        }

        void toggleChannels(bool isInput, int bitA, int bitB, bool active) {
            if (owner.host.deviceManager == nullptr) return;
            auto setup = owner.host.deviceManager->getAudioDeviceSetup();
            if (auto* dev = owner.host.deviceManager->getCurrentAudioDevice()) {
                if (isInput) {
                    setup.inputChannels = dev->getActiveInputChannels();
                    setup.useDefaultInputChannels = false;
                    setup.inputChannels.setBit(bitA, active);
                    if (bitB >= 0) setup.inputChannels.setBit(bitB, active);
                } else {
                    setup.outputChannels = dev->getActiveOutputChannels();
                    setup.useDefaultOutputChannels = false;
                    setup.outputChannels.setBit(bitA, active);
                    if (bitB >= 0) setup.outputChannels.setBit(bitB, active);
                }
                lastError = owner.host.deviceManager->setAudioDeviceSetup(setup, true);
                if (lastError.isEmpty() && owner.host.saveAudioState) owner.host.saveAudioState();
                refreshFromDeviceManager(false);
            }
        }

        StudioSettingsContent& owner;
        bool separateInOut = true;
        juce::String lastError;

        juce::Rectangle<int> telemetryRowRect, deviceCardRect, channelsCardRect, feedbackCardRect;
        juce::Rectangle<int> lblDriverRect, lblOutRect, lblInRect, lblSrRect, lblBufRect, errorRect;
        juce::Rectangle<int> lblOutChRect, lblInChRect, feedbackTextRect;

        juce::ComboBox driverBox, outputBox, inputBox, srBox, bufBox;
        juce::TextButton asioPanelBtn, testToneBtn, rescanBtn, defaultsBtn;
        settings_ui::ChannelToggleGrid outChannelsGrid, inChannelsGrid;
        settings_ui::HorizontalPeakBar outPeakBar, inPeakBar;
        settings_ui::StudioSwitchPill muteInputSwitch;
    };

    // =========================================================================
    // TAB 2: INPUT & DSP
    // =========================================================================
    class InputDspPage : public juce::Component {
    public:
        explicit InputDspPage(StudioSettingsContent& o)
            : owner(o),
              inputModePills({ "Stereo (L+R)", "Mono Ch 1 (L)", "Mono Ch 2 (R)", "Sum Mono" }, settings_ui::kAccentCyan),
              hpfPills({ "OFF", "40 Hz", "60 Hz", "80 Hz" }, settings_ui::kAccentCyan),
              phaseSwitch(settings_ui::kAccentAmber, "INVERTED 180", "NORMAL"),
              namNormSwitch(settings_ui::kAccentGreen, "-18 LUFS ON", "RAW GAIN"),
              safetyLimiterSwitch(settings_ui::kAccentGreen, "LIMITER ON", "BYPASSED"),
              inMeter("IN"),
              outMeter("OUT") {
            inputModePills.onChange = [this](int idx) {
                owner.processor.setInputChannelMode(static_cast<InputChannelMode>(idx));
            };
            hpfPills.onChange = [this](int idx) {
                static constexpr float kFreqs[4] = { 0.0f, 40.0f, 60.0f, 80.0f };
                owner.processor.setInputRumbleHpfHz(kFreqs[std::clamp(idx, 0, 3)]);
            };
            phaseSwitch.onToggle = [this](bool inv) { owner.processor.setInputPhaseInvert(inv); };
            namNormSwitch.onToggle = [this](bool en) { owner.processor.setNamLoudnessNormalization(en); };
            safetyLimiterSwitch.onToggle = [this](bool en) { owner.processor.setOutputSafetyLimiter(en); };

            auto setupLinear = [](juce::Slider& s, const juce::String& suffix) {
                s.setSliderStyle(juce::Slider::LinearHorizontal);
                s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 24);
                s.setTextValueSuffix(suffix);
                s.setColour(juce::Slider::trackColourId, settings_ui::kAccentCyan);
                s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
                s.setColour(juce::Slider::backgroundColourId, settings_ui::kBgRecessed);
            };
            setupLinear(inTrimSlider, " dB");
            setupLinear(outTrimSlider, " dB");
            inTrimSlider.setDoubleClickReturnValue(true, 0.0);
            outTrimSlider.setDoubleClickReturnValue(true, 0.0);

            inTrimAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.processor.getAPVTS(), params::kInputGain, inTrimSlider);
            outTrimAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.processor.getAPVTS(), params::kOutputGain, outTrimSlider);

            setupLinear(tunerRefSlider, " Hz");
            tunerRefSlider.setRange(415.0, 465.0, 0.5);
            tunerRefSlider.setDoubleClickReturnValue(true, 440.0);
            tunerRefSlider.onValueChange = [this] {
                owner.processor.setTunerReferenceHz(static_cast<float>(tunerRefSlider.getValue()));
            };

            btn432.setButtonText("432 Hz");
            btn440.setButtonText("440 Hz (Std)");
            btn442.setButtonText("442 Hz");
            btn432.onClick = [this] { tunerRefSlider.setValue(432.0, juce::sendNotificationSync); };
            btn440.onClick = [this] { tunerRefSlider.setValue(440.0, juce::sendNotificationSync); };
            btn442.onClick = [this] { tunerRefSlider.setValue(442.0, juce::sendNotificationSync); };

            resetDspBtn.setButtonText("RESET DSP SETTINGS TO DEFAULTS");
            resetDspBtn.onClick = [this] {
                owner.processor.resetStudioSettingsToDefaults();
                syncFromProcessor();
            };

            for (juce::Component* c : std::initializer_list<juce::Component*>{
                     &inputModePills, &hpfPills, &phaseSwitch,
                     &inTrimSlider, &outTrimSlider, &inMeter, &outMeter,
                     &tunerRefSlider, &btn432, &btn440, &btn442,
                     &namNormSwitch, &safetyLimiterSwitch, &resetDspBtn }) {
                addAndMakeVisible(*c);
            }
        }

        int computePreferredHeight() const { return 634; }

        void syncFromProcessor() {
            inputModePills.setSelectedIndex(static_cast<int>(owner.processor.getInputChannelMode()), false);
            const float hpf = owner.processor.getInputRumbleHpfHz();
            int hpfIdx = 0;
            if (hpf >= 70.0f)      hpfIdx = 3;
            else if (hpf >= 50.0f) hpfIdx = 2;
            else if (hpf >= 20.0f) hpfIdx = 1;
            hpfPills.setSelectedIndex(hpfIdx, false);

            phaseSwitch.setState(owner.processor.isInputPhaseInvert(), false);
            namNormSwitch.setState(owner.processor.isNamLoudnessNormalization(), false);
            safetyLimiterSwitch.setState(owner.processor.isOutputSafetyLimiter(), false);
            tunerRefSlider.setValue(static_cast<double>(owner.processor.getTunerReferenceHz()), juce::dontSendNotification);
            repaint();
        }

        void updateLive() {
            const auto& chain = owner.processor.getSignalChain();
            inMeter.setPeaks(chain.getInputPeakL(), chain.getInputPeakR());
            outMeter.setPeaks(chain.getOutputPeakL(), chain.getOutputPeakR());
        }

        void paint(juce::Graphics& g) override {
            using namespace settings_ui;
            g.fillAll(kBgCanvas);

            drawCardFrame(g, inputCardRect, "GUITAR INPUT ROUTING & SIGNAL CONDITIONING",
                          "Use Mono Ch 1 (L) or Ch 2 (R) for single-cable guitar interfaces so audio centers cleanly in Stereo.");
            auto drawLabel = [&](juce::Rectangle<int> r, const juce::String& title, const juce::String& hint = {}) {
                g.setColour(kTextPrimary);
                g.setFont(juce::Font(juce::FontOptions(10.2f, juce::Font::bold)));
                g.drawText(title, r.withHeight(hint.isEmpty() ? r.getHeight() : 15), juce::Justification::centredLeft, true);
                if (hint.isNotEmpty()) {
                    g.setColour(kTextMuted);
                    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::plain)));
                    g.drawText(hint, r.withTrimmedTop(15), juce::Justification::centredLeft, true);
                }
            };
            drawLabel(lblModeRect,  "Input Channel Mode");
            drawLabel(lblHpfRect,   "Input Sub-Rumble HPF");
            drawLabel(lblPhaseRect, "Input Polarity / Phase", "Inverts polarity 180 deg at the input stage");
            drawLabel(lblInTrimRect, "Input Trim Gain");
            drawLabel(lblOutTrimRect, "Master Output Trim");

            drawCardFrame(g, tunerCardRect, "CHROMATIC TUNER CALIBRATION",
                          "Global A4 reference pitch applied to the top rack chromatic tuner (saved automatically).");
            drawLabel(lblTunerRect, "A4 Reference Pitch");

            drawCardFrame(g, engineCardRect, "NEURAL NAM ENGINE & MASTER PROTECTION",
                          "Control neural capture loudness normalization and output peak limiting.");
            drawLabel(lblNamNormRect, "NAM Auto Loudness (-18 LUFS)",
                      "Levels volume across .nam models; disable for raw uncalibrated capture gain");
            drawLabel(lblLimitRect, "Output Safety Soft-Limiter",
                      "Transparent soft-knee ceiling at -0.05 dBFS preventing harsh digital clipping");
        }

        void resized() override {
            auto area = getLocalBounds().reduced(18, 16);

            inputCardRect = area.removeFromTop(268);
            {
                auto in = inputCardRect.reduced(14, 12);
                in.removeFromTop(38);
                const int lw = 152;

                auto r1 = in.removeFromTop(30);
                lblModeRect = r1.removeFromLeft(lw);
                inputModePills.setBounds(r1);
                in.removeFromTop(10);

                auto r2 = in.removeFromTop(30);
                lblHpfRect = r2.removeFromLeft(lw);
                hpfPills.setBounds(r2.withWidth(std::min(300, r2.getWidth())));
                in.removeFromTop(10);

                auto r3 = in.removeFromTop(32);
                phaseSwitch.setBounds(r3.removeFromRight(156).withHeight(28));
                lblPhaseRect = r3;
                in.removeFromTop(10);

                auto r4 = in.removeFromTop(28);
                lblInTrimRect = r4.removeFromLeft(lw);
                inMeter.setBounds(r4.removeFromRight(180).reduced(0, 1));
                r4.removeFromRight(8);
                inTrimSlider.setBounds(r4);
                in.removeFromTop(8);

                auto r5 = in.removeFromTop(28);
                lblOutTrimRect = r5.removeFromLeft(lw);
                outMeter.setBounds(r5.removeFromRight(180).reduced(0, 1));
                r5.removeFromRight(8);
                outTrimSlider.setBounds(r5);
            }

            area.removeFromTop(14);
            tunerCardRect = area.removeFromTop(108);
            {
                auto in = tunerCardRect.reduced(14, 12);
                in.removeFromTop(38);
                auto row = in.removeFromTop(30);
                lblTunerRect = row.removeFromLeft(130);
                btn442.setBounds(row.removeFromRight(62));
                row.removeFromRight(5);
                btn440.setBounds(row.removeFromRight(88));
                row.removeFromRight(5);
                btn432.setBounds(row.removeFromRight(62));
                row.removeFromRight(10);
                tunerRefSlider.setBounds(row);
            }

            area.removeFromTop(14);
            engineCardRect = area.removeFromTop(142);
            {
                auto in = engineCardRect.reduced(14, 12);
                in.removeFromTop(38);

                auto r1 = in.removeFromTop(34);
                namNormSwitch.setBounds(r1.removeFromRight(156).withHeight(28));
                lblNamNormRect = r1;
                in.removeFromTop(10);

                auto r2 = in.removeFromTop(34);
                safetyLimiterSwitch.setBounds(r2.removeFromRight(156).withHeight(28));
                lblLimitRect = r2;
            }

            area.removeFromTop(14);
            resetDspBtn.setBounds(area.removeFromTop(28).removeFromRight(230));
        }

    private:
        StudioSettingsContent& owner;
        juce::Rectangle<int> inputCardRect, tunerCardRect, engineCardRect;
        juce::Rectangle<int> lblModeRect, lblHpfRect, lblPhaseRect, lblInTrimRect, lblOutTrimRect;
        juce::Rectangle<int> lblTunerRect, lblNamNormRect, lblLimitRect;

        settings_ui::SegmentedPillGroup inputModePills, hpfPills;
        settings_ui::StudioSwitchPill phaseSwitch, namNormSwitch, safetyLimiterSwitch;
        juce::Slider inTrimSlider, outTrimSlider, tunerRefSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inTrimAtt, outTrimAtt;
        settings_ui::HorizontalPeakBar inMeter, outMeter;
        juce::TextButton btn432, btn440, btn442, resetDspBtn;
    };

    // =========================================================================
    // TAB 3: MIDI & CONTROL
    // =========================================================================
    class MidiControlPage : public juce::Component {
    public:
        explicit MidiControlPage(StudioSettingsContent& o)
            : owner(o),
              pcSwitch(settings_ui::kAccentGreen, "ENABLED", "IGNORED"),
              midiDevModel(*this),
              mappingModel(*this) {
            rescanMidiBtn.setButtonText("RESCAN MIDI");
            rescanMidiBtn.onClick = [this] { refreshMidiDevices(); };

            midiDeviceList.setModel(&midiDevModel);
            midiDeviceList.setRowHeight(32);
            midiDeviceList.setColour(juce::ListBox::backgroundColourId, settings_ui::kBgRecessed);
            midiDeviceList.setColour(juce::ListBox::outlineColourId, settings_ui::kBorderSubtle);
            midiDeviceList.setOutlineThickness(1);

            for (int cc = 0; cc <= 127; ++cc) {
                juce::String lbl = "CC #" + juce::String(cc);
                if (cc == 1)  lbl += "  (Mod Wheel)";
                if (cc == 4)  lbl += "  (Foot Controller)";
                if (cc == 7)  lbl += "  (Volume)";
                if (cc == 11) lbl += "  (Expression - Default)";
                if (cc == 64) lbl += "  (Sustain / Switch)";
                expCcBox.addItem(lbl, cc + 1);
            }
            expCcBox.onChange = [this] {
                const int cc = expCcBox.getSelectedId() - 1;
                if (cc >= 0 && cc <= 127) {
                    owner.processor.setExpressionPedalCc(cc);
                }
            };

            expLearnBtn.setButtonText("MIDI LEARN EXP");
            expLearnBtn.onClick = [this] {
                const bool next = !owner.processor.isExpressionPedalMidiLearnArmed();
                owner.processor.armExpressionPedalMidiLearn(next);
                updateLive();
            };

            pcSwitch.onToggle = [this](bool en) {
                owner.processor.setMidiProgramChangeEnabled(en);
            };

            learnNextBtn.setButtonText("LEARN CONTROL");
            learnNextBtn.setTooltip("Arm Global MIDI Learn: click any knob/switch in PedalBoard, then move your MIDI controller");
            learnNextBtn.onClick = [this] {
                auto& mgr = owner.processor.getMidiLearnManager();
                if (mgr.isLearnArmed()) mgr.cancelLearn();
                else                    mgr.armGlobalLearn();
                updateLive();
            };

            clearAllMappingsBtn.setButtonText("CLEAR ALL");
            clearAllMappingsBtn.onClick = [this] {
                owner.processor.getMidiLearnManager().clearAllMappings();
                refreshMappings();
            };

            mappingsList.setModel(&mappingModel);
            mappingsList.setRowHeight(32);
            mappingsList.setColour(juce::ListBox::backgroundColourId, settings_ui::kBgRecessed);
            mappingsList.setColour(juce::ListBox::outlineColourId, settings_ui::kBorderSubtle);
            mappingsList.setOutlineThickness(1);

            for (juce::Component* c : std::initializer_list<juce::Component*>{
                     &rescanMidiBtn, &midiDeviceList,
                     &expCcBox, &expLearnBtn, &pcSwitch,
                     &learnNextBtn, &clearAllMappingsBtn, &mappingsList }) {
                addAndMakeVisible(*c);
            }
        }

        ~MidiControlPage() override {
            midiDeviceList.setModel(nullptr);
            mappingsList.setModel(nullptr);
        }

        int computePreferredHeight() const { return 656; }

        void refreshMidiDevices() {
            midiDevices.clear();
            const bool standalone = owner.host.isAvailable();
            midiDeviceList.setVisible(standalone);
            rescanMidiBtn.setVisible(standalone);
            if (standalone) {
                const auto devs = juce::MidiInput::getAvailableDevices();
                for (const auto& d : devs) {
                    midiDevices.push_back(d);
                }
                midiDeviceList.updateContent();
            }
            expCcBox.setSelectedId(owner.processor.getExpressionPedalCc() + 1, juce::dontSendNotification);
            pcSwitch.setState(owner.processor.isMidiProgramChangeEnabled(), false);
            refreshMappings();
            resized();
            repaint();
        }

        void refreshMappings() {
            activeMappings = owner.processor.getMidiLearnManager().getAllMappings();
            clearAllMappingsBtn.setEnabled(!activeMappings.empty());
            mappingsList.updateContent();
            repaint();
        }

        void updateLive() {
            const bool expArmed = owner.processor.isExpressionPedalMidiLearnArmed();
            expLearnBtn.setButtonText(expArmed ? "MOVE PEDAL..." : "MIDI LEARN EXP");
            expLearnBtn.setColour(juce::TextButton::buttonColourId,
                                  expArmed ? juce::Colour(0xffb45309) : juce::Colour(0xff1e2433));

            const int curExpCc = owner.processor.getExpressionPedalCc();
            if (expCcBox.getSelectedId() != curExpCc + 1) {
                expCcBox.setSelectedId(curExpCc + 1, juce::dontSendNotification);
            }

            const bool mgrArmed = owner.processor.getMidiLearnManager().isLearnArmed();
            learnNextBtn.setButtonText(mgrArmed ? "CANCEL LEARN" : "LEARN CONTROL");
            learnNextBtn.setColour(juce::TextButton::buttonColourId,
                                   mgrArmed ? juce::Colour(0xffb45309) : juce::Colour(0xff0f4c5c));

            const uint64_t mVer = owner.processor.getMidiLearnManager().getVersion();
            if (mVer != lastObservedMidiVer) {
                lastObservedMidiVer = mVer;
                refreshMappings();
            }

            repaint(monitorCardRect);
        }

        void paint(juce::Graphics& g) override {
            using namespace settings_ui;
            g.fillAll(kBgCanvas);

            // Card 1: Live MIDI Activity Monitor
            drawCardFrame(g, monitorCardRect, "LIVE MIDI ACTIVITY MONITOR");
            auto oled = monitorCardRect.reduced(14, 10).withTrimmedTop(26);
            g.setColour(kBgRecessed);
            g.fillRoundedRectangle(oled.toFloat(), 6.0f);
            g.setColour(kBorderSubtle);
            g.drawRoundedRectangle(oled.toFloat(), 6.0f, 1.0f);

            const auto snap = owner.processor.getLastMidiActivity();
            const int64_t nowMs = static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes());
            const bool recent = (snap.type != 0 && (nowMs - snap.timestampMs) >= 0 && (nowMs - snap.timestampMs) < 420);

            auto inner = oled.reduced(12, 0);
            g.setColour(recent ? kAccentGreen : juce::Colour(0xff293345));
            g.fillEllipse(static_cast<float>(inner.getX()), static_cast<float>(inner.getCentreY()) - 4.5f, 9.0f, 9.0f);

            juce::String msg = "Waiting for incoming MIDI controller data (CC, Program Change, or Notes)...";
            if (snap.type == 1) {
                msg = "CH " + juce::String(snap.channel) + "   |   CC #" + juce::String(snap.data1)
                    + "   =   " + juce::String(snap.data2) + "  (" + juce::String(static_cast<int>(std::lround(snap.data2 * 100.0 / 127.0))) + "%)";
            } else if (snap.type == 2) {
                msg = "CH " + juce::String(snap.channel) + "   |   PROGRAM CHANGE #" + juce::String(snap.data1);
            } else if (snap.type == 3) {
                msg = "CH " + juce::String(snap.channel) + "   |   NOTE ON #" + juce::String(snap.data1) + "  vel=" + juce::String(snap.data2);
            }

            g.setColour(recent ? kTextPrimary : (snap.type != 0 ? kTextSecondary : kTextMuted));
            g.setFont(juce::Font(juce::FontOptions(10.5f, recent ? juce::Font::bold : juce::Font::plain)));
            g.drawText(msg, inner.withTrimmedLeft(18), juce::Justification::centredLeft, true);

            // Card 2: MIDI Input Devices
            drawCardFrame(g, devicesCardRect, "MIDI INPUT DEVICES",
                          owner.host.isAvailable()
                              ? "Click any connected MIDI foot controller or USB MIDI keyboard to enable / disable."
                              : "MIDI inputs are routed from your Host DAW MIDI track directly into PedalBoard.");
            if (!owner.host.isAvailable()) {
                g.setColour(kTextMuted);
                g.setFont(juce::Font(juce::FontOptions(10.2f, juce::Font::plain)));
                g.drawText("Route your MIDI foot controller track to the PedalBoard VST3 instance inside your DAW.",
                           devicesCardRect.reduced(16, 48), juce::Justification::centredLeft, true);
            } else if (midiDevices.empty()) {
                g.setColour(kTextMuted);
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
                g.drawText("No MIDI input hardware detected. Connect a USB MIDI controller and click RESCAN MIDI.",
                           midiDeviceList.getBounds().reduced(12, 6), juce::Justification::centredLeft, true);
            }

            // Card 3: Expression Pedal & Program Change
            drawCardFrame(g, controlCardRect, "EXPRESSION PEDAL & PROGRAM CHANGE",
                          "Assign a hardware MIDI expression pedal (Wah / Whammy / Pitch) and preset Program Change receive.");
            g.setColour(kTextPrimary);
            g.setFont(juce::Font(juce::FontOptions(10.2f, juce::Font::bold)));
            g.drawText("Expression Pedal CC", lblExpRect, juce::Justification::centredLeft);
            g.drawText("MIDI Program Change", lblPcRect, juce::Justification::centredLeft);

            // Card 4: Active MIDI CC Mappings
            drawCardFrame(g, mappingsCardRect,
                          "ACTIVE MIDI CC MAPPINGS (" + juce::String(activeMappings.size()) + ")",
                          "Right-click any knob or footswitch on the board to bind MIDI CC, or click a row below to remove.");
        }

        void resized() override {
            auto area = getLocalBounds().reduced(18, 16);

            monitorCardRect = area.removeFromTop(76);
            area.removeFromTop(14);

            const int devH = owner.host.isAvailable() ? 148 : 88;
            devicesCardRect = area.removeFromTop(devH);
            if (owner.host.isAvailable()) {
                auto in = devicesCardRect.reduced(14, 12);
                rescanMidiBtn.setBounds(in.removeFromTop(24).removeFromRight(96));
                in.removeFromTop(18);
                midiDeviceList.setBounds(in);
            }

            area.removeFromTop(14);
            controlCardRect = area.removeFromTop(126);
            {
                auto in = controlCardRect.reduced(14, 12);
                in.removeFromTop(38);
                auto r1 = in.removeFromTop(28);
                lblExpRect = r1.removeFromLeft(148);
                expLearnBtn.setBounds(r1.removeFromRight(124));
                r1.removeFromRight(8);
                expCcBox.setBounds(r1);

                in.removeFromTop(10);
                auto r2 = in.removeFromTop(28);
                lblPcRect = r2.removeFromLeft(148);
                pcSwitch.setBounds(r2.removeFromRight(140));
            }

            area.removeFromTop(14);
            mappingsCardRect = area.removeFromTop(214);
            {
                auto in = mappingsCardRect.reduced(14, 12);
                auto topBtns = in.removeFromTop(24);
                clearAllMappingsBtn.setBounds(topBtns.removeFromRight(84));
                topBtns.removeFromRight(6);
                learnNextBtn.setBounds(topBtns.removeFromRight(116));
                in.removeFromTop(18);
                mappingsList.setBounds(in);
            }
        }

    private:
        class MidiDevListModel : public juce::ListBoxModel {
        public:
            explicit MidiDevListModel(MidiControlPage& p) : page(p) {}
            int getNumRows() override { return static_cast<int>(page.midiDevices.size()); }

            void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool) override {
                using namespace settings_ui;
                if (row < 0 || row >= static_cast<int>(page.midiDevices.size())) return;
                const auto& d = page.midiDevices[static_cast<size_t>(row)];
                const bool enabled = (page.owner.host.deviceManager != nullptr)
                                         && page.owner.host.deviceManager->isMidiInputDeviceEnabled(d.identifier);

                auto r = juce::Rectangle<float>(4.0f, 2.0f, static_cast<float>(w - 8), static_cast<float>(h - 4));
                g.setColour(enabled ? kAccentCyan.withAlpha(0.14f) : kBgCard);
                g.fillRoundedRectangle(r, 5.0f);
                g.setColour(enabled ? kAccentCyan.withAlpha(0.65f) : kBorderSubtle);
                g.drawRoundedRectangle(r, 5.0f, 1.0f);

                g.setColour(enabled ? kAccentGreen : juce::Colour(0xff334155));
                g.fillEllipse(r.getX() + 10.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);

                auto badge = r.removeFromRight(68.0f).reduced(6.0f, 5.0f);
                g.setColour(enabled ? kAccentGreen.withAlpha(0.20f) : kBgRecessed);
                g.fillRoundedRectangle(badge, 4.0f);
                g.setColour(enabled ? kAccentGreen : kTextMuted);
                g.setFont(juce::Font(juce::FontOptions(8.6f, juce::Font::bold)));
                g.drawText(enabled ? "ACTIVE" : "OFF", badge, juce::Justification::centred);

                g.setColour(enabled ? kTextPrimary : kTextSecondary);
                g.setFont(juce::Font(juce::FontOptions(10.5f, enabled ? juce::Font::bold : juce::Font::plain)));
                g.drawText(d.name, r.withTrimmedLeft(24.0f).withTrimmedRight(6.0f), juce::Justification::centredLeft, true);
            }

            void listBoxItemClicked(int row, const juce::MouseEvent&) override {
                if (row < 0 || row >= static_cast<int>(page.midiDevices.size())) return;
                if (auto* dm = page.owner.host.deviceManager) {
                    const auto& d = page.midiDevices[static_cast<size_t>(row)];
                    const bool cur = dm->isMidiInputDeviceEnabled(d.identifier);
                    dm->setMidiInputDeviceEnabled(d.identifier, !cur);
                    if (page.owner.host.saveAudioState) page.owner.host.saveAudioState();
                    page.midiDeviceList.repaint();
                }
            }

        private:
            MidiControlPage& page;
        };

        class MappingListModel : public juce::ListBoxModel {
        public:
            explicit MappingListModel(MidiControlPage& p) : page(p) {}
            int getNumRows() override { return std::max(1, static_cast<int>(page.activeMappings.size())); }

            void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool) override {
                using namespace settings_ui;
                if (page.activeMappings.empty()) {
                    g.setColour(kTextMuted);
                    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
                    g.drawText("No MIDI CC mappings yet — click LEARN CONTROL or right-click any knob/switch.",
                               juce::Rectangle<int>(10, 0, w - 20, h), juce::Justification::centredLeft, true);
                    return;
                }
                if (row < 0 || row >= static_cast<int>(page.activeMappings.size())) return;
                const auto& m = page.activeMappings[static_cast<size_t>(row)];

                auto r = juce::Rectangle<float>(4.0f, 2.0f, static_cast<float>(w - 8), static_cast<float>(h - 4));
                g.setColour(kBgCard);
                g.fillRoundedRectangle(r, 5.0f);
                g.setColour(kBorderSubtle);
                g.drawRoundedRectangle(r, 5.0f, 1.0f);

                auto ccChip = r.removeFromLeft(66.0f).reduced(5.0f, 4.0f);
                g.setColour(kAccentCyan.withAlpha(0.16f));
                g.fillRoundedRectangle(ccChip, 4.0f);
                g.setColour(kAccentCyan);
                g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
                g.drawText("CC #" + juce::String(m.ccNumber), ccChip, juce::Justification::centred);

                auto rmChip = r.removeFromRight(72.0f).reduced(6.0f, 4.0f);
                g.setColour(kAccentRed.withAlpha(0.16f));
                g.fillRoundedRectangle(rmChip, 4.0f);
                g.setColour(juce::Colour(0xfffca5a5));
                g.setFont(juce::Font(juce::FontOptions(8.6f, juce::Font::bold)));
                g.drawText("REMOVE", rmChip, juce::Justification::centred);

                g.setColour(kTextPrimary);
                g.setFont(juce::Font(juce::FontOptions(10.2f, juce::Font::plain)));
                g.drawText(m.paramId, r.reduced(6.0f, 0.0f), juce::Justification::centredLeft, true);
            }

            void listBoxItemClicked(int row, const juce::MouseEvent&) override {
                if (row < 0 || row >= static_cast<int>(page.activeMappings.size())) return;
                page.owner.processor.getMidiLearnManager().clearMappingForParam(page.activeMappings[static_cast<size_t>(row)].paramId);
                page.refreshMappings();
            }

        private:
            MidiControlPage& page;
        };

        StudioSettingsContent& owner;
        juce::Rectangle<int> monitorCardRect, devicesCardRect, controlCardRect, mappingsCardRect;
        juce::Rectangle<int> lblExpRect, lblPcRect;

        juce::TextButton rescanMidiBtn, expLearnBtn, learnNextBtn, clearAllMappingsBtn;
        juce::ComboBox expCcBox;
        settings_ui::StudioSwitchPill pcSwitch;

        std::vector<juce::MidiDeviceInfo> midiDevices;
        MidiDevListModel midiDevModel;
        juce::ListBox midiDeviceList;

        std::vector<MidiCcBindingInfo> activeMappings;
        MappingListModel mappingModel;
        juce::ListBox mappingsList;
        uint64_t lastObservedMidiVer = 0;
    };

    // =========================================================================
    // TAB 4: STORAGE & CLOUD
    // =========================================================================
    class StorageCloudPage : public juce::Component {
    public:
        explicit StorageCloudPage(StudioSettingsContent& o) : owner(o) {
            openPresetsBtn.setButtonText("OPEN FOLDER");
            openPresetsBtn.onClick = [this] {
                owner.processor.getPresetManager().getUserPresetDirectory().revealToUser();
            };

            openCloudBtn.setButtonText("OPEN TONE3000 CLOUD");
            openCloudBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f766e));
            openCloudBtn.onClick = [this] {
                if (owner.openCloudCb) owner.openCloudCb();
            };

            openCacheBtn.setButtonText("OPEN CACHE");
            openCacheBtn.onClick = [] {
                t3k::Tone3000Client::getCacheDirectory().revealToUser();
            };

            clearCacheBtn.setButtonText("CLEAR CACHE");
            clearCacheBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff451a20));
            clearCacheBtn.onClick = [this] { confirmAndClearCache(); };

            openConfigBtn.setButtonText("OPEN CONFIG DIR");
            openConfigBtn.onClick = [] {
                PedalBoardAudioProcessor::getStudioSettingsFile().getParentDirectory().revealToUser();
            };

            for (juce::Component* c : { &openPresetsBtn, &openCloudBtn, &openCacheBtn, &clearCacheBtn, &openConfigBtn }) {
                addAndMakeVisible(*c);
            }
        }

        int computePreferredHeight() const { return 570; }

        void refreshStorageStats() {
            const auto presetDir = owner.processor.getPresetManager().getUserPresetDirectory();
            presetPathStr = presetDir.getFullPathName();
            userPresetCount = presetDir.findChildFiles(juce::File::findFiles, true, "*.pbpreset").size();

            const auto cacheDir = t3k::Tone3000Client::getCacheDirectory();
            cachePathStr = cacheDir.getFullPathName();
            const auto cached = t3k::Tone3000Client::scanLocalCache();
            cachedFileCount = static_cast<int>(cached.size());
            juce::int64 totalBytes = 0;
            for (const auto& f : cached) totalBytes += f.sizeBytes;
            cacheSizeMb = static_cast<double>(totalBytes) / (1024.0 * 1024.0);
            clearCacheBtn.setEnabled(cachedFileCount > 0);

            if (owner.toneClient != nullptr && owner.toneClient->isConnected()) {
                cloudConnected = true;
                const auto u = owner.toneClient->getCachedUser();
                cloudAccountText = u.username.isNotEmpty() ? ("Connected as @" + u.username) : "Connected to TONE3000";
            } else {
                cloudConnected = false;
                cloudAccountText = "Not signed in (Public catalog & Offline cache available)";
            }
            configPathStr = PedalBoardAudioProcessor::getStudioSettingsFile().getFullPathName();
            repaint();
        }

        void paint(juce::Graphics& g) override {
            using namespace settings_ui;
            g.fillAll(kBgCanvas);

            // Card 1: User Presets
            drawCardFrame(g, presetsCardRect, "USER PRESETS LIBRARY",
                          "Custom .pbpreset tone patches saved from PedalBoard.");
            drawPathBox(g, presetBoxRect, presetPathStr,
                        juce::String(userPresetCount) + (userPresetCount == 1 ? " user preset saved" : " user presets saved"));

            // Card 2: TONE3000 Cloud & Offline Cache
            drawCardFrame(g, cloudCardRect, "TONE3000 CLOUD & OFFLINE CAPTURE CACHE",
                          "Downloaded .nam neural captures and .wav cabinet IRs stay cached locally for instant offline loading.");
            {
                auto acctR = cloudAcctRect.toFloat();
                g.setColour(kBgRecessed);
                g.fillRoundedRectangle(acctR, 6.0f);
                g.setColour(cloudConnected ? kAccentCyan.withAlpha(0.55f) : kBorderSubtle);
                g.drawRoundedRectangle(acctR, 6.0f, 1.0f);
                g.setColour(cloudConnected ? kAccentCyan : kTextMuted);
                g.fillEllipse(acctR.getX() + 10.0f, acctR.getCentreY() - 4.0f, 8.0f, 8.0f);
                g.setColour(cloudConnected ? kTextPrimary : kTextSecondary);
                g.setFont(juce::Font(juce::FontOptions(10.2f, cloudConnected ? juce::Font::bold : juce::Font::plain)));
                g.drawText(cloudAccountText, cloudAcctRect.withTrimmedLeft(24).withTrimmedRight(8),
                           juce::Justification::centredLeft, true);
            }
            drawPathBox(g, cacheBoxRect, cachePathStr,
                        juce::String(cachedFileCount) + " cached capture(s)  |  " + juce::String(cacheSizeMb, 2) + " MB on disk");

            // Card 3: System & Engine Diagnostics
            drawCardFrame(g, diagCardRect, "SYSTEM & ENGINE DIAGNOSTICS",
                          "Build architecture, active neural engine status, and studio configuration location.");
            auto specArea = diagCardRect.reduced(14, 12).withTrimmedTop(40).withTrimmedBottom(42);
            const int rowH = 22;
            const int colGap = 16;
            const int colW = (specArea.getWidth() - colGap) / 2;

            auto drawSpecRow = [&](juce::Rectangle<int> r, const juce::String& k, const juce::String& v, juce::Colour vCol = kTextPrimary) {
                g.setColour(kTextMuted);
                g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
                g.drawText(k, r.removeFromLeft(112), juce::Justification::centredLeft, true);
                g.setColour(vCol);
                g.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::plain)));
                g.drawText(v, r, juce::Justification::centredLeft, true);
            };

            const int activeNams = owner.processor.getSignalChain().getActiveNamModelCount();
            auto leftCol = specArea.withWidth(colW);
            auto rightCol = specArea.withTrimmedLeft(colW + colGap);

            drawSpecRow(leftCol.removeFromTop(rowH), "Application", "PedalBoard v1.0.0");
            drawSpecRow(leftCol.removeFromTop(rowH), "Runtime Format",
                        owner.processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone ? "Standalone (.exe)" : "VST3 Plugin");
            drawSpecRow(leftCol.removeFromTop(rowH), "Framework", "JUCE 8.0.9 + OpenGL 2D");
            drawSpecRow(leftCol.removeFromTop(rowH), "Neural Engine",
                        "NAM Core 0.6.0 (A1+A2 Fast)", kAccentCyan);

            drawSpecRow(rightCol.removeFromTop(rowH), "Active NAM Slots",
                        juce::String(activeNams) + " loaded", activeNams > 0 ? kAccentGreen : kTextSecondary);
#if defined(PEDALBOARD_HAS_ASIO) && PEDALBOARD_HAS_ASIO
            drawSpecRow(rightCol.removeFromTop(rowH), "Audio Drivers", "ASIO + WASAPI + DirectSound");
#else
            drawSpecRow(rightCol.removeFromTop(rowH), "Audio Drivers", "WASAPI + DirectSound");
#endif
#if defined(PEDALBOARD_HAS_WEBVIEW2) && PEDALBOARD_HAS_WEBVIEW2
            drawSpecRow(rightCol.removeFromTop(rowH), "Cloud Browser", "Microsoft Edge WebView2");
#else
            drawSpecRow(rightCol.removeFromTop(rowH), "Cloud Browser", "Native REST Client");
#endif
            drawSpecRow(rightCol.removeFromTop(rowH), "Studio Config", "studio_settings.json");

            // Bottom config path strip
            drawPathBox(g, configBoxRect, configPathStr, {});
        }

        void resized() override {
            auto area = getLocalBounds().reduced(18, 16);

            presetsCardRect = area.removeFromTop(108);
            {
                auto in = presetsCardRect.reduced(14, 12);
                in.removeFromTop(38);
                openPresetsBtn.setBounds(in.removeFromRight(108).withHeight(34));
                in.removeFromRight(8);
                presetBoxRect = in.withHeight(38);
            }

            area.removeFromTop(14);
            cloudCardRect = area.removeFromTop(162);
            {
                auto in = cloudCardRect.reduced(14, 12);
                in.removeFromTop(38);

                auto r1 = in.removeFromTop(32);
                openCloudBtn.setBounds(r1.removeFromRight(156));
                r1.removeFromRight(8);
                cloudAcctRect = r1;

                in.removeFromTop(10);
                auto r2 = in.removeFromTop(42);
                clearCacheBtn.setBounds(r2.removeFromRight(102).withHeight(34));
                r2.removeFromRight(6);
                openCacheBtn.setBounds(r2.removeFromRight(98).withHeight(34));
                r2.removeFromRight(8);
                cacheBoxRect = r2.withHeight(38);
            }

            area.removeFromTop(14);
            diagCardRect = area.removeFromTop(196);
            {
                auto in = diagCardRect.reduced(14, 12);
                auto bot = in.removeFromBottom(32);
                openConfigBtn.setBounds(bot.removeFromRight(124));
                bot.removeFromRight(8);
                configBoxRect = bot;
            }
        }

    private:
        static void drawPathBox(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& path, const juce::String& sub) {
            using namespace settings_ui;
            g.setColour(kBgRecessed);
            g.fillRoundedRectangle(r.toFloat(), 6.0f);
            g.setColour(kBorderSubtle);
            g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.0f);
            auto in = r.reduced(10, 4);
            g.setColour(kTextPrimary);
            g.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::plain)));
            if (sub.isEmpty()) {
                g.drawText(path, in, juce::Justification::centredLeft, true);
            } else {
                g.drawText(path, in.removeFromTop(16), juce::Justification::centredLeft, true);
                g.setColour(kTextMuted);
                g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
                g.drawText(sub, in, juce::Justification::centredLeft, true);
            }
        }

        void confirmAndClearCache() {
            auto opts = juce::MessageBoxOptions::makeOptionsOkCancel(
                juce::MessageBoxIconType::WarningIcon,
                "Clear TONE3000 Offline Cache",
                "Delete all " + juce::String(cachedFileCount) + " cached .nam and .wav files (" +
                    juce::String(cacheSizeMb, 2) + " MB) from disk? You can re-download them anytime from TONE3000.",
                "Clear Cache", "Cancel", this);
            juce::Component::SafePointer<StorageCloudPage> safeThis(this);
            juce::AlertWindow::showAsync(opts, [safeThis](int result) {
                if (safeThis == nullptr || result != 1) return;
                const auto cached = t3k::Tone3000Client::scanLocalCache();
                for (const auto& item : cached) {
                    item.file.deleteFile();
                }
                safeThis->refreshStorageStats();
            });
        }

        StudioSettingsContent& owner;
        juce::Rectangle<int> presetsCardRect, cloudCardRect, diagCardRect;
        juce::Rectangle<int> presetBoxRect, cloudAcctRect, cacheBoxRect, configBoxRect;

        juce::TextButton openPresetsBtn, openCloudBtn, openCacheBtn, clearCacheBtn, openConfigBtn;
        juce::String presetPathStr, cachePathStr, configPathStr, cloudAccountText;
        int userPresetCount = 0;
        int cachedFileCount = 0;
        double cacheSizeMb = 0.0;
        bool cloudConnected = false;
    };

    void layoutActivePage() {
        const int availW = std::max(460, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
        if (auto* viewed = viewport.getViewedComponent()) {
            int prefH = 580;
            if (viewed == audioPage.get())        prefH = audioPage->computePreferredHeight();
            else if (viewed == dspPage.get())     prefH = dspPage->computePreferredHeight();
            else if (viewed == midiPage.get())    prefH = midiPage->computePreferredHeight();
            else if (viewed == storagePage.get()) prefH = storagePage->computePreferredHeight();
            viewed->setBounds(0, 0, availW, std::max(prefH, viewport.getHeight()));
        }
    }

    void refreshAllPages(bool forceDeviceScan) {
        if (audioPage != nullptr)   audioPage->refreshFromDeviceManager(forceDeviceScan);
        if (dspPage != nullptr)     dspPage->syncFromProcessor();
        if (midiPage != nullptr)    midiPage->refreshMidiDevices();
        if (storagePage != nullptr) storagePage->refreshStorageStats();
        layoutActivePage();
    }

    void changeListenerCallback(juce::ChangeBroadcaster* source) override {
        if (source == host.deviceManager && audioPage != nullptr) {
            audioPage->refreshFromDeviceManager(false);
            if (midiPage != nullptr) midiPage->refreshMidiDevices();
            repaint(railStatusRect);
        }
    }

    void timerCallback() override {
        if (!isVisible()) return;
        if (activeTab == Tab::AudioIo && audioPage != nullptr)        audioPage->updateLive();
        else if (activeTab == Tab::InputDsp && dspPage != nullptr)    dspPage->updateLive();
        else if (activeTab == Tab::MidiControl && midiPage != nullptr) midiPage->updateLive();
        repaint(railStatusRect);
    }

    PedalBoardAudioProcessor& processor;
    t3k::Tone3000Client* toneClient = nullptr;
    std::function<void()> openCloudCb;
    StandaloneHostHandle host;

    Tab activeTab = Tab::AudioIo;
    juce::Rectangle<int> railStatusRect;

    settings_ui::SettingsNavButton navAudioBtn, navDspBtn, navMidiBtn, navStorageBtn;
    juce::Viewport viewport;

    std::unique_ptr<AudioIoPage> audioPage;
    std::unique_ptr<InputDspPage> dspPage;
    std::unique_ptr<MidiControlPage> midiPage;
    std::unique_ptr<StorageCloudPage> storagePage;
};

// ==============================================================================
// Floating Resizable Dark-Titlebar Studio Settings DocumentWindow
// ==============================================================================
class StudioSettingsWindow : public juce::DocumentWindow {
public:
    StudioSettingsWindow(PedalBoardAudioProcessor& proc,
                         t3k::Tone3000Client* cloudClient,
                         juce::LookAndFeel* lnf,
                         std::function<void()> onOpenCloudBrowser = {},
                         std::function<void(juce::DocumentWindow&)> onApplyDarkFrame = {})
        : juce::DocumentWindow("PedalBoard Studio Settings",
                               settings_ui::kBgCanvas,
                               juce::DocumentWindow::minimiseButton
                                   | juce::DocumentWindow::maximiseButton
                                   | juce::DocumentWindow::closeButton,
                               false),
          applyDarkFrameCb(std::move(onApplyDarkFrame)) {
        if (lnf != nullptr) {
            setLookAndFeel(lnf);
        }
        content = new StudioSettingsContent(proc, cloudClient, std::move(onOpenCloudBrowser));
        setContentOwned(content, true);
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setResizeLimits(740, 520, 1400, 1000);
        addToDesktop();
        if (applyDarkFrameCb) {
            applyDarkFrameCb(*this);
        }
    }

    ~StudioSettingsWindow() override {
        if (content != nullptr) {
            content->onWindowHidden();
        }
        setLookAndFeel(nullptr);
    }

    void showCentered(juce::Component* anchor,
                      StudioSettingsContent::Tab initialTab = StudioSettingsContent::Tab::AudioIo) {
        if (!positionedOnce) {
            positionedOnce = true;
            int w = 920;
            int h = 640;
            if (const auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                w = std::clamp(disp->userArea.getWidth() - 180, 760, 960);
                h = std::clamp(disp->userArea.getHeight() - 120, 540, 680);
            }
            if (anchor != nullptr) {
                centreAroundComponent(anchor, w, h);
            } else {
                centreWithSize(w, h);
            }
            if (applyDarkFrameCb) {
                applyDarkFrameCb(*this);
            }
        }
        if (content != nullptr) {
            content->selectTab(initialTab);
            content->onWindowShown();
        }
        setVisible(true);
        toFront(true);
    }

    void hideWindow() {
        if (content != nullptr) {
            content->onWindowHidden();
        }
        setVisible(false);
    }

    void closeButtonPressed() override {
        hideWindow();
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey) {
            hideWindow();
            return true;
        }
        return DocumentWindow::keyPressed(key);
    }

private:
    StudioSettingsContent* content = nullptr;
    std::function<void(juce::DocumentWindow&)> applyDarkFrameCb;
    bool positionedOnce = false;
};

} // namespace pedalboard
