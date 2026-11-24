#pragma once

#include "SkinManager.h"

#include <algorithm>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard {

// Modern Flagship Dark-Glass LookAndFeel (v1.0) paired with 64-frame 3D PBR rotary knobs & fader caps:
// - Glowing LED progress arc around 3D PBR knobs
// - Soft dark-glass pill value readouts (no harsh white rectangle outlines!)
// - Sleek pill-rounded dark-glass ComboBoxes with vector chevron arrows
// - Modern dark-glass PopupMenus & gradient pill buttons
class PedalLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PedalLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff0d1016));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff141923));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2c3548));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xfff1f5f9));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff94a3b8));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff131722));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe2e8f0));
        setColour(juce::PopupMenu::headerTextColourId, juce::Colour(0xff60a5fa));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2563eb));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2433));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2563eb));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe2e8f0));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override {
        const auto bounds = juce::Rectangle<float>(
            static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(width), static_cast<float>(height)
        ).reduced(1.5f);

        const float diameter = std::min(bounds.getWidth(), bounds.getHeight());
        if (diameter < 10.0f) return;

        const float radius = diameter * 0.5f;
        const auto centre = bounds.getCentre();
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId, true).isTransparent()
                                ? juce::Colour(0xff38bdf8)
                                : slider.findColour(juce::Slider::rotarySliderFillColourId, true);

        const auto style = slider.getProperties().getWithDefault("knobStyle", "chrome_knurled").toString();
        const bool showTicks   = static_cast<bool>(slider.getProperties().getWithDefault("showTicks", true));
        const bool bypassed    = static_cast<bool>(slider.getProperties().getWithDefault("bypassed", false));
        const bool compactTrim = static_cast<bool>(slider.getProperties().getWithDefault("compactTrim", false));
        const bool hovered     = slider.isMouseOverOrDragging();

        // Modern glowing LED progress arc + crisp recessed track ring tightly hugging the 3D PBR knob
        // Match the 64-frame PBR filmstrip's 270-deg sweep (225 deg -> 495 deg) so tip dot aligns with pointer!
        constexpr float kFilmStartAngle = 3.9269908f; // 225 deg (7:30)
        constexpr float kFilmEndAngle   = 8.6393798f; // 495 deg (4:30)
        const auto& stripPreview = SkinManager::instance().getImage(SkinAsset::KnobChromeKnurled64);
        const float arcStart = stripPreview.isValid() ? kFilmStartAngle : rotaryStartAngle;
        const float arcEnd   = stripPreview.isValid() ? kFilmEndAngle   : rotaryEndAngle;

        float spriteScale = 0.98f;
        if (showTicks && (radius >= 14.0f || compactTrim)) {
            const float arcRadius = radius * (compactTrim ? 0.87f : 0.88f);
            const float currentAngle = arcStart + sliderPosProportional * (arcEnd - arcStart);

            // Background recessed track arc
            juce::Path trackArc;
            trackArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                   arcStart, arcEnd, true);
            g.setColour(juce::Colour(0xff090c12).withAlpha(0.88f));
            g.strokePath(trackArc, juce::PathStrokeType(compactTrim ? 2.4f : 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(juce::Colours::white.withAlpha(bypassed ? 0.09f : 0.16f));
            g.strokePath(trackArc, juce::PathStrokeType(compactTrim ? 1.3f : 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Glowing active value arc (completely OFF with ZERO color highlight when effect is bypassed!)
            if (!bypassed && sliderPosProportional > 0.005f) {
                juce::Path valueArc;
                valueArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                       arcStart, currentAngle, true);

                // Soft outer bloom
                g.setColour(accent.withAlpha(hovered ? 0.36f : 0.20f));
                g.strokePath(valueArc, juce::PathStrokeType(compactTrim ? 3.2f : 4.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                // Crisp core LED arc
                g.setColour(accent.brighter(hovered ? 0.22f : 0.10f));
                g.strokePath(valueArc, juce::PathStrokeType(compactTrim ? 1.8f : 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                // Glowing tip indicator dot
                if (!compactTrim) {
                    const float tipAngle = currentAngle - 1.5707963f;
                    const float tipX = centre.x + std::cos(tipAngle) * arcRadius;
                    const float tipY = centre.y + std::sin(tipAngle) * arcRadius;
                    g.setColour(juce::Colours::white.withAlpha(0.92f));
                    g.fillEllipse(tipX - 1.7f, tipY - 1.7f, 3.4f, 3.4f);
                }
            }
            spriteScale = compactTrim ? 0.90f : 0.94f;
        }

        // Resolve 64-frame 3D PBR filmstrip from SkinManager
        SkinAsset assetId = SkinAsset::KnobChromeKnurled64;
        if (style == "vintage_skirted" || style == "stomp_fluted" || style == "stomp_vintage") {
            assetId = SkinAsset::KnobVintageSkirted64;
        } else if (style == "gold_plexi") {
            assetId = SkinAsset::KnobGoldPlexi64;
        } else if (style == "cyan_stomp") {
            assetId = SkinAsset::KnobCyanStomp64;
        } else {
            assetId = SkinAsset::KnobChromeKnurled64;
        }

        const auto& strip = SkinManager::instance().getImage(assetId);
        if (strip.isValid()) {
            const float drawD = diameter * spriteScale;
            const auto dest = juce::Rectangle<float>(centre.x - drawD * 0.5f,
                                                     centre.y - drawD * 0.5f,
                                                     drawD, drawD);
            SkinManager::drawFilmstripFrame(g, strip, 64, sliderPosProportional, dest, bypassed ? 0.62f : 1.0f);

            if (hovered && !bypassed) {
                g.setColour(accent.withAlpha(slider.isMouseButtonDown() ? 0.55f : 0.25f));
                g.drawEllipse(dest.reduced(drawD * 0.09f), 1.3f);
            }
            return;
        }

        // Fallback procedural knob
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float knobR = radius * 0.74f;
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(centre.x - knobR, centre.y - knobR + 2.5f, knobR * 2.0f, knobR * 2.0f);
        g.setColour(juce::Colour(0xff252b37));
        g.fillEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        juce::Path ptr;
        ptr.addRoundedRectangle(-1.3f, -knobR + 2.5f, 2.6f, knobR * 0.72f, 1.0f);
        ptr.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
        g.setColour(juce::Colours::white);
        g.fillPath(ptr);
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          juce::Slider::SliderStyle style,
                          juce::Slider& slider) override {
        if (style != juce::Slider::LinearVertical) {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto faderStyle = slider.getProperties().getWithDefault("faderStyle", "eq").toString();
        const auto accent = slider.findColour(juce::Slider::trackColourId, true).isTransparent()
                                ? juce::Colour(0xff22c55e)
                                : slider.findColour(juce::Slider::trackColourId, true);

        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float top = static_cast<float>(y) + 8.0f;
        const float bot = static_cast<float>(y + height) - 8.0f;
        const float mid = (top + bot) * 0.5f;

        if (faderStyle == "console") {
            g.setColour(juce::Colour(0xff07090d));
            g.fillRoundedRectangle(cx - 3.0f, top, 6.0f, bot - top, 3.0f);

            g.setColour(juce::Colours::white.withAlpha(0.18f));
            for (int t = 0; t <= 8; ++t) {
                const float ty = top + (bot - top) * (static_cast<float>(t) / 8.0f);
                const float tw = (t == 2 || t == 8) ? 10.0f : 6.0f;
                g.drawHorizontalLine(static_cast<int>(ty), cx - 5.0f - tw, cx - 5.0f);
                g.drawHorizontalLine(static_cast<int>(ty), cx + 5.0f, cx + 5.0f + tw);
            }

            g.setColour(accent.withAlpha(0.28f));
            g.fillRoundedRectangle(cx - 3.5f, sliderPos, 7.0f, std::max(2.0f, bot - sliderPos), 3.5f);
            g.setColour(accent.withAlpha(0.88f));
            g.fillRoundedRectangle(cx - 2.0f, sliderPos, 4.0f, std::max(2.0f, bot - sliderPos), 2.0f);

            const auto& capImg = SkinManager::instance().getImage(SkinAsset::FaderConsoleCap);
            const auto cap = juce::Rectangle<float>(cx - 16.0f, sliderPos - 12.0f, 32.0f, 24.0f);
            if (capImg.isValid()) {
                g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
                g.drawImage(capImg, cap);
            } else {
                g.setColour(juce::Colour(0xff94a3b8));
                g.fillRoundedRectangle(cap.reduced(2.0f), 3.0f);
            }
            return;
        }

        // Graphic EQ hardware slider inside recessed dark channel
        g.setColour(juce::Colour(0xff070a10));
        g.fillRoundedRectangle(cx - 2.4f, top, 4.8f, bot - top, 2.4f);

        for (int t = 0; t <= 4; ++t) {
            const float ty = top + (bot - top) * (static_cast<float>(t) / 4.0f);
            g.setColour(juce::Colours::white.withAlpha(t == 2 ? 0.34f : 0.12f));
            g.drawHorizontalLine(static_cast<int>(ty), cx - 6.5f, cx + 6.5f);
        }

        const bool bypassed = static_cast<bool>(slider.getProperties().getWithDefault("bypassed", false));
        const float barTop = std::min(mid, sliderPos);
        const float barBot = std::max(mid, sliderPos);
        if (!bypassed) {
            g.setColour(accent.withAlpha(0.30f));
            g.fillRoundedRectangle(cx - 3.2f, barTop, 6.4f, std::max(2.0f, barBot - barTop), 3.0f);
            g.setColour(accent);
            g.fillRoundedRectangle(cx - 1.8f, barTop, 3.6f, std::max(2.0f, barBot - barTop), 1.8f);
        } else {
            g.setColour(juce::Colour(0xff38404f));
            g.fillRoundedRectangle(cx - 1.8f, barTop, 3.6f, std::max(2.0f, barBot - barTop), 1.8f);
        }

        const auto thumb = juce::Rectangle<float>(cx - 8.0f, sliderPos - 5.0f, 16.0f, 10.0f);
        g.setColour(juce::Colours::black.withAlpha(0.60f));
        g.fillRoundedRectangle(thumb.translated(0.0f, 1.5f), 3.0f);
        juce::ColourGradient capGrad(juce::Colour(0xff3e4756), thumb.getX(), thumb.getY(),
                                     juce::Colour(0xff1b2029), thumb.getX(), thumb.getBottom(), false);
        g.setGradientFill(capGrad);
        g.fillRoundedRectangle(thumb, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(bypassed ? 0.14f : 0.28f));
        g.drawRoundedRectangle(thumb, 3.0f, 0.8f);
        g.setColour(bypassed ? juce::Colour(0xff525b6c) : accent.brighter(0.35f));
        g.fillRoundedRectangle(thumb.getX() + 2.5f, sliderPos - 1.0f, thumb.getWidth() - 5.0f, 2.0f, 1.0f);
    }

    // Soft dark-glass pill capsule for slider value readout labels (replaces harsh white rectangular boxes!)
    void drawLabel(juce::Graphics& g, juce::Label& label) override {
        if (auto* parentSlider = dynamic_cast<juce::Slider*>(label.getParentComponent())) {
            const bool bypassed = static_cast<bool>(parentSlider->getProperties().getWithDefault("bypassed", false));
            auto r = label.getLocalBounds().toFloat().reduced(1.5f, 0.5f);
            g.setColour(juce::Colour(0xff0b0f17).withAlpha(bypassed ? 0.72f : 0.90f));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(juce::Colour(bypassed ? 0xff222733 : 0xff263144).withAlpha(bypassed ? 0.60f : 0.75f));
            g.drawRoundedRectangle(r, 4.0f, 0.8f);

            if (!label.isBeingEdited()) {
                g.setColour(bypassed ? juce::Colour(0xff64748b) : juce::Colour(0xffe2e8f0));
                g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
                g.drawText(label.getText(), r.reduced(2.0f, 0.0f), juce::Justification::centred, false);
            }
            return;
        }
        LookAndFeel_V4::drawLabel(g, label);
    }

    // Modern dark-glass pill ComboBox with smooth vector chevron
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int, int, int, int, juce::ComboBox& box) override {
        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        const bool hovered = box.isMouseOverOrDragging() || box.isPopupActive();
        const float corner = 5.5f;

        juce::ColourGradient bg(juce::Colour(isButtonDown ? 0xff111622 : (hovered ? 0xff1c2333 : 0xff151a26)),
                                0.0f, 0.0f,
                                juce::Colour(0xff0e121b),
                                0.0f, bounds.getHeight(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(bounds, corner);

        g.setColour(hovered ? juce::Colour(0xff3b82f6).withAlpha(0.80f) : juce::Colour(0xff2c3547));
        g.drawRoundedRectangle(bounds, corner, hovered ? 1.1f : 0.9f);

        // Sleek vector chevron arrow on right
        const float ax = bounds.getRight() - 10.5f;
        const float ay = bounds.getCentreY();
        juce::Path chev;
        chev.startNewSubPath(ax - 3.6f, ay - 1.6f);
        chev.lineTo(ax, ay + 2.2f);
        chev.lineTo(ax + 3.6f, ay - 1.6f);
        g.setColour(hovered ? juce::Colour(0xff60a5fa) : juce::Colour(0xff94a3b8));
        g.strokePath(chev, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(8, 1, std::max(10, box.getWidth() - 24), box.getHeight() - 2);
        label.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    }

    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override {
        const juce::Font f(juce::FontOptions(11.0f, juce::Font::plain));
        const int w = std::clamp(static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(f, tipText))) + 18, 48, 360);
        const int h = 24;
        return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - w - 6 : screenPos.x + 12,
                                    screenPos.y + 18, w, h).constrainedWithin(parentArea.reduced(4));
    }

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        g.setColour(juce::Colour(0xff101520).withAlpha(0.96f));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(juce::Colour(0xff303b50));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

        g.setColour(juce::Colour(0xffe2e8f0));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
        g.drawText(text, bounds.reduced(8.0f, 2.0f), juce::Justification::centred, true);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override {
        const auto r = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        g.setColour(juce::Colour(0xff111621));
        g.fillRect(r);
        g.setColour(juce::Colour(0xff2e384d));
        g.drawRect(r, 1.0f);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        const float px = std::clamp(static_cast<float>(buttonHeight) * 0.43f, 9.8f, 11.8f);
        return juce::Font(juce::FontOptions(px, juce::Font::bold));
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto variant = button.getProperties().getWithDefault("btnVariant", "").toString();
        const bool isToggled = button.getToggleState();
        const bool bypassed  = static_cast<bool>(button.getProperties().getWithDefault("bypassed", false));
        const float corner   = std::min(5.0f, bounds.getHeight() * 0.24f);

        if (variant == "ghost") {
            if (isButtonDown || isMouseOverButton || isToggled) {
                auto fill = isToggled ? backgroundColour : juce::Colour(isButtonDown ? 0xff19202d : 0xff1e2534);
                g.setColour(fill);
                g.fillRoundedRectangle(bounds, corner);
                g.setColour(juce::Colours::white.withAlpha(isToggled ? 0.22f : 0.12f));
                g.drawRoundedRectangle(bounds, corner, 0.85f);
            }
            return;
        }

        auto base = backgroundColour;
        if (isButtonDown) {
            base = base.darker(0.14f);
        } else if (isMouseOverButton) {
            base = base.brighter(0.09f);
        }

        // Refined matte anodized studio surface (no heavy glossy 3D bevels)
        juce::ColourGradient grad(base.brighter(0.05f), bounds.getX(), bounds.getY(),
                                  base.darker(0.08f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, corner);

        if (bypassed) {
            g.setColour(juce::Colours::white.withAlpha(isMouseOverButton ? 0.16f : 0.08f));
            g.drawRoundedRectangle(bounds, corner, 0.85f);
        } else {
            g.setColour(isToggled ? juce::Colours::white.withAlpha(0.30f)
                                  : (isMouseOverButton ? juce::Colours::white.withAlpha(0.24f)
                                                      : juce::Colours::white.withAlpha(0.11f)));
            g.drawRoundedRectangle(bounds, corner, 0.9f);
        }
    }
};

} // namespace pedalboard
