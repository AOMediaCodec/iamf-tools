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

#include "iamf/cli/itu_1770_4/loudness_calculator_itu_1770_4.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/sample_processing_utils.h"
#include "iamf/obu/mix_presentation.h"
#include "include/ebur128_analyzer.h"

namespace iamf_tools {

namespace {
// This implementation flattens data to interleaved format before passing to the
// library.
constexpr auto kInterleavesSampleLayout =
    loudness::EbuR128Analyzer::INTERLEAVED;

absl::Status FloatToQ7_8WithDebuggingMessage(float value, int16_t& output,
                                             absl::string_view context) {
  if (const auto& status = FloatToQ7_8(value, output); !status.ok()) {
    // Prepend some better context to the error message.
    return absl::Status(status.code(), absl::StrCat("Failed to set ", context,
                                                    "`FloatToQ7_8` returned: ",
                                                    status.message()));
  }
  return absl::OkStatus();
}

bool IncludeTruePeak(uint8_t info_type) {
  return info_type & LoudnessInfo::kTruePeak;
}

// Returns the channel weights as per Table 4 and Table 5 of ITU-1770-4 when the
// channels are ordered according to the `iamf-tools` output order (see
// iamf/cli/testdata/README#Output-wav-files).
absl::StatusOr<std::vector<float>> GetItu1770_4ChannelWeights(
    const Layout& layout) {
  switch (layout.layout_type) {
    using enum Layout::LayoutType;
    case Layout::kLayoutTypeLoudspeakersSsConvention: {
      const auto& ss_layout =
          std::get<LoudspeakersSsConventionLayout>(layout.specific_layout);

      using enum LoudspeakersSsConventionLayout::SoundSystem;
      static const absl::NoDestructor<absl::flat_hash_map<
          LoudspeakersSsConventionLayout::SoundSystem, std::vector<float>>>
          kSsLayoutToItu1440_4Weights({
              {kSoundSystemA_0_2_0, {1.0f, 1.0f}},
              {kSoundSystemB_0_5_0, {1.0f, 1.0f, 1.0f, 0.0f, 1.41f, 1.41f}},
              {kSoundSystemC_2_5_0,
               {1.0f, 1.0f, 1.0f, 0.0f, 1.41f, 1.41f, 1.0f, 1.0f}},
              {kSoundSystemD_4_5_0,
               {1.0f, 1.0f, 1.0f, 0.0f, 1.41f, 1.41f, 1.0f, 1.0f, 1.0f, 1.0f}},
              {kSoundSystemE_4_5_1,
               {1.0f, 1.0f, 1.0f, 0.0f, 1.41f, 1.41f, 1.0f, 1.0f, 1.0f, 1.0f,
                1.0f}},
              {kSoundSystemF_3_7_0,
               {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.41f, 1.41f, 1.0f, 1.0f, 1.0f,
                0.0f, 0.0f}},
              {kSoundSystemG_4_9_0,
               {1.0f, 1.0f, 1.0f, 0.0f, 1.41f, 1.41f, 1.0f, 1.0f, 1.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 1.0f}},
              {kSoundSystemH_9_10_3,
               {1.41f, 1.41f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                1.0f,  0.0f,  1.41f, 1.41, 1.0f, 1.0f, 1.0f, 1.0f,
                1.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f}},
              {kSoundSystemI_0_7_0,
               {1.0f, 1.0f, 1.0f, 0.0f, 1.41f, 1.41f, 1.0f, 1.0f}},
              {kSoundSystemJ_4_7_0,
               {1.0f, 1.0f, 1.0f, 0.0f, 1.41f, 1.41f, 1.0f, 1.0f, 1.0f, 1.0f,
                1.0f, 1.0f}},
              {kSoundSystem10_2_7_0,
               {1.0f, 1.0f, 1.0f, 0.0f, 1.41f, 1.41f, 1.0f, 1.0f, 1.0f, 1.0f}},
              {kSoundSystem11_2_3_0, {1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f}},
              {kSoundSystem12_0_1_0, {1.0f}},
              {kSoundSystem13_6_9_0,
               {1.41f, 1.41f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.41f, 1.41,
                1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}},
          });

      auto it = kSsLayoutToItu1440_4Weights->find(ss_layout.sound_system);
      if (it == kSsLayoutToItu1440_4Weights->end()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Weights are not known for sound_system= ",
                         ss_layout.sound_system));
      }
      return it->second;
    }
    case Layout::kLayoutTypeBinaural:
      return std::vector<float>{1.0f, 1.0f};
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Weights are not known for reserved layout_type= ",
                       layout.layout_type));
  }
}

// Flattens to the output buffer, and returns a span to the relevant slice of
// data.
absl::StatusOr<absl::Span<const uint8_t>> FlattenToInterleavedPcm(
    absl::Span<const absl::Span<const int32_t>> channel_time_samples,
    size_t max_num_samples_per_frame, int32_t expected_num_channels,
    int32_t bit_depth, std::vector<uint8_t>& interleaved_pcm_buffer) {
  if (channel_time_samples.size() != expected_num_channels) {
    return absl::InvalidArgumentError(
        absl::StrCat("Input samples are not stored in ", expected_num_channels,
                     " channels."));
  }

  const size_t num_samples_per_channel =
      channel_time_samples.empty() ? 0 : channel_time_samples[0].size();
  if (num_samples_per_channel > max_num_samples_per_frame) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Input number of samples per channel (", num_samples_per_channel,
        ") is greater than the pre-configured number of samples per frame (",
        max_num_samples_per_frame, ")"));
  }

  const bool all_channels_have_expected_num_samples =
      std::all_of(channel_time_samples.begin(), channel_time_samples.end(),
                  [&num_samples_per_channel](const auto& channel) {
                    return channel.size() == num_samples_per_channel;
                  });
  if (!all_channels_have_expected_num_samples) {
    return absl::InvalidArgumentError(
        absl::StrCat("Detected a channel which does not contain ",
                     num_samples_per_channel, " ticks."));
  }

  size_t write_position = 0;
  const size_t interleaved_pcm_buffer_size = interleaved_pcm_buffer.size();
  for (int t = 0; t < num_samples_per_channel; t++) {
    for (int c = 0; c < expected_num_channels; ++c) {
      // The buffer is pre-allocated to fit the largest accepted input. But for
      // safety, check the write position does not exceed the buffer size.
      CHECK(write_position < interleaved_pcm_buffer_size);
      // `WritePcmSample` requires the input sample to be in the upper
      // bits of the first argument.
      RETURN_IF_NOT_OK(WritePcmSample(
          static_cast<uint32_t>(channel_time_samples[c][t]), bit_depth,
          /*big_endian=*/false, interleaved_pcm_buffer.data(), write_position));
    }
  }

  return absl::MakeConstSpan(interleaved_pcm_buffer).first(write_position);
}

}  // namespace

std::unique_ptr<LoudnessCalculatorItu1770_4>
LoudnessCalculatorItu1770_4::CreateForLayout(
    const MixPresentationLayout& layout, uint32_t num_samples_per_frame,
    int32_t rendered_sample_rate, int32_t rendered_bit_depth) {
  const auto weights = GetItu1770_4ChannelWeights(layout.loudness_layout);
  if (!weights.ok()) {
    LOG(ERROR) << "Failed to get channel weights: " << weights.status();
    return nullptr;
  }

  // Configure a bit-depth to measure loudness on.
  int32_t bit_depth_to_measure_loudness;
  loudness::EbuR128Analyzer::SampleFormat sample_format;
  switch (rendered_bit_depth) {
    case 16:
      bit_depth_to_measure_loudness = 16;
      sample_format = loudness::EbuR128Analyzer::S16;
      break;
    case 24:
    case 32:
      // The underlying library does not support 24-bit input, so intentionally
      // handle it the same as 32-bit.
      bit_depth_to_measure_loudness = 32;
      sample_format = loudness::EbuR128Analyzer::S32;
      break;
    default:
      LOG(ERROR) << "Unsupported bit depth: " << rendered_bit_depth;
      return nullptr;
  }
  // Later code assumes this is always a multiple of 8.
  CHECK_EQ(bit_depth_to_measure_loudness % 8, 0);

  const size_t num_channels = weights->size();

  const bool enable_true_peak_measurement =
      IncludeTruePeak(layout.loudness.info_type);
  LOG(INFO) << "Creating AudioLoudness1770:";
  LOG(INFO) << "  num_channels= " << num_channels;
  LOG(INFO) << "  sample_rate= " << rendered_sample_rate;
  LOG(INFO) << "  bit_depth_to_measure_loudness= "
            << bit_depth_to_measure_loudness;
  LOG(INFO) << "  sample_format= " << sample_format;
  LOG(INFO) << "  enable_true_peak_measurement= "
            << enable_true_peak_measurement;
  LOG(INFO) << "  weights= " << absl::StrJoin(*weights, ", ");

  return absl::WrapUnique(new LoudnessCalculatorItu1770_4(
      num_samples_per_frame, num_channels, *weights, rendered_sample_rate,
      bit_depth_to_measure_loudness, sample_format, layout.loudness,
      enable_true_peak_measurement));
}

absl::Status LoudnessCalculatorItu1770_4::AccumulateLoudnessForSamples(
    absl::Span<const absl::Span<const int32_t>> channel_time_samples) {
  auto interleaved_pcm_span = FlattenToInterleavedPcm(
      channel_time_samples, num_samples_per_frame_, num_channels_,
      bit_depth_to_measure_loudness_, interleaved_pcm_buffer_);
  if (!interleaved_pcm_span.ok()) {
    return interleaved_pcm_span.status();
  }

  const int64_t num_samples_per_channel =
      channel_time_samples.empty() ? 0 : channel_time_samples[0].size();
  ebu_r128_analyzer_.Process(
      static_cast<const void*>(interleaved_pcm_span->data()),
      num_samples_per_channel, sample_format_, kInterleavesSampleLayout);

  return absl::OkStatus();
}

absl::StatusOr<LoudnessInfo> LoudnessCalculatorItu1770_4::QueryLoudness()
    const {
  constexpr float kMinQ7_8 = -128.0;
  constexpr float kMaxQ7_8 = 128.0 - 1 / 256.0;

  float calculated_integrated_loudness = kMinQ7_8;
  float calculated_digital_peak = kMinQ7_8;
  float calculated_true_peak = kMinQ7_8;

  if (const auto& integrated_loudness =
          ebu_r128_analyzer_.GetRelativeGatedIntegratedLoudness();
      !integrated_loudness.has_value()) {
    // TODO(b/274740345): Figure out if there is a better solution for short
    //                    audio sequences.
    LOG(WARNING) << "Loudness cannot be computed or is too low; "
                 << "using minimal value representable by Q7.8.";
    // OK. Fallback to the default values.
  } else {
    calculated_integrated_loudness =
        std::clamp(*integrated_loudness, kMinQ7_8, kMaxQ7_8);
    calculated_digital_peak =
        std::clamp(ebu_r128_analyzer_.digital_peak_dbfs(), kMinQ7_8, kMaxQ7_8);
    calculated_true_peak =
        std::clamp(ebu_r128_analyzer_.true_peak_dbfs(), kMinQ7_8, kMaxQ7_8);
  }

  // Initialize the output based on the user-provided loudness info. This allows
  // loudnesses that this module does not support (i.e. anchored loudness,
  // loudness extensions) to have a fallback.
  LoudnessInfo output_loudness = user_provided_loudness_info_;

  RETURN_IF_NOT_OK(FloatToQ7_8WithDebuggingMessage(
      calculated_integrated_loudness, output_loudness.integrated_loudness,
      "integrated loudness"));
  RETURN_IF_NOT_OK(FloatToQ7_8WithDebuggingMessage(
      calculated_digital_peak, output_loudness.digital_peak, "digital peak"));
  if (IncludeTruePeak(user_provided_loudness_info_.info_type)) {
    RETURN_IF_NOT_OK(FloatToQ7_8WithDebuggingMessage(
        calculated_true_peak, output_loudness.true_peak, "true peak"));
  }

  return output_loudness;
}

}  // namespace iamf_tools
