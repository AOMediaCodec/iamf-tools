#include "iamf/cli/encoder_base.h"

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/audio_frame.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/codec_config.h"
#include "iamf/ia.h"
#include "iamf/obu_header.h"

namespace iamf_tools {
namespace {

using testing::Return;

constexpr DecodedUleb128 kCodecConfigId = 159;

class MockEncoder : public EncoderBase {
 public:
  MockEncoder()
      : EncoderBase(false,
                    CodecConfigObu(ObuHeader(), kCodecConfigId, CodecConfig()),
                    0) {}

  MOCK_METHOD(
      absl::Status, EncodeAudioFrame,
      (int input_bit_depth, const std::vector<std::vector<int32_t>>& samples,
       std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data),
      (override));

  MOCK_METHOD(absl::Status, InitializeEncoder, (), (override));
  MOCK_METHOD(absl::Status, SetNumberOfSamplesToDelayAtStart, (), (override));
};

TEST(EncoderBaseTest, InitializeSucceeds) {
  MockEncoder encoder;
  EXPECT_CALL(encoder, InitializeEncoder()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(encoder, SetNumberOfSamplesToDelayAtStart())
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_TRUE(encoder.Initialize().ok());
}

TEST(EncoderBaseTest, InitializeFailsWhenInitializeEncoderFails) {
  MockEncoder encoder;
  EXPECT_CALL(encoder, InitializeEncoder())
      .WillOnce(Return(absl::UnknownError("")));

  EXPECT_EQ(encoder.Initialize().code(), absl::StatusCode::kUnknown);
}

TEST(EncoderBaseTest,
     InitializeFailsWhenSetNumberOfSamplesToDelayAtStartFails) {
  MockEncoder encoder;
  EXPECT_CALL(encoder, InitializeEncoder()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(encoder, SetNumberOfSamplesToDelayAtStart())
      .WillOnce(Return(absl::UnknownError("")));

  EXPECT_EQ(encoder.Initialize().code(), absl::StatusCode::kUnknown);
}

TEST(EncoderBaseTest, FinalizeAndFlushAppendAudioFrames) {
  MockEncoder encoder;

  // Expect the returned `audio_frames` is just the same as before calling
  // `FinalizeAndFlush()`, because we know an empty list
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
  EXPECT_TRUE(encoder.FinalizeAndFlush(audio_frames).ok());

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
