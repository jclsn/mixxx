#include "waveform/waveformdetail.h"

#include <algorithm>
#include <limits>

#include "waveform/trackdecodecache.h"
#include "waveform/trackdecoder.h"

namespace {

// One entry per deck is enough for the common case
constexpr int kMaxCachedTracks = 4;

std::int16_t toInt16(CSAMPLE sample) {
    constexpr float kScale = std::numeric_limits<std::int16_t>::max();
    return static_cast<std::int16_t>(std::clamp(sample, -1.f, 1.f) * kScale);
}

ConstWaveformDetailPointer decode(const TrackPointer& pTrack) {
    WaveformDetail::Builder builder;
    const bool decoded = trackDecoder::decodeStereo(
            pTrack,
            [&builder](mixxx::audio::SampleRate, SINT frameLength) {
                builder.reserve(frameLength);
            },
            [&builder](const CSAMPLE* pStereoSamples, SINT frameCount) {
                builder.addFrames(pStereoSamples, frameCount);
            });
    if (!decoded) {
        return nullptr;
    }

    return builder.finish();
}

} // namespace

void WaveformDetail::Builder::addFrames(const CSAMPLE* pStereoSamples, SINT frameCount) {
    for (SINT i = 0; i < frameCount; ++i) {
        const CSAMPLE sample = (pStereoSamples[2 * i] + pStereoSamples[2 * i + 1]) / 2;
        if (m_framesInBin == 0) {
            m_binMin = sample;
            m_binMax = sample;
        } else {
            m_binMin = std::min(m_binMin, sample);
            m_binMax = std::max(m_binMax, sample);
        }

        if (++m_framesInBin == kFramesPerBin) {
            m_bins.push_back({toInt16(m_binMin), toInt16(m_binMax)});
            m_framesInBin = 0;
        }
    }
}

std::shared_ptr<const WaveformDetail> WaveformDetail::Builder::finish() {
    if (m_framesInBin > 0) {
        m_bins.push_back({toInt16(m_binMin), toInt16(m_binMax)});
        m_framesInBin = 0;
    }

    return std::make_shared<const WaveformDetail>(std::move(m_bins));
}

namespace waveformDetailCache {

ConstWaveformDetailPointer request(const TrackPointer& pTrack) {
    static TrackDecodeCache<ConstWaveformDetailPointer> s_cache(&decode, kMaxCachedTracks);
    return s_cache.request(pTrack);
}

} // namespace waveformDetailCache
