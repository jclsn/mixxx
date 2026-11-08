#include "waveform/highresolutionwaveform.h"

#include <algorithm>
#include <cmath>

#include "analyzer/analyzerwaveform.h"
#include "engine/filters/enginefilterbessel4.h"
#include "waveform/trackdecodecache.h"
#include "waveform/trackdecoder.h"

namespace {

// One entry per deck is enough for the common case
constexpr int kMaxCachedTracks = 4;

// Same layout as the texture of Waveform
int computeTextureStride(int size) {
    int stride = 256;
    while (stride * stride < size) {
        stride *= 2;
    }
    return stride;
}

unsigned char toUnsignedChar(CSAMPLE peak) {
    // Same scaling as the analysis waveform, see WaveformStride
    return static_cast<unsigned char>(std::min(255.0f, 255.0f * peak + 0.5f));
}

ConstHighResolutionWaveformPointer decode(const TrackPointer& pTrack) {
    std::unique_ptr<HighResolutionWaveform::Builder> pBuilder;
    const bool decoded = trackDecoder::decodeStereo(
            pTrack,
            [&pBuilder](mixxx::audio::SampleRate sampleRate, SINT frameLength) {
                pBuilder = std::make_unique<HighResolutionWaveform::Builder>(
                        sampleRate, frameLength);
            },
            [&pBuilder](const CSAMPLE* pStereoSamples, SINT frameCount) {
                pBuilder->addFrames(pStereoSamples, frameCount);
            });
    if (!decoded) {
        return nullptr;
    }

    return pBuilder->finish();
}

} // namespace

// The same filters as for the analysis waveform
struct HighResolutionWaveform::Builder::Filters {
    Filters(mixxx::audio::SampleRate sampleRate)
            : low(sampleRate, AnalyzerWaveform::kLowMidFreqHz),
              mid(sampleRate, AnalyzerWaveform::kLowMidFreqHz, AnalyzerWaveform::kMidHighFreqHz),
              high(sampleRate, AnalyzerWaveform::kMidHighFreqHz) {
        // Settle the filters for silence in the preroll to avoid ramping,
        // like the analysis waveform does
        low.assumeSettled();
        mid.assumeSettled();
        high.assumeSettled();
    }

    EngineFilterBessel4Low low;
    EngineFilterBessel4Band mid;
    EngineFilterBessel4High high;
};

HighResolutionWaveform::Builder::Builder(mixxx::audio::SampleRate sampleRate, SINT frameLength)
        : m_pFilters(std::make_unique<Filters>(sampleRate)),
          m_audioVisualRatio(static_cast<double>(sampleRate) / kVisualSampleRate),
          m_nextVisualFrameEnd(m_audioVisualRatio) {
    m_data.reserve(static_cast<std::size_t>(frameLength / m_audioVisualRatio + 1) * ChannelCount);
}

HighResolutionWaveform::Builder::~Builder() = default;

void HighResolutionWaveform::Builder::addFrames(const CSAMPLE* pStereoSamples, SINT frameCount) {
    const std::size_t sampleCount = static_cast<std::size_t>(frameCount) * ChannelCount;
    m_low.resize(sampleCount);
    m_mid.resize(sampleCount);
    m_high.resize(sampleCount);
    m_pFilters->low.process(pStereoSamples, m_low.data(), sampleCount);
    m_pFilters->mid.process(pStereoSamples, m_mid.data(), sampleCount);
    m_pFilters->high.process(pStereoSamples, m_high.data(), sampleCount);

    for (SINT frame = 0; frame < frameCount; ++frame) {
        for (int chn = 0; chn < ChannelCount; ++chn) {
            const SINT i = frame * ChannelCount + chn;
            CSAMPLE* pPeaks = m_peaks[chn];
            pPeaks[AllBand] = std::max(pPeaks[AllBand], std::fabs(pStereoSamples[i]));
            pPeaks[Low] = std::max(pPeaks[Low], std::fabs(m_low[i]));
            pPeaks[Mid] = std::max(pPeaks[Mid], std::fabs(m_mid[i]));
            pPeaks[High] = std::max(pPeaks[High], std::fabs(m_high[i]));
        }

        ++m_frame;
        if (m_frame >= m_nextVisualFrameEnd) {
            storeVisualFrame();
            m_nextVisualFrameEnd += m_audioVisualRatio;
        }
    }
}

void HighResolutionWaveform::Builder::storeVisualFrame() {
    for (int chn = 0; chn < ChannelCount; ++chn) {
        CSAMPLE* pPeaks = m_peaks[chn];
        m_data.push_back({toUnsignedChar(pPeaks[Low]),
                toUnsignedChar(pPeaks[Mid]),
                toUnsignedChar(pPeaks[High]),
                toUnsignedChar(pPeaks[AllBand])});
        std::fill(pPeaks, pPeaks + BandCount, 0.0f);
    }
}

std::shared_ptr<const HighResolutionWaveform> HighResolutionWaveform::Builder::finish() {
    if (m_frame > m_nextVisualFrameEnd - m_audioVisualRatio) {
        storeVisualFrame();
    }

    return std::make_shared<const HighResolutionWaveform>(m_audioVisualRatio, std::move(m_data));
}

HighResolutionWaveform::HighResolutionWaveform(
        double audioVisualRatio, std::vector<WaveformFilteredData> data)
        : m_audioVisualRatio(audioVisualRatio),
          m_data(std::move(data)),
          m_textureStride(computeTextureStride(static_cast<int>(m_data.size()))) {
}

namespace highResolutionWaveformCache {

ConstHighResolutionWaveformPointer request(const TrackPointer& pTrack) {
    static TrackDecodeCache<ConstHighResolutionWaveformPointer> s_cache(
            &decode, kMaxCachedTracks);
    return s_cache.request(pTrack);
}

} // namespace highResolutionWaveformCache
