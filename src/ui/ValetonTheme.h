#pragma once

#include "../engine/ModuleId.h"
#include <algorithm>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard::valeton {

// Clean Valeton GP-200 reference visual language (matching ref/ screenshots 215546, 215608, 215612, 215659):
// - Vivid category-coded stylized mini pedal / amp / cab silhouettes with metallic side jack stubs
// - Top cobalt-blue ON / dark-slate OFF pill toggles & model dropdown bars
// - Clean minimalist 2-part stompbox enclosures (colored outer shell + recessed dark knob bay + raised front bumper)
// Paired with 100% AmpliTube-5 tactile 64-frame PBR knobs, sliders, and workflow!

inline juce::Colour stageCanvasBg() noexcept   { return juce::Colour(0xff1b1f27); }
inline juce::Colour chainStripBg() noexcept    { return juce::Colour(0xff232730); }
inline juce::Colour pillOnBlue() noexcept      { return juce::Colour(0xff2563eb); }
inline juce::Colour pillOffSlate() noexcept    { return juce::Colour(0xff3c414d); }
inline juce::Colour selectHaloBlue() noexcept  { return juce::Colour(0xff1d4ed8); }

// Exact signature palette from ref/ screenshots (215546, 215608, 215612)
inline juce::Colour colourFor(ModuleId id) noexcept {
    switch (id) {
        case ModuleId::NoiseGate:    return juce::Colour(0xffe6e9ef); // NR  - Crisp Silver-White
        case ModuleId::Compressor:   return juce::Colour(0xff1e78ff); // PRE/CMP - Electric Royal Blue
        case ModuleId::Wah:          return juce::Colour(0xfff5c518); // WAH - Bright Golden Yellow
        case ModuleId::Overdrive:    return juce::Colour(0xfff01e3c); // DST - Vibrant Crimson Red
        case ModuleId::AmpHead:      return juce::Colour(0xffd4b46a); // AMP - Gold Faceplate Accent
        case ModuleId::CabinetIR:    return juce::Colour(0xffd8dde6); // CAB - White Piping / Silver Accent
        case ModuleId::GraphicEq:    return juce::Colour(0xff17e2a5); // EQ  - Mint Turquoise Green
        case ModuleId::StereoChorus: return juce::Colour(0xff1ab5fe); // MOD - Vivid Sky-Cyan
        case ModuleId::StereoDelay:  return juce::Colour(0xffe02f75); // DLY - Vibrant Hot-Pink Magenta
        case ModuleId::Reverb:       return juce::Colour(0xfff69c1b); // RVB - Warm Tangerine Orange
        default:                     return juce::Colour(0xff94a3b8);
    }
}

inline const char* gpCodeFor(ModuleId id) noexcept {
    switch (id) {
        case ModuleId::NoiseGate:    return "NR";
        case ModuleId::Compressor:   return "PRE";
        case ModuleId::Wah:          return "WAH";
        case ModuleId::Overdrive:    return "DST";
        case ModuleId::AmpHead:      return "AMP";
        case ModuleId::CabinetIR:    return "CAB";
        case ModuleId::GraphicEq:    return "EQ";
        case ModuleId::StereoChorus: return "MOD";
        case ModuleId::StereoDelay:  return "DLY";
        case ModuleId::Reverb:       return "RVB";
        default:                     return "FX";
    }
}

// Clean matte charcoal studio backdrop with modern ambient stage lighting & brushed rails
inline void drawCleanStageBackdrop(juce::Graphics& g, juce::Rectangle<float> bounds) {
    juce::ColourGradient bg(
        juce::Colour(0xff1c2230), bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.25f,
        juce::Colour(0xff0d1017), bounds.getCentreX(), bounds.getBottom(),
        true
    );
    g.setGradientFill(bg);
    g.fillRect(bounds);

    // Subtle dark aluminum pedalboard mounting rails
    const float r1 = bounds.getY() + bounds.getHeight() * 0.26f;
    const float r2 = bounds.getY() + bounds.getHeight() * 0.72f;
    for (float ry : { r1, r2 }) {
        auto rail = juce::Rectangle<float>(bounds.getX() + 10.0f, ry - 5.0f, bounds.getWidth() - 20.0f, 10.0f);
        juce::ColourGradient rg(juce::Colour(0xff181d28), 0.0f, rail.getY(),
                                juce::Colour(0xff0f131b), 0.0f, rail.getBottom(), false);
        g.setGradientFill(rg);
        g.fillRoundedRectangle(rail, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawHorizontalLine(static_cast<int>(rail.getY()), rail.getX() + 4.0f, rail.getRight() - 4.0f);
    }

    // Fine engineering dot grid
    g.setColour(juce::Colours::white.withAlpha(0.022f));
    for (float y = bounds.getY() + 18.0f; y < bounds.getBottom() - 8.0f; y += 26.0f) {
        for (float x = bounds.getX() + 18.0f; x < bounds.getRight() - 8.0f; x += 26.0f) {
            g.fillEllipse(x, y, 1.5f, 1.5f);
        }
    }
}

// Clean Valeton 2-Part Hardware Chassis (for Amp Head, Cab Room, Rack units)
inline void drawCleanChassis(juce::Graphics& g,
                             juce::Rectangle<float> bounds,
                             juce::Colour accent,
                             bool active,
                             float radius = 10.0f,
                             float headerH = 28.0f) {
    // Soft ambient shadow + active glow
    g.setColour(juce::Colours::black.withAlpha(0.52f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 5.0f), radius);
    if (active) {
        g.setColour(accent.withAlpha(0.09f));
        g.fillRoundedRectangle(bounds.expanded(2.5f), radius + 2.0f);
    }

    const auto topCol = active ? juce::Colour(0xff202634) : juce::Colour(0xff161922);
    const auto botCol = active ? juce::Colour(0xff131720) : juce::Colour(0xff10131a);
    juce::ColourGradient bodyGrad(topCol, bounds.getX(), bounds.getY(),
                                  botCol, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bodyGrad);
    g.fillRoundedRectangle(bounds, radius);

    if (headerH > 0.0f) {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip;
        clip.addRoundedRectangle(bounds, radius);
        g.reduceClipRegion(clip);

        auto hdr = bounds.withHeight(headerH);
        const auto hCol = active ? accent : juce::Colour(0xff232833);
        juce::ColourGradient hdrGrad(hCol.brighter(active ? 0.10f : 0.04f).withAlpha(active ? 0.95f : 0.92f), hdr.getX(), hdr.getY(),
                                     hCol.darker(active ? 0.12f : 0.06f).withAlpha(active ? 0.95f : 0.92f), hdr.getX(), hdr.getBottom(), false);
        g.setGradientFill(hdrGrad);
        g.fillRect(hdr);
        g.setColour(juce::Colours::white.withAlpha(active ? 0.28f : 0.05f));
        g.fillRect(hdr.withHeight(1.2f));
    }

    g.setColour(active ? accent.withAlpha(0.62f) : juce::Colour(0xff262c38));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, active ? 1.4f : 1.0f);
}

// Modernized 2-Part Stompbox Enclosure matching ref/Screenshot_20261123_215659_Chrome.jpg:
// - Vibrant anodized outer frame with subtle active ambient glow
// - Deep recessed dark-glass upper control bay
// - Raised solid-color bottom bumper lip with recessed footswitch bezel
inline float pedalBumperHeight(float pedalHeight) noexcept {
    return std::clamp(pedalHeight * 0.23f, 56.0f, 88.0f);
}

inline void drawRefTwoPartPedalChassis(juce::Graphics& g,
                                       juce::Rectangle<float> bounds,
                                       juce::Colour bodyCol,
                                       bool active,
                                       float& outBayTop,
                                       float& outBayBottom,
                                       float& outBumperTop) {
    const float r = 11.5f;
    // When bypassed/turned OFF, use pure neutral dark graphite with ZERO color highlight!
    const auto col = active ? bodyCol : juce::Colour(0xff252a35);

    // Side metallic 1/4" jack stubs with chrome sheen
    const float jY = bounds.getY() + bounds.getHeight() * 0.44f;
    const float jH = 15.0f;
    juce::ColourGradient jackGrad(juce::Colour(0xffe2e8f0), 0.0f, jY - jH * 0.5f,
                                  juce::Colour(0xff64748b), 0.0f, jY + jH * 0.5f, false);
    g.setGradientFill(jackGrad);
    g.setOpacity(active ? 0.90f : 0.40f);
    g.fillRoundedRectangle(bounds.getX() - 3.8f, jY - jH * 0.5f, 5.2f, jH, 1.8f);
    g.fillRoundedRectangle(bounds.getRight() - 1.4f, jY - jH * 0.5f, 5.2f, jH, 1.8f);
    g.setOpacity(1.0f);

    // Ambient colored under-glow when pedal is engaged + deep drop shadow
    if (active) {
        g.setColour(col.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds.expanded(2.5f).translated(0.0f, 3.0f), r + 2.0f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.56f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 5.5f), r);

    // Molded anodized outer body with smooth 3D gradient sheen
    juce::ColourGradient shellGrad(col.brighter(0.18f), bounds.getX(), bounds.getY(),
                                   col.darker(0.22f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(shellGrad);
    g.fillRoundedRectangle(bounds, r);

    // Crisp outer specular rim highlight
    g.setColour(juce::Colours::white.withAlpha(active ? 0.34f : 0.10f));
    g.drawRoundedRectangle(bounds.reduced(0.6f), r, 1.2f);

    // Upper Recessed Dark-Glass Control Bay (proportional bumper height for all window sizes!)
    const float padX = (bounds.getWidth() < 110.0f) ? 4.5f : 6.5f;
    outBayTop = bounds.getY() + 8.0f;
    outBumperTop = bounds.getBottom() - pedalBumperHeight(bounds.getHeight());
    outBayBottom = outBumperTop - 5.0f;

    auto bay = juce::Rectangle<float>(bounds.getX() + padX, outBayTop,
                                      bounds.getWidth() - padX * 2.0f,
                                      std::max(40.0f, outBayBottom - outBayTop));
    juce::ColourGradient bayGrad(juce::Colour(0xff141923), bay.getX(), bay.getY(),
                                 juce::Colour(0xff0c0f16), bay.getX(), bay.getBottom(), false);
    g.setGradientFill(bayGrad);
    g.fillRoundedRectangle(bay, 7.5f);
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawRoundedRectangle(bay.reduced(0.5f), 7.5f, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    g.drawRoundedRectangle(bay.expanded(0.5f), 8.0f, 0.8f);

    // Raised Front Step / Bumper Lip with sleek top highlight
    auto lip = juce::Rectangle<float>(bounds.getX() + 3.0f, outBumperTop,
                                      bounds.getWidth() - 6.0f, bounds.getBottom() - outBumperTop - 3.0f);
    juce::ColourGradient lipGrad(col.brighter(0.22f), lip.getX(), lip.getY(),
                                 col.darker(0.16f), lip.getX(), lip.getBottom(), false);
    g.setGradientFill(lipGrad);
    g.fillRoundedRectangle(lip, 8.0f);
    g.setColour(juce::Colours::white.withAlpha(active ? 0.38f : 0.10f));
    g.drawHorizontalLine(static_cast<int>(lip.getY() + 1.0f), lip.getX() + 7.0f, lip.getRight() - 7.0f);
}

// Helper for crisp geometric hardware glyph inside AMP/CAB/Rack headers
inline void drawModuleGlyph(juce::Graphics& g,
                            juce::Rectangle<float> area,
                            ModuleId mod,
                            juce::Colour col) {
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float s = std::min(area.getWidth(), area.getHeight()) * 0.42f;
    g.setColour(col);

    switch (mod) {
        case ModuleId::NoiseGate: {
            juce::Path p;
            p.startNewSubPath(cx - s, cy);
            p.lineTo(cx - s * 0.72f, cy - s * 0.45f);
            p.lineTo(cx - s * 0.48f, cy + s * 0.50f);
            p.lineTo(cx - s * 0.24f, cy - s * 0.35f);
            p.lineTo(cx - s * 0.02f, cy + s * 0.18f);
            p.lineTo(cx + s * 0.14f, cy);
            p.lineTo(cx + s, cy);
            g.strokePath(p, juce::PathStrokeType(1.6f));
            break;
        }
        case ModuleId::Compressor: {
            g.drawEllipse(cx - s * 0.65f, cy - s * 0.65f, s * 1.3f, s * 1.3f, 1.6f);
            g.drawLine(cx, cy - s * 0.60f, cx, cy, 1.8f);
            break;
        }
        case ModuleId::Wah: {
            juce::Path p;
            p.startNewSubPath(cx - s, cy + s * 0.45f);
            p.quadraticTo(cx - s * 0.25f, cy + s * 0.45f, cx, cy - s * 0.65f);
            p.quadraticTo(cx + s * 0.25f, cy + s * 0.45f, cx + s, cy + s * 0.45f);
            g.strokePath(p, juce::PathStrokeType(1.8f));
            break;
        }
        case ModuleId::Overdrive: {
            juce::Path p;
            const float pts[9] = { 0.0f, -0.55f, 0.55f, -0.75f, 0.70f, -0.60f, 0.55f, -0.35f, 0.0f };
            for (int i = 0; i < 9; ++i) {
                const float x = cx - s + (static_cast<float>(i) / 8.0f) * 2.0f * s;
                const float y = cy + pts[i] * s * 0.85f;
                if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
            }
            g.strokePath(p, juce::PathStrokeType(1.5f));
            break;
        }
        case ModuleId::AmpHead: {
            auto box = juce::Rectangle<float>(cx - s * 1.1f, cy - s * 0.45f, s * 2.2f, s * 1.05f);
            g.drawRoundedRectangle(box, 2.2f, 1.5f);
            g.drawRoundedRectangle(cx - s * 0.42f, box.getY() - s * 0.26f, s * 0.84f, s * 0.26f, 1.2f, 1.2f);
            break;
        }
        case ModuleId::CabinetIR: {
            auto box = juce::Rectangle<float>(cx - s * 0.85f, cy - s * 0.78f, s * 1.70f, s * 1.56f);
            g.drawRoundedRectangle(box, 2.2f, 1.5f);
            g.drawEllipse(cx - s * 0.48f, cy - s * 0.48f, s * 0.96f, s * 0.96f, 1.3f);
            g.fillEllipse(cx - s * 0.16f, cy - s * 0.16f, s * 0.32f, s * 0.32f);
            break;
        }
        case ModuleId::GraphicEq: {
            const float pos[3] = { -0.25f, 0.25f, -0.10f };
            for (int i = 0; i < 3; ++i) {
                const float x = cx + (static_cast<float>(i) - 1.0f) * s * 0.65f;
                g.drawLine(x, cy - s * 0.60f, x, cy + s * 0.60f, 1.3f);
                g.fillRoundedRectangle(x - 2.4f, cy + pos[i] * s - 2.0f, 4.8f, 4.0f, 1.2f);
            }
            break;
        }
        case ModuleId::StereoChorus: {
            juce::Path w;
            for (int i = 0; i <= 20; ++i) {
                const float t = static_cast<float>(i) / 20.0f;
                const float x = cx - s + t * 2.0f * s;
                const float y = cy + std::sin(t * 18.8495f) * s * 0.42f;
                if (i == 0) w.startNewSubPath(x, y); else w.lineTo(x, y);
            }
            g.strokePath(w, juce::PathStrokeType(1.5f));
            break;
        }
        case ModuleId::StereoDelay: {
            for (int i = 0; i < 5; ++i) {
                const float x = cx - s * 0.8f + static_cast<float>(i) * (s * 0.40f);
                const float bh = s * (0.35f + 0.16f * static_cast<float>(i));
                g.fillRoundedRectangle(x - 1.4f, cy - bh * 0.5f, 2.8f, bh, 1.0f);
            }
            break;
        }
        case ModuleId::Reverb: {
            g.fillEllipse(cx - 2.4f, cy - 2.4f, 4.8f, 4.8f);
            for (int i = 1; i <= 2; ++i) {
                const float rx = s * (0.38f * static_cast<float>(i));
                const float ry = s * (0.30f * static_cast<float>(i));
                g.drawEllipse(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f, 1.2f);
            }
            break;
        }
        default: break;
    }
}

// ==============================================================================
// Exact Stylized Mini Pedal / Amp / Cab Vector Icons matching ref/215546, 215608, 215612!
// ==============================================================================
inline void drawStylizedPedalIcon(juce::Graphics& g,
                                  juce::Rectangle<float> bounds,
                                  ModuleId mod,
                                  bool active,
                                  juce::Colour customAccent = juce::Colours::transparentBlack,
                                  const juce::String& codeOverride = {}) {
    const auto baseCol = customAccent.isTransparent() ? colourFor(mod) : customAccent;
    const auto code = codeOverride.isNotEmpty() ? codeOverride : juce::String(gpCodeFor(mod));

    // When bypassed, dim smoothly to ~36% brightness just like ref/215546!
    const auto col = active ? baseCol
                            : baseCol.withMultipliedSaturation(0.55f).withMultipliedBrightness(0.32f);

    // 1. WAH: Tall tapered CryBaby yellow rocker with stacked "W A H" (matching 215608 & 215612!)
    if (mod == ModuleId::Wah) {
        const float h = bounds.getHeight();
        const float w = std::min(bounds.getWidth() * 0.68f, h * 0.56f);
        const float cx = bounds.getCentreX();
        const float y0 = bounds.getY() + 1.0f;
        const float y1 = bounds.getBottom() - 1.0f;

        // Side metallic 1/4" jack stubs
        g.setColour(juce::Colour(active ? 0xffcbd5e1 : 0xff525a6a));
        g.fillRoundedRectangle(cx - w * 0.56f, bounds.getCentreY() - 4.0f, w * 1.12f, 8.0f, 2.0f);

        // Tapered yellow cast body
        juce::Path body;
        body.startNewSubPath(cx - w * 0.50f, y0 + 3.0f);
        body.lineTo(cx + w * 0.50f, y0 + 3.0f);
        body.lineTo(cx + w * 0.42f, y1 - 2.0f);
        body.lineTo(cx - w * 0.42f, y1 - 2.0f);
        body.closeSubPath();

        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillPath(body, juce::AffineTransform::translation(0.0f, 2.0f));

        juce::ColourGradient grad(col.brighter(0.14f), cx, y0, col.darker(0.16f), cx, y1, false);
        g.setGradientFill(grad);
        g.fillPath(body);
        g.setColour(col.darker(0.30f));
        g.strokePath(body, juce::PathStrokeType(1.2f));

        // Inner dark ribbed rubber rocker pad
        juce::Path pad;
        pad.startNewSubPath(cx - w * 0.34f, y0 + 6.5f);
        pad.lineTo(cx + w * 0.34f, y0 + 6.5f);
        pad.lineTo(cx + w * 0.27f, y1 - 5.5f);
        pad.lineTo(cx - w * 0.27f, y1 - 5.5f);
        pad.closeSubPath();
        g.setColour(juce::Colour(0xff14161b));
        g.fillPath(pad);

        // Stacked bold "W", "A", "H"
        g.setColour(active ? juce::Colours::white : juce::Colour(0xff6b7280));
        const float fontH = std::clamp(h * 0.21f, 8.5f, 14.0f);
        g.setFont(juce::Font(juce::FontOptions(fontH, juce::Font::bold)));
        const float innerH = (y1 - y0 - 14.0f) / 3.0f;
        const char* letters[3] = { "W", "A", "H" };
        for (int i = 0; i < 3; ++i) {
            g.drawText(letters[i],
                       juce::Rectangle<float>(cx - w * 0.35f, y0 + 7.0f + static_cast<float>(i) * innerH, w * 0.70f, innerH),
                       juce::Justification::centred);
        }
        return;
    }

    // 2. AMP: Compact horizontal dark amp head with carry handle, "AMP" grill, cream-gold panel + red switch & 3 knobs!
    if (mod == ModuleId::AmpHead) {
        const float h = std::min(bounds.getHeight() * 0.74f, bounds.getWidth() * 0.64f);
        const float w = std::min(bounds.getWidth() * 0.94f, h * 1.48f);
        auto box = juce::Rectangle<float>(bounds.getCentreX() - w * 0.5f,
                                          bounds.getCentreY() - h * 0.5f + 2.0f,
                                          w, h);

        // Top carry handle
        g.setColour(juce::Colour(active ? 0xff474e5e : 0xff282c36));
        g.drawRoundedRectangle(box.getCentreX() - w * 0.22f, box.getY() - 4.0f, w * 0.44f, 5.0f, 2.0f, 1.5f);

        // Drop shadow + dark amp shell
        g.setColour(juce::Colours::black.withAlpha(0.50f));
        g.fillRoundedRectangle(box.translated(0.0f, 2.0f), 4.5f);
        g.setColour(juce::Colour(active ? 0xff1d2027 : 0xff13151b));
        g.fillRoundedRectangle(box, 4.5f);
        g.setColour(col.withAlpha(active ? 0.85f : 0.32f));
        g.drawRoundedRectangle(box.reduced(0.6f), 4.5f, 1.2f);

        // Inner dark grille ("AMP")
        auto inner = box.reduced(3.2f, 3.2f);
        auto topGrill = inner.withHeight(inner.getHeight() * 0.56f);
        g.setColour(juce::Colour(0xff101217));
        g.fillRoundedRectangle(topGrill, 2.0f);
        g.setColour(active ? juce::Colours::white : juce::Colour(0xff6b7280));
        g.setFont(juce::Font(juce::FontOptions(std::clamp(h * 0.31f, 8.5f, 13.0f), juce::Font::bold)));
        g.drawText(code, topGrill, juce::Justification::centred);

        // Lower warm gold / channel-colored faceplate with tiny red power rocker & 3 circular knobs!
        auto face = inner.withTop(topGrill.getBottom() + 1.5f);
        g.setColour(col);
        g.fillRoundedRectangle(face, 2.0f);

        // Tiny power switch on left (neutral dark unlit when bypassed!)
        const float swH = std::max(4.0f, face.getHeight() * 0.56f);
        g.setColour(active ? juce::Colour(0xffef4444) : juce::Colour(0xff3a404d));
        g.fillRoundedRectangle(face.getX() + 3.0f, face.getCentreY() - swH * 0.5f, swH * 0.8f, swH, 1.0f);

        // 3 mini circular silver knobs
        const float knD = std::max(4.0f, face.getHeight() * 0.54f);
        const float startX = face.getX() + swH + 7.0f;
        const float span = std::max(10.0f, face.getRight() - startX - 4.0f);
        for (int k = 0; k < 3; ++k) {
            const float kx = startX + (static_cast<float>(k) + 0.5f) * (span / 3.0f);
            g.setColour(juce::Colour(0xff1b1e24));
            g.fillEllipse(kx - knD * 0.5f, face.getCentreY() - knD * 0.5f, knD, knD);
            g.setColour(active ? juce::Colour(0xffe2e8f0) : juce::Colour(0xff64748b));
            g.fillEllipse(kx - knD * 0.32f, face.getCentreY() - knD * 0.32f, knD * 0.64f, knD * 0.64f);
        }
        return;
    }

    // 3. CAB: Square Matte-Black Speaker Cabinet with Crisp White Piping + 4 Silver Corner Brackets + "CAB"!
    if (mod == ModuleId::CabinetIR) {
        const float sz = std::min(bounds.getWidth() * 0.86f, bounds.getHeight() * 0.86f);
        auto box = juce::Rectangle<float>(bounds.getCentreX() - sz * 0.5f,
                                          bounds.getCentreY() - sz * 0.5f,
                                          sz, sz);

        g.setColour(juce::Colours::black.withAlpha(0.50f));
        g.fillRoundedRectangle(box.translated(0.0f, 2.0f), 4.0f);
        g.setColour(juce::Colour(active ? 0xff1e2128 : 0xff13151b));
        g.fillRoundedRectangle(box, 4.0f);

        // Crisp white/accent inner piping frame
        auto frame = box.reduced(3.8f);
        g.setColour(juce::Colour(0xff111318));
        g.fillRoundedRectangle(frame, 2.0f);
        g.setColour(col.withAlpha(active ? 0.92f : 0.35f));
        g.drawRoundedRectangle(frame, 2.0f, 1.5f);

        // 4 Metallic corner brackets
        g.setColour(juce::Colour(active ? 0xffcbd5e1 : 0xff4b5563));
        const float cSz = 4.2f;
        g.fillRect(box.getX(), box.getY(), cSz, cSz);
        g.fillRect(box.getRight() - cSz, box.getY(), cSz, cSz);
        g.fillRect(box.getX(), box.getBottom() - cSz, cSz, cSz);
        g.fillRect(box.getRight() - cSz, box.getBottom() - cSz, cSz, cSz);

        g.setColour(active ? juce::Colours::white : juce::Colour(0xff6b7280));
        g.setFont(juce::Font(juce::FontOptions(std::clamp(sz * 0.25f, 8.5f, 13.0f), juce::Font::bold)));
        g.drawText(code, frame, juce::Justification::centred);
        return;
    }

    // 4. STANDARD COMPACT STOMPBOXES (NR White, PRE Blue, DST Red, EQ Mint, MOD Cyan, DLY Pink, RVB Orange)
    // Exact silhouette from ref/215608 & 215612: rounded body + silver side jack stubs + upper graphic + lower footswitch pad!
    const float h = bounds.getHeight() * 0.90f;
    const float w = std::min(bounds.getWidth() * 0.78f, h * 0.76f);
    auto body = juce::Rectangle<float>(bounds.getCentreX() - w * 0.5f,
                                       bounds.getCentreY() - h * 0.5f,
                                       w, h);

    // Side metallic 1/4" jack plugs protruding left & right!
    g.setColour(juce::Colour(active ? 0xffd1d5db : 0xff4b5563));
    const float jH = std::max(6.0f, h * 0.16f);
    g.fillRoundedRectangle(body.getX() - 3.2f, body.getCentreY() - jH * 0.5f, body.getWidth() + 6.4f, jH, 2.0f);

    // Drop shadow + vibrant molded pedal body
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(body.translated(0.0f, 2.0f), 6.0f);

    juce::ColourGradient bodyGrad(col.brighter(0.12f), body.getX(), body.getY(),
                                  col.darker(0.14f), body.getX(), body.getBottom(), false);
    g.setGradientFill(bodyGrad);
    g.fillRoundedRectangle(body, 6.0f);
    g.setColour(juce::Colours::white.withAlpha(active ? 0.28f : 0.08f));
    g.drawRoundedRectangle(body.reduced(0.5f), 6.0f, 0.9f);

    // Upper graphic zone (top 50% of pedal)
    auto topZone = body.withHeight(body.getHeight() * 0.50f).reduced(4.0f, 4.0f);
    const bool isLightBody = (mod == ModuleId::NoiseGate);
    const auto inkCol = isLightBody
                            ? juce::Colour(active ? 0xff3b4252 : 0xff1e222b)
                            : col.darker(0.55f);

    if (mod == ModuleId::Compressor) {
        // Electric Blue PRE/CMP: crisp circular light-blue knob icon at top (neutral dark gray when bypassed!)
        const float kd = std::min(topZone.getWidth(), topZone.getHeight()) * 0.76f;
        auto kr = juce::Rectangle<float>(topZone.getCentreX() - kd * 0.5f, topZone.getCentreY() - kd * 0.5f, kd, kd);
        g.setColour(active ? juce::Colour(0xff93c5fd) : juce::Colour(0xff3a404d));
        g.fillEllipse(kr);
        g.setColour(active ? juce::Colours::white : juce::Colour(0xff64748b));
        g.drawEllipse(kr, 1.0f);
        g.fillRoundedRectangle(kr.getCentreX() - 1.3f, kr.getY() + 1.5f, 2.6f, kd * 0.44f, 1.0f);
    } else if (mod == ModuleId::GraphicEq) {
        // Mint Turquoise EQ: 3 vertical dark slots with crisp white slider caps (neutral dark gray when bypassed!)
        const float capOffsets[3] = { -0.18f, 0.16f, -0.06f };
        for (int i = 0; i < 3; ++i) {
            const float sx = topZone.getX() + (static_cast<float>(i) + 0.5f) * (topZone.getWidth() / 3.0f);
            g.setColour(active ? juce::Colour(0xff0b1f19) : juce::Colour(0xff14171f));
            g.fillRoundedRectangle(sx - 1.2f, topZone.getY() + 1.5f, 2.4f, topZone.getHeight() - 3.0f, 1.0f);
            g.setColour(active ? juce::Colours::white : juce::Colour(0xff64748b));
            const float cy = topZone.getCentreY() + capOffsets[i] * topZone.getHeight();
            g.fillRoundedRectangle(sx - 2.6f, cy - 3.0f, 5.2f, 6.0f, 1.5f);
        }
    } else if (mod == ModuleId::StereoDelay) {
        // Hot-Pink DLY: 6 crisp vertical echo bars (matching 215612!)
        const int nBars = 6;
        const float step = topZone.getWidth() / static_cast<float>(nBars);
        g.setColour(inkCol);
        for (int i = 0; i < nBars; ++i) {
            const float bx = topZone.getX() + (static_cast<float>(i) + 0.5f) * step;
            const float bh = topZone.getHeight() * (0.34f + 0.11f * static_cast<float>(i));
            g.fillRoundedRectangle(bx - 1.1f, topZone.getCentreY() - bh * 0.5f, 2.2f, bh, 0.8f);
        }
    } else if (mod == ModuleId::NoiseGate) {
        // Crisp Silver-White NR: noisy spike fading to flat line (matching 215608!)
        juce::Path p;
        const float yMid = topZone.getCentreY();
        const float amp = topZone.getHeight() * 0.44f;
        const float xs[9] = { 0.0f, 0.10f, 0.20f, 0.28f, 0.36f, 0.44f, 0.54f, 0.66f, 1.0f };
        const float ys[9] = { 0.0f, -0.75f, 0.85f, -0.95f, 0.65f, -0.35f, 0.12f, 0.0f, 0.0f };
        for (int i = 0; i < 9; ++i) {
            const float px = topZone.getX() + xs[i] * topZone.getWidth();
            const float py = yMid + ys[i] * amp;
            if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
        }
        g.setColour(inkCol);
        g.strokePath(p, juce::PathStrokeType(1.3f));
    } else if (mod == ModuleId::Overdrive) {
        // Crimson Red DST: dense jagged distortion waveform (matching 215608!)
        juce::Path p;
        const float yMid = topZone.getCentreY();
        const float amp = topZone.getHeight() * 0.45f;
        const float ys[13] = { 0.2f, -0.8f, 0.9f, -0.95f, 0.6f, -0.85f, 0.95f, -0.75f, 0.85f, -0.9f, 0.7f, -0.5f, 0.1f };
        for (int i = 0; i < 13; ++i) {
            const float px = topZone.getX() + (static_cast<float>(i) / 12.0f) * topZone.getWidth();
            const float py = yMid + ys[i] * amp;
            if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
        }
        g.setColour(inkCol);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    } else if (mod == ModuleId::StereoChorus) {
        // Sky-Cyan MOD: clean repeating modulation wave (matching 215608!)
        juce::Path wPath;
        const float yMid = topZone.getCentreY();
        const float amp = topZone.getHeight() * 0.38f;
        for (int i = 0; i <= 24; ++i) {
            const float t = static_cast<float>(i) / 24.0f;
            const float px = topZone.getX() + t * topZone.getWidth();
            const float py = yMid + std::sin(t * 25.1327f) * amp;
            if (i == 0) wPath.startNewSubPath(px, py); else wPath.lineTo(px, py);
        }
        g.setColour(inkCol);
        g.strokePath(wPath, juce::PathStrokeType(1.3f));
    } else {
        // Tangerine Orange RVB: concentric reverberation sound waves (matching 215546!)
        const float cx = topZone.getCentreX();
        const float cy = topZone.getCentreY();
        g.setColour(inkCol);
        g.fillEllipse(cx - 2.2f, cy - 2.2f, 4.4f, 4.4f);
        for (int i = 1; i <= 3; ++i) {
            const float rx = topZone.getWidth() * (0.14f * static_cast<float>(i));
            const float ry = topZone.getHeight() * (0.24f * static_cast<float>(i));
            g.drawEllipse(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f, 1.1f);
        }
    }

    // Lower Raised Footswitch Pad (dark charcoal embossed pad with bold code — exact ref/215546 look!)
    auto pad = body.withTop(body.getY() + body.getHeight() * 0.52f).reduced(3.5f, 3.0f);
    g.setColour(juce::Colour(active ? 0xff1c2029 : 0xff12151b));
    g.fillRoundedRectangle(pad, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(active ? 0.16f : 0.06f));
    g.drawRoundedRectangle(pad, 4.0f, 0.8f);

    g.setColour(active ? juce::Colours::white : juce::Colour(0xff6b7280));
    g.setFont(juce::Font(juce::FontOptions(std::clamp(pad.getHeight() * 0.56f, 8.5f, 12.5f), juce::Font::bold)));
    g.drawText(code, pad, juce::Justification::centred);
}

} // namespace pedalboard::valeton
