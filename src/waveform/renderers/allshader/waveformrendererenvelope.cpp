#include "waveform/renderers/allshader/waveformrendererenvelope.h"

#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "rendergraph/material/rgbamaterial.h"
#include "rendergraph/vertexupdaters/rgbavertexupdater.h"
#include "track/track.h"
#include "waveform/renderers/waveformspectrumcolor.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"

using namespace rendergraph;

namespace {

// Thickness of the line in logical pixels. Bass heavy parts are drawn
// thicker, so the slow bass waves stand out from the thin treble.
constexpr float kLineWidthBass = 2.0f;
constexpr float kLineWidthTreble = 1.0f;

struct ColumnColor {
    QVector3D color;
    // 0 = only bass, 1 = only highs, see waveformSpectrumColor::spectralPosition()
    float spectralPosition;
    // Scales the amplitude by the visual EQ gains, as the envelope waveforms do
    float eqGain;
};

ColumnColor columnColor(const WaveformData* data,
        int dataSize,
        int visualFrame,
        float lowGain,
        float midGain,
        float highGain) {
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
    float unscaled = 0.0f;
    // data is interleaved left / right
    for (int i = visualFrame * 2; i < visualFrame * 2 + 2; ++i) {
        if (i < 0 || i >= dataSize) {
            continue;
        }

        const WaveformData& waveformData = data[i];
        low += waveformData.filtered.low * lowGain;
        mid += waveformData.filtered.mid * midGain;
        high += waveformData.filtered.high * highGain;
        unscaled += static_cast<float>(waveformData.filtered.low +
                waveformData.filtered.mid + waveformData.filtered.high);
    }

    const float eqGain = unscaled > 0.0f ? (low + mid + high) / unscaled : 1.0f;
    const float spectralPosition = waveformSpectrumColor::spectralPosition(low, mid, high);

    return {waveformSpectrumColor::color(spectralPosition), spectralPosition, eqGain};
}

struct SampleRange {
    float low;
    float high;
};

/// The part of the wave shape within the column starting at binPosition.
SampleRange traceRange(const std::vector<WaveformDetail::Bin>& bins,
        double binPosition,
        double binsPerPixel) {
    const int binCount = static_cast<int>(bins.size());
    if (binsPerPixel >= 1.0) {
        const int firstBin = std::clamp(static_cast<int>(binPosition), 0, binCount - 1);
        const int lastBin = std::clamp(
                static_cast<int>(binPosition + binsPerPixel), firstBin + 1, binCount);
        SampleRange range{static_cast<float>(bins[firstBin].min),
                static_cast<float>(bins[firstBin].max)};
        for (int i = firstBin + 1; i < lastBin; ++i) {
            range.low = std::min(range.low, static_cast<float>(bins[i].min));
            range.high = std::max(range.high, static_cast<float>(bins[i].max));
        }
        return range;
    }

    // Less than one bin per pixel: interpolate between the bin centers to get
    // a smooth line instead of steps
    const double center = binPosition + binsPerPixel / 2.0 - 0.5;
    const int bin = std::clamp(static_cast<int>(std::floor(center)), 0, binCount - 2);
    const float t = std::clamp(static_cast<float>(center - bin), 0.f, 1.f);

    return {bins[bin].min + (bins[bin + 1].min - bins[bin].min) * t,
            bins[bin].max + (bins[bin + 1].max - bins[bin].max) * t};
}

} // namespace

namespace allshader {

WaveformRendererEnvelope::WaveformRendererEnvelope(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options) {
    initForRectangles<RGBAMaterial>(0);
    setUsePreprocess(true);
}

void WaveformRendererEnvelope::onSetup(const QDomNode&) {
}

void WaveformRendererEnvelope::preprocess() {
    if (!preprocessInner()) {
        if (geometry().vertexCount() != 0) {
            geometry().allocate(0);
            markDirtyGeometry();
        }
    }
}

bool WaveformRendererEnvelope::preprocessInner() {
    TrackPointer pTrack = m_waveformRenderer->getTrackInfo();
    if (!pTrack) {
        return false;
    }

    ConstWaveformPointer waveform = pTrack->getWaveform();
    if (waveform.isNull()) {
        return false;
    }

    const int dataSize = waveform->getDataSize();
    if (dataSize <= 1) {
        return false;
    }

    const WaveformData* data = waveform->data();
    if (data == nullptr) {
        return false;
    }

#ifdef __STEM__
    auto stemInfo = pTrack->getStemInfo();
    // If this track is a stem track, skip the rendering
    if (!stemInfo.isEmpty() && waveform->hasStem() && !m_ignoreStem) {
        return false;
    }
#endif

    const ConstWaveformDetailPointer pDetail = detail(pTrack);
    if (!pDetail) {
        // Still decoding
        return false;
    }

    const std::vector<WaveformDetail::Bin>& bins = pDetail->bins();
    const int binCount = static_cast<int>(bins.size());
    if (binCount < 2) {
        return false;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const int pixelLength = static_cast<int>(m_waveformRenderer->getLength() * devicePixelRatio);
    const float invDevicePixelRatio = 1.f / devicePixelRatio;
    const float halfPixelSize = 0.5f / devicePixelRatio;

    // See waveformrenderersimple.cpp for a detailed explanation of the frame and index calculation
    const int visualFramesSize = dataSize / 2;
    const double firstVisualFrame =
            m_waveformRenderer->getFirstDisplayedPosition() * visualFramesSize;
    const double lastVisualFrame =
            m_waveformRenderer->getLastDisplayedPosition() * visualFramesSize;

    // Represents the # of visual frames per horizontal pixel.
    const double visualIncrementPerPixel =
            (lastVisualFrame - firstVisualFrame) / static_cast<double>(pixelLength);
    if (visualIncrementPerPixel == 0.0) {
        return false;
    }

    float allGain = 1.0f;
    float lowGain = 1.0f;
    float midGain = 1.0f;
    float highGain = 1.0f;
    getGains(&allGain, &lowGain, &midGain, &highGain);

    const float halfBreadth = static_cast<float>(m_waveformRenderer->getBreadth()) / 2.0f;
    const float heightFactor =
            allGain * halfBreadth / std::numeric_limits<std::int16_t>::max();

    // Same alignment as the envelope waveforms
    const double firstColumnVisualFrame =
            qRound(firstVisualFrame / visualIncrementPerPixel) * visualIncrementPerPixel;
    const double binsPerPixel = visualIncrementPerPixel *
            waveform->getAudioVisualRatio() / WaveformDetail::kFramesPerBin;
    // Position of the left edge of the first column in bins
    const double firstBinPosition =
            (firstColumnVisualFrame - visualIncrementPerPixel / 2.0) *
            waveform->getAudioVisualRatio() / WaveformDetail::kFramesPerBin;

    // 1st pass: the line within each column and its color
    m_traceLow.assign(pixelLength, 0.0f);
    m_traceHigh.assign(pixelLength, 0.0f);
    m_colors.resize(pixelLength);
    m_lineWidths.resize(pixelLength);
    m_scales.resize(pixelLength);
    float previousCenter = 0.0f;
    for (int pos = 0; pos < pixelLength; ++pos) {
        const double xVisualFrame = firstColumnVisualFrame + pos * visualIncrementPerPixel;
        const ColumnColor color = columnColor(data,
                dataSize,
                static_cast<int>(xVisualFrame),
                lowGain,
                midGain,
                highGain);
        m_colors[pos] = color.color;
        m_scales[pos] = heightFactor * color.eqGain;
        m_lineWidths[pos] = kLineWidthBass +
                (kLineWidthTreble - kLineWidthBass) * color.spectralPosition;

        const double binPosition = firstBinPosition + pos * binsPerPixel;
        // Silence before the start and after the end of the track
        if (binPosition + binsPerPixel > 0.0 && binPosition < binCount) {
            SampleRange range = traceRange(bins, binPosition, binsPerPixel);
            const float center = (range.low + range.high) / 2.0f;
            // Connect to the previous column, otherwise steep slopes would
            // break up into separate dots
            if (pos > 0) {
                range.low = std::min(range.low, previousCenter);
                range.high = std::max(range.high, previousCenter);
            }
            previousCenter = center;
            m_traceLow[pos] = range.low;
            m_traceHigh[pos] = range.high;
        }
    }

    // 2nd pass: widen the line horizontally by the line width as well.
    // Otherwise steep slopes would only be one pixel wide, much thinner than
    // flat parts.
    m_lineLow.resize(pixelLength);
    m_lineHigh.resize(pixelLength);
    for (int pos = 0; pos < pixelLength; ++pos) {
        // Rounded down, so thin treble lines are not widened at all
        const int lineRadius = static_cast<int>(m_lineWidths[pos] * devicePixelRatio / 2.0f);
        const int first = std::max(pos - lineRadius, 0);
        const int last = std::min(pos + lineRadius + 1, pixelLength);
        m_lineLow[pos] = *std::min_element(
                m_traceLow.begin() + first, m_traceLow.begin() + last);
        m_lineHigh[pos] = *std::max_element(
                m_traceHigh.begin() + first, m_traceHigh.begin() + last);
    }

    const int numVerticesPerRectangle = 6; // 2 triangles
    // One rectangle per column. There is no axis, silence is drawn as a flat
    // line in the waveform color.
    const int reserved = numVerticesPerRectangle * pixelLength;

    geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
    geometry().allocate(reserved);
    markDirtyGeometry();

    RGBAVertexUpdater vertexUpdater{geometry().vertexDataAs<Geometry::RGBAColoredPoint2D>()};

    // 3rd pass: draw
    for (int pos = 0; pos < pixelLength; ++pos) {
        const float halfLineWidth = m_lineWidths[pos] / 2.0f;
        const float top = halfBreadth - m_lineHigh[pos] * m_scales[pos] - halfLineWidth;
        const float bottom = halfBreadth - m_lineLow[pos] * m_scales[pos] + halfLineWidth;

        const float fpos = static_cast<float>(pos) * invDevicePixelRatio;
        vertexUpdater.addRectangle({fpos - halfPixelSize, top},
                {fpos + halfPixelSize, bottom},
                QVector4D(m_colors[pos], 1.0f));
    }

    DEBUG_ASSERT(reserved == vertexUpdater.index());

    markDirtyMaterial();

    return true;
}

ConstWaveformDetailPointer WaveformRendererEnvelope::detail(const TrackPointer& pTrack) {
    const QString location = pTrack->getLocation();
    if (location != m_detailLocation) {
        m_detailLocation = location;
        m_pDetail.reset();
    }

    if (!m_pDetail) {
        m_pDetail = waveformDetailCache::request(pTrack);
    }

    return m_pDetail;
}

} // namespace allshader
