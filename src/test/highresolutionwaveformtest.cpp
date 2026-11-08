#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "waveform/highresolutionwaveform.h"

namespace {

constexpr auto kSampleRate = mixxx::audio::SampleRate(44100);
// 44100 Hz / 4410 Hz
constexpr int kFramesPerVisualFrame = 10;

std::vector<CSAMPLE> stereoSine(
        double frequency, CSAMPLE leftAmplitude, CSAMPLE rightAmplitude, SINT frameCount) {
    std::vector<CSAMPLE> samples;
    for (SINT i = 0; i < frameCount; ++i) {
        const double value = std::sin(2 * M_PI * frequency * i / kSampleRate);
        samples.push_back(static_cast<CSAMPLE>(leftAmplitude * value));
        samples.push_back(static_cast<CSAMPLE>(rightAmplitude * value));
    }
    return samples;
}

} // namespace

TEST(HighResolutionWaveformTest, hasTenTimesTheAnalysisResolution) {
    const SINT frameCount = kSampleRate;
    const std::vector<CSAMPLE> samples(frameCount * 2, 0.0f);

    HighResolutionWaveform::Builder builder(kSampleRate, frameCount);
    builder.addFrames(samples.data(), frameCount);
    const auto pWaveform = builder.finish();

    EXPECT_EQ(kFramesPerVisualFrame, pWaveform->getAudioVisualRatio());
    // Two channels per visual frame
    EXPECT_EQ(2 * HighResolutionWaveform::kVisualSampleRate, pWaveform->getDataSize());
    EXPECT_GE(pWaveform->getTextureStride() * pWaveform->getTextureStride(),
            pWaveform->getDataSize());
}

TEST(HighResolutionWaveformTest, storesPeaksPerChannel) {
    const SINT frameCount = 100 * kFramesPerVisualFrame;
    std::vector<CSAMPLE> samples;
    for (SINT i = 0; i < frameCount; ++i) {
        samples.push_back(0.5f);
        samples.push_back(-0.25f);
    }

    HighResolutionWaveform::Builder builder(kSampleRate, frameCount);
    builder.addFrames(samples.data(), frameCount);
    const auto pWaveform = builder.finish();

    ASSERT_EQ(200, pWaveform->getDataSize());
    // Same scaling as the analysis waveform
    EXPECT_EQ(128, pWaveform->data()[100].all);
    EXPECT_EQ(64, pWaveform->data()[101].all);
}

TEST(HighResolutionWaveformTest, splitsIntoBands) {
    const SINT frameCount = kSampleRate / 2;
    const std::vector<CSAMPLE> bass = stereoSine(60, 0.8f, 0.8f, frameCount);
    const std::vector<CSAMPLE> treble = stereoSine(8000, 0.8f, 0.8f, frameCount);

    HighResolutionWaveform::Builder bassBuilder(kSampleRate, frameCount);
    bassBuilder.addFrames(bass.data(), frameCount);
    HighResolutionWaveform::Builder trebleBuilder(kSampleRate, frameCount);
    trebleBuilder.addFrames(treble.data(), frameCount);

    // Peaks over a range in the middle, after the filters have settled
    const auto maxBands = [](const ConstHighResolutionWaveformPointer& pWaveform) {
        WaveformFilteredData peaks{};
        for (int i = 2 * 900; i < 2 * 1100; ++i) {
            const WaveformFilteredData& data = pWaveform->data()[i];
            peaks.low = std::max(peaks.low, data.low);
            peaks.high = std::max(peaks.high, data.high);
        }
        return peaks;
    };
    const WaveformFilteredData bassPeaks = maxBands(bassBuilder.finish());
    const WaveformFilteredData treblePeaks = maxBands(trebleBuilder.finish());
    EXPECT_GT(bassPeaks.low, 4 * bassPeaks.high);
    EXPECT_GT(treblePeaks.high, 4 * treblePeaks.low);
}
