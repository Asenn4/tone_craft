#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace pedalboard {

// Allocation-free stereo biquad filter (RBJ Audio EQ Cookbook, Direct Form II Transposed).
class BiquadFilter {
public:
    void reset() noexcept {
        z1L = z2L = z1R = z2R = 0.0f;
    }

    void setLowPass(double sampleRate, double freqHz, double q = 0.70710678) noexcept {
        const double sr = std::max(8000.0, sampleRate);
        const double f = std::clamp(freqHz, 20.0, sr * 0.45);
        const double w0 = 2.0 * std::numbers::pi * f / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw / (2.0 * std::max(0.1, q));

        const double a0 = 1.0 + alpha;
        setNormalized((1.0 - cosw) * 0.5, 1.0 - cosw, (1.0 - cosw) * 0.5, a0, -2.0 * cosw, 1.0 - alpha);
    }

    void setHighPass(double sampleRate, double freqHz, double q = 0.70710678) noexcept {
        const double sr = std::max(8000.0, sampleRate);
        const double f = std::clamp(freqHz, 20.0, sr * 0.45);
        const double w0 = 2.0 * std::numbers::pi * f / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw / (2.0 * std::max(0.1, q));

        const double a0 = 1.0 + alpha;
        setNormalized((1.0 + cosw) * 0.5, -(1.0 + cosw), (1.0 + cosw) * 0.5, a0, -2.0 * cosw, 1.0 - alpha);
    }

    void setBandPass(double sampleRate, double freqHz, double q = 0.70710678) noexcept {
        const double sr = std::max(8000.0, sampleRate);
        const double f = std::clamp(freqHz, 20.0, sr * 0.45);
        const double w0 = 2.0 * std::numbers::pi * f / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw / (2.0 * std::max(0.1, q));

        const double a0 = 1.0 + alpha;
        setNormalized(alpha, 0.0, -alpha, a0, -2.0 * cosw, 1.0 - alpha);
    }

    void setPeaking(double sampleRate, double freqHz, double q, double gainDb) noexcept {
        const double sr = std::max(8000.0, sampleRate);
        const double f = std::clamp(freqHz, 20.0, sr * 0.45);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * std::numbers::pi * f / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw / (2.0 * std::max(0.1, q));

        const double a0 = 1.0 + alpha / A;
        setNormalized(1.0 + alpha * A, -2.0 * cosw, 1.0 - alpha * A, a0, -2.0 * cosw, 1.0 - alpha / A);
    }

    void setLowShelf(double sampleRate, double freqHz, double gainDb, double slope = 0.8) noexcept {
        const double sr = std::max(8000.0, sampleRate);
        const double f = std::clamp(freqHz, 20.0, sr * 0.45);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * std::numbers::pi * f / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw * 0.5 * std::sqrt(std::max(0.01, (A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0));
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

        const double b0_ = A * ((A + 1.0) - (A - 1.0) * cosw + twoSqrtAAlpha);
        const double b1_ = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw);
        const double b2_ = A * ((A + 1.0) - (A - 1.0) * cosw - twoSqrtAAlpha);
        const double a0_ = (A + 1.0) + (A - 1.0) * cosw + twoSqrtAAlpha;
        const double a1_ = -2.0 * ((A - 1.0) + (A + 1.0) * cosw);
        const double a2_ = (A + 1.0) + (A - 1.0) * cosw - twoSqrtAAlpha;
        setNormalized(b0_, b1_, b2_, a0_, a1_, a2_);
    }

    void setHighShelf(double sampleRate, double freqHz, double gainDb, double slope = 0.8) noexcept {
        const double sr = std::max(8000.0, sampleRate);
        const double f = std::clamp(freqHz, 20.0, sr * 0.45);
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * std::numbers::pi * f / sr;
        const double cosw = std::cos(w0);
        const double sinw = std::sin(w0);
        const double alpha = sinw * 0.5 * std::sqrt(std::max(0.01, (A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0));
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

        const double b0_ = A * ((A + 1.0) + (A - 1.0) * cosw + twoSqrtAAlpha);
        const double b1_ = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw);
        const double b2_ = A * ((A + 1.0) + (A - 1.0) * cosw - twoSqrtAAlpha);
        const double a0_ = (A + 1.0) - (A - 1.0) * cosw + twoSqrtAAlpha;
        const double a1_ = 2.0 * ((A - 1.0) - (A + 1.0) * cosw);
        const double a2_ = (A + 1.0) - (A - 1.0) * cosw - twoSqrtAAlpha;
        setNormalized(b0_, b1_, b2_, a0_, a1_, a2_);
    }

    inline float processSample(int channel, float x) noexcept {
        float& z1 = (channel == 0) ? z1L : z1R;
        float& z2 = (channel == 0) ? z2L : z2R;
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    void setNormalized(double b0_, double b1_, double b2_, double a0_, double a1_, double a2_) noexcept {
        const double inv = 1.0 / a0_;
        b0 = static_cast<float>(b0_ * inv);
        b1 = static_cast<float>(b1_ * inv);
        b2 = static_cast<float>(b2_ * inv);
        a1 = static_cast<float>(a1_ * inv);
        a2 = static_cast<float>(a2_ * inv);
    }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1L = 0.0f, z2L = 0.0f, z1R = 0.0f, z2R = 0.0f;
};

} // namespace pedalboard
