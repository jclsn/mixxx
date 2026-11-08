#pragma once

#include "nonglwaveformwidgetabstract.h"
#include "waveform/renderers/waveformrenderersignalbase.h"

class QWidget;

class SpectrumWaveformWidget : public NonGLWaveformWidgetAbstract {
    Q_OBJECT
  public:
    WaveformWidgetType::Type getType() const override {
        return WaveformWidgetType::Spectrum;
    }

    static inline bool useOpenGl() {
        return false;
    }
    static inline bool useOpenGles() {
        return false;
    }
    static inline bool useOpenGLShaders() {
        return false;
    }
    static inline bool useTextureForWaveform() {
        return false;
    }
    static inline WaveformWidgetCategory category() {
        return WaveformWidgetCategory::Software;
    }

  protected:
    void castToQWidget() override;
    void paintEvent(QPaintEvent* event) override;

  private:
    SpectrumWaveformWidget(const QString& group,
            QWidget* parent,
            WaveformRendererSignalBase::Options options);
    friend class WaveformWidgetFactory;
};
