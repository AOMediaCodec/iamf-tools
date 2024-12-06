#include "iamf/cli/codec/decoder_base.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

// A mock to be able to test the abstract base class.
class MockDecoder : public DecoderBase {
 public:
  MockDecoder(int num_channels, int num_samples_per_channel)
      : DecoderBase(num_channels, num_samples_per_channel) {}

  // Helpers to expose the values for expectations.
  int GetNumChannels() const { return num_channels_; }
  int GetNumSamplesPerChannel() const { return num_samples_per_channel_; }

  // Unimplemented implementations for base class pure virtual methods
  // that we won't test.
  absl::Status Initialize() override {
    return absl::UnimplementedError("Not implemented");
  }

  absl::Status DecodeAudioFrame(
      const std::vector<uint8_t>& encoded_frame) override {
    return absl::UnimplementedError("Not implemented");
  }
};

TEST(DecoderBaseTest, TestConstruction) {
  const int kExpectedNumChannels = 9;
  const int kExpectedNumSamplesPerChannel = 5400;
  MockDecoder decoder(kExpectedNumChannels, kExpectedNumSamplesPerChannel);
  EXPECT_EQ(decoder.GetNumChannels(), kExpectedNumChannels);
  EXPECT_EQ(decoder.GetNumSamplesPerChannel(), kExpectedNumSamplesPerChannel);
}

}  // namespace
}  // namespace iamf_tools
