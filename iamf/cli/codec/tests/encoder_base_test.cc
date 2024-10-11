/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/codec/encoder_base.h"

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

using testing::Return;

constexpr DecodedUleb128 kCodecConfigId = 159;
constexpr bool kValidateCodecDelay = true;
constexpr bool kDontValidateCodecDelay = false;

class MockEncoder : public EncoderBase {
 public:
  MockEncoder()
      : EncoderBase(CodecConfigObu(ObuHeader(), kCodecConfigId, CodecConfig()),
                    0) {}

  MOCK_METHOD(
      absl::Status, EncodeAudioFrame,
      (int input_bit_depth, const std::vector<std::vector<int32_t>>& samples,
       std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data),
      (override));

  MOCK_METHOD(absl::Status, InitializeEncoder, (), (override));
  MOCK_METHOD(absl::Status, SetNumberOfSamplesToDelayAtStart,
              (bool validate_codec_delay), (override));
};

TEST(EncoderBaseTest, InitializeSucceeds) {
  MockEncoder encoder;
  EXPECT_CALL(encoder, InitializeEncoder()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(encoder, SetNumberOfSamplesToDelayAtStart(kValidateCodecDelay))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_THAT(encoder.Initialize(kValidateCodecDelay), IsOk());
}

TEST(EncoderBaseTest, InitializeFailsWhenInitializeEncoderFails) {
  MockEncoder encoder;
  EXPECT_CALL(encoder, InitializeEncoder())
      .WillOnce(Return(absl::UnknownError("")));

  EXPECT_EQ(encoder.Initialize(kValidateCodecDelay).code(),
            absl::StatusCode::kUnknown);
}

TEST(EncoderBaseTest,
     InitializePropagatesValidatePreSkipToSetNumberOfSamplesToDelayAtStart) {
  MockEncoder encoder;
  EXPECT_CALL(encoder,
              SetNumberOfSamplesToDelayAtStart(kDontValidateCodecDelay));

  encoder.Initialize(kDontValidateCodecDelay).IgnoreError();
}

TEST(EncoderBaseTest, SetNumberOfSamplesToDelayAtStartDefaultsToSuccess) {
  MockEncoder encoder;
  EXPECT_CALL(encoder, SetNumberOfSamplesToDelayAtStart(kValidateCodecDelay));

  EXPECT_THAT(encoder.Initialize(kValidateCodecDelay), IsOk());
}

TEST(EncoderBaseTest,
     InitializeFailsWhenSetNumberOfSamplesToDelayAtStartFails) {
  MockEncoder encoder;
  EXPECT_CALL(encoder, InitializeEncoder()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(encoder, SetNumberOfSamplesToDelayAtStart(kValidateCodecDelay))
      .WillOnce(Return(absl::UnknownError("")));

  EXPECT_EQ(encoder.Initialize(kValidateCodecDelay).code(),
            absl::StatusCode::kUnknown);
}

TEST(EncoderBaseTest, FinalizeAndPopAppendNothingWhenNoFramesAvailable) {
  MockEncoder encoder;

  // Expect the returned `audio_frames` is just the same as before calling
  // `Finalize()` and `Pop()`, because we know an empty list
  // (`finalized_audio_frames_`) is appended at the end.
  const DecodedUleb128 kSubstreamId = 137;
  const int32_t kStartTimestamp = 77;
  const int32_t kEndTimestamp = 101;
  const std::vector<uint8_t> kAudioFrame = {1, 7, 5, 3};
  AudioFrameObu obu(ObuHeader(), kSubstreamId, kAudioFrame);
  std::list<AudioFrameWithData> audio_frames;
  audio_frames.emplace_back(AudioFrameWithData{
      .obu = std::move(obu),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .audio_element_with_data = nullptr,
  });

  EXPECT_THAT(encoder.Finalize(), IsOk());

  // Since nothing has been added, there are no frames available.
  EXPECT_FALSE(encoder.FramesAvailable());

  // `Pop()` still works, just adding nothing.
  EXPECT_THAT(encoder.Pop(audio_frames), IsOk());

  // Expect the `audio_frames` is unaltered.
  ASSERT_EQ(audio_frames.size(), 1);
  const auto& only_frame = audio_frames.back();
  EXPECT_EQ(only_frame.obu.GetSubstreamId(), kSubstreamId);
  EXPECT_EQ(only_frame.obu.audio_frame_, kAudioFrame);
  EXPECT_EQ(only_frame.start_timestamp, kStartTimestamp);
  EXPECT_EQ(only_frame.end_timestamp, kEndTimestamp);
  EXPECT_EQ(only_frame.audio_element_with_data, nullptr);
}

TEST(EncoderBaseTest, DefaultZeroNumberOfSamplesToDelayAtStart) {
  MockEncoder encoder;

  EXPECT_EQ(encoder.GetNumberOfSamplesToDelayAtStart(), 0);
}

}  // namespace
}  // namespace iamf_tools
