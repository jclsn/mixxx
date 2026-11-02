#pragma once

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

namespace allshader {
class WaveformRendererSpectrum;
} // namespace allshader

/// Colors each waveform column by its spectral balance: bass heavy content
/// is drawn in red/orange, mids in magenta/pink and highs in blue/cyan.
/// Every column is drawn with a vertical gradient from a bright core at the
/// axis to a translucent edge, which gives the waveform a soft, glowing look.
/// With the SplitStereoSignal option, the left and right channel are colored
/// individually.
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

    const bool m_splitStereoSignal;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererSpectrum);
};
