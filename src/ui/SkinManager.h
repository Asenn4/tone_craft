#pragma once

#include "BinaryData.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace pedalboard {

enum class SkinAsset : int {
    KnobVintageSkirted64 = 0,
    KnobGoldPlexi64,
    KnobChromeKnurled64,
    KnobCyanStomp64,
    FootswitchChrome2,
    ToggleChrome2,
    FaderConsoleCap,
    Count
};

// Lightweight cache for tactile AmpliTube-style 64-frame knob & hardware switch filmstrips.
// Supports optional user override PNGs in Documents/PedalBoard/Skins/Default/,
// falling back seamlessly to embedded BinaryData.
class SkinManager {
public:
    static SkinManager& instance() {
        static SkinManager mgr;
        return mgr;
    }

    const juce::Image& getImage(SkinAsset id) {
        const auto idx = static_cast<size_t>(id);
        if (idx >= images.size()) {
            return emptyImage;
        }
        if (!loaded[idx]) {
            loadAsset(id);
        }
        return images[idx];
    }

    void reloadAll() {
        loaded.fill(false);
        for (auto& img : images) {
            img = {};
        }
    }

    static juce::File getUserSkinDirectory() {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("PedalBoard")
            .getChildFile("Skins")
            .getChildFile("Default");
    }

    static void drawFilmstripFrame(juce::Graphics& g,
                                   const juce::Image& strip,
                                   int numFrames,
                                   float normalizedValue,
                                   juce::Rectangle<float> destBounds,
                                   float opacity = 1.0f) {
        if (!strip.isValid() || numFrames <= 0) return;
        const int frameW = strip.getWidth();
        const int frameH = strip.getHeight() / numFrames;
        if (frameW <= 0 || frameH <= 0) return;

        const float clamped = std::clamp(normalizedValue, 0.0f, 1.0f);
        const int frameIdx = std::clamp(
            static_cast<int>(std::lround(clamped * static_cast<float>(numFrames - 1))),
            0, numFrames - 1
        );

        g.setOpacity(opacity);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(strip,
                    static_cast<int>(std::floor(destBounds.getX())),
                    static_cast<int>(std::floor(destBounds.getY())),
                    static_cast<int>(std::ceil(destBounds.getWidth())),
                    static_cast<int>(std::ceil(destBounds.getHeight())),
                    0,
                    frameIdx * frameH,
                    frameW,
                    frameH);
        g.setOpacity(1.0f);
    }

    static void drawSwitchFrame(juce::Graphics& g,
                                const juce::Image& strip,
                                int frameIndex,
                                int numFrames,
                                juce::Rectangle<float> destBounds,
                                float opacity = 1.0f) {
        if (!strip.isValid() || numFrames <= 0) return;
        const int frameW = strip.getWidth();
        const int frameH = strip.getHeight() / numFrames;
        if (frameW <= 0 || frameH <= 0) return;

        const int clampedIdx = std::clamp(frameIndex, 0, numFrames - 1);
        g.setOpacity(opacity);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(strip,
                    static_cast<int>(std::floor(destBounds.getX())),
                    static_cast<int>(std::floor(destBounds.getY())),
                    static_cast<int>(std::ceil(destBounds.getWidth())),
                    static_cast<int>(std::ceil(destBounds.getHeight())),
                    0,
                    clampedIdx * frameH,
                    frameW,
                    frameH);
        g.setOpacity(1.0f);
    }

private:
    SkinManager() = default;

    struct AssetSpec {
        const char* fileName;
        const char* binaryName;
    };

    static AssetSpec specFor(SkinAsset id) noexcept {
        switch (id) {
            case SkinAsset::KnobVintageSkirted64: return { "knob_vintage_skirted_64.png","knob_vintage_skirted_64_png" };
            case SkinAsset::KnobGoldPlexi64:      return { "knob_gold_plexi_64.png",     "knob_gold_plexi_64_png" };
            case SkinAsset::KnobChromeKnurled64:  return { "knob_chrome_knurled_64.png", "knob_chrome_knurled_64_png" };
            case SkinAsset::KnobCyanStomp64:      return { "knob_cyan_stomp_64.png",     "knob_cyan_stomp_64_png" };
            case SkinAsset::FootswitchChrome2:    return { "footswitch_chrome_2.png",    "footswitch_chrome_2_png" };
            case SkinAsset::ToggleChrome2:        return { "toggle_chrome_2.png",        "toggle_chrome_2_png" };
            case SkinAsset::FaderConsoleCap:      return { "fader_console_cap.png",      "fader_console_cap_png" };
            default:                              return { "", "" };
        }
    }

    void loadAsset(SkinAsset id) {
        const auto idx = static_cast<size_t>(id);
        loaded[idx] = true;
        const auto spec = specFor(id);

        const auto userFile = getUserSkinDirectory().getChildFile(spec.fileName);
        if (userFile.existsAsFile()) {
            auto img = juce::ImageFileFormat::loadFrom(userFile);
            if (img.isValid()) {
                images[idx] = std::move(img);
                return;
            }
        }

        int dataSize = 0;
        if (const char* data = BinaryData::getNamedResource(spec.binaryName, dataSize); data != nullptr && dataSize > 0) {
            images[idx] = juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(dataSize));
        }
    }

    std::array<juce::Image, static_cast<size_t>(SkinAsset::Count)> images {};
    std::array<bool, static_cast<size_t>(SkinAsset::Count)> loaded {};
    juce::Image emptyImage {};
};

} // namespace pedalboard
