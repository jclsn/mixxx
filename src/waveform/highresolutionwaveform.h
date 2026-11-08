#pragma once

#include <memory>
#include <vector>

#include "audio/types.h"
#include "track/track_decl.h"
#include "util/types.h"
#include "waveform/waveform.h"

/// The analysis waveform (low / mid / high band peaks per channel) at a ten
/// times higher resolution, computed from the decoded track.
///
/// The analysis waveform's 441 Hz are too coarse when zooming in deeply, each
/// visual frame then spans many pixels. The high detail waveforms use this one
/// instead in that case. Unlike Waveform, it only stores the band data without
/// stems and without padding, to keep the memory usage low.
class HighResolutionWaveform {
  public:
    static constexpr int kVisualSampleRate = 4410;

    /// Accumulates interleaved stereo samples into the band peaks.
    class Builder {
      public:
        Builder(mixxx::audio::SampleRate sampleRate, SINT frameLength);
        ~Builder();

        void addFrames(const CSAMPLE* pStereoSamples, SINT frameCount);

        /// Adds the last, incomplete visual frame and returns the result.
        std::shared_ptr<const HighResolutionWaveform> finish();

      private:
        void storeVisualFrame();

        struct Filters;
        std::unique_ptr<Filters> m_pFilters;
        std::vector<CSAMPLE> m_low;
        std::vector<CSAMPLE> m_mid;
        std::vector<CSAMPLE> m_high;

        const double m_audioVisualRatio;
        std::vector<WaveformFilteredData> m_data;
        // Peaks of the current visual frame per channel and band
        CSAMPLE m_peaks[ChannelCount][BandCount]{};
        SINT m_frame{0};
        double m_nextVisualFrameEnd;
    };

    HighResolutionWaveform(double audioVisualRatio, std::vector<WaveformFilteredData> data);

    /// Audio frames per visual frame
    double getAudioVisualRatio() const {
        return m_audioVisualRatio;
    }
    /// Number of visual samples, i.e. twice the number of visual frames
    int getDataSize() const {
        return static_cast<int>(m_data.size());
    }
    /// Interleaved left / right
    const WaveformFilteredData* data() const {
        return m_data.data();
    }
    /// Width and height of a square texture the data fits into, like
    /// Waveform::getTextureStride()
    int getTextureStride() const {
        return m_textureStride;
    }

  private:
    const double m_audioVisualRatio;
    const std::vector<WaveformFilteredData> m_data;
    const int m_textureStride;
};

using ConstHighResolutionWaveformPointer = std::shared_ptr<const HighResolutionWaveform>;

namespace highResolutionWaveformCache {

/// Returns the high resolution waveform of the track if it is available.
/// Otherwise schedules computing it in the background and returns nullptr.
/// Thread-safe.
ConstHighResolutionWaveformPointer request(const TrackPointer& pTrack);

} // namespace highResolutionWaveformCache
