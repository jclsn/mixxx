#pragma once

#include <QString>

#include "rendergraph/geometrynode.h"
#include "track/track_decl.h"
#include "util/class.h"
#include "waveform/highresolutionwaveform.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

namespace allshader {
class WaveformRendererSpectrum;
} // namespace allshader

/// Colors each waveform column by its spectral balance: bass heavy content
/// is drawn in red/orange, mids in magenta/pink and highs in blue/cyan.
/// Every column is drawn with a vertical gradient from a bright core at the
/// axis to a translucent edge, which gives the waveform a soft, glowing look.
/// With the SplitStereoSignal option, the left and right channel are colored
/// individually. With the HighDetail option, a higher resolution waveform is
/// used when zooming in deeply.
class allshader::WaveformRendererSpectrum final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::GeometryNode {
  public:
    explicit WaveformRendererSpectrum(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererSignalBase::Options options);

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    // Virtuals for rendergraph::Node
    void preprocess() override;

  private:
    bool preprocessInner();
    ConstHighResolutionWaveformPointer highResolutionWaveform(const TrackPointer& pTrack);
    template<typename Data>
    bool drawColumns(const Data* data, int dataSize);

    const bool m_splitStereoSignal;
    const bool m_highDetail;

    QString m_highResolutionLocation;
    ConstHighResolutionWaveformPointer m_pHighResolutionWaveform;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererSpectrum);
};
