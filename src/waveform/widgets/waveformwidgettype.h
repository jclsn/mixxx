#pragma once

#include <QFlags>
#include <array>
// required for Qt-Macros
#include <qobjectdefs.h>

class WaveformWidgetType {
  public:
    enum Type {
        // The order must not be changed because the waveforms are referenced
        // from the sorted preferences by a number.
        Empty = 0,
        Simple = 5,    // 5  Simple GL
        Filtered = 7,  // 7  Filtered GLSL
        HSV = 8,       // 8  HSV
        VSyncTest = 9, // 9  VSync GL
        RGB = 12,      // 12 RGB GLSL
        Stacked = 16,  // 16 RGB Stacked
        // Values up to 26 were used by removed waveform types, see upgrade.cpp
        Spectrum = 27, // 27 Spectrum colored
        // 28 Actual wave shape, colored like Spectrum
        Envelope = 28,
        Invalid, // Don't use! Used to indicate invalid/unknown type, as
                 // Count_WaveformWidgetType used to.
    };
    static constexpr std::array kValues = {
            WaveformWidgetType::Empty,
            WaveformWidgetType::Simple,
            WaveformWidgetType::Filtered,
            WaveformWidgetType::HSV,
            WaveformWidgetType::VSyncTest,
            WaveformWidgetType::RGB,
            WaveformWidgetType::Stacked,
            WaveformWidgetType::Spectrum,
            WaveformWidgetType::Envelope,
    };
};

// Note: all values are also available in builds without MIXXX_USE_QOPENGL
// This is required to reset an unsupported backend from config
enum class WaveformWidgetBackend {
    None = 0,
    GL,
    GLSL,
    AllShader,
};
