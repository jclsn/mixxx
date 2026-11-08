#pragma once

#include <QColor>
#include <QtGlobal>

#include "waveform/overviewtype.h"
#include "waveform/waveform.h"

class QPainter;
class QVector3D;
class WaveformSignalColors;

namespace waveformOverviewRenderer {
/// This returns the normalized fullsize image
/// for the library's overview column.
QImage render(ConstWaveformPointer pWaveform,
        mixxx::OverviewType type,
        const WaveformSignalColors& signalColors,
        bool mono = false);

/// These paint methods return the fullsize image
/// They allow "mono" rendering (mono-mixdown, bottom-aligned).
/// Note: Don't use mono = true with WOverview, it's not adjusted yet! It does some
/// additional scaling for normalization which will atm cut off the bottom part.
void drawWaveformPartRGB(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        const WaveformSignalColors& signalColors,
        bool mono = false);
void drawWaveformPartLMH(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        const WaveformSignalColors& signalColors,
        bool mono = false);
void drawWaveformPartHSV(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        const WaveformSignalColors& signalColors,
        bool mono = false);
void drawWaveformPartSpectrum(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        bool mono = false);
void drawWaveformPartEnvelope(
        QPainter* pPainter,
        ConstWaveformPointer pWaveform,
        int* start,
        int end,
        bool mono = false);

/// Draws a single Spectrum overview column from y0 (the axis) to y1, with a
/// light core near the axis and a translucent outer part.
void drawSpectrumColumn(QPainter* pPainter, qreal x, qreal y0, qreal y1, const QVector3D& color);
/// Draws a single Envelope overview column from y0 (the axis) to y1 in a
/// solid color.
void drawEnvelopeColumn(QPainter* pPainter, qreal x, qreal y0, qreal y1, const QVector3D& color);
} // namespace waveformOverviewRenderer
