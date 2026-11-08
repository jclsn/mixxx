#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "track/track_decl.h"
#include "util/types.h"

/// Signed peaks of the decoded audio at a much higher resolution than the
/// analysis waveform.
///
/// The analysis waveform only stores magnitudes at 441 Hz, which is too coarse
/// to show the shape of the signal. The detail keeps the sign, so the actual
/// wave shape (e.g. the sine of a kick drum) can be drawn.
class WaveformDetail {
  public:
    /// Number of audio frames summarized by one bin.
    static constexpr int kFramesPerBin = 8;

    /// Minimum and maximum of the mono mix within one bin, scaled to the
    /// int16 range.
    struct Bin {
        std::int16_t min;
        std::int16_t max;
    };

    /// Accumulates interleaved stereo samples into bins.
    class Builder {
      public:
        void reserve(SINT frameCount) {
            m_bins.reserve(frameCount / kFramesPerBin + 1);
        }

        void addFrames(const CSAMPLE* pStereoSamples, SINT frameCount);

        /// Adds the last, incomplete bin and returns the result.
        std::shared_ptr<const WaveformDetail> finish();

      private:
        std::vector<Bin> m_bins;
        CSAMPLE m_binMin{0};
        CSAMPLE m_binMax{0};
        int m_framesInBin{0};
    };

    explicit WaveformDetail(std::vector<Bin> bins)
            : m_bins(std::move(bins)) {
    }

    const std::vector<Bin>& bins() const {
        return m_bins;
    }

  private:
    const std::vector<Bin> m_bins;
};

using ConstWaveformDetailPointer = std::shared_ptr<const WaveformDetail>;

/// Decodes waveform details in the background and keeps those of the most
/// recently requested tracks. All functions are thread-safe, so they can be
/// called from the render thread.
namespace waveformDetailCache {

/// Returns the detail of the track if it is available. Otherwise schedules
/// decoding it in the background and returns nullptr.
ConstWaveformDetailPointer request(const TrackPointer& pTrack);

} // namespace waveformDetailCache
