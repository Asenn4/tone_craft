#pragma once

#include "../dsp/LooperRecorder.h"
#include "../plugin/Parameters.h"
#include "../plugin/PluginProcessor.h"
#include "MidiBindableControl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard {

// Minimalist crisp vector transport icon button used inside the unified Looper & WAV Recorder capsules.
class DockIconButton : public juce::Button {
public:
    enum class Glyph { RecDot, PlayTri, OverdubRing, StopSq, UndoArc, ClearTrash, Folder };

    DockIconButton(Glyph initialGlyph, const juce::String& labelText = {})
        : juce::Button(labelText),
          glyph(initialGlyph),
          caption(labelText) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setVisualState(Glyph gType,
                        const juce::String& newCaption,
                        juce::Colour fillCol,
                        juce::Colour iconCol,
                        juce::Colour textCol) {
        if (glyph != gType || caption != newCaption || bgCol != fillCol || glyphCol != iconCol || labelCol != textCol) {
            glyph = gType;
            caption = newCaption;
            bgCol = fillCol;
            glyphCol = iconCol;
            labelCol = textCol;
            repaint();
        }
    }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override {
        auto b = getLocalBounds().toFloat().reduced(0.5f, 1.0f);
        const bool enabled = isEnabled();

        if (!bgCol.isTransparent()) {
            auto fill = enabled ? (down ? bgCol.darker(0.16f) : (hovered ? bgCol.brighter(0.12f) : bgCol))
                                : bgCol.withAlpha(0.25f);
            g.setColour(fill);
            g.fillRoundedRectangle(b, 3.5f);
        } else if (enabled && (hovered || down)) {
            g.setColour(juce::Colours::white.withAlpha(down ? 0.10f : 0.055f));
            g.fillRoundedRectangle(b, 3.5f);
        }

        const float alpha = enabled ? 1.0f : 0.30f;
        const auto icCol = (enabled && hovered && glyphCol.getSaturation() < 0.15f)
                               ? juce::Colour(0xfff1f5f9)
                               : glyphCol.withMultipliedAlpha(alpha);
        const auto txCol = (enabled && hovered && labelCol.getSaturation() < 0.15f)
                               ? juce::Colour(0xfff8fafc)
                               : labelCol.withMultipliedAlpha(alpha);

        if (caption.isEmpty()) {
            drawGlyph(g, b.getCentre(), icCol);
        } else {
            const float iconX = b.getX() + 13.0f;
            drawGlyph(g, { iconX, b.getCentreY() }, icCol);
            g.setColour(txCol);
            g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
            g.drawText(caption, b.withTrimmedLeft(22.0f).withTrimmedRight(5.0f), juce::Justification::centredLeft, true);
        }
    }

private:
    void drawGlyph(juce::Graphics& g, juce::Point<float> c, juce::Colour col) const {
        g.setColour(col);
        switch (glyph) {
            case Glyph::RecDot: {
                g.fillEllipse(c.x - 4.2f, c.y - 4.2f, 8.4f, 8.4f);
                break;
            }
            case Glyph::PlayTri: {
                juce::Path p;
                p.addTriangle(c.x - 3.2f, c.y - 4.4f, c.x - 3.2f, c.y + 4.4f, c.x + 4.5f, c.y);
                g.fillPath(p);
                break;
            }
            case Glyph::OverdubRing: {
                g.drawEllipse(c.x - 4.6f, c.y - 4.6f, 9.2f, 9.2f, 1.35f);
                g.fillEllipse(c.x - 2.0f, c.y - 2.0f, 4.0f, 4.0f);
                break;
            }
            case Glyph::StopSq: {
                g.fillRoundedRectangle(c.x - 3.8f, c.y - 3.8f, 7.6f, 7.6f, 1.2f);
                break;
            }
            case Glyph::UndoArc: {
                juce::Path arc;
                arc.addCentredArc(c.x + 0.5f, c.y + 1.2f, 4.2f, 4.2f, 0.0f, -1.85f, 1.55f, true);
                g.strokePath(arc, juce::PathStrokeType(1.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                juce::Path head;
                head.addTriangle(c.x - 5.2f, c.y - 1.5f, c.x - 1.5f, c.y - 4.2f, c.x - 1.2f, c.y - 0.1f);
                g.fillPath(head);
                break;
            }
            case Glyph::ClearTrash: {
                // Clean studio X / clear icon
                g.drawLine(c.x - 3.8f, c.y - 3.8f, c.x + 3.8f, c.y + 3.8f, 1.45f);
                g.drawLine(c.x + 3.8f, c.y - 3.8f, c.x - 3.8f, c.y + 3.8f, 1.45f);
                break;
            }
            case Glyph::Folder: {
                juce::Path p;
                p.startNewSubPath(c.x - 5.4f, c.y - 3.4f);
                p.lineTo(c.x - 1.4f, c.y - 3.4f);
                p.lineTo(c.x - 0.1f, c.y - 1.9f);
                p.lineTo(c.x + 5.4f, c.y - 1.9f);
                p.lineTo(c.x + 5.4f, c.y + 4.1f);
                p.lineTo(c.x - 5.4f, c.y + 4.1f);
                p.closeSubPath();
                g.strokePath(p, juce::PathStrokeType(1.25f));
                break;
            }
        }
    }

    Glyph glyph;
    juce::String caption;
    juce::Colour bgCol { juce::Colours::transparentBlack };
    juce::Colour glyphCol { 0xff9aa6bc };
    juce::Colour labelCol { 0xffcbd5e1 };
};

// Direct 1-click Segmented Pre-DI / Post-FX routing control (matching StageNavBar's SERIAL | DUAL A/B!)
class LooperRoutingToggle : public juce::Component,
                            public juce::SettableTooltipClient {
public:
    explicit LooperRoutingToggle(PedalBoardAudioProcessor& proc) : processor(proc) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("PRE: Loops dry DI guitar before pedals/amp (ideal for dialing tone)  |  POST: Loops full processed rig");
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const int next = (e.position.x < getWidth() * 0.5f) ? 0 : 1;
        if (next != hoverHalf) { hoverHalf = next; repaint(); }
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoverHalf != -1) { hoverHalf = -1; repaint(); }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) {
            // Preserve MIDI CC Learn on right-click!
            auto& mgr = processor.getMidiLearnManager();
            juce::PopupMenu m;
            m.addItem(1, "MIDI Learn Pre/Post Looper Switch");
            if (mgr.getCcForParam(params::kLooperPreFx) >= 0) {
                m.addItem(2, "Clear MIDI CC #" + juce::String(mgr.getCcForParam(params::kLooperPreFx)));
            }
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [&mgr](int res) {
                if (res == 1) mgr.armLearn(params::kLooperPreFx);
                else if (res == 2) mgr.clearMappingForParam(params::kLooperPreFx);
            });
            return;
        }
        const bool wantPre = (e.position.x < getWidth() * 0.5f);
        if (auto* p = processor.getAPVTS().getParameter(params::kLooperPreFx)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(wantPre ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
        repaint();
    }

    void paint(juce::Graphics& g) override {
        bool isPre = false;
        if (auto* raw = processor.getAPVTS().getRawParameterValue(params::kLooperPreFx)) {
            isPre = raw->load(std::memory_order_relaxed) > 0.5f;
        }

        auto track = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff090c12));
        g.fillRoundedRectangle(track, 5.0f);
        g.setColour(juce::Colour(0xff232b3a));
        g.drawRoundedRectangle(track, 5.0f, 0.9f);

        const float halfW = track.getWidth() * 0.5f;
        auto drawHalf = [&](juce::Rectangle<float> r, const char* txt, bool active, bool hov, juce::Colour dotCol) {
            r = r.reduced(1.5f);
            if (active) {
                g.setColour(juce::Colour(0xff1e2636));
                g.fillRoundedRectangle(r, 3.6f);
                g.setColour(dotCol.withAlpha(0.45f));
                g.drawRoundedRectangle(r, 3.6f, 0.85f);
            } else if (hov) {
                g.setColour(juce::Colours::white.withAlpha(0.04f));
                g.fillRoundedRectangle(r, 3.6f);
            }

            const float dotR = 2.1f;
            const float dotX = r.getX() + 7.5f;
            g.setColour(active ? dotCol : juce::Colour(0xff3e4759));
            g.fillEllipse(dotX - dotR, r.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);

            g.setColour(active ? juce::Colour(0xfff1f5f9) : juce::Colour(hov ? 0xffb8c2d4 : 0xff6b768c));
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
            g.drawText(txt, r.withTrimmedLeft(11.5f), juce::Justification::centred, true);
        };

        drawHalf(track.withWidth(halfW), "PRE",  isPre,  hoverHalf == 0, juce::Colour(0xff38bdf8));
        drawHalf(track.withTrimmedLeft(halfW), "POST", !isPre, hoverHalf == 1, juce::Colour(0xfff59e0b));
    }

private:
    PedalBoardAudioProcessor& processor;
    int hoverHalf = -1;
};

// Interactive Studio Looper Waveform Display (calm neutral when empty — ZERO bright cyan highlight on empty state!)
class LooperWaveformView : public juce::Component,
                           public juce::SettableTooltipClient {
public:
    explicit LooperWaveformView(const StereoLooper& l, std::function<void()> onClickTimeline)
        : looper(l),
          clickCb(std::move(onClickTimeline)) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Looper Timeline — Click or press Space to Record / Play / Overdub (S to Stop)");
    }

    void mouseEnter(const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { hovered = false; repaint(); }
    void mouseDown(const juce::MouseEvent&) override  { if (clickCb) clickCb(); }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat().reduced(0.5f);
        const auto st = looper.getState();

        // Calm neutral graphite when Empty or Stopped (zero distracting neon highlight!)
        juce::Colour borderCol(hovered ? 0xff343f54 : 0xff222a38);
        juce::Colour stateCol(0xff5a6578);
        juce::String stateLabel = "READY";

        switch (st) {
            case LooperState::Recording:
                borderCol = juce::Colour(0xffdc2626).withAlpha(0.75f);
                stateCol  = juce::Colour(0xffef4444);
                stateLabel = "REC";
                break;
            case LooperState::Playing:
                borderCol = juce::Colour(0xff22c55e).withAlpha(0.60f);
                stateCol  = juce::Colour(0xff22c55e);
                stateLabel = "PLAY";
                break;
            case LooperState::Overdubbing:
                borderCol = juce::Colour(0xfff59e0b).withAlpha(0.68f);
                stateCol  = juce::Colour(0xfff59e0b);
                stateLabel = "DUB";
                break;
            case LooperState::Stopped:
                borderCol = juce::Colour(hovered ? 0xff3a465c : 0xff2b3446);
                stateCol  = juce::Colour(0xff8a96ab);
                stateLabel = "STOP";
                break;
            case LooperState::Empty:
            default:
                break;
        }

        g.setColour(juce::Colour(0xff090c12));
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(borderCol);
        g.drawRoundedRectangle(area, 5.0f, 1.0f);

        auto inner = area.reduced(6.0f, 3.0f);

        // Left status column (State dot + label + duration + optional L2 undo badge)
        const float badgeW = std::min(72.0f, inner.getWidth() * 0.25f);
        auto badgeRect = inner.removeFromLeft(badgeW);

        auto topBadge = badgeRect.removeFromTop(badgeRect.getHeight() * 0.54f);
        const float dotR = 2.6f;
        g.setColour(stateCol);
        g.fillEllipse(topBadge.getX() + 1.0f, topBadge.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);

        g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
        g.drawText(stateLabel, topBadge.withTrimmedLeft(9.5f), juce::Justification::centredLeft, false);

        if (looper.canUndo() && badgeW >= 62.0f) {
            auto l2 = topBadge.removeFromRight(18.0f).withSizeKeepingCentre(17.0f, 12.0f);
            g.setColour(juce::Colour(0xff1e2636));
            g.fillRoundedRectangle(l2, 2.5f);
            g.setColour(juce::Colour(0xfffbbf24));
            g.setFont(juce::Font(juce::FontOptions(7.5f, juce::Font::bold)));
            g.drawText("L2", l2, juce::Justification::centred);
        }

        const double secs = looper.getLoopSeconds();
        const juce::String timeTxt = (st == LooperState::Recording)
            ? (juce::String(secs, 1) + "s / 45s")
            : (secs > 0.01 ? (juce::String(secs, 1) + "s") : "00.0s");
        g.setColour(st == LooperState::Empty ? juce::Colour(0xff4c5669) : juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(8.6f, juce::Font::bold)));
        g.drawText(timeTxt, badgeRect.withTrimmedLeft(1.0f), juce::Justification::centredLeft, true);

        // Subtle separator between status readout and waveform lane
        g.setColour(juce::Colour(0xff1b2230));
        g.drawVerticalLine(static_cast<int>(inner.getX()), area.getY() + 5.0f, area.getBottom() - 5.0f);
        inner.removeFromLeft(6.0f);
        if (inner.getWidth() < 32.0f) return;

        // Subtle beat/quarter division grid ticks (25%, 50%, 75%)
        const float midY = inner.getCentreY();
        g.setColour(juce::Colours::white.withAlpha(0.055f));
        g.drawHorizontalLine(static_cast<int>(midY), inner.getX(), inner.getRight());
        for (int q = 1; q <= 3; ++q) {
            const float qx = inner.getX() + inner.getWidth() * (static_cast<float>(q) * 0.25f);
            g.setColour(juce::Colours::white.withAlpha(q == 2 ? 0.08f : 0.045f));
            g.drawVerticalLine(static_cast<int>(qx), inner.getY() + 2.0f, inner.getBottom() - 2.0f);
        }

        // Understated keyboard shortcut cue on right when empty
        if (st == LooperState::Empty) {
            if (inner.getWidth() >= 140.0f) {
                g.setColour(juce::Colour(hovered ? 0xff6b778f : 0xff3b4456));
                g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
                g.drawText("SPACE", inner.reduced(4.0f, 0.0f), juce::Justification::centredRight);
            }
            return;
        }

        // Progress fill behind playhead
        const float progress = (st == LooperState::Recording)
            ? std::clamp(static_cast<float>(secs / static_cast<double>(StereoLooper::kMaxSeconds)), 0.0f, 1.0f)
            : looper.getProgress();

        if (st == LooperState::Playing || st == LooperState::Overdubbing) {
            g.setColour(stateCol.withAlpha(0.07f));
            g.fillRect(inner.withWidth(inner.getWidth() * progress));
        }

        // Mirrored crisp studio waveform bars
        std::array<float, StereoLooper::kWaveformBins> bins {};
        looper.getWaveformPreview(bins);
        const float binW = inner.getWidth() / static_cast<float>(StereoLooper::kWaveformBins);
        const float maxHalfH = inner.getHeight() * 0.44f;

        for (int i = 0; i < StereoLooper::kWaveformBins; ++i) {
            const float pk = std::clamp(bins[static_cast<size_t>(i)], 0.0f, 1.0f);
            if (pk > 0.004f) {
                const float barFrac = static_cast<float>(i) / static_cast<float>(StereoLooper::kWaveformBins);
                const bool played = (st == LooperState::Playing || st == LooperState::Overdubbing) && (barFrac <= progress);
                const float h = std::max(2.0f, pk * maxHalfH * 2.0f);
                g.setColour(stateCol.withAlpha(played ? 0.90f : 0.45f));
                g.fillRoundedRectangle(inner.getX() + static_cast<float>(i) * binW,
                                       midY - h * 0.5f,
                                       std::max(1.2f, binW - 1.0f),
                                       h, 0.6f);
            }
        }

        // Razor-sharp playhead line + top marker triangle
        if (st == LooperState::Playing || st == LooperState::Overdubbing || st == LooperState::Recording) {
            const float px = inner.getX() + progress * inner.getWidth();
            g.setColour(juce::Colours::white.withAlpha(0.92f));
            g.fillRect(px - 0.8f, inner.getY(), 1.6f, inner.getHeight());
            juce::Path tri;
            tri.addTriangle(px - 3.0f, inner.getY(), px + 3.0f, inner.getY(), px, inner.getY() + 3.6f);
            g.fillPath(tri);
        }
    }

private:
    const StereoLooper& looper;
    std::function<void()> clickCb;
    bool hovered = false;
};

// Flagship Bottom Studio Dock:
// Unified Looper Transport Strip | PRE/POST Segmented Switch | Interactive Waveform | Matched LVL Trim | Unified 24-bit WAV Recorder Capsule
class LooperRecorderBar : public juce::Component {
public:
    explicit LooperRecorderBar(PedalBoardAudioProcessor& proc)
        : processor(proc),
          recPlayDubBtn(DockIconButton::Glyph::RecDot, "REC"),
          stopBtn(DockIconButton::Glyph::StopSq),
          undoBtn(DockIconButton::Glyph::UndoArc),
          clearBtn(DockIconButton::Glyph::ClearTrash),
          routingToggle(proc),
          waveformView(proc.getSignalChain().getLooper(), [this] { triggerLooperRecPlayDub(); }),
#if defined(PEDALBOARD_TEMP_DI_TESTER) && PEDALBOARD_TEMP_DI_TESTER
          tempDiPlayBtn(DockIconButton::Glyph::PlayTri, "DI"),
#endif
          wavRecBtn(DockIconButton::Glyph::RecDot, "WAV"),
          wavFolderBtn(DockIconButton::Glyph::Folder) {
        recPlayDubBtn.setTooltip("Looper: Record -> Play -> Overdub (Space)");
        recPlayDubBtn.onClick = [this] { triggerLooperRecPlayDub(); };
        addAndMakeVisible(recPlayDubBtn);

        stopBtn.setTooltip("Stop / Resume Looper Playback (S)");
        stopBtn.onClick = [this] { triggerLooperStopToggle(); };
        addAndMakeVisible(stopBtn);

        undoBtn.setTooltip("Undo Last Overdub Layer");
        undoBtn.onClick = [this] {
            processor.getSignalChain().getLooper().triggerUndo();
            refreshLiveState();
        };
        addAndMakeVisible(undoBtn);

        clearBtn.setTooltip("Clear Looper Memory");
        clearBtn.onClick = [this] {
            processor.getSignalChain().getLooper().clear();
            refreshLiveState();
        };
        addAndMakeVisible(clearBtn);

        addAndMakeVisible(routingToggle);
        addAndMakeVisible(waveformView);

        // Matched Studio Compact Trim Knob for Looper Playback Level (consistent with Top Bar IN/OUT!)
        loopLvlKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        loopLvlKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        loopLvlKnob.setPopupDisplayEnabled(true, false, nullptr);
        loopLvlKnob.setTextValueSuffix(" dB");
        loopLvlKnob.setTooltip("Loop Playback Level (double-click to reset 0 dB)");
        loopLvlKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff22c55e));
        loopLvlKnob.getProperties().set("knobStyle", "chrome_knurled");
        loopLvlKnob.getProperties().set("showTicks", true);
        loopLvlKnob.getProperties().set("compactTrim", true);
        loopLvlKnob.setDoubleClickReturnValue(true, 0.0);
        loopLvlKnob.configureMidiLearn(&processor.getMidiLearnManager(), params::kLooperLevel);
        addAndMakeVisible(loopLvlKnob);
        loopLvlAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getAPVTS(), params::kLooperLevel, loopLvlKnob
        );

        // Unified 24-bit Stereo WAV Recorder Capsule
        wavRecBtn.setTooltip("Record live 24-bit stereo WAV to Documents\\PedalBoard\\Recordings");
        wavRecBtn.onClick = [this] {
            auto& rec = processor.getSignalChain().getRecorder();
            if (rec.isRecording()) rec.stopRecording();
            else                   rec.startRecording();
            refreshLiveState();
        };
        addAndMakeVisible(wavRecBtn);

        wavFolderBtn.setTooltip("Open Recordings Folder");
        wavFolderBtn.onClick = [this] {
            processor.getSignalChain().getRecorder().getRecordingsDirectory().revealToUser();
        };
        addAndMakeVisible(wavFolderBtn);

#if defined(PEDALBOARD_TEMP_DI_TESTER) && PEDALBOARD_TEMP_DI_TESTER
        tempDiPlayBtn.setTooltip("Temporary Clean Guitar DI Sample Player (feeds directly into SignalChain input)");
        tempDiPlayBtn.onClick = [this] {
            processor.getSignalChain().getTempDiPlayer().togglePlay();
            refreshLiveState();
        };
        addAndMakeVisible(tempDiPlayBtn);

        tempDiRhythmBtn.setButtonText("RHY");
        tempDiRhythmBtn.setTooltip("Test Sample 1: Electric Guitar Rhythm Clean");
        tempDiRhythmBtn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        tempDiRhythmBtn.onClick = [this] {
            processor.getSignalChain().getTempDiPlayer().selectTrack(TempDiSamplePlayer::Track::Rhythm);
            refreshLiveState();
        };
        addAndMakeVisible(tempDiRhythmBtn);

        tempDiSoloBtn.setButtonText("SOLO");
        tempDiSoloBtn.setTooltip("Test Sample 2: Clean Guitar Solo");
        tempDiSoloBtn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        tempDiSoloBtn.onClick = [this] {
            processor.getSignalChain().getTempDiPlayer().selectTrack(TempDiSamplePlayer::Track::Solo);
            refreshLiveState();
        };
        addAndMakeVisible(tempDiSoloBtn);
#endif

        refreshLiveState();
    }

    void triggerLooperRecPlayDub() {
        processor.getSignalChain().getLooper().triggerRecPlayOverdub();
        refreshLiveState();
    }

    void triggerLooperStopToggle() {
        processor.getSignalChain().getLooper().triggerStopToggle();
        refreshLiveState();
    }

    void mouseDown(const juce::MouseEvent& e) override {
        // Clicking the saved filename inside the WAV capsule opens the Recordings folder directly!
        if (wavInfoRect.contains(e.getPosition())) {
            const auto lastFile = processor.getSignalChain().getRecorder().getLastRecordedFile();
            if (lastFile.existsAsFile()) {
                lastFile.revealToUser();
            }
        }
    }

    void refreshLiveState() {
        const auto& looper = processor.getSignalChain().getLooper();
        const auto st = looper.getState();

        switch (st) {
            case LooperState::Empty:
                recPlayDubBtn.setVisualState(DockIconButton::Glyph::RecDot, "REC",
                                             juce::Colour(0xff241317), juce::Colour(0xffef4444), juce::Colour(0xffe2e8f0));
                break;
            case LooperState::Recording:
                recPlayDubBtn.setVisualState(DockIconButton::Glyph::PlayTri, "PLAY",
                                             juce::Colour(0xffdc2626), juce::Colours::white, juce::Colours::white);
                break;
            case LooperState::Playing:
                recPlayDubBtn.setVisualState(DockIconButton::Glyph::OverdubRing, "DUB",
                                             juce::Colour(0xff14532d), juce::Colour(0xff4ade80), juce::Colours::white);
                break;
            case LooperState::Overdubbing:
                recPlayDubBtn.setVisualState(DockIconButton::Glyph::PlayTri, "PLAY",
                                             juce::Colour(0xffb45309), juce::Colours::white, juce::Colours::white);
                break;
            case LooperState::Stopped:
                recPlayDubBtn.setVisualState(DockIconButton::Glyph::OverdubRing, "DUB",
                                             juce::Colour(0xff1c2230), juce::Colour(0xfff59e0b), juce::Colour(0xffcbd5e1));
                break;
        }

        const bool hasLoop = (st != LooperState::Empty);
        stopBtn.setEnabled(hasLoop);
        stopBtn.setVisualState(st == LooperState::Stopped ? DockIconButton::Glyph::PlayTri : DockIconButton::Glyph::StopSq,
                               {}, juce::Colours::transparentBlack,
                               st == LooperState::Stopped ? juce::Colour(0xff22c55e) : juce::Colour(0xff9aa6bc),
                               juce::Colour(0xffcbd5e1));

        undoBtn.setEnabled(looper.canUndo());
        undoBtn.setVisualState(DockIconButton::Glyph::UndoArc, {}, juce::Colours::transparentBlack,
                               looper.canUndo() ? juce::Colour(0xfffbbf24) : juce::Colour(0xff7d8aa0),
                               juce::Colour(0xffcbd5e1));

        clearBtn.setEnabled(hasLoop);
        clearBtn.setVisualState(DockIconButton::Glyph::ClearTrash, {}, juce::Colours::transparentBlack,
                                juce::Colour(0xff9aa6bc), juce::Colour(0xffcbd5e1));

        // Zero color glow on Loop Level knob when Looper is empty!
        const bool knobBypassed = !hasLoop;
        if (static_cast<bool>(loopLvlKnob.getProperties().getWithDefault("bypassed", false)) != knobBypassed) {
            loopLvlKnob.getProperties().set("bypassed", knobBypassed);
            loopLvlKnob.repaint();
        }

        routingToggle.repaint();
        waveformView.repaint();

        const auto& rec = processor.getSignalChain().getRecorder();
        const bool isRec = rec.isRecording();
        if (isRec) {
            wavRecBtn.setVisualState(DockIconButton::Glyph::StopSq, "STOP",
                                     juce::Colour(0xffdc2626), juce::Colours::white, juce::Colours::white);
        } else {
            wavRecBtn.setVisualState(DockIconButton::Glyph::RecDot, "WAV",
                                     juce::Colour(0xff181d28), juce::Colour(0xffef4444), juce::Colour(0xffcbd5e1));
        }

        wavFolderBtn.setVisualState(DockIconButton::Glyph::Folder, {}, juce::Colours::transparentBlack,
                                    juce::Colour(0xff8e9bb2), juce::Colour(0xffcbd5e1));

#if defined(PEDALBOARD_TEMP_DI_TESTER) && PEDALBOARD_TEMP_DI_TESTER
        auto& di = processor.getSignalChain().getTempDiPlayer();
        di.reclaimRetired();
        const bool diPlaying = di.isPlaying();
        const bool isSolo = (di.getSelectedTrack() == TempDiSamplePlayer::Track::Solo);
        if (diPlaying) {
            tempDiPlayBtn.setVisualState(DockIconButton::Glyph::StopSq, "DI",
                                         juce::Colour(0xff0e7490), juce::Colours::white, juce::Colours::white);
        } else {
            tempDiPlayBtn.setVisualState(DockIconButton::Glyph::PlayTri, "DI",
                                         juce::Colour(0xff181d28), juce::Colour(0xff9aa6bc), juce::Colour(0xffcbd5e1));
        }

        auto styleSeg = [&](juce::TextButton& b, bool sel) {
            b.setColour(juce::TextButton::buttonColourId,
                        sel ? (diPlaying ? juce::Colour(0xff155e75) : juce::Colour(0xff232b3a))
                            : juce::Colour(0xff0d1118));
            b.setColour(juce::TextButton::textColourOffId,
                        sel ? (diPlaying ? juce::Colour(0xffe0f2fe) : juce::Colour(0xffcbd5e1))
                            : juce::Colour(0xff64748b));
        };
        styleSeg(tempDiRhythmBtn, !isSolo);
        styleSeg(tempDiSoloBtn, isSolo);
#endif
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        // Matte studio dock background (matches TopRackBar & StageNavBar)
        juce::ColourGradient bg(juce::Colour(0xff131720), 0.0f, 0.0f,
                                juce::Colour(0xff0d1017), 0.0f, bounds.getHeight(), false);
        g.setGradientFill(bg);
        g.fillRect(bounds);

        g.setColour(juce::Colour(0xff212836));
        g.drawHorizontalLine(0, 0.0f, bounds.getWidth());

        // Left understated section title
        g.setColour(juce::Colour(0xff68748a));
        g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
        g.drawText("LOOPER", looperTitleRect, juce::Justification::centredLeft);

        // 1. Unified Recessed Looper Transport Cluster Chassis
        if (!transportClusterRect.isEmpty()) {
            auto tr = transportClusterRect.toFloat();
            g.setColour(juce::Colour(0xff0a0d13));
            g.fillRoundedRectangle(tr, 5.5f);
            g.setColour(juce::Colour(0xff232b3a));
            g.drawRoundedRectangle(tr, 5.5f, 1.0f);

            g.setColour(juce::Colour(0xff1e2533));
            g.drawVerticalLine(recPlayDubBtn.getRight(), tr.getY() + 4.0f, tr.getBottom() - 4.0f);
            g.drawVerticalLine(stopBtn.getRight(),       tr.getY() + 5.0f, tr.getBottom() - 5.0f);
            g.drawVerticalLine(undoBtn.getRight(),       tr.getY() + 5.0f, tr.getBottom() - 5.0f);
        }

        // 2. Looper Level micro-label
        if (!lvlTagRect.isEmpty()) {
            const bool hasLoop = (processor.getSignalChain().getLooper().getState() != LooperState::Empty);
            g.setColour(hasLoop ? juce::Colour(0xff8a96ab) : juce::Colour(0xff525d70));
            g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
            g.drawText("LVL", lvlTagRect, juce::Justification::centred);
        }

        // Architectural hairline divider before WAV Recorder
        g.setColour(juce::Colour(0xff212836));
        g.drawVerticalLine(recorderDividerX, 9.0f, bounds.getHeight() - 9.0f);

#if defined(PEDALBOARD_TEMP_DI_TESTER) && PEDALBOARD_TEMP_DI_TESTER
        if (!tempDiCapsuleRect.isEmpty()) {
            const bool diOn = processor.getSignalChain().getTempDiPlayer().isPlaying();
            auto dr = tempDiCapsuleRect.toFloat();
            g.setColour(juce::Colour(0xff0a0d13));
            g.fillRoundedRectangle(dr, 5.5f);
            g.setColour(diOn ? juce::Colour(0xff06b6d4).withAlpha(0.75f) : juce::Colour(0xff232b3a));
            g.drawRoundedRectangle(dr, 5.5f, 1.0f);
            g.setColour(juce::Colour(0xff1e2533));
            g.drawVerticalLine(tempDiPlayBtn.getRight(), dr.getY() + 4.0f, dr.getBottom() - 4.0f);
        }
#endif

        // 3. Unified 24-bit WAV Recorder Capsule
        if (!wavCapsuleRect.isEmpty()) {
            const auto& rec = processor.getSignalChain().getRecorder();
            const bool isRec = rec.isRecording();
            auto wr = wavCapsuleRect.toFloat();
            g.setColour(juce::Colour(0xff0a0d13));
            g.fillRoundedRectangle(wr, 5.5f);
            g.setColour(isRec ? juce::Colour(0xffdc2626).withAlpha(0.75f) : juce::Colour(0xff232b3a));
            g.drawRoundedRectangle(wr, 5.5f, 1.0f);

            g.setColour(juce::Colour(0xff1e2533));
            g.drawVerticalLine(wavRecBtn.getRight(),   wr.getY() + 4.0f, wr.getBottom() - 4.0f);
            g.drawVerticalLine(wavFolderBtn.getX(),    wr.getY() + 4.0f, wr.getBottom() - 4.0f);

            const int totalSecs = static_cast<int>(rec.getRecordedSeconds());
            const auto timeStr = juce::String::formatted("%02d:%02d", totalSecs / 60, totalSecs % 60);

            auto info = wavInfoRect.toFloat().reduced(6.0f, 2.0f);
            if (isRec) {
                g.setColour(juce::Colour(0xfff87171));
                g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
                g.drawText(timeStr, info.removeFromLeft(44.0f), juce::Justification::centredLeft);
                g.setColour(juce::Colour(0xffef4444).withAlpha(0.85f));
                g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
                g.drawText("REC  24-BIT", info, juce::Justification::centredLeft, true);
            } else {
                const auto lastFile = rec.getLastRecordedFile();
                if (lastFile.existsAsFile()) {
                    g.setColour(juce::Colour(0xff22c55e));
                    g.fillEllipse(info.getX(), info.getCentreY() - 2.2f, 4.4f, 4.4f);
                    g.setColour(juce::Colour(0xffcbd5e1));
                    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
                    g.drawText(lastFile.getFileName(), info.withTrimmedLeft(8.5f), juce::Justification::centredLeft, true);
                } else {
                    g.setColour(juce::Colour(0xff7d8aa0));
                    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
                    g.drawText(timeStr, info.removeFromLeft(40.0f), juce::Justification::centredLeft);
                    g.setColour(juce::Colour(0xff4e596c));
                    g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
                    g.drawText("24-BIT WAV", info, juce::Justification::centredLeft, true);
                }
            }
        }
    }

    void resized() override {
        const int totalW = getWidth();
        const int totalH = getHeight();
        const bool compact = (totalW < 1140);
        const bool shortBar = (totalH < 50);

        auto area = getLocalBounds().reduced(compact ? 8 : 10, shortBar ? 6 : 7);
        const int ctrlH = area.getHeight();
        const int pillH = std::min(30, ctrlH);

        // Left: Section title + Unified Transport Strip + PRE/POST Segmented Toggle
        looperTitleRect = area.removeFromLeft(compact ? 42 : 48);

        const int recW = compact ? 54 : 60;
        const int icoW = compact ? 27 : 30;
        const int transportW = recW + icoW * 3;
        transportClusterRect = area.removeFromLeft(transportW).withSizeKeepingCentre(transportW, pillH);
        {
            auto tIn = transportClusterRect;
            recPlayDubBtn.setBounds(tIn.removeFromLeft(recW));
            stopBtn.setBounds(tIn.removeFromLeft(icoW));
            undoBtn.setBounds(tIn.removeFromLeft(icoW));
            clearBtn.setBounds(tIn);
        }

        area.removeFromLeft(compact ? 6 : 8);
        const int segW = compact ? 92 : 104;
        routingToggle.setBounds(area.removeFromLeft(segW).withSizeKeepingCentre(segW, pillH));
        area.removeFromLeft(compact ? 8 : 10);

        // Far Right: Unified 24-bit WAV Recorder Capsule
        const int wavW = std::clamp(totalW * 21 / 100, 212, 272);
        wavCapsuleRect = area.removeFromRight(wavW).withSizeKeepingCentre(wavW, pillH);
        {
            auto wIn = wavCapsuleRect;
            wavRecBtn.setBounds(wIn.removeFromLeft(compact ? 54 : 60));
            wavFolderBtn.setBounds(wIn.removeFromRight(pillH));
            wavInfoRect = wIn;
        }

        area.removeFromRight(compact ? 8 : 10);
        recorderDividerX = area.getRight();
        area.removeFromRight(compact ? 8 : 10);

#if defined(PEDALBOARD_TEMP_DI_TESTER) && PEDALBOARD_TEMP_DI_TESTER
        const int diW = compact ? 118 : 132;
        tempDiCapsuleRect = area.removeFromRight(diW).withSizeKeepingCentre(diW, pillH);
        {
            auto dIn = tempDiCapsuleRect;
            tempDiPlayBtn.setBounds(dIn.removeFromLeft(compact ? 44 : 48));
            const int half = dIn.getWidth() / 2;
            tempDiRhythmBtn.setBounds(dIn.removeFromLeft(half).reduced(1, 2));
            tempDiSoloBtn.setBounds(dIn.reduced(1, 2));
        }
        area.removeFromRight(compact ? 6 : 8);
#endif

        // Looper Level Trim Knob + micro-label
        const int knobSz = std::min(shortBar ? 30 : 34, ctrlH + 2);
        auto kBox = area.removeFromRight(knobSz);
        loopLvlKnob.setBounds(kBox.withSizeKeepingCentre(knobSz, knobSz));
        area.removeFromRight(2);
        lvlTagRect = area.removeFromRight(compact ? 22 : 24);
        area.removeFromRight(compact ? 6 : 8);

        // Center: Interactive Studio Waveform Timeline
        waveformView.setBounds(area.withSizeKeepingCentre(area.getWidth(), std::min(34, ctrlH + 2)));
    }

private:
    PedalBoardAudioProcessor& processor;
    int recorderDividerX = 900;

    juce::Rectangle<int> looperTitleRect;
    juce::Rectangle<int> transportClusterRect;
    juce::Rectangle<int> lvlTagRect;
    juce::Rectangle<int> wavCapsuleRect;
    juce::Rectangle<int> wavInfoRect;

    DockIconButton recPlayDubBtn;
    DockIconButton stopBtn;
    DockIconButton undoBtn;
    DockIconButton clearBtn;
    LooperRoutingToggle routingToggle;

    LooperWaveformView waveformView;
    MidiBindableSlider loopLvlKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> loopLvlAttachment;

    DockIconButton wavRecBtn;
    DockIconButton wavFolderBtn;
#if defined(PEDALBOARD_TEMP_DI_TESTER) && PEDALBOARD_TEMP_DI_TESTER
    juce::Rectangle<int> tempDiCapsuleRect;
    DockIconButton tempDiPlayBtn;
    juce::TextButton tempDiRhythmBtn;
    juce::TextButton tempDiSoloBtn;
#endif
};

} // namespace pedalboard
