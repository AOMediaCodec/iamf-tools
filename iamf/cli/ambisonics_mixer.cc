/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/ambisonics_mixer.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using absl::MakeConstSpan;

constexpr std::array<int16_t, 256> kThirdOrderDemixingMatrixFromLibopus = {
    8192,   8192,   8192,   8192,   8192,   8192,   8192,   8192,   8192,
    8192,   8192,   8192,   8192,   8192,   8192,   8192,   0,      -9779,
    9779,   6263,   8857,   0,      6263,   13829,  9779,   -13829, 0,
    -6263,  0,      -8857,  -6263,  -9779,  -3413,  3413,   3413,   -11359,
    11359,  11359,  -11359, -3413,  3413,   -3413,  -3413,  -11359, 11359,
    11359,  -11359, 3413,   13829,  9779,   -9779,  6263,   0,      8857,
    -6263,  0,      9779,   0,      -13829, 6263,   -8857,  0,      -6263,
    -9779,  0,      -15617, -15617, 6406,   0,      0,      -6406,  0,
    15617,  0,      0,      -6406,  0,      0,      6406,   15617,  0,
    -5003,  5003,   -10664, 15081,  0,      -10664, -7075,  5003,   7075,
    0,      10664,  0,      -15081, 10664,  -5003,  -8176,  -8176,  -8176,
    8208,   8208,   8208,   8208,   -8176,  -8176,  -8176,  -8176,  8208,
    8208,   8208,   8208,   -8176,  -7075,  5003,   -5003,  -10664, 0,
    15081,  10664,  0,      5003,   0,      7075,   -10664, -15081, 0,
    10664,  -5003,  15617,  0,      0,      0,      -6406,  6406,   0,
    -15617, 0,      -15617, 15617,  0,      6406,   -6406,  0,      0,
    0,      0,      0,      -11393, 11393,  2993,   -4233,  0,      2993,
    -16112, 11393,  16112,  0,      -2993,  0,      4233,   0,      0,
    0,      -9974,  -9974,  -13617, 0,      0,      13617,  0,      9974,
    0,      0,      13617,  0,      0,      0,      0,      0,      5579,
    -5579,  10185,  14403,  0,      10185,  -7890,  -5579,  7890,   0,
    -10185, 0,      -14403, 0,      0,      11826,  -11826, -11826, -901,
    901,    901,    -901,   11826,  -11826, 11826,  11826,  -901,   901,
    901,    0,      0,      -7890,  -5579,  5579,   10185,  0,      14403,
    -10185, 0,      -5579,  0,      7890,   10185,  -14403, 0,      0,
    0,      -9974,  0,      0,      0,      -13617, 13617,  0,      9974,
    0,      9974,   -9974,  0,      13617,  -13617, 0,      0,      0,
    0,      16112,  -11393, 11393,  -2993,  0,      4233,   2993,   0,
    -11393, 0,      -16112, -2993};

AmbisonicsConfig CreateMonoConfig(AmbisonicsMixer::Preset preset) {
  uint8_t substream_count;
  switch (preset) {
    using enum AmbisonicsMixer::Preset;
    case kBestPracticeForOrder0:
      substream_count = 1;
      break;
    case kBestPracticeForOrder1:
      substream_count = 4;
      break;
    case kBestPracticeForOrder2:
      substream_count = 9;
      break;
    case kBestPracticeForOrder3:
      substream_count = 16;
      break;
  }

  constexpr std::array<uint8_t, 16> kMonoAmbisonicsChannelMapping = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  auto mono_config = AmbisonicsMonoConfig::Create(
      substream_count,
      MakeConstSpan(kMonoAmbisonicsChannelMapping).first(substream_count));
  // All options are hard-coded to be valid.
  ABSL_CHECK(mono_config.ok());
  return AmbisonicsConfig{.ambisonics_config = *mono_config};
}

AmbisonicsConfig CreateProjectConfigForThirdOrder() {
  auto projection_config = AmbisonicsProjectionConfig::Create(
      /*output_channel_count=*/16, /*substream_count=*/8,
      /*coupled_substream_count=*/8,
      MakeConstSpan(kThirdOrderDemixingMatrixFromLibopus));
  ABSL_CHECK_OK(projection_config);

  return AmbisonicsConfig{.ambisonics_config = *projection_config};
}

AmbisonicsConfig MakeAmbisonicsConfig(CodecConfig::CodecId codec_id,
                                      AmbisonicsMixer::Preset preset) {
  if (codec_id == CodecConfig::CodecId::kCodecIdOpus &&
      preset == AmbisonicsMixer::Preset::kBestPracticeForOrder3) {
    return CreateProjectConfigForThirdOrder();
  }

  return CreateMonoConfig(preset);
}

}  // namespace

AmbisonicsMixer AmbisonicsMixer::MakeFromPreset(
    CodecConfig::CodecId codec_id, Preset preset,
    size_t max_input_samples_per_frame) {
  auto config = MakeAmbisonicsConfig(codec_id, preset);
  return AmbisonicsMixer(config, max_input_samples_per_frame);
}

AmbisonicsMixer AmbisonicsMixer::MakeFromAmbisonicsConfig(
    const AmbisonicsConfig& config, size_t max_input_samples_per_frame) {
  return AmbisonicsMixer(config, max_input_samples_per_frame);
}

AmbisonicsConfig AmbisonicsMixer::GetAmbisonicsConfig() const {
  return ambisonics_config_;
}

absl::Status AmbisonicsMixer::PushFrameDerived(
    absl::Span<const absl::Span<const InternalSampleType>>
        channel_time_samples) {
  // TODO(b/450899154): Implement mixing for presets that use projection.

  // For mono mixing and user-supplied configurations, we just copy the input
  // samples to the output.
  for (size_t c = 0; c < num_channels_; ++c) {
    output_channel_time_samples_[c].assign(channel_time_samples[c].begin(),
                                           channel_time_samples[c].end());
  }
  return absl::OkStatus();
}

AmbisonicsMixer::AmbisonicsMixer(AmbisonicsConfig config,
                                 size_t max_input_samples_per_frame)
    : SampleProcessorBase(max_input_samples_per_frame,
                          config.GetOutputChannelCount(),
                          max_input_samples_per_frame),
      ambisonics_config_(config) {}

}  // namespace iamf_tools
