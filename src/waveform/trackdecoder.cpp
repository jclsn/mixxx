#include "waveform/trackdecoder.h"

#include <algorithm>

#include "analyzer/constants.h"
#include "sources/audiosourcestereoproxy.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/logger.h"
#include "util/samplebuffer.h"

namespace {

const mixxx::Logger kLogger("TrackDecoder");

} // namespace

namespace trackDecoder {

bool decodeStereo(const TrackPointer& pTrack,
        const std::function<void(mixxx::audio::SampleRate sampleRate, SINT frameLength)>& start,
        const std::function<void(const CSAMPLE* pStereoSamples, SINT frameCount)>& addFrames) {
    mixxx::AudioSource::OpenParams openParams;
    openParams.setChannelCount(mixxx::kAnalysisChannels);
    mixxx::AudioSourcePointer pAudioSource =
            SoundSourceProxy(pTrack).openAudioSource(openParams);
    if (!pAudioSource) {
        kLogger.warning() << "Failed to open" << pTrack->getLocation();
        return false;
    }

    // Mix down mono and multi channel (stem) files to stereo
    pAudioSource = mixxx::AudioSourceStereoProxy::create(
            pAudioSource, mixxx::kAnalysisFramesPerChunk);

    start(pAudioSource->getSignalInfo().getSampleRate(), pAudioSource->frameLength());
    mixxx::SampleBuffer sampleBuffer(mixxx::kAnalysisFramesPerChunk * mixxx::kAnalysisChannels);

    mixxx::IndexRange remainingFrameRange = pAudioSource->frameIndexRange();
    while (!remainingFrameRange.empty()) {
        const mixxx::IndexRange chunkFrameRange = remainingFrameRange.splitAndShrinkFront(
                std::min(mixxx::kAnalysisFramesPerChunk, remainingFrameRange.length()));
        const mixxx::ReadableSampleFrames readableSampleFrames =
                pAudioSource->readSampleFrames(mixxx::WritableSampleFrames(
                        chunkFrameRange, mixxx::SampleBuffer::WritableSlice(sampleBuffer)));

        const SINT frameCount = readableSampleFrames.frameLength();
        addFrames(readableSampleFrames.readableData(), frameCount);

        // The file may be shorter than announced, stop at its actual end
        if (frameCount < chunkFrameRange.length()) {
            break;
        }
    }

    return true;
}

} // namespace trackDecoder
