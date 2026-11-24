#pragma once

#include "../dsp/ChromaticTuner.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "../plugin/StandaloneBridge.h"
#include "MidiBindableControl.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard {

// Precision segmented stereo peak meter with falling peak-hold & clickable clip LED reset.
class LevelMeterComponent : public juce::Component,
                            public juce::SettableTooltipClient {
public:
    explicit LevelMeterComponent(juce::String labelText = {}, bool showHeaderLabel = true)
        : label(std::move(labelText)),
          showLabel(showHeaderLabel) {}

    void setLevels(float newL, float newR) noexcept {
        levelL = std::max(newL, levelL * 0.83f);
        levelR = std::max(newR, levelR * 0.83f);

        auto updateHold = [](float cur, float& hold, int& holdTicks, int& clipTicks) {
            if (cur >= hold) {
                hold = cur;
                holdTicks = 18;
            } else if (holdTicks > 0) {
                --holdTicks;
            } else {
                hold *= 0.94f;
            }
            if (cur >= 0.99f) {
                clipTicks = 65;
            } else if (clipTicks > 0) {
                --clipTicks;
            }
        };

        updateHold(newL, peakHoldL, holdTicksL, clipTicksL);
        updateHold(newR, peakHoldR, holdTicksR, clipTicksR);
        repaint();
    }

    void mouseDown(const juce::MouseEvent&) override {
        clipTicksL = 0;
        clipTicksR = 0;
        peakHoldL = 0.0f;
        peakHoldR = 0.0f;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        if (showLabel && label.isNotEmpty()) {
            g.setColour(juce::Colour(0xff8a96ab));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(label, area.removeFromTop(11.0f), juce::Justification::centred);
        }

        area.reduce(1.0f, 1.0f);
        if (area.getWidth() < 4.0f || area.getHeight() < 8.0f) return;

        const float gap = (area.getWidth() >= 14.0f) ? 2.5f : 1.5f;
        const float barW = (area.getWidth() - gap) * 0.5f;
        drawChannel(g, area.withWidth(barW), levelL, peakHoldL, clipTicksL > 0);
        drawChannel(g, area.withX(area.getX() + barW + gap).withWidth(barW), levelR, peakHoldR, clipTicksR > 0);
    }

private:
    static float linearToNorm(float peak) noexcept {
        const float db = juce::Decibels::gainToDecibels(peak, -60.0f);
        return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
    }

    static void drawChannel(juce::Graphics& g,
                            juce::Rectangle<float> r,
                            float linearPeak,
                            float peakHold,
                            bool clipped) {
        g.setColour(juce::Colour(0xff080a0f));
        g.fillRoundedRectangle(r, 1.5f);

        // Top 3px dedicated clip LED
        const float clipH = std::min(3.5f, r.getHeight() * 0.12f);
        auto clipBox = r.removeFromTop(clipH);
        g.setColour(clipped ? juce::Colour(0xffef4444) : juce::Colour(0xff191e28));
        g.fillRoundedRectangle(clipBox.reduced(0.4f, 0.3f), 1.0f);
        r.removeFromTop(1.2f);

        if (r.getHeight() < 6.0f) return;

        const float norm = linearToNorm(linearPeak);
        const float holdNorm = linearToNorm(peakHold);

        const int segCount = std::clamp(static_cast<int>(r.getHeight() / 3.0f), 8, 22);
        const float segPitch = r.getHeight() / static_cast<float>(segCount);
        const float segH = std::max(1.2f, segPitch - 1.0f);
        const int litSegs = static_cast<int>(std::round(norm * static_cast<float>(segCount)));
        const int holdSeg = (holdNorm > 0.02f)
                                ? std::clamp(static_cast<int>(std::round(holdNorm * static_cast<float>(segCount))) - 1, 0, segCount - 1)
                                : -1;

        for (int s = 0; s < segCount; ++s) {
            const float frac = static_cast<float>(s + 1) / static_cast<float>(segCount);
            const float sy = r.getBottom() - static_cast<float>(s + 1) * segPitch + 0.5f;
            const bool isLit = (s < litSegs) || (s == holdSeg);

            juce::Colour segCol;
            if (frac > 0.88f)      segCol = juce::Colour(0xffef4444); // Red (>-7 dB)
            else if (frac > 0.68f) segCol = juce::Colour(0xfff59e0b); // Amber (-19..-7 dB)
            else                   segCol = juce::Colour(0xff22c55e); // Emerald green

            g.setColour(isLit ? segCol.withAlpha(s < litSegs ? 0.94f : 0.78f)
                              : juce::Colour(0xff151a24));
            g.fillRect(r.getX() + 0.5f, sy, std::max(1.5f, r.getWidth() - 1.0f), segH);
        }
    }

    juce::String label;
    bool showLabel = true;
    float levelL = 0.0f;
    float levelR = 0.0f;
    float peakHoldL = 0.0f;
    float peakHoldR = 0.0f;
    int holdTicksL = 0;
    int holdTicksR = 0;
    int clipTicksL = 0;
    int clipTicksR = 0;
};

// Compact, precision studio OLED chromatic tuner with integrated Silent-Tune Mute toggle.
class TunerDisplayComponent : public juce::Component,
                              public juce::SettableTooltipClient {
public:
    TunerDisplayComponent() {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Chromatic Tuner (A4=440 Hz) — Click to toggle Silent Tuning Mute (M)");
    }

    void setOnToggleMute(std::function<void()> cb) { onToggleMute = std::move(cb); }
    void setOnRightClick(std::function<void()> cb) { onRightClick = std::move(cb); }

    void setReading(const TunerReading& r, bool isMuted, float refHz = 440.0f) {
        reading = r;
        muted = isMuted;
        if (std::abs(a4RefHz - refHz) > 0.05f) {
            a4RefHz = refHz;
            const juce::String hzLabel = juce::String(a4RefHz, std::abs(a4RefHz - std::round(a4RefHz)) < 0.05f ? 0 : 1);
            setTooltip("Chromatic Tuner (A4=" + hzLabel + " Hz) — Click to toggle Silent Tuning Mute (M)");
        }
        repaint();
    }

    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { hovered = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) {
            if (onRightClick) onRightClick();
            return;
        }
        if (onToggleMute) onToggleMute();
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat().reduced(0.5f);
        const bool inTune = reading.valid && std::abs(reading.cents) <= 3.5f;

        // Matte recessed OLED housing
        g.setColour(juce::Colour(0xff0a0d13));
        g.fillRoundedRectangle(area, 5.5f);

        const auto borderCol = muted
            ? juce::Colour(0xffdc2626).withAlpha(0.75f)
            : (reading.valid ? (inTune ? juce::Colour(0xff22c55e).withAlpha(0.60f)
                                       : juce::Colour(0xfff59e0b).withAlpha(0.50f))
                             : juce::Colour(hovered ? 0xff364156 : 0xff222a38));
        g.setColour(borderCol);
        g.drawRoundedRectangle(area, 5.5f, 1.0f);

        // Right integrated Silent-Tune Mute cell
        const float muteW = std::min(30.0f, area.getWidth() * 0.16f);
        auto muteCell = area.removeFromRight(muteW);
        {
            juce::Graphics::ScopedSaveState ss(g);
            juce::Path clip;
            clip.addRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f);
            g.reduceClipRegion(clip);
            if (muted) {
                g.setColour(juce::Colour(0xffdc2626).withAlpha(0.24f));
                g.fillRect(muteCell);
            } else if (hovered) {
                g.setColour(juce::Colours::white.withAlpha(0.04f));
                g.fillRect(muteCell);
            }
        }
        g.setColour(juce::Colour(0xff1e2533));
        g.drawVerticalLine(static_cast<int>(muteCell.getX()), area.getY() + 4.0f, area.getBottom() - 4.0f);

        // Crisp speaker-mute vector icon
        drawSpeakerIcon(g, muteCell.getCentre(), muted ? juce::Colour(0xfff87171) : juce::Colour(hovered ? 0xffcbd5e1 : 0xff64748b), muted);

        // Left Note + Cents Readout
        auto inner = area.reduced(6.0f, 3.0f);
        const float noteW = std::clamp(inner.getWidth() * 0.30f, 44.0f, 58.0f);
        auto noteBox = inner.removeFromLeft(noteW);

        const auto noteCol = !reading.valid
            ? (muted ? juce::Colour(0xfff87171) : juce::Colour(0xff5a6578))
            : (inTune ? juce::Colour(0xff22c55e) : juce::Colour(0xfffbbf24));

        g.setColour(noteCol);
        g.setFont(juce::Font(juce::FontOptions(noteBox.getHeight() < 30.0f ? 13.5f : 15.5f, juce::Font::bold)));
        g.drawText(reading.valid ? reading.noteName : "--",
                   noteBox.removeFromTop(noteBox.getHeight() * 0.62f),
                   juce::Justification::centred, false);

        juce::String subStr;
        juce::Colour subCol(0xff525d70);
        if (reading.valid) {
            if (inTune) {
                subStr = "IN TUNE";
                subCol = juce::Colour(0xff22c55e);
            } else {
                const int ct = static_cast<int>(std::lround(reading.cents));
                subStr = (ct > 0 ? "+" : "") + juce::String(ct) + " ct";
                subCol = juce::Colour(0xfff59e0b);
            }
        } else if (muted) {
            subStr = "MUTED";
            subCol = juce::Colour(0xfff87171);
        } else {
            subStr = juce::String(a4RefHz, std::abs(a4RefHz - std::round(a4RefHz)) < 0.05f ? 0 : 1) + " Hz";
        }
        g.setColour(subCol);
        g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
        g.drawText(subStr, noteBox, juce::Justification::centred, true);

        // Center Calibrated Pitch Meter
        inner.removeFromLeft(4.0f);
        if (inner.getWidth() < 44.0f) return;

        auto scale = inner.reduced(2.0f, 4.0f);
        const float cy = scale.getCentreY() + 1.5f;
        const float leftX = scale.getX() + 11.0f;
        const float rightX = scale.getRight() - 11.0f;
        const float cx = (leftX + rightX) * 0.5f;
        const float halfSpan = std::max(10.0f, (rightX - leftX) * 0.5f);

        // Subtle flat & sharp end markers
        g.setColour(juce::Colour(0xff4c576b));
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.drawText("b", juce::Rectangle<float>(scale.getX(), scale.getY(), 10.0f, scale.getHeight()), juce::Justification::centred);
        g.drawText("#", juce::Rectangle<float>(scale.getRight() - 10.0f, scale.getY(), 10.0f, scale.getHeight()), juce::Justification::centred);

        // Recessed scale rail
        g.setColour(juce::Colour(0xff1a2130));
        g.fillRoundedRectangle(leftX, cy - 1.5f, rightX - leftX, 3.0f, 1.5f);

        // 9 subtle calibrated ticks (-50 .. 0 .. +50 cents)
        for (int i = -4; i <= 4; ++i) {
            const float frac = static_cast<float>(i) / 4.0f;
            const float tx = cx + frac * halfSpan;
            const bool isZero = (i == 0);
            const bool isMajor = (std::abs(i) == 2 || std::abs(i) == 4);
            const float tickH = isZero ? 10.0f : (isMajor ? 6.5f : 4.0f);
            g.setColour(isZero ? (inTune ? juce::Colour(0xff22c55e) : juce::Colour(0xff22c55e).withAlpha(0.55f))
                               : juce::Colours::white.withAlpha(isMajor ? 0.20f : 0.10f));
            g.fillRect(tx - (isZero ? 0.9f : 0.5f), cy - tickH * 0.5f, isZero ? 1.8f : 1.0f, tickH);
        }

        // Crisp top center triangle reticle
        juce::Path tri;
        tri.addTriangle(cx - 3.2f, scale.getY() + 0.5f, cx + 3.2f, scale.getY() + 0.5f, cx, scale.getY() + 4.5f);
        g.setColour(inTune ? juce::Colour(0xff22c55e) : juce::Colour(0xff4b566a));
        g.fillPath(tri);

        // Live pitch indicator needle
        if (reading.valid) {
            const float norm = std::clamp(reading.cents / 50.0f, -1.0f, 1.0f);
            const float xPos = cx + norm * halfSpan;

            // Fill trace from center to needle
            const float traceX = std::min(cx, xPos);
            const float traceW = std::abs(xPos - cx);
            if (traceW > 1.0f) {
                g.setColour(noteCol.withAlpha(0.32f));
                g.fillRoundedRectangle(traceX, cy - 2.0f, traceW, 4.0f, 2.0f);
            }

            // Glowing precision cursor
            g.setColour(noteCol.withAlpha(0.28f));
            g.fillRoundedRectangle(xPos - 5.0f, cy - 7.5f, 10.0f, 15.0f, 3.0f);
            g.setColour(noteCol);
            g.fillRoundedRectangle(xPos - 2.0f, cy - 6.5f, 4.0f, 13.0f, 1.8f);
        }
    }

private:
    static void drawSpeakerIcon(juce::Graphics& g, juce::Point<float> c, juce::Colour col, bool isMuted) {
        g.setColour(col);
        juce::Path spk;
        spk.startNewSubPath(c.x - 5.5f, c.y - 2.2f);
        spk.lineTo(c.x - 2.5f, c.y - 2.2f);
        spk.lineTo(c.x + 0.5f, c.y - 4.8f);
        spk.lineTo(c.x + 0.5f, c.y + 4.8f);
        spk.lineTo(c.x - 2.5f, c.y + 2.2f);
        spk.lineTo(c.x - 5.5f, c.y + 2.2f);
        spk.closeSubPath();
        g.fillPath(spk);

        if (isMuted) {
            g.drawLine(c.x + 2.5f, c.y - 2.6f, c.x + 6.5f, c.y + 2.6f, 1.5f);
            g.drawLine(c.x + 6.5f, c.y - 2.6f, c.x + 2.5f, c.y + 2.6f, 1.5f);
        } else {
            juce::Path arc1;
            arc1.addCentredArc(c.x + 0.8f, c.y, 3.5f, 3.5f, 0.0f, 0.85f, 2.29f, true);
            g.strokePath(arc1, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path arc2;
            arc2.addCentredArc(c.x + 0.8f, c.y, 6.0f, 6.0f, 0.0f, 0.80f, 2.34f, true);
            g.strokePath(arc2, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    TunerReading reading;
    bool muted = false;
    bool hovered = false;
    float a4RefHz = 440.0f;
    std::function<void()> onToggleMute;
    std::function<void()> onRightClick;
};

// Compact, precision studio OLED DSP CPU usage meter positioned beside the Chromatic Tuner.
class CpuMeterComponent : public juce::Component,
                          public juce::SettableTooltipClient {
public:
    CpuMeterComponent() {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Real-Time Audio DSP CPU Load — Click to open Studio Settings (Audio I/O)");
    }

    void setOnClick(std::function<void()> cb) { onClickCb = std::move(cb); }

    void setCpuPercent(float newPct) noexcept {
        const float clamped = std::clamp(newPct, 0.0f, 100.0f);
        displayedPct = displayedPct * 0.72f + clamped * 0.28f;
        if (displayedPct < 0.05f) displayedPct = 0.0f;
        repaint();
    }

    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { hovered = false; repaint(); }
    void mouseUp(const juce::MouseEvent& e) override {
        if (getLocalBounds().contains(e.getPosition()) && onClickCb) {
            onClickCb();
        }
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat().reduced(0.5f);
        const bool highLoad = (displayedPct >= 75.0f);
        const bool medLoad  = (displayedPct >= 50.0f);

        // Matte recessed OLED housing matching TunerDisplayComponent
        g.setColour(juce::Colour(0xff0a0d13));
        g.fillRoundedRectangle(area, 5.5f);

        const auto borderCol = highLoad
            ? juce::Colour(0xffef4444).withAlpha(0.75f)
            : (medLoad ? juce::Colour(0xfff59e0b).withAlpha(0.55f)
                       : juce::Colour(hovered ? 0xff364156 : 0xff222a38));
        g.setColour(borderCol);
        g.drawRoundedRectangle(area, 5.5f, 1.0f);

        auto inner = area.reduced(6.0f, 4.0f);
        auto topRow = inner.removeFromTop(14.0f);

        g.setColour(juce::Colour(0xff64748b));
        g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
        g.drawText("CPU", topRow.removeFromLeft(22.0f), juce::Justification::centredLeft, false);

        const auto valCol = highLoad
            ? juce::Colour(0xfff87171)
            : (medLoad ? juce::Colour(0xfffbbf24) : juce::Colour(0xffe2e8f0));
        g.setColour(valCol);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        const juce::String pctTxt = (displayedPct >= 10.0f)
            ? (juce::String(static_cast<int>(std::lround(displayedPct))) + "%")
            : (juce::String(displayedPct, 1) + "%");
        g.drawText(pctTxt, topRow, juce::Justification::centredRight, false);

        // Segmented horizontal DSP load bar
        auto bar = inner.removeFromBottom(6.0f);
        if (bar.getWidth() < 12.0f) return;

        constexpr int kSegs = 10;
        const float gap = 1.5f;
        const float segW = (bar.getWidth() - gap * static_cast<float>(kSegs - 1)) / static_cast<float>(kSegs);
        const int litSegs = (displayedPct > 0.3f)
            ? std::clamp(static_cast<int>(std::ceil((displayedPct / 100.0f) * static_cast<float>(kSegs))), 1, kSegs)
            : 0;

        for (int s = 0; s < kSegs; ++s) {
            const float sx = bar.getX() + static_cast<float>(s) * (segW + gap);
            const float frac = static_cast<float>(s + 1) / static_cast<float>(kSegs);
            juce::Colour segCol(0xff22c55e);
            if (frac > 0.75f)      segCol = juce::Colour(0xffef4444);
            else if (frac > 0.50f) segCol = juce::Colour(0xfff59e0b);

            g.setColour(s < litSegs ? segCol.withAlpha(0.92f) : juce::Colour(0xff161c28));
            g.fillRoundedRectangle(sx, bar.getY(), std::max(1.5f, segW), bar.getHeight(), 1.2f);
        }
    }

private:
    float displayedPct = 0.0f;
    bool hovered = false;
    std::function<void()> onClickCb;
};

// Clean vector icon / utility button for the Top Studio Header (no generic glossy gradients!)
class TopUtilityButton : public juce::Button {
public:
    enum class Glyph { Prev, Next, Save, Load, Midi, Audio };

    TopUtilityButton(Glyph glyphType, const juce::String& labelText = {}, bool framed = true)
        : juce::Button(labelText),
          glyph(glyphType),
          caption(labelText),
          drawFrame(framed) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setCaption(const juce::String& txt) {
        if (caption != txt) {
            caption = txt;
            repaint();
        }
    }

    void setAccentState(bool active, juce::Colour col) {
        if (accentActive != active || accentCol != col) {
            accentActive = active;
            accentCol = col;
            repaint();
        }
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override {
        auto b = getLocalBounds().toFloat().reduced(drawFrame ? 0.5f : 0.0f);

        if (drawFrame) {
            const auto bg = accentActive
                ? accentCol.withAlpha(down ? 0.30f : (hovered ? 0.24f : 0.16f))
                : juce::Colour(down ? 0xff10141d : (hovered ? 0xff1c2230 : 0xff141923));
            g.setColour(bg);
            g.fillRoundedRectangle(b, 5.0f);

            g.setColour(accentActive ? accentCol.withAlpha(0.75f)
                                     : juce::Colour(hovered ? 0xff3b465c : 0xff242c3b));
            g.drawRoundedRectangle(b, 5.0f, 0.9f);
        } else if (hovered || down) {
            g.setColour(juce::Colours::white.withAlpha(down ? 0.10f : 0.06f));
            g.fillRect(b.reduced(1.0f));
        }

        const auto fg = accentActive
            ? accentCol.brighter(0.35f)
            : juce::Colour(hovered ? 0xfff1f5f9 : 0xff9aa6bc);

        if (caption.isEmpty()) {
            drawGlyph(g, b.getCentre(), fg);
        } else {
            const float iconX = b.getX() + 13.0f;
            drawGlyph(g, { iconX, b.getCentreY() }, fg);
            g.setColour(fg);
            g.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::bold)));
            g.drawText(caption, b.withTrimmedLeft(22.0f).withTrimmedRight(6.0f), juce::Justification::centredLeft, true);
        }
    }

private:
    void drawGlyph(juce::Graphics& g, juce::Point<float> c, juce::Colour col) const {
        g.setColour(col);
        switch (glyph) {
            case Glyph::Prev: {
                juce::Path p;
                p.startNewSubPath(c.x + 2.2f, c.y - 4.0f);
                p.lineTo(c.x - 2.2f, c.y);
                p.lineTo(c.x + 2.2f, c.y + 4.0f);
                g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                break;
            }
            case Glyph::Next: {
                juce::Path p;
                p.startNewSubPath(c.x - 2.2f, c.y - 4.0f);
                p.lineTo(c.x + 2.2f, c.y);
                p.lineTo(c.x - 2.2f, c.y + 4.0f);
                g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                break;
            }
            case Glyph::Save: {
                // Minimalist studio disk glyph
                auto r = juce::Rectangle<float>(c.x - 5.2f, c.y - 5.2f, 10.4f, 10.4f);
                juce::Path p;
                p.startNewSubPath(r.getX(), r.getY());
                p.lineTo(r.getRight() - 2.6f, r.getY());
                p.lineTo(r.getRight(), r.getY() + 2.6f);
                p.lineTo(r.getRight(), r.getBottom());
                p.lineTo(r.getX(), r.getBottom());
                p.closeSubPath();
                g.strokePath(p, juce::PathStrokeType(1.25f));
                g.drawRect(r.getX() + 2.5f, r.getY(), 4.5f, 3.2f, 1.0f);
                g.drawRect(r.getX() + 2.2f, r.getBottom() - 4.2f, 6.0f, 4.2f, 1.0f);
                break;
            }
            case Glyph::Load: {
                // Minimalist folder glyph
                juce::Path p;
                p.startNewSubPath(c.x - 5.8f, c.y - 3.6f);
                p.lineTo(c.x - 1.5f, c.y - 3.6f);
                p.lineTo(c.x - 0.2f, c.y - 2.0f);
                p.lineTo(c.x + 5.8f, c.y - 2.0f);
                p.lineTo(c.x + 5.8f, c.y + 4.4f);
                p.lineTo(c.x - 5.8f, c.y + 4.4f);
                p.closeSubPath();
                g.strokePath(p, juce::PathStrokeType(1.25f));
                break;
            }
            case Glyph::Midi: {
                // 5-pin MIDI DIN connector icon
                g.drawEllipse(c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f, 1.25f);
                const float pts[5][2] = {
                    { -3.0f, 0.2f }, { -2.0f, -2.2f }, { 0.0f, -3.0f }, { 2.0f, -2.2f }, { 3.0f, 0.2f }
                };
                for (const auto& pt : pts) {
                    g.fillEllipse(c.x + pt[0] - 0.85f, c.y + pt[1] - 0.85f, 1.7f, 1.7f);
                }
                break;
            }
            case Glyph::Audio: {
                // Clean 3-row audio mixer/sliders icon
                const float ys[3] = { -3.5f, 0.0f, 3.5f };
                const float xs[3] = { -1.5f, 2.2f, -2.5f };
                for (int i = 0; i < 3; ++i) {
                    g.drawLine(c.x - 5.5f, c.y + ys[i], c.x + 5.5f, c.y + ys[i], 1.15f);
                    g.fillEllipse(c.x + xs[i] - 1.5f, c.y + ys[i] - 1.5f, 3.0f, 3.0f);
                }
                break;
            }
        }
    }

    Glyph glyph;
    juce::String caption;
    bool drawFrame = true;
    bool accentActive = false;
    juce::Colour accentCol { 0xff3b82f6 };
};

// Center clickable preset selector display inside the unified Preset Cluster (NO "[BROWSER]" text!)
class PresetDisplayButton : public juce::Button {
public:
    PresetDisplayButton() : juce::Button("Preset") {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Click to open Preset Browser (P)  |  Use [ / ] for Prev/Next");
    }

    void setPresetInfo(const juce::String& slotTag, const juce::String& name) {
        if (slotCode != slotTag || presetName != name) {
            slotCode = slotTag;
            presetName = name;
            repaint();
        }
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override {
        auto b = getLocalBounds().toFloat();
        if (hovered || down) {
            g.setColour(juce::Colours::white.withAlpha(down ? 0.08f : 0.04f));
            g.fillRect(b.reduced(0.0f, 1.0f));
        }

        auto inner = b.reduced(7.0f, 2.0f);

        // Subtle preset slot index chip on left
        if (slotCode.isNotEmpty() && inner.getWidth() > 110.0f) {
            auto chip = inner.removeFromLeft(24.0f).withSizeKeepingCentre(22.0f, 15.0f);
            g.setColour(juce::Colour(0xff181e2a));
            g.fillRoundedRectangle(chip, 3.0f);
            g.setColour(juce::Colour(0xff7d8aa0));
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.drawText(slotCode, chip, juce::Justification::centred, false);
            inner.removeFromLeft(5.0f);
        }

        // Crisp downward dropdown chevron on right
        auto chevBox = inner.removeFromRight(12.0f);
        const float cx = chevBox.getCentreX();
        const float cy = chevBox.getCentreY();
        juce::Path chev;
        chev.startNewSubPath(cx - 3.2f, cy - 1.4f);
        chev.lineTo(cx, cy + 2.0f);
        chev.lineTo(cx + 3.2f, cy - 1.4f);
        g.setColour(juce::Colour(hovered ? 0xffe2e8f0 : 0xff6e7b91));
        g.strokePath(chev, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Clean warm-white preset name
        g.setColour(hovered ? juce::Colours::white : juce::Colour(0xfff1f5f9));
        g.setFont(juce::Font(juce::FontOptions(11.8f, juce::Font::bold)));
        g.drawText(presetName, inner.withTrimmedRight(3.0f), juce::Justification::centredLeft, true);
    }

private:
    juce::String slotCode;
    juce::String presetName;
};

// Flagship Pro-Audio Top Header Bar:
// Minimalist Wordmark | Unified Preset Cluster + Vector Utilities | Interactive Chromatic Tuner | Matched I/O Trim + Segmented Meters
class TopRackBar : public juce::Component {
public:
    using Callback = std::function<void()>;

    TopRackBar(PedalBoardAudioProcessor& proc,
               Callback onToggleBrowser,
               Callback onSave,
               Callback onLoad,
               Callback onAudioSettings = {})
        : processor(proc),
          audioSettingsCb(std::move(onAudioSettings)),
          prevPresetBtn(TopUtilityButton::Glyph::Prev, {}, false),
          nextPresetBtn(TopUtilityButton::Glyph::Next, {}, false),
          saveBtn(TopUtilityButton::Glyph::Save),
          loadBtn(TopUtilityButton::Glyph::Load),
          midiLearnBtn(TopUtilityButton::Glyph::Midi, "MIDI"),
          audioSetupBtn(TopUtilityButton::Glyph::Audio, "SETUP"),
          inMeter({}, false),
          outMeter({}, false) {
        prevPresetBtn.setTooltip("Previous Preset ([)");
        prevPresetBtn.onClick = [this] { stepFactoryPreset(-1); };
        addAndMakeVisible(prevPresetBtn);

        presetDisplayBtn.onClick = std::move(onToggleBrowser);
        addAndMakeVisible(presetDisplayBtn);

        nextPresetBtn.setTooltip("Next Preset (])");
        nextPresetBtn.onClick = [this] { stepFactoryPreset(+1); };
        addAndMakeVisible(nextPresetBtn);

        saveBtn.setTooltip("Save Preset to File (.pbpreset)");
        saveBtn.onClick = std::move(onSave);
        addAndMakeVisible(saveBtn);

        loadBtn.setTooltip("Load Preset from File (.pbpreset)");
        loadBtn.onClick = std::move(onLoad);
        addAndMakeVisible(loadBtn);

        midiLearnBtn.setTooltip("MIDI CC Manager — Right-click any knob or footswitch to bind MIDI CC");
        midiLearnBtn.onClick = [this] {
            auto& mgr = processor.getMidiLearnManager();
            if (mgr.isLearnArmed()) {
                mgr.cancelLearn();
            } else {
                juce::PopupMenu m;
                m.addItem(2, "Learn Next Clicked Knob / Switch...");
                m.addSeparator();
                m.addSectionHeader("Active MIDI CC Mappings");
                const auto mappings = mgr.getAllMappings();
                if (mappings.empty()) {
                    m.addItem(99, "None mapped yet (Right-click any control)", false, false);
                } else {
                    for (size_t i = 0; i < mappings.size(); ++i) {
                        m.addItem(100 + static_cast<int>(i),
                                  "Remove CC #" + juce::String(mappings[i].ccNumber) + " -> " + mappings[i].paramId);
                    }
                    m.addSeparator();
                    m.addItem(1, "Clear All MIDI Mappings");
                }
                m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&midiLearnBtn),
                                [&mgr, mappings](int res) {
                                    if (res == 2) {
                                        mgr.armGlobalLearn();
                                    } else if (res == 1) {
                                        mgr.clearAllMappings();
                                    } else if (res >= 100 && res < 100 + static_cast<int>(mappings.size())) {
                                        mgr.clearMappingForParam(mappings[static_cast<size_t>(res - 100)].paramId);
                                    }
                                });
            }
            refreshMidiButton();
        };
        addAndMakeVisible(midiLearnBtn);

        audioSetupBtn.setTooltip("Studio Settings (Ctrl+,) — Audio I/O, Input Routing, Tuner A4, MIDI & Storage");
        audioSetupBtn.onClick = [this] {
            if (audioSettingsCb) audioSettingsCb();
        };
        addAndMakeVisible(audioSetupBtn);

        tunerDisplay.setOnToggleMute([this] { toggleTunerMute(); });
        tunerDisplay.setOnRightClick([this] {
            showMidiLearnMenu(processor.getMidiLearnManager(), params::kTunerMute, &tunerDisplay);
        });
        addAndMakeVisible(tunerDisplay);

        cpuMeter.setOnClick([this] {
            if (audioSettingsCb) audioSettingsCb();
        });
        addAndMakeVisible(cpuMeter);

        configureTrimKnob(inKnob,  params::kInputGain,  juce::Colour(0xff38bdf8), "Input Trim (double-click to reset 0 dB)",  inAttachment);
        configureTrimKnob(outKnob, params::kOutputGain, juce::Colour(0xff22c55e), "Output Level (double-click to reset 0 dB)", outAttachment);

        inMeter.setTooltip("Stereo Input Level (click to clear clip)");
        outMeter.setTooltip("Stereo Output Level (click to clear clip)");
        addAndMakeVisible(inMeter);
        addAndMakeVisible(outMeter);

        syncFromProcessor();
    }

    void stepFactoryPreset(int delta) {
        const int n = processor.getNumPrograms();
        if (n <= 0) return;
        const int cur = processor.getCurrentProgram();
        const int next = (cur + delta + n) % n;
        processor.setCurrentProgram(next);
    }

    void toggleTunerMute() {
        if (auto* p = processor.getAPVTS().getParameter(params::kTunerMute)) {
            const bool cur = p->getValue() > 0.5f;
            p->beginChangeGesture();
            p->setValueNotifyingHost(cur ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
    }

    void syncFromProcessor() {
        updatePresetLcdText();
        refreshMidiButton();
        repaint();
    }

    void updatePresetLcdText() {
        const auto name = processor.getActivePresetName();
        const int idx = processor.getCurrentProgram();
        const int total = processor.getNumPrograms();
        juce::String slotTag = "USR";
        if (name == "Clean Init") {
            slotTag = "INIT";
        } else if (idx >= 0 && idx < total && processor.getProgramName(idx) == name) {
            slotTag = juce::String::formatted("%02d", idx + 1);
        }
        presetDisplayBtn.setPresetInfo(slotTag, name);
    }

    void updateLiveMetersAndTuner() {
        const auto& chain = processor.getSignalChain();
        inMeter.setLevels(chain.getInputPeakL(), chain.getInputPeakR());
        outMeter.setLevels(chain.getOutputPeakL(), chain.getOutputPeakR());

        bool muted = false;
        if (auto* raw = processor.getAPVTS().getRawParameterValue(params::kTunerMute)) {
            muted = raw->load(std::memory_order_relaxed) > 0.5f;
        }
        tunerDisplay.setReading(chain.getTuner().analyze(), muted, processor.getTunerReferenceHz());

        float cpuPct = processor.getDspCpuLoadPercent();
        if (const auto host = queryStandaloneAudioHost(); host.deviceManager != nullptr) {
            cpuPct = std::max(cpuPct, static_cast<float>(host.deviceManager->getCpuUsage() * 100.0));
        }
        cpuMeter.setCpuPercent(cpuPct);

        refreshMidiButton();
        const double sr = processor.getSampleRate();
        if (std::abs(sr - lastDisplayedSampleRate) > 1.0) {
            lastDisplayedSampleRate = sr;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Architectural matte charcoal header surface
        juce::ColourGradient bg(juce::Colour(0xff141821), 0.0f, 0.0f,
                                juce::Colour(0xff0e1118), 0.0f, bounds.getHeight(), false);
        g.setGradientFill(bg);
        g.fillRect(bounds);

        g.setColour(juce::Colour(0xff222938));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

        const bool compact = (getWidth() < 1140);
        const float midY = bounds.getCentreY();

        // 1. Minimalist Studio Wordmark (zero neon badges!)
        const float markSz = 20.0f;
        auto mark = juce::Rectangle<float>(12.0f, midY - markSz * 0.5f, markSz, markSz);
        g.setColour(juce::Colour(0xff1a202d));
        g.fillRoundedRectangle(mark, 4.5f);
        g.setColour(juce::Colour(0xff2f394d));
        g.drawRoundedRectangle(mark, 4.5f, 1.0f);

        juce::Path wave;
        wave.startNewSubPath(mark.getX() + 3.5f, mark.getCentreY());
        wave.lineTo(mark.getX() + markSz * 0.36f, mark.getY() + 4.5f);
        wave.lineTo(mark.getX() + markSz * 0.64f, mark.getBottom() - 4.5f);
        wave.lineTo(mark.getRight() - 3.5f, mark.getCentreY());
        g.setColour(juce::Colour(0xff60a5fa));
        g.strokePath(wave, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float textX = mark.getRight() + 8.0f;
        const float textW = std::max(40.0f, static_cast<float>(brandDividerX) - textX - 6.0f);
        g.setColour(juce::Colour(0xfff1f5f9));
        g.setFont(juce::Font(juce::FontOptions(compact ? 12.6f : 13.6f, juce::Font::bold)));
        g.drawText("PEDALBOARD",
                   juce::Rectangle<float>(textX, midY - 12.5f, textW, 14.0f),
                   juce::Justification::centredLeft, true);

        // Understated real engine telemetry line (sample rate + driver/format)
        const double sr = processor.getSampleRate();
        const juce::String srTxt = (sr > 1000.0) ? (juce::String(sr / 1000.0, 1) + " kHz") : "48.0 kHz";
#if defined(PEDALBOARD_HAS_ASIO) && PEDALBOARD_HAS_ASIO
        const juce::String drvTxt = (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone) ? "ASIO" : "VST3";
#else
        const juce::String drvTxt = (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone) ? "WASAPI" : "VST3";
#endif
        g.setColour(juce::Colour(0xff5f6b80));
        g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
        g.drawText(srTxt + juce::String::fromUTF8("  \xC2\xB7  ") + drvTxt,
                   juce::Rectangle<float>(textX, midY + 2.0f, textW, 11.0f),
                   juce::Justification::centredLeft, true);

        // Architectural hairline dividers
        g.setColour(juce::Colour(0xff212836));
        g.drawVerticalLine(brandDividerX, 10.0f, bounds.getHeight() - 10.0f);
        g.drawVerticalLine(utilDividerX,  12.0f, bounds.getHeight() - 12.0f);

        // 2. Unified Recessed Preset Cluster Chassis (< | Preset Name ▾ | >)
        if (!presetClusterRect.isEmpty()) {
            auto pr = presetClusterRect.toFloat();
            g.setColour(juce::Colour(0xff0a0d13));
            g.fillRoundedRectangle(pr, 5.5f);
            g.setColour(juce::Colour(0xff242d3e));
            g.drawRoundedRectangle(pr, 5.5f, 1.0f);

            g.setColour(juce::Colour(0xff1e2533));
            g.drawVerticalLine(prevPresetBtn.getRight(), pr.getY() + 4.0f, pr.getBottom() - 4.0f);
            g.drawVerticalLine(nextPresetBtn.getX(),     pr.getY() + 4.0f, pr.getBottom() - 4.0f);
        }

        // 3. Symmetrical Studio I/O Recessed Plate (IN | OUT)
        if (!ioPlateRect.isEmpty()) {
            auto ior = ioPlateRect.toFloat();
            g.setColour(juce::Colour(0xff0b0e14));
            g.fillRoundedRectangle(ior, 5.5f);
            g.setColour(juce::Colour(0xff222a38));
            g.drawRoundedRectangle(ior, 5.5f, 1.0f);

            // Center hairline between IN and OUT
            g.setColour(juce::Colour(0xff1f2634));
            g.drawVerticalLine(ioMidDividerX, ior.getY() + 5.0f, ior.getBottom() - 5.0f);

            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.setColour(juce::Colour(0xff7c8aa0));
            g.drawText("IN",  inTagRect,  juce::Justification::centred);
            g.drawText("OUT", outTagRect, juce::Justification::centred);
        }
    }

    void resized() override {
        const int totalW = getWidth();
        const int totalH = getHeight();
        const bool compact = (totalW < 1140);
        const bool shortBar = (totalH < 50);

        auto area = getLocalBounds().reduced(compact ? 8 : 10, shortBar ? 6 : 8);
        const int ctrlH = area.getHeight();
        const int pillH = std::min(30, ctrlH);

        // 1. Left: Minimalist Wordmark
        const int brandW = compact ? 118 : 134;
        area.removeFromLeft(brandW);
        brandDividerX = area.getX();
        area.removeFromLeft(compact ? 10 : 12);

        // 2. Unified Preset Cluster + Save/Load + MIDI/Audio Utilities
        const int arrowW = compact ? 22 : 24;
        const int presetNameW = std::clamp(totalW * 15 / 100, 132, 214);
        presetClusterRect = area.removeFromLeft(arrowW * 2 + presetNameW).withSizeKeepingCentre(arrowW * 2 + presetNameW, pillH);

        auto pIn = presetClusterRect;
        prevPresetBtn.setBounds(pIn.removeFromLeft(arrowW));
        nextPresetBtn.setBounds(pIn.removeFromRight(arrowW));
        presetDisplayBtn.setBounds(pIn);

        area.removeFromLeft(5);
        const int sqW = pillH;
        saveBtn.setBounds(area.removeFromLeft(sqW).withSizeKeepingCentre(sqW, pillH));
        area.removeFromLeft(3);
        loadBtn.setBounds(area.removeFromLeft(sqW).withSizeKeepingCentre(sqW, pillH));

        area.removeFromLeft(compact ? 8 : 10);
        utilDividerX = area.getX();
        area.removeFromLeft(compact ? 8 : 10);

        const int midiW = compact ? 58 : 66;
        midiLearnBtn.setBounds(area.removeFromLeft(midiW).withSizeKeepingCentre(midiW, pillH));
        if (audioSetupBtn.isVisible()) {
            area.removeFromLeft(4);
            const int audioW = compact ? 62 : 70;
            audioSetupBtn.setBounds(area.removeFromLeft(audioW).withSizeKeepingCentre(audioW, pillH));
        }

        // 3. Far Right: Symmetrical Studio I/O Module (IN [knob][meter] | OUT [knob][meter])
        const int tagW   = compact ? 20 : 24;
        const int knobSz = std::min(shortBar ? 30 : 34, ctrlH + 2);
        const int meterW = compact ? 14 : 16;
        const int halfW  = 6 + tagW + 2 + knobSz + 4 + meterW + 6;
        const int ioTotalW = halfW * 2 + 1;

        ioPlateRect = area.removeFromRight(ioTotalW).withSizeKeepingCentre(ioTotalW, std::min(36, ctrlH + 4));
        auto ioIn = ioPlateRect;

        auto inHalf = ioIn.removeFromLeft(halfW);
        ioMidDividerX = ioIn.getX();
        ioIn.removeFromLeft(1);
        auto outHalf = ioIn;

        auto layoutIoHalf = [&](juce::Rectangle<int> r, juce::Rectangle<int>& tagR, juce::Slider& knob, LevelMeterComponent& meter) {
            r.reduce(5, 2);
            tagR = r.removeFromLeft(tagW);
            r.removeFromLeft(2);
            auto kBox = r.removeFromLeft(knobSz);
            knob.setBounds(kBox.withSizeKeepingCentre(knobSz, knobSz));
            r.removeFromLeft(4);
            meter.setBounds(r.removeFromLeft(meterW).reduced(0, 1));
        };

        layoutIoHalf(inHalf,  inTagRect,  inKnob,  inMeter);
        layoutIoHalf(outHalf, outTagRect, outKnob, outMeter);

        // 4. Center: Precision Chromatic Tuner + Live DSP CPU Usage Pill
        area.reduce(compact ? 6 : 12, 0);
        const int cpuW = compact ? 64 : 78;
        const int gap = 6;
        const int availW = area.getWidth();
        const bool showCpu = (availW >= 190);
        cpuMeter.setVisible(showCpu);

        const int tunerMaxW = showCpu ? std::min(266, availW - cpuW - gap) : std::min(290, availW);
        const int tunerW = std::clamp(tunerMaxW, 120, 290);
        const int tunerH = std::min(34, ctrlH + 2);
        const int clusterW = showCpu ? (tunerW + gap + cpuW) : tunerW;
        auto cluster = area.withSizeKeepingCentre(clusterW, tunerH);
        tunerDisplay.setBounds(cluster.removeFromLeft(tunerW));
        if (showCpu) {
            cluster.removeFromLeft(gap);
            cpuMeter.setBounds(cluster);
        }
    }

private:
    void refreshMidiButton() {
        const auto& mgr = processor.getMidiLearnManager();
        if (mgr.isLearnArmed()) {
            const bool waitingForKnob = mgr.getLearningParamId().isEmpty();
            midiLearnBtn.setCaption(waitingForKnob ? "CLICK" : "CC...");
            midiLearnBtn.setAccentState(true, juce::Colour(0xfff59e0b));
        } else {
            const auto count = mgr.getAllMappings().size();
            midiLearnBtn.setCaption(count > 0 ? ("MIDI " + juce::String(count)) : "MIDI");
            midiLearnBtn.setAccentState(count > 0, juce::Colour(0xff3b82f6));
        }
    }

    void configureTrimKnob(MidiBindableSlider& s,
                           const char* paramId,
                           juce::Colour accent,
                           const juce::String& tip,
                           std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setPopupDisplayEnabled(true, false, nullptr);
        s.setTextValueSuffix(" dB");
        s.setTooltip(tip);
        s.setColour(juce::Slider::rotarySliderFillColourId, accent);
        s.getProperties().set("knobStyle", "chrome_knurled");
        s.getProperties().set("showTicks", true);
        s.getProperties().set("compactTrim", true);
        s.setDoubleClickReturnValue(true, 0.0);
        s.configureMidiLearn(&processor.getMidiLearnManager(), paramId);
        addAndMakeVisible(s);

        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getAPVTS(), paramId, s
        );
    }

    PedalBoardAudioProcessor& processor;
    Callback audioSettingsCb;

    int brandDividerX = 140;
    int utilDividerX = 430;
    int ioMidDividerX = 1000;
    double lastDisplayedSampleRate = 0.0;
    juce::Rectangle<int> presetClusterRect;
    juce::Rectangle<int> ioPlateRect;
    juce::Rectangle<int> inTagRect, outTagRect;

    TopUtilityButton prevPresetBtn;
    PresetDisplayButton presetDisplayBtn;
    TopUtilityButton nextPresetBtn;
    TopUtilityButton saveBtn, loadBtn, midiLearnBtn, audioSetupBtn;

    TunerDisplayComponent tunerDisplay;
    CpuMeterComponent cpuMeter;
    MidiBindableSlider inKnob, outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAttachment, outAttachment;
    LevelMeterComponent inMeter, outMeter;
};

} // namespace pedalboard
