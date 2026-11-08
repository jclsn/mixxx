#pragma once

#include <functional>

#include "audio/types.h"
#include "track/track_decl.h"
#include "util/types.h"

namespace trackDecoder {

/// Decodes the whole track, mixed down to stereo, and passes the interleaved
/// samples chunk by chunk to addFrames. Before that, start is called with the
/// sample rate and the announced length of the track. Returns false if the
/// track could not be opened.
///
/// Decoding takes a while, so this is meant to be called from a background
/// thread.
bool decodeStereo(const TrackPointer& pTrack,
        const std::function<void(mixxx::audio::SampleRate sampleRate, SINT frameLength)>& start,
        const std::function<void(const CSAMPLE* pStereoSamples, SINT frameCount)>& addFrames);

} // namespace trackDecoder
