#include "waveform/waveformdetail.h"

#include <QList>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QtConcurrentRun>
#include <algorithm>
#include <limits>

#include "analyzer/constants.h"
#include "sources/audiosourcestereoproxy.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/logger.h"
#include "util/samplebuffer.h"

namespace {

const mixxx::Logger kLogger("WaveformDetail");

// One entry per deck is enough for the common case
constexpr int kMaxCachedTracks = 4;

struct CacheEntry {
    QString location;
    // nullptr if decoding failed, to avoid retrying it on every frame
    ConstWaveformDetailPointer pDetail;
};

struct CacheState {
    QMutex mutex;
    // Most recently decoded first
    QList<CacheEntry> entries;
    QSet<QString> pending;
};

std::shared_ptr<CacheState> cacheState() {
    static const auto s_pState = std::make_shared<CacheState>();
    return s_pState;
}

std::int16_t toInt16(CSAMPLE sample) {
    constexpr float kScale = std::numeric_limits<std::int16_t>::max();
    return static_cast<std::int16_t>(std::clamp(sample, -1.f, 1.f) * kScale);
}

ConstWaveformDetailPointer decode(const TrackPointer& pTrack) {
    mixxx::AudioSource::OpenParams openParams;
    openParams.setChannelCount(mixxx::kAnalysisChannels);
    mixxx::AudioSourcePointer pAudioSource =
            SoundSourceProxy(pTrack).openAudioSource(openParams);
    if (!pAudioSource) {
        kLogger.warning() << "Failed to open" << pTrack->getLocation();
        return nullptr;
    }

    // Mix down mono and multi channel (stem) files to stereo
    pAudioSource = mixxx::AudioSourceStereoProxy::create(
            pAudioSource, mixxx::kAnalysisFramesPerChunk);

    WaveformDetail::Builder builder;
    builder.reserve(pAudioSource->frameLength());
    mixxx::SampleBuffer sampleBuffer(mixxx::kAnalysisFramesPerChunk * mixxx::kAnalysisChannels);

    mixxx::IndexRange remainingFrameRange = pAudioSource->frameIndexRange();
    while (!remainingFrameRange.empty()) {
        const mixxx::IndexRange chunkFrameRange = remainingFrameRange.splitAndShrinkFront(
                std::min(mixxx::kAnalysisFramesPerChunk, remainingFrameRange.length()));
        const mixxx::ReadableSampleFrames readableSampleFrames =
                pAudioSource->readSampleFrames(mixxx::WritableSampleFrames(
                        chunkFrameRange, mixxx::SampleBuffer::WritableSlice(sampleBuffer)));

        const SINT frameCount = readableSampleFrames.frameLength();
        builder.addFrames(readableSampleFrames.readableData(), frameCount);

        // The file may be shorter than announced, stop at its actual end
        if (frameCount < chunkFrameRange.length()) {
            break;
        }
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
    if (!pTrack) {
        return nullptr;
    }

    const QString location = pTrack->getLocation();
    const std::shared_ptr<CacheState> pState = cacheState();
    const QMutexLocker locker(&pState->mutex);

    const auto it = std::find_if(pState->entries.begin(),
            pState->entries.end(),
            [&location](const CacheEntry& entry) {
                return entry.location == location;
            });
    if (it != pState->entries.end()) {
        return it->pDetail;
    }

    if (pState->pending.contains(location)) {
        return nullptr;
    }

    pState->pending.insert(location);
    // Capturing the track keeps it alive while decoding, the same way the
    // cover art loading does.
    (void)QtConcurrent::run([pTrack, location, pState] {
        ConstWaveformDetailPointer pDetail = decode(pTrack);
        kLogger.debug() << "Decoded" << location;

        const QMutexLocker locker(&pState->mutex);
        pState->pending.remove(location);
        pState->entries.prepend(CacheEntry{location, std::move(pDetail)});
        while (pState->entries.size() > kMaxCachedTracks) {
            pState->entries.removeLast();
        }
    });

    return nullptr;
}

} // namespace waveformDetailCache
