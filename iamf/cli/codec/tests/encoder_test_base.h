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
#ifndef CLI_TESTS_ENCODER_TEST_BASE_H_
#define CLI_TESTS_ENCODER_TEST_BASE_H_

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using ::absl_testing::IsOk;

constexpr bool kValidateCodecDelay = true;

class EncoderTestBase {
 public:
  EncoderTestBase() = default;
  virtual ~EncoderTestBase() = 0;

 protected:
  virtual void ConstructEncoder() = 0;

  void InitExpectOk() {
    ConstructEncoder();
    EXPECT_THAT(encoder_->Initialize(kValidateCodecDelay), IsOk());
  }

  void EncodeAudioFrame(const std::vector<std::vector<int32_t>>& pcm_samples,
                        bool expected_encode_frame_is_ok = true) {
    // `EncodeAudioFrame` only passes on most of the data in the input
    // `AudioFrameWithData`. Simulate the timestamp to ensure frames are
    // returned in the correct order, but most other fields do not matter.
    const InternalTimestamp next_timestamp =
        cur_timestamp_ + static_cast<InternalTimestamp>(num_samples_per_frame_);
    auto partial_audio_frame_with_data =
        absl::WrapUnique(new AudioFrameWithData{
            .obu = AudioFrameObu(
                {
                    .type_specific_flag = false,
                    .num_samples_to_trim_at_end = 0,
                    .num_samples_to_trim_at_start = 0,
                },
                0, {}),
            .start_timestamp = cur_timestamp_,
            .end_timestamp = next_timestamp,
        });
    cur_timestamp_ = next_timestamp;

    // Encode the frame as requested.
    EXPECT_EQ(encoder_
                  ->EncodeAudioFrame(pcm_samples,
                                     std::move(partial_audio_frame_with_data))
                  .ok(),
              expected_encode_frame_is_ok);
  }

  // Finalizes the encoder and only validates the number and order of output
  // frames is consistent with the input frames. Returns the output audio
  // frames.
  std::list<AudioFrameWithData> FinalizeAndValidateOrderOnly(
      int expected_num_frames) {
    std::list<AudioFrameWithData> output_audio_frames;
    EXPECT_THAT(encoder_->Finalize(), IsOk());

    // Pop all the frames.
    for (int i = 0; i < expected_num_frames; i++) {
      EXPECT_THAT(encoder_->Pop(output_audio_frames), IsOk());
    }
    EXPECT_EQ(output_audio_frames.size(), expected_num_frames);

    // Check that there are no more frames left.
    EXPECT_FALSE(encoder_->FramesAvailable());

    ValidateOrder(output_audio_frames);
    return output_audio_frames;
  }

  // Finalizes the encoder and validates the content of the `audio_frame_`s
  // matches the expected data in the expected order.
  void FinalizeAndValidate() {
    const auto output_audio_frames =
        FinalizeAndValidateOrderOnly(expected_audio_frames_.size());

    // Validate the `audio_frame_` data is identical to the expected data.
    EXPECT_EQ(output_audio_frames.size(), expected_audio_frames_.size());
    auto output_iter = output_audio_frames.begin();
    auto expected_iter = expected_audio_frames_.begin();
    while (output_iter != output_audio_frames.end()) {
      EXPECT_EQ(output_iter->obu.audio_frame_, *expected_iter);
      output_iter++;
      expected_iter++;
    }
  }

  int num_channels_ = 1;
  uint32_t num_samples_per_frame_ = 1;
  std::unique_ptr<EncoderBase> encoder_;

  std::list<std::vector<uint8_t>> expected_audio_frames_ = {};

 private:
  void ValidateOrder(const std::list<AudioFrameWithData>& output_audio_frames) {
    // Validate that the timestamps match the expected order.
    int expected_start_timestamp_ = 0;
    for (const auto& output_audio_frame : output_audio_frames) {
      EXPECT_EQ(output_audio_frame.start_timestamp, expected_start_timestamp_);
      expected_start_timestamp_ += num_samples_per_frame_;
    }
  }

  InternalTimestamp cur_timestamp_ = 0;
};

}  // namespace iamf_tools

#endif  // CLI_TESTS_ENCODER_TEST_BASE_H_
