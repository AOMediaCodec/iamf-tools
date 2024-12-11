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

#include "iamf/cli/renderer/audio_element_renderer_ambisonics_to_channel.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/renderer/loudspeakers_renderer.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/macros.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
namespace iamf_tools {
namespace {

absl::Status GetAmbisonicsOrder(const uint8_t channel_count, int& order) {
  // IAMF section 3.6.3 (https://aomediacodec.github.io/iamf/#obu-codecconfig)
  // only permits ambisonics orders [0, 14].
  static const int kMaxAmbisonicsOrder = 14;
  for (int i = 0; i < kMaxAmbisonicsOrder; i++) {
    const int expected_count = (i + 1) * (i + 1);
    if (static_cast<int>(channel_count) == expected_count) {
      order = i;
      return absl::OkStatus();
    } else if (static_cast<int>(channel_count) < expected_count) {
      // Set to upper bound when `IGNORE_ERRORS_USE_ONLY_FOR_IAMF_TEST_SUITE` is
      // defined.
      order = i;
      break;
    }
  }

  return absl::UnknownError(absl::StrCat(
      channel_count, " is not a valid number of ambisonics channels."));
}

absl::Status InitializeChannelLabelsSceneBased(
    const AmbisonicsConfig& ambisonics_config,
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    std::vector<ChannelLabel::Label>& channel_labels) {
  switch (ambisonics_config.ambisonics_mode) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case kAmbisonicsModeMono: {
      const auto& ambisonics_mono_config =
          std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config);
      RETURN_IF_NOT_OK(
          ambisonics_mono_config.Validate(audio_substream_ids.size()));
      channel_labels.resize(ambisonics_mono_config.output_channel_count);
      for (int i = 0; i < channel_labels.size(); ++i) {
        const uint8_t substream_id_index =
            ambisonics_mono_config.channel_mapping[i];
        if (substream_id_index ==
            AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber) {
          // Mixed-order ambisonics representation: this channel is missing.
          channel_labels[i] = ChannelLabel::kOmitted;
        } else {
          const auto substream_id = audio_substream_ids[substream_id_index];
          const auto& labels = substream_id_to_labels.at(substream_id);

          // For ambisonics mode = MONO, each substream should correspond to
          // only one channel.
          if (labels.size() != 1) {
            return absl::InvalidArgumentError(absl::StrCat(
                "Expected one channel per substream for `kAmbisonicsModeMono`. "
                "substream_id= ",
                substream_id, " contains ", labels.size(), " channels."));
          }
          channel_labels[i] = labels.front();
        }
      }

      break;
    }
    case kAmbisonicsModeProjection: {
      const auto& ambisonics_projection_config =
          std::get<AmbisonicsProjectionConfig>(
              ambisonics_config.ambisonics_config);
      RETURN_IF_NOT_OK(
          ambisonics_projection_config.Validate(audio_substream_ids.size()));
      const int num_channels =
          ambisonics_projection_config.substream_count +
          ambisonics_projection_config.coupled_substream_count;
      channel_labels.reserve(num_channels);
      for (int i = 0; i < ambisonics_projection_config.substream_count; i++) {
        const auto substream_id = audio_substream_ids[i];
        for (const auto& label : substream_id_to_labels.at(substream_id)) {
          channel_labels.push_back(label);
        }
      }

      if (channel_labels.size() != num_channels) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Inconsistent number of channels. channel_labels->size()= ",
            channel_labels.size(), " vs num_channels= ", num_channels));
      }

      break;
    }
    default:
      return absl::UnimplementedError(absl::StrCat(
          "Unsupported ambisonics_mode= ", ambisonics_config.ambisonics_mode));
  }

  return absl::OkStatus();
}

}  // namespace

std::unique_ptr<AudioElementRendererAmbisonicsToChannel>
AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
    const AmbisonicsConfig& ambisonics_config,
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const Layout& playback_layout, size_t num_samples_per_frame) {
  // Exclude unsupported modes first, and deal with only mono or projection
  // in the rest of the code.
  const auto mode = ambisonics_config.ambisonics_mode;
  if (mode != AmbisonicsConfig::kAmbisonicsModeMono &&
      mode != AmbisonicsConfig::kAmbisonicsModeProjection) {
    LOG(ERROR) << "Unsupported ambisonics mode. mode= " << mode;
    return nullptr;
  }
  const bool is_mono = mode == AmbisonicsConfig::kAmbisonicsModeMono;

  // Input key for ambisonics is "A{ambisonics_order}".
  const int output_channel_count =
      is_mono
          ? std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config)
                .output_channel_count
          : std::get<AmbisonicsProjectionConfig>(
                ambisonics_config.ambisonics_config)
                .output_channel_count;
  std::vector<ChannelLabel::Label> channel_labels;
  if (const auto status = InitializeChannelLabelsSceneBased(
          ambisonics_config, audio_substream_ids, substream_id_to_labels,
          channel_labels);
      !status.ok()) {
    LOG(ERROR) << status;
    return nullptr;
  }

  const auto& output_key =
      renderer_utils::LookupOutputKeyFromPlaybackLayout(playback_layout);
  if (!output_key.ok()) {
    LOG(ERROR) << output_key.status();
    return nullptr;
  }

  int ambisonics_order = 0;
  if (const auto status =
          GetAmbisonicsOrder(output_channel_count, ambisonics_order);
      !status.ok()) {
    LOG(ERROR) << status;
    return nullptr;
  }
  const std::string input_key = absl::StrCat("A", ambisonics_order);
  const auto& gains = LookupPrecomputedGains(input_key, *output_key);
  if (!gains.ok()) {
    LOG(ERROR) << gains.status();
    return nullptr;
  }
  const std::string gains_map_key =
      absl::StrCat("A", ambisonics_order, "->", *output_key);

  int32_t num_output_channels = 0;
  if (!MixPresentationObu::GetNumChannelsFromLayout(playback_layout,
                                                    num_output_channels)
           .ok()) {
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererAmbisonicsToChannel(
      static_cast<size_t>(num_output_channels), num_samples_per_frame,
      ambisonics_config, channel_labels, *gains));
}

absl::Status AudioElementRendererAmbisonicsToChannel::RenderSamples(
    absl::Span<const std::vector<InternalSampleType>> samples_to_render,
    std::vector<InternalSampleType>& rendered_samples) {
  // Render the samples.
  RETURN_IF_NOT_OK(RenderAmbisonicsToLoudspeakers(
      samples_to_render, ambisonics_config_, gains_, rendered_samples));

  return absl::OkStatus();
}

}  // namespace iamf_tools
