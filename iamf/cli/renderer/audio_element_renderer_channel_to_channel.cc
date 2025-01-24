
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

#include "iamf/cli/renderer/audio_element_renderer_channel_to_channel.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/renderer/loudspeakers_renderer.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/macros.h"
#include "iamf/common/map_utils.h"
#include "iamf/common/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

constexpr absl::string_view kMonoInputKey = "0+1+0";
constexpr absl::string_view kStereoInputKey = "0+2+0";
constexpr absl::string_view k5_1_chInputKey = "0+5+0";
constexpr absl::string_view k5_1_2_chInputKey = "2+5+0";
constexpr absl::string_view k5_1_4InputKey = "4+5+0";
constexpr absl::string_view k7_1_0InputKey = "0+7+0";
constexpr absl::string_view k7_1_4InputKey = "4+7+0";
constexpr absl::string_view k7_1_2InputKey = "7.1.2";
constexpr absl::string_view k3_1_2InputKey = "3.1.2";
constexpr absl::string_view k9_1_6InputKey = "9.1.6";

// TODO(b/359180486): Unify with `IsExpandedLayoutEquivalentToSoundSystem` in
//                    `audio_element_passthrough.cc`.
absl::StatusOr<absl::string_view> LookupInputKeyFromLoudspeakerLayout(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout expanded_layout) {
  switch (expanded_layout) {
    using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kExpandedLayoutStereoS:
      return k5_1_4InputKey;
    case kExpandedLayoutLFE:
    case kExpandedLayoutStereoSS:
    case kExpandedLayoutStereoRS:
    case kExpandedLayoutStereoTF:
    case kExpandedLayoutStereoTB:
    case kExpandedLayoutTop4Ch:
    case kExpandedLayout3_0_ch:
      return k7_1_4InputKey;
    case kExpandedLayout9_1_6_ch:
    case kExpandedLayoutStereoF:
    case kExpandedLayoutStereoSi:
    case kExpandedLayoutStereoTpSi:
    case kExpandedLayoutTop6Ch:
      return k9_1_6InputKey;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Channel order not found for layout= ", expanded_layout));
  }
}

absl::StatusOr<absl::string_view> LookupInputKeyFromLoudspeakerLayout(
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
        expanded_loudspeaker_layout) {
  if (loudspeaker_layout == ChannelAudioLayerConfig::kLayoutExpanded) {
    RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                      "expanded_loudspeaker_layout"));
    return LookupInputKeyFromLoudspeakerLayout(*expanded_loudspeaker_layout);
  }

  using enum LoudspeakersSsConventionLayout::SoundSystem;
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;

  static const absl::NoDestructor<absl::flat_hash_map<
      ChannelAudioLayerConfig::LoudspeakerLayout, absl::string_view>>
      kLoudspeakerLayoutToInputKey({
          {kLayoutMono, kMonoInputKey},
          {kLayoutStereo, kStereoInputKey},
          {kLayout5_1_ch, k5_1_chInputKey},
          {kLayout5_1_2_ch, k5_1_2_chInputKey},
          {kLayout5_1_4_ch, k5_1_4InputKey},
          {kLayout7_1_ch, k7_1_0InputKey},
          {kLayout7_1_4_ch, k7_1_4InputKey},
          {kLayout7_1_2_ch, k7_1_2InputKey},
          {kLayout3_1_2_ch, k3_1_2InputKey},
      });

  return LookupInMap(*kLoudspeakerLayoutToInputKey, loudspeaker_layout,
                     "Input key for `LoudspeakerLayout`");
}

}  // namespace

std::unique_ptr<AudioElementRendererChannelToChannel>
AudioElementRendererChannelToChannel::CreateFromScalableChannelLayoutConfig(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& playback_layout, size_t num_samples_per_frame) {
  if (scalable_channel_layout_config.channel_audio_layer_configs.empty()) {
    LOG(ERROR) << "No channel audio layer configs provided.";
    return nullptr;
  }
  const auto& highest_channel_audio_layer_config =
      scalable_channel_layout_config.channel_audio_layer_configs.back();
  const auto& ordered_labels =
      ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
          highest_channel_audio_layer_config.loudspeaker_layout,
          highest_channel_audio_layer_config.expanded_loudspeaker_layout);
  if (!ordered_labels.ok()) {
    LOG(ERROR) << ordered_labels.status();
    return nullptr;
  }

  const auto& input_key = LookupInputKeyFromLoudspeakerLayout(
      highest_channel_audio_layer_config.loudspeaker_layout,
      highest_channel_audio_layer_config.expanded_loudspeaker_layout);
  if (!input_key.ok()) {
    LOG(ERROR) << input_key.status();
    return nullptr;
  }
  const auto& output_key =
      renderer_utils::LookupOutputKeyFromPlaybackLayout(playback_layout);
  if (!output_key.ok()) {
    LOG(ERROR) << output_key.status();
    return nullptr;
  }

  const auto& gains = LookupPrecomputedGains(*input_key, *output_key);
  if (!gains.ok()) {
    LOG(ERROR) << gains.status();
    return nullptr;
  }

  int32_t num_output_channels = 0;
  if (!MixPresentationObu::GetNumChannelsFromLayout(playback_layout,
                                                    num_output_channels)
           .ok()) {
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererChannelToChannel(
      *input_key, *output_key, static_cast<size_t>(num_output_channels),
      num_samples_per_frame, *ordered_labels, *gains));
}

absl::Status AudioElementRendererChannelToChannel::RenderSamples(
    absl::Span<const std::vector<InternalSampleType>> samples_to_render,
    std::vector<InternalSampleType>& rendered_samples) {
  // Render the samples.
  RETURN_IF_NOT_OK(RenderChannelLayoutToLoudspeakers(
      samples_to_render, current_labeled_frame_->demixing_params,
      ordered_labels_, input_key_, output_key_, gains_, rendered_samples));

  return absl::OkStatus();
}

}  // namespace iamf_tools
