#include <gtest/gtest.h>

#include <vector>

#include "waveform/waveformdetail.h"

namespace {

std::vector<CSAMPLE> stereoFrames(const std::vector<CSAMPLE>& left,
        const std::vector<CSAMPLE>& right) {
    std::vector<CSAMPLE> samples;
    for (std::size_t i = 0; i < left.size(); ++i) {
        samples.push_back(left[i]);
        samples.push_back(right[i]);
    }
    return samples;
}

} // namespace

TEST(WaveformDetailTest, keepsSignOfMonoMix) {
    // One bin of positive and one bin of negative samples
    std::vector<CSAMPLE> left(2 * WaveformDetail::kFramesPerBin, 0.5f);
    std::vector<CSAMPLE> right(2 * WaveformDetail::kFramesPerBin, 0.5f);
    for (int i = WaveformDetail::kFramesPerBin; i < 2 * WaveformDetail::kFramesPerBin; ++i) {
        left[i] = -1.0f;
        right[i] = 0.0f;
    }
    const std::vector<CSAMPLE> samples = stereoFrames(left, right);

    WaveformDetail::Builder builder;
    builder.addFrames(samples.data(), static_cast<SINT>(left.size()));
    const auto pDetail = builder.finish();

    ASSERT_EQ(2, pDetail->bins().size());
    EXPECT_EQ(16383, pDetail->bins()[0].min);
    EXPECT_EQ(16383, pDetail->bins()[0].max);
    EXPECT_EQ(-16383, pDetail->bins()[1].min);
    EXPECT_EQ(-16383, pDetail->bins()[1].max);
}

TEST(WaveformDetailTest, tracksMinAndMaxAcrossChunks) {
    // A ramp from -1 to 1 within one bin, added in two chunks
    std::vector<CSAMPLE> ramp;
    for (int i = 0; i < WaveformDetail::kFramesPerBin; ++i) {
        ramp.push_back(-1.0f + 2.0f * i / (WaveformDetail::kFramesPerBin - 1));
    }
    const std::vector<CSAMPLE> samples = stereoFrames(ramp, ramp);
    const SINT firstChunkFrames = 3;

    WaveformDetail::Builder builder;
    builder.addFrames(samples.data(), firstChunkFrames);
    builder.addFrames(samples.data() + 2 * firstChunkFrames,
            WaveformDetail::kFramesPerBin - firstChunkFrames);
    const auto pDetail = builder.finish();

    ASSERT_EQ(1, pDetail->bins().size());
    EXPECT_EQ(-32767, pDetail->bins()[0].min);
    EXPECT_EQ(32767, pDetail->bins()[0].max);
}

TEST(WaveformDetailTest, keepsIncompleteLastBin) {
    const std::vector<CSAMPLE> samples = stereoFrames({0.25f, 0.75f, 2.0f}, {0.25f, 0.75f, 2.0f});

    WaveformDetail::Builder builder;
    builder.addFrames(samples.data(), 3);
    const auto pDetail = builder.finish();

    ASSERT_EQ(1, pDetail->bins().size());
    EXPECT_EQ(8191, pDetail->bins()[0].min);
    // Clipped to the int16 range
    EXPECT_EQ(32767, pDetail->bins()[0].max);
}
