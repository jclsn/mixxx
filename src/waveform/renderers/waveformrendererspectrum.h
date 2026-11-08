#pragma once

#include "util/class.h"
#include "waveformrenderersignalbase.h"

/// Software (QPainter) version of allshader::WaveformRendererSpectrum, used
/// when waveform acceleration is disabled. It draws the envelope only.
class WaveformRendererSpectrum : public WaveformRendererSignalBase {
  public:
    explicit WaveformRendererSpectrum(
            WaveformWidgetRenderer* waveformWidget, ::WaveformRendererSignalBase::Options options);

    void onSetup(const QDomNode& node) override;

    void draw(QPainter* painter, QPaintEvent* event) override;

  private:
    DISALLOW_COPY_AND_ASSIGN(WaveformRendererSpectrum);
};
