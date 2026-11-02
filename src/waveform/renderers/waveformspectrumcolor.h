#pragma once

#include <QColor>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstddef>

#include "util/math.h"

/// Color mapping shared by the Spectrum waveform and the Spectrum overview.
///
/// Each column is colored by its spectral balance: bass heavy content is
/// drawn in orange/red, mids in magenta/pink and highs in blue/cyan.
namespace waveformSpectrumColor {

struct GradientStop {
    float position;
    QVector3D color;
};

// Maps the spectral balance of a column (0 = only bass, 1 = only highs)
// to a color: orange -> red -> magenta -> violet -> blue -> cyan.
constexpr std::array kGradient = {
        GradientStop{0.00f, QVector3D(1.00f, 0.35f, 0.05f)},
        GradientStop{0.25f, QVector3D(1.00f, 0.12f, 0.30f)},
        GradientStop{0.45f, QVector3D(1.00f, 0.20f, 0.80f)},
        GradientStop{0.65f, QVector3D(0.55f, 0.35f, 1.00f)},
        GradientStop{0.82f, QVector3D(0.10f, 0.60f, 1.00f)},
        GradientStop{1.00f, QVector3D(0.15f, 1.00f, 0.75f)},
};

// The filtered mid and high bands carry much less energy than the low band.
// Weight them up so that hi-hats and vocals actually shift the color.
constexpr float kMidWeight = 1.5f;
constexpr float kHighWeight = 2.5f;

// Amount of white mixed into the core of each column, and the opacity of its
// outer edge. Together they produce the glow effect.
constexpr float kCoreWhiteMix = 0.15f;
constexpr float kEdgeAlpha = 0.45f;

// Quiet passages are drawn darker than loud ones.
constexpr float kMinBrightness = 0.75f;

/// Returns the spectral balance of the given band values, ranging from
/// 0 (only bass) to 1 (only highs).
inline float spectralPosition(float low, float mid, float high) {
    const float weightedMid = mid * kMidWeight;
    const float weightedHigh = high * kHighWeight;
    const float total = low + weightedMid + weightedHigh;
    if (total <= 0.0f) {
        return 0.0f;
    }

    return (0.5f * weightedMid + weightedHigh) / total;
}

/// Returns the color for a spectral position, see spectralPosition().
inline QVector3D color(float position) {
    position = math_clamp(position, 0.f, 1.f);
    for (std::size_t i = 1; i < kGradient.size(); ++i) {
        const GradientStop& upper = kGradient[i];
        if (position <= upper.position) {
            const GradientStop& lower = kGradient[i - 1];
            const float t = (position - lower.position) / (upper.position - lower.position);
            return lower.color + (upper.color - lower.color) * t;
        }
    }

    return kGradient.back().color;
}

/// Returns the brightness factor for an amplitude in the range [0, 1].
inline float brightness(float amplitude) {
    return kMinBrightness + (1.0f - kMinBrightness) * std::sqrt(math_clamp(amplitude, 0.f, 1.f));
}

/// Returns the lighter core color for the given (edge) color.
inline QVector3D coreColor(const QVector3D& color) {
    return color + (QVector3D(1.f, 1.f, 1.f) - color) * kCoreWhiteMix;
}

inline QColor toQColor(const QVector3D& color, float alpha = 1.0f) {
    return QColor::fromRgbF(color.x(), color.y(), color.z(), alpha);
}

} // namespace waveformSpectrumColor
