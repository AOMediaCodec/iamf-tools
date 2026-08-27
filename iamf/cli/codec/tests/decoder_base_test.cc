/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
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
