#include "waveform/renderers/allshader/waveformrendererspectrum.h"

#include <QVector3D>
#include <QVector4D>

#include "rendergraph/material/rgbamaterial.h"
#include "rendergraph/vertexupdaters/rgbavertexupdater.h"
#include "track/track.h"
#include "util/math.h"
#include "waveform/highresolutionwaveform.h"
#include "waveform/renderers/waveformspectrumcolor.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"

using namespace rendergraph;

namespace {

const WaveformFilteredData& filtered(const WaveformData& data) {
    return data.filtered;
}

const WaveformFilteredData& filtered(const WaveformFilteredData& data) {
    return data;
}

} // namespace

namespace allshader {

WaveformRendererSpectrum::WaveformRendererSpectrum(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_splitStereoSignal(options.testFlag(
                  ::WaveformRendererSignalBase::Option::SplitStereoSignal)),
          m_highDetail(options.testFlag(::WaveformRendererSignalBase::Option::HighDetail)) {
    initForRectangles<RGBAMaterial>(0);
    setUsePreprocess(true);
}

void WaveformRendererSpectrum::onSetup(const QDomNode&) {
}

void WaveformRendererSpectrum::preprocess() {
    if (!preprocessInner()) {
        if (geometry().vertexCount() != 0) {
            geometry().allocate(0);
            markDirtyGeometry();
        }
    }
}

bool WaveformRendererSpectrum::preprocessInner() {
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

    // The analysis waveform is too coarse when a visual frame spans more
    // than a pixel, use the high resolution one then once it is available
    if (m_highDetail && m_waveformRenderer->getVisualSamplePerPixel() < 1.0) {
        ConstHighResolutionWaveformPointer pHighResolution = highResolutionWaveform(pTrack);
        if (pHighResolution && pHighResolution->getDataSize() > 1) {
            return drawColumns(pHighResolution->data(), pHighResolution->getDataSize());
        }
    }

    return drawColumns(data, dataSize);
}

ConstHighResolutionWaveformPointer WaveformRendererSpectrum::highResolutionWaveform(
        const TrackPointer& pTrack) {
    const QString location = pTrack->getLocation();
    if (location != m_highResolutionLocation) {
        m_highResolutionLocation = location;
        m_pHighResolutionWaveform.reset();
    }

    if (!m_pHighResolutionWaveform) {
        m_pHighResolutionWaveform = highResolutionWaveformCache::request(pTrack);
    }

    return m_pHighResolutionWaveform;
}

template<typename Data>
bool WaveformRendererSpectrum::drawColumns(const Data* data, int dataSize) {
    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const int length = static_cast<int>(m_waveformRenderer->getLength());
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

    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    const float halfBreadth = breadth / 2.0f;
    const float heightFactor = allGain * halfBreadth / m_maxValue;

    // Effective visual frame for x
    double xVisualFrame = qRound(firstVisualFrame / visualIncrementPerPixel) *
            visualIncrementPerPixel;

    const int numVerticesPerRectangle = 6; // 2 triangles
    // One rectangle for the axis, two gradient rectangles (top and bottom
    // half) per column
    const int reserved = numVerticesPerRectangle * (2 * pixelLength + 1);

    geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
    geometry().allocate(reserved);
    markDirtyGeometry();

    RGBAVertexUpdater vertexUpdater{geometry().vertexDataAs<Geometry::RGBAColoredPoint2D>()};
    vertexUpdater.addRectangle({0.f, halfBreadth - 0.5f},
            {static_cast<float>(length), halfBreadth + 0.5f},
            {m_axesColor_r, m_axesColor_g, m_axesColor_b, m_axesColor_a});

    const double maxSamplingRange = visualIncrementPerPixel / 2.0;

    for (int pos = 0; pos < pixelLength; ++pos) {
        const int visualFrameStart = std::lround(xVisualFrame - maxSamplingRange);
        const int visualFrameStop = std::lround(xVisualFrame + maxSamplingRange);

        const int visualIndexStart = std::max(visualFrameStart * 2, 0);
        const int visualIndexStop =
                std::min(std::max(visualFrameStop, visualFrameStart + 1) * 2, dataSize - 1);

        const float fpos = static_cast<float>(pos) * invDevicePixelRatio;

        // per channel
        float maxAll[2]{};
        float eqGain[2] = {1.0f, 1.0f};
        float maxLow[2]{};
        float maxMid[2]{};
        float maxHigh[2]{};

        for (int chn = 0; chn < 2; chn++) {
            // Find the max values for low, mid, high and all in the waveform data
            float maxLowU = 0.0f;
            float maxMidU = 0.0f;
            float maxHighU = 0.0f;
            float maxAllU = 0.0f;
            // data is interleaved left / right
            for (int i = visualIndexStart + chn; i < visualIndexStop + chn; i += 2) {
                const WaveformFilteredData& waveformData = filtered(data[i]);

                maxLowU = math_max(maxLowU, static_cast<float>(waveformData.low));
                maxMidU = math_max(maxMidU, static_cast<float>(waveformData.mid));
                maxHighU = math_max(maxHighU, static_cast<float>(waveformData.high));
                maxAllU = math_max(maxAllU, static_cast<float>(waveformData.all));
            }

            maxLow[chn] = maxLowU * lowGain;
            maxMid[chn] = maxMidU * midGain;
            maxHigh[chn] = maxHighU * highGain;
            maxAll[chn] = maxAllU;

            const float allUnscaled = maxLowU + maxMidU + maxHighU;
            if (allUnscaled > 0.0f) {
                eqGain[chn] = (maxLow[chn] + maxMid[chn] + maxHigh[chn]) / allUnscaled;
            }
        }

        // With stereo coloration, the top half (left channel) and the bottom
        // half (right channel) are colored individually
        QVector3D color[2];
        if (m_splitStereoSignal) {
            for (int chn = 0; chn < 2; chn++) {
                color[chn] = waveformSpectrumColor::color(
                                     waveformSpectrumColor::spectralPosition(
                                             maxLow[chn], maxMid[chn], maxHigh[chn])) *
                        waveformSpectrumColor::brightness(maxAll[chn] / m_maxValue);
            }
        } else {
            const float spectralPosition = waveformSpectrumColor::spectralPosition(
                    maxLow[0] + maxLow[1], maxMid[0] + maxMid[1], maxHigh[0] + maxHigh[1]);
            const float amplitude = math_max(maxAll[0], maxAll[1]) / m_maxValue;
            color[0] = waveformSpectrumColor::color(spectralPosition) *
                    waveformSpectrumColor::brightness(amplitude);
            color[1] = color[0];
        }

        // maxAll[0] is for left channel, maxAll[1] is for right channel
        const float top = halfBreadth - heightFactor * eqGain[0] * maxAll[0];
        const float bottom = halfBreadth + heightFactor * eqGain[1] * maxAll[1];

        vertexUpdater.addRectangleVGradient({fpos - halfPixelSize, top},
                {fpos + halfPixelSize, halfBreadth},
                QVector4D(color[0], waveformSpectrumColor::kEdgeAlpha),
                QVector4D(waveformSpectrumColor::coreColor(color[0]), 1.0f));
        vertexUpdater.addRectangleVGradient({fpos - halfPixelSize, halfBreadth},
                {fpos + halfPixelSize, bottom},
                QVector4D(waveformSpectrumColor::coreColor(color[1]), 1.0f),
                QVector4D(color[1], waveformSpectrumColor::kEdgeAlpha));

        xVisualFrame += visualIncrementPerPixel;
    }

    DEBUG_ASSERT(reserved == vertexUpdater.index());

    markDirtyMaterial();

    return true;
}

} // namespace allshader
