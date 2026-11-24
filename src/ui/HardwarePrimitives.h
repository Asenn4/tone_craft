#pragma once

#include <algorithm>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard::hw {

// Procedural skeuomorphic studio hardware rendering primitives (100% Native JUCE Graphics).

inline void drawScrew(juce::Graphics& g, float cx, float cy, float r) {
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillEllipse(cx - r, cy - r + 1.0f, r * 2.0f, r * 2.0f);

    juce::ColourGradient grad(
        juce::Colour(0xff94a3b8), cx - r * 0.6f, cy - r * 0.6f,
        juce::Colour(0xff1e2430), cx + r * 0.7f, cy + r * 0.7f,
        false
    );
    g.setGradientFill(grad);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

    g.setColour(juce::Colour(0xff0b0e14).withAlpha(0.85f));
    g.drawLine(cx - r * 0.55f, cy - r * 0.55f, cx + r * 0.55f, cy + r * 0.55f, std::max(1.0f, r * 0.28f));
    g.drawLine(cx + r * 0.45f, cy - r * 0.45f, cx - r * 0.45f, cy + r * 0.45f, std::max(0.8f, r * 0.22f));
}

inline void drawTolexTexture(juce::Graphics& g,
                             juce::Rectangle<float> bounds,
                             float cornerRadius,
                             juce::Colour baseColour = juce::Colour(0xff17191e)) {
    // Deep drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 6.0f), cornerRadius + 1.0f);

    // Subtle leatherette gradient
    juce::ColourGradient grad(
        baseColour.brighter(0.08f), bounds.getX(), bounds.getY(),
        baseColour.darker(0.28f), bounds.getX(), bounds.getBottom(),
        false
    );
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, cornerRadius);

    // Deterministic leatherette pebble grain clipped to chassis
    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip;
        clip.addRoundedRectangle(bounds, cornerRadius);
        g.reduceClipRegion(clip);

        const float step = 7.0f;
        int row = 0;
        for (float y = bounds.getY(); y < bounds.getBottom(); y += step, ++row) {
            const float xOff = (row & 1) ? 3.5f : 0.0f;
            for (float x = bounds.getX() + xOff; x < bounds.getRight(); x += step) {
                const uint32_t h = static_cast<uint32_t>(static_cast<int>(x) * 73856093u ^ static_cast<int>(y) * 19349663u);
                const float alpha = ((h & 0x1Fu) / 31.0f) * 0.045f;
                g.setColour((h & 0x20u) ? juce::Colours::white.withAlpha(alpha)
                                        : juce::Colours::black.withAlpha(alpha * 1.4f));
                g.fillEllipse(x, y, 4.2f, 3.2f);
            }
        }
    }

    // Beveled rim
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.2f);
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawRoundedRectangle(bounds.reduced(1.5f), cornerRadius, 1.0f);
}

inline void drawBrushedMetal(juce::Graphics& g,
                             juce::Rectangle<float> bounds,
                             juce::Colour baseColour,
                             float cornerRadius,
                             bool horizontalGrain = true) {
    juce::ColourGradient grad(
        baseColour.brighter(0.22f), bounds.getX(), bounds.getY(),
        baseColour.darker(0.22f), bounds.getX(), bounds.getBottom(),
        false
    );
    grad.addColour(0.35, baseColour.brighter(0.06f));
    grad.addColour(0.65, baseColour.darker(0.08f));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, cornerRadius);

    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip;
        clip.addRoundedRectangle(bounds, cornerRadius);
        g.reduceClipRegion(clip);

        if (horizontalGrain) {
            for (float y = bounds.getY(); y < bounds.getBottom(); y += 2.0f) {
                const int yi = static_cast<int>(y);
                const float a = ((yi * 37) % 11) * 0.0045f;
                g.setColour((yi & 1) ? juce::Colours::white.withAlpha(a) : juce::Colours::black.withAlpha(a));
                g.drawHorizontalLine(yi, bounds.getX(), bounds.getRight());
            }
        } else {
            for (float x = bounds.getX(); x < bounds.getRight(); x += 2.0f) {
                const int xi = static_cast<int>(x);
                const float a = ((xi * 29) % 11) * 0.0045f;
                g.setColour((xi & 1) ? juce::Colours::white.withAlpha(a) : juce::Colours::black.withAlpha(a));
                g.drawVerticalLine(xi, bounds.getY(), bounds.getBottom());
            }
        }

        // Subtle diagonal studio light reflection
        juce::ColourGradient sheen(
            juce::Colours::white.withAlpha(0.11f), bounds.getX(), bounds.getY(),
            juce::Colours::transparentWhite, bounds.getX() + bounds.getWidth() * 0.55f, bounds.getBottom(),
            false
        );
        g.setGradientFill(sheen);
        g.fillRect(bounds);
    }

    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawRoundedRectangle(bounds.reduced(1.5f), cornerRadius, 1.0f);
}

inline void drawDiamondPlate(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius) {
    drawBrushedMetal(g, bounds, juce::Colour(0xff2a303c), cornerRadius, true);

    juce::Graphics::ScopedSaveState ss(g);
    juce::Path clip;
    clip.addRoundedRectangle(bounds.reduced(2.0f), cornerRadius);
    g.reduceClipRegion(clip);

    const float sp = 18.0f;
    int row = 0;
    for (float y = bounds.getY() + 6.0f; y < bounds.getBottom(); y += sp, ++row) {
        for (float x = bounds.getX() + 6.0f; x < bounds.getRight(); x += sp) {
            const bool flip = ((row + static_cast<int>(x / sp)) & 1) != 0;
            const float angle = flip ? 0.785398f : -0.785398f;
            juce::Path lug;
            lug.addEllipse(-5.2f, -1.6f, 10.4f, 3.2f);
            lug.applyTransform(juce::AffineTransform::rotation(angle).translated(x, y));

            g.setColour(juce::Colours::black.withAlpha(0.42f));
            g.fillPath(lug, juce::AffineTransform::translation(0.8f, 1.0f));
            g.setColour(juce::Colour(0xff525d70).withAlpha(0.55f));
            g.fillPath(lug);
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.strokePath(lug, juce::PathStrokeType(0.6f));
        }
    }
}

inline void drawMetalCornerProtector(juce::Graphics& g, juce::Rectangle<float> bounds, float sz = 20.0f) {
    auto drawOne = [&](float x, float y, float dx, float dy) {
        juce::Path p;
        p.startNewSubPath(x, y);
        p.lineTo(x + dx * sz, y);
        p.lineTo(x + dx * sz * 0.72f, y + dy * sz * 0.36f);
        p.lineTo(x + dx * sz * 0.36f, y + dy * sz * 0.72f);
        p.lineTo(x, y + dy * sz);
        p.closeSubPath();

        g.setColour(juce::Colour(0xff2c3340));
        g.fillPath(p);
        g.setColour(juce::Colours::white.withAlpha(0.24f));
        g.strokePath(p, juce::PathStrokeType(1.0f));
        drawScrew(g, x + dx * sz * 0.32f, y + dy * sz * 0.32f, 2.3f);
    };

    drawOne(bounds.getX(),     bounds.getY(),      1.0f,  1.0f);
    drawOne(bounds.getRight(), bounds.getY(),     -1.0f,  1.0f);
    drawOne(bounds.getX(),     bounds.getBottom(), 1.0f, -1.0f);
    drawOne(bounds.getRight(), bounds.getBottom(),-1.0f, -1.0f);
}

inline void drawJewelLamp(juce::Graphics& g, float cx, float cy, float r, juce::Colour colour, bool lit) {
    // Chrome bezel ring
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(cx - r * 1.35f, cy - r * 1.35f + 1.5f, r * 2.7f, r * 2.7f);
    juce::ColourGradient bezel(
        juce::Colour(0xffcbd5e1), cx - r, cy - r,
        juce::Colour(0xff334155), cx + r, cy + r,
        false
    );
    g.setGradientFill(bezel);
    g.fillEllipse(cx - r * 1.3f, cy - r * 1.3f, r * 2.6f, r * 2.6f);

    if (lit) {
        // Outer radial bloom
        juce::ColourGradient glow(
            colour.withAlpha(0.55f), cx, cy,
            colour.withAlpha(0.0f), cx + r * 3.0f, cy,
            true
        );
        g.setGradientFill(glow);
        g.fillEllipse(cx - r * 3.0f, cy - r * 3.0f, r * 6.0f, r * 6.0f);

        // Faceted jewel lens
        juce::ColourGradient lens(
            juce::Colours::white, cx - r * 0.3f, cy - r * 0.3f,
            colour.darker(0.3f), cx + r * 0.8f, cy + r * 0.8f,
            true
        );
        lens.addColour(0.45, colour.brighter(0.35f));
        g.setGradientFill(lens);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    } else {
        g.setColour(colour.withMultipliedSaturation(0.35f).withMultipliedBrightness(0.22f));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    // Specular jewel facet lines
    g.setColour(juce::Colours::white.withAlpha(lit ? 0.55f : 0.16f));
    g.drawEllipse(cx - r * 0.52f, cy - r * 0.52f, r * 1.04f, r * 1.04f, 0.8f);
}

inline void drawVacuumTube(juce::Graphics& g, juce::Rectangle<float> area, float glowIntensity, bool active) {
    const float w = std::min(area.getWidth(), 34.0f);
    const float h = area.getHeight();
    auto r = juce::Rectangle<float>(area.getCentreX() - w * 0.5f, area.getY(), w, h);

    // Ceramic socket base
    auto baseRect = r.removeFromBottom(h * 0.22f).reduced(2.0f, 0.0f);
    g.setColour(juce::Colour(0xff232832));
    g.fillRoundedRectangle(baseRect, 2.5f);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(baseRect, 2.5f, 0.8f);

    // Glass envelope
    auto glass = r.reduced(3.0f, 0.0f);
    const float glow = active ? std::clamp(0.45f + glowIntensity * 0.55f, 0.0f, 1.0f) : 0.0f;

    if (glow > 0.01f) {
        juce::ColourGradient halo(
            juce::Colour(0xffff7a1a).withAlpha(0.38f * glow), glass.getCentreX(), glass.getCentreY(),
            juce::Colours::transparentBlack, glass.getCentreX() + w * 1.4f, glass.getCentreY(),
            true
        );
        g.setGradientFill(halo);
        g.fillEllipse(glass.expanded(w * 0.85f, 8.0f));
    }

    // Glass body
    g.setColour(juce::Colour(0xff1e232c).withAlpha(0.75f));
    g.fillRoundedRectangle(glass, glass.getWidth() * 0.45f);

    // Silver getter flash at top dome
    auto dome = glass.withHeight(glass.getHeight() * 0.26f);
    juce::ColourGradient getter(
        juce::Colour(0xff94a3b8).withAlpha(0.45f), dome.getX(), dome.getY(),
        juce::Colours::transparentBlack, dome.getX(), dome.getBottom(),
        false
    );
    g.setGradientFill(getter);
    g.fillRoundedRectangle(dome, glass.getWidth() * 0.45f);

    // Internal anode plate structure
    auto plate = glass.reduced(glass.getWidth() * 0.24f, glass.getHeight() * 0.22f);
    g.setColour(juce::Colour(0xff3b4250));
    g.fillRoundedRectangle(plate, 2.0f);

    // Glowing orange heater filament top & bottom
    if (glow > 0.01f) {
        const auto filCol = juce::Colour(0xffff9b2e).withAlpha(glow);
        g.setColour(filCol);
        g.fillEllipse(glass.getCentreX() - 3.5f, plate.getY() - 2.0f, 7.0f, 5.0f);
        g.fillEllipse(glass.getCentreX() - 4.0f, plate.getBottom() - 2.0f, 8.0f, 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.75f * glow));
        g.fillEllipse(glass.getCentreX() - 1.5f, plate.getBottom() - 0.5f, 3.0f, 3.0f);
    }

    // Left glass highlight streak
    g.setColour(juce::Colours::white.withAlpha(0.24f));
    g.fillRoundedRectangle(glass.getX() + 3.0f, glass.getY() + 6.0f, 2.2f, glass.getHeight() - 12.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.drawRoundedRectangle(glass, glass.getWidth() * 0.45f, 1.0f);
}

inline void drawSpeakerGrillCloth(juce::Graphics& g,
                                  juce::Rectangle<float> bounds,
                                  int cols,
                                  int rows,
                                  juce::Colour clothBase,
                                  juce::Colour weaveAccent) {
    g.setColour(clothBase);
    g.fillRoundedRectangle(bounds, 4.0f);

    juce::Graphics::ScopedSaveState ss(g);
    juce::Path clip;
    clip.addRoundedRectangle(bounds, 4.0f);
    g.reduceClipRegion(clip);

    // Speaker cones subtly visible behind grill cloth
    const float cellW = bounds.getWidth() / static_cast<float>(cols);
    const float cellH = bounds.getHeight() / static_cast<float>(rows);
    const float coneR = std::min(cellW, cellH) * 0.41f;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const float cx = bounds.getX() + (static_cast<float>(c) + 0.5f) * cellW;
            const float cy = bounds.getY() + (static_cast<float>(r) + 0.5f) * cellH;

            juce::ColourGradient coneGrad(
                juce::Colours::black.withAlpha(0.65f), cx, cy,
                juce::Colours::black.withAlpha(0.10f), cx + coneR, cy,
                true
            );
            coneGrad.addColour(0.62, juce::Colours::black.withAlpha(0.45f));
            g.setGradientFill(coneGrad);
            g.fillEllipse(cx - coneR, cy - coneR, coneR * 2.0f, coneR * 2.0f);

            // Dust cap ring
            const float capR = coneR * 0.30f;
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.drawEllipse(cx - coneR * 0.85f, cy - coneR * 0.85f, coneR * 1.7f, coneR * 1.7f, 1.5f);
        }
    }

    // Basket-weave grill threads
    g.setColour(weaveAccent.withAlpha(0.18f));
    for (float y = bounds.getY(); y < bounds.getBottom(); y += 4.0f) {
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    for (float x = bounds.getX(); x < bounds.getRight(); x += 5.0f) {
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Inner recess shadow
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 3.0f);
}

inline void drawFootswitchStomp(juce::Graphics& g, float cx, float cy, float r, bool hovered) {
    // Shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(cx - r * 1.15f, cy - r * 1.15f + 2.5f, r * 2.3f, r * 2.3f);

    // Hex nut base
    juce::Path hex;
    for (int i = 0; i < 6; ++i) {
        const float a = static_cast<float>(i) * 1.0471975f;
        const float px = cx + std::cos(a) * r * 1.18f;
        const float py = cy + std::sin(a) * r * 1.18f;
        if (i == 0) hex.startNewSubPath(px, py);
        else hex.lineTo(px, py);
    }
    hex.closeSubPath();
    g.setColour(juce::Colour(0xff64748b));
    g.fillPath(hex);
    g.setColour(juce::Colours::white.withAlpha(0.32f));
    g.strokePath(hex, juce::PathStrokeType(1.0f));

    // Chrome plunger button
    juce::ColourGradient cap(
        juce::Colours::white.withAlpha(hovered ? 0.98f : 0.88f), cx - r * 0.45f, cy - r * 0.45f,
        juce::Colour(0xff475569), cx + r * 0.7f, cy + r * 0.7f,
        true
    );
    g.setGradientFill(cap);
    g.fillEllipse(cx - r * 0.75f, cy - r * 0.75f, r * 1.5f, r * 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawEllipse(cx - r * 0.75f, cy - r * 0.75f, r * 1.5f, r * 1.5f, 1.0f);
}

inline void drawPatchCable(juce::Graphics& g,
                           juce::Point<float> p1,
                           juce::Point<float> p2,
                           juce::Colour cableCol = juce::Colour(0xff232730)) {
    const float midX = (p1.x + p2.x) * 0.5f;
    const float sag = 10.0f;

    juce::Path cable;
    cable.startNewSubPath(p1);
    cable.cubicTo(midX, p1.y + sag, midX, p2.y + sag, p2.x, p2.y);

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.strokePath(cable, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::translation(0.0f, 2.5f));
    g.setColour(cableCol);
    g.strokePath(cable, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.strokePath(cable, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::translation(0.0f, -1.0f));

    // Chrome jack plug caps
    auto drawPlug = [&](juce::Point<float> pt) {
        g.setColour(juce::Colour(0xff94a3b8));
        g.fillRoundedRectangle(pt.x - 4.0f, pt.y - 6.0f, 8.0f, 12.0f, 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawRoundedRectangle(pt.x - 4.0f, pt.y - 6.0f, 8.0f, 12.0f, 2.0f, 0.8f);
    };
    drawPlug(p1);
    drawPlug(p2);
}

} // namespace pedalboard::hw
