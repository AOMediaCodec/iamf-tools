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

#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/loudspeakers_renderer.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
namespace iamf_tools {

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
    ABSL_LOG(ERROR) << "Unsupported ambisonics mode. mode= " << mode;
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
  if (const auto status =
          GetChannelLabelsForAmbisonics(ambisonics_config, audio_substream_ids,
                                        substream_id_to_labels, channel_labels);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }

  const auto& output_key = LookupOutputKeyFromPlaybackLayout(playback_layout);
  if (!output_key.ok()) {
    ABSL_LOG(ERROR) << output_key.status();
    return nullptr;
  }

  int ambisonics_order = 0;
  if (const auto status =
          GetAmbisonicsOrder(output_channel_count, ambisonics_order);
      !status.ok()) {
    ABSL_LOG(ERROR) << status;
    return nullptr;
  }
  const std::string input_key = absl::StrCat("A", ambisonics_order);
  const auto& gains = LookupPrecomputedGains(input_key, *output_key);
  if (!gains.ok()) {
    ABSL_LOG(ERROR) << gains.status();
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
    absl::Span<const absl::Span<const InternalSampleType>> samples_to_render) {
  // Render the samples.
  RETURN_IF_NOT_OK(RenderAmbisonicsToLoudspeakers(
      samples_to_render, ambisonics_config_, gains_, rendered_samples_));
  return absl::OkStatus();
}

}  // namespace iamf_tools
