#pragma once

#include <QString>
#include <QVector3D>
#include <vector>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"
#include "waveform/waveformdetail.h"

namespace allshader {
class WaveformRendererEnvelope;
} // namespace allshader

/// Draws the actual (signed) wave shape of the decoded audio: the minimum and
/// maximum of the signal within each pixel column, connected to a line. When
/// zoomed in, e.g. the sine of a kick drum is visible, when zoomed out the
/// columns span whole cycles and form the envelope. See WaveformDetail.
/// The line is colored by the spectral balance like the Spectrum waveform and
/// drawn thicker for bass heavy parts. Nothing is drawn until the track has
/// been decoded.
class allshader::WaveformRendererEnvelope final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::GeometryNode {
  public:
    explicit WaveformRendererEnvelope(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererSignalBase::Options options);

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    // Virtuals for rendergraph::Node
    void preprocess() override;

  private:
    bool preprocessInner();
    ConstWaveformDetailPointer detail(const TrackPointer& pTrack);

    QString m_detailLocation;
    ConstWaveformDetailPointer m_pDetail;

    // Per column buffers, reused to avoid allocations on every frame
    std::vector<QVector3D> m_colors;
    std::vector<float> m_lineWidths;
    std::vector<float> m_scales;
    std::vector<float> m_traceLow;
    std::vector<float> m_traceHigh;
    std::vector<float> m_lineLow;
    std::vector<float> m_lineHigh;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererEnvelope);
};
