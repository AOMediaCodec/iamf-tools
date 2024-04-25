#include "iamf/cli/audio_frame_decoder.h"

#include <cstdint>
#include <filesystem>
#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

constexpr DecodedUleb128 kCodecConfigId = 44;
constexpr uint32_t kSampleRate = 16000;
constexpr DecodedUleb128 kAudioElementId = 13;
constexpr DecodedUleb128 kSubstreamId = 0;
const int kNumChannels = 1;
const int kNumSamplesPerFrame = 8;
const int kBytesPerSample = 2;
constexpr absl::string_view kWavFilePrefix = "test";

TEST(AudioFrameDecoderTest, NoAudioFrames) {
  AudioFrameDecoder decoder(::testing::TempDir(), kWavFilePrefix);

  std::list<DecodedAudioFrame> decoded_audio_frames;
  EXPECT_TRUE(decoder.Decode({}, decoded_audio_frames).ok());

  EXPECT_TRUE(decoded_audio_frames.empty());
}

std::list<AudioFrameWithData> PrepareEncodedAudioFrames(
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {kSubstreamId}, codec_config_obus,
      audio_elements);

  std::list<AudioFrameWithData> encoded_audio_frames;
  encoded_audio_frames.push_back({
      .obu = AudioFrameObu(
          ObuHeader(), kSubstreamId,
          /*audio_frame=*/
          std::vector<uint8_t>(kNumSamplesPerFrame * kBytesPerSample, 0)),
      .start_timestamp = 0,
      .end_timestamp = kNumSamplesPerFrame,
      .raw_samples = std::vector<std::vector<int32_t>>(
          kNumSamplesPerFrame, std::vector<int32_t>(kNumChannels, 0)),
      .audio_element_with_data = &audio_elements.at(kAudioElementId),
  });

  return encoded_audio_frames;
}

TEST(AudioFrameDecoderTest, DecodeLpcmFrame) {
  AudioFrameDecoder decoder(::testing::TempDir(), kWavFilePrefix);

  // Encoded frames.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> encoded_audio_frames =
      PrepareEncodedAudioFrames(codec_config_obus, audio_elements);

  // Decode.
  std::list<DecodedAudioFrame> decoded_audio_frames;
  EXPECT_TRUE(decoder.Decode(encoded_audio_frames, decoded_audio_frames).ok());

  // Validate.
  EXPECT_EQ(decoded_audio_frames.size(), 1);
  const auto& decoded_audio_frame = decoded_audio_frames.back();
  EXPECT_EQ(decoded_audio_frame.substream_id, kSubstreamId);
  EXPECT_EQ(decoded_audio_frame.start_timestamp, 0);
  EXPECT_EQ(decoded_audio_frame.end_timestamp, kNumSamplesPerFrame);
  EXPECT_EQ(decoded_audio_frame.audio_element_with_data,
            &audio_elements.at(kAudioElementId));

  // For LPCM, decoded samples are identical to raw amples.
  EXPECT_EQ(decoded_audio_frame.decoded_samples,
            encoded_audio_frames.back().raw_samples);
}

std::filesystem::path GetFirstExpectedWavFile(uint32_t substream_id) {
  return std::filesystem::path(::testing::TempDir()) /
         absl::StrCat(kWavFilePrefix, "_decoded_substream_", substream_id,
                      ".wav");
}

void CleanupExpectedFileForSubstream(uint32_t substream_id) {
  std::filesystem::remove(GetFirstExpectedWavFile(substream_id));
}

void DecodeEightSampleAudioFrame(uint32_t num_samples_to_trim_at_end = 0,
                                 uint32_t num_samples_to_trim_at_start = 0) {
  CleanupExpectedFileForSubstream(kSubstreamId);
  AudioFrameDecoder decoder(::testing::TempDir(), kWavFilePrefix);
  // Encoded frames.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> encoded_audio_frames =
      PrepareEncodedAudioFrames(codec_config_obus, audio_elements);
  encoded_audio_frames.front().obu.header_.num_samples_to_trim_at_end =
      num_samples_to_trim_at_end;
  encoded_audio_frames.front().obu.header_.num_samples_to_trim_at_start =
      num_samples_to_trim_at_start;
  // Decode.
  std::list<DecodedAudioFrame> decoded_audio_frames;
  EXPECT_TRUE(decoder.Decode(encoded_audio_frames, decoded_audio_frames).ok());
}

TEST(AudioFrameDecoderTest, WritesDebuggingWavFileWithExpectedNumberOfSamples) {
  DecodeEightSampleAudioFrame();

  EXPECT_TRUE(std::filesystem::exists(GetFirstExpectedWavFile(kSubstreamId)));
  WavReader reader(GetFirstExpectedWavFile(kSubstreamId).string(),
                   kNumSamplesPerFrame);
  EXPECT_EQ(reader.remaining_samples(), kNumSamplesPerFrame);
}

TEST(AudioFrameDecoderTest, DebuggingWavFileHasSamplesTrimmed) {
  const uint32_t kNumSamplesToTrimAtEnd = 5;
  const uint32_t kNumSamplesToTrimAtStart = 2;
  DecodeEightSampleAudioFrame(kNumSamplesToTrimAtEnd, kNumSamplesToTrimAtStart);
  const uint32_t kExpectedNumSamples = 1;
  EXPECT_TRUE(std::filesystem::exists(GetFirstExpectedWavFile(kSubstreamId)));
  WavReader reader(GetFirstExpectedWavFile(kSubstreamId).string(),
                   kNumSamplesPerFrame);

  EXPECT_EQ(reader.remaining_samples(), kExpectedNumSamples);
}

// TODO(b/308073716): Add tests for more kinds of decoders.

}  // namespace
}  // namespace iamf_tools
