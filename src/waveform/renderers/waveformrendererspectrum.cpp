#include "waveform/renderers/waveformrendererspectrum.h"

#include <QPainter>
#include <QVector3D>

#include "util/math.h"
#include "util/painterscope.h"
#include "waveform/renderers/waveformoverviewrenderer.h"
#include "waveform/renderers/waveformsignalcolors.h"
#include "waveform/renderers/waveformspectrumcolor.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"

WaveformRendererSpectrum::WaveformRendererSpectrum(
        WaveformWidgetRenderer* waveformWidgetRenderer,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidgetRenderer, options) {
}

void WaveformRendererSpectrum::onSetup(const QDomNode&) {
}

void WaveformRendererSpectrum::draw(QPainter* painter, QPaintEvent*) {
    ConstWaveformPointer pWaveform = m_waveformRenderer->getWaveform();
    if (pWaveform.isNull()) {
        return;
    }

    const double audioVisualRatio = pWaveform->getAudioVisualRatio();
    if (audioVisualRatio <= 0) {
        return;
    }

    const int dataSize = pWaveform->getDataSize();
    if (dataSize <= 1) {
        return;
    }

    const WaveformData* data = pWaveform->data();
    if (data == nullptr) {
        return;
    }

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0) {
        return;
    }

    PainterScope painterScope(painter);

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    painter->setRenderHints(QPainter::Antialiasing, false);
    painter->setWorldMatrixEnabled(false);
    painter->resetTransform();

    // Rotate if drawing vertical waveforms
    // and revert devicePixelRatio scaling in x direction.
    if (m_waveformRenderer->getOrientation() == Qt::Vertical) {
        painter->setTransform(QTransform(0, 1 / devicePixelRatio, 1, 0, 0, 0));
    } else {
        painter->setTransform(QTransform(1 / devicePixelRatio, 0, 0, 1, 0, 0));
    }

    // See waveformrendererhsv.cpp for a detailed explanation of the index calculation
    const double firstVisualIndex =
            m_waveformRenderer->getFirstDisplayedPosition() * trackSamples /
            audioVisualRatio;
    const double lastVisualIndex =
            m_waveformRenderer->getLastDisplayedPosition() * trackSamples /
            audioVisualRatio;
    const float length = m_waveformRenderer->getLength() * devicePixelRatio;
    // Represents the # of waveform data points per horizontal pixel.
    const double gain = (lastVisualIndex - firstVisualIndex) / length;

    float allGain = 1.0f;
    float lowGain = 1.0f;
    float midGain = 1.0f;
    float highGain = 1.0f;
    getGains(&allGain, &lowGain, &midGain, &highGain);

    const int breadth = m_waveformRenderer->getBreadth();
    const float halfBreadth = static_cast<float>(breadth) / 2.0f;
    const float heightFactor = allGain * halfBreadth / 255.0f;

    // Draw the reference line
    painter->setPen(m_waveformRenderer->getWaveformSignalColors()->getAxesColor());
    painter->drawLine(QLineF(0, halfBreadth, length, halfBreadth));

    const int lastVisualFrame = dataSize / 2 - 1;
    for (int x = 0; x < static_cast<int>(length); ++x) {
        const double xVisualSampleIndex = gain * x + firstVisualIndex;
        const double maxSamplingRange = gain / 2.0;
        const int visualFrameStart = math_clamp(
                static_cast<int>(xVisualSampleIndex / 2.0 - maxSamplingRange + 0.5),
                0,
                lastVisualFrame);
        const int visualFrameStop = math_clamp(
                static_cast<int>(xVisualSampleIndex / 2.0 + maxSamplingRange + 0.5),
                0,
                lastVisualFrame);

        float maxLow[2]{};
        float maxMid[2]{};
        float maxHigh[2]{};
        float maxAll[2]{};
        // data is interleaved left / right
        for (int i = visualFrameStart * 2; i <= visualFrameStop * 2; i += 2) {
            for (int chn = 0; chn < 2; ++chn) {
                const WaveformData& waveformData = data[i + chn];
                maxLow[chn] = math_max(maxLow[chn], static_cast<float>(waveformData.filtered.low));
                maxMid[chn] = math_max(maxMid[chn], static_cast<float>(waveformData.filtered.mid));
                maxHigh[chn] = math_max(
                        maxHigh[chn], static_cast<float>(waveformData.filtered.high));
                maxAll[chn] = math_max(maxAll[chn], static_cast<float>(waveformData.filtered.all));
            }
        }

        if (maxAll[0] <= 0.0f && maxAll[1] <= 0.0f) {
            continue;
        }

        float eqGain[2] = {1.0f, 1.0f};
        for (int chn = 0; chn < 2; ++chn) {
            const float allUnscaled = maxLow[chn] + maxMid[chn] + maxHigh[chn];
            maxLow[chn] *= lowGain;
            maxMid[chn] *= midGain;
            maxHigh[chn] *= highGain;
            if (allUnscaled > 0.0f) {
                eqGain[chn] = (maxLow[chn] + maxMid[chn] + maxHigh[chn]) / allUnscaled;
            }
        }

        const float spectralPosition = waveformSpectrumColor::spectralPosition(
                maxLow[0] + maxLow[1], maxMid[0] + maxMid[1], maxHigh[0] + maxHigh[1]);
        const float amplitude = math_max(maxAll[0], maxAll[1]) / 255.0f;
        const QVector3D color = waveformSpectrumColor::color(spectralPosition) *
                waveformSpectrumColor::brightness(amplitude);

        const float heightLeft = heightFactor * eqGain[0] * maxAll[0];
        const float heightRight = heightFactor * eqGain[1] * maxAll[1];
        switch (m_alignment) {
        case Qt::AlignBottom:
        case Qt::AlignRight:
            waveformOverviewRenderer::drawSpectrumColumn(painter,
                    x,
                    breadth,
                    breadth - math_max(heightLeft, heightRight),
                    color);
            break;
        case Qt::AlignTop:
        case Qt::AlignLeft:
            waveformOverviewRenderer::drawSpectrumColumn(
                    painter, x, 0, math_max(heightLeft, heightRight), color);
            break;
        default:
            waveformOverviewRenderer::drawSpectrumColumn(
                    painter, x, halfBreadth, halfBreadth - heightLeft, color);
            waveformOverviewRenderer::drawSpectrumColumn(
                    painter, x, halfBreadth, halfBreadth + heightRight, color);
            break;
        }
    }
}
