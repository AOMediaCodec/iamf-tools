#include "iamf/cli/codec/decoder_base.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "iamf/obu/substream_channel_count.h"

namespace iamf_tools {
namespace {

// A mock to be able to test the abstract base class.
class MockDecoder : public DecoderBase {
 public:
  MockDecoder(SubstreamChannelCount channel_count, int num_samples_per_channel)
      : DecoderBase(channel_count, num_samples_per_channel) {}

  // Helpers to expose the values for expectations.
  int num_channels() const { return channel_count_.num_channels(); }
  int GetNumSamplesPerChannel() const { return num_samples_per_channel_; }

  // Unimplemented implementations for base class pure virtual methods
  // that we won't test.
  absl::Status DecodeAudioFrame(
      absl::Span<const uint8_t> encoded_frame) override {
    return absl::UnimplementedError("Not implemented");
  }
};

TEST(DecoderBaseTest, TestConstruction) {
  const int kExpectedNumChannels = 2;
  const int kExpectedNumSamplesPerChannel = 5400;
  MockDecoder decoder(SubstreamChannelCount::MakeCoupled(),
                      kExpectedNumSamplesPerChannel);
  EXPECT_EQ(decoder.num_channels(), kExpectedNumChannels);
  EXPECT_EQ(decoder.GetNumSamplesPerChannel(), kExpectedNumSamplesPerChannel);
}

}  // namespace
}  // namespace iamf_tools
