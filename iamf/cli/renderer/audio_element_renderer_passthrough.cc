
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

#include "iamf/cli/renderer/audio_element_renderer_passthrough.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
namespace iamf_tools {
namespace {

using enum LoudspeakersSsConventionLayout::SoundSystem;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;
using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;

absl::StatusOr<bool> IsLoudspeakerLayoutEquivalentToSoundSystem(
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    LoudspeakersSsConventionLayout::SoundSystem layout) {
  static const absl::NoDestructor<
      absl::flat_hash_map<LoudspeakersSsConventionLayout::SoundSystem,
                          ChannelAudioLayerConfig::LoudspeakerLayout>>
      kSoundSystemToLoudspeakerLayout({
          {kSoundSystem12_0_1_0, kLayoutMono},
          {kSoundSystemA_0_2_0, kLayoutStereo},
          {kSoundSystemB_0_5_0, kLayout5_1_ch},
          {kSoundSystemC_2_5_0, kLayout5_1_2_ch},
          {kSoundSystemD_4_5_0, kLayout5_1_4_ch},
          {kSoundSystemI_0_7_0, kLayout7_1_ch},
          {kSoundSystem10_2_7_0, kLayout7_1_2_ch},
          {kSoundSystemJ_4_7_0, kLayout7_1_4_ch},
          {kSoundSystem11_2_3_0, kLayout3_1_2_ch},
      });

  const auto equivalent_loudspeaker_layout_iter =
      kSoundSystemToLoudspeakerLayout->find(layout);
  if (equivalent_loudspeaker_layout_iter ==
      kSoundSystemToLoudspeakerLayout->end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Sound system not found for layout= ", layout));
  }
  return equivalent_loudspeaker_layout_iter->second == loudspeaker_layout;
}

// Several expanded layouts are defined as being based on a particular sound
// system. The passthrough renderer can be used with the associated sound system
// if the expanded layout is a based on the sound system. Other channels can be
// omitted.
absl::StatusOr<bool> IsExpandedLoudspeakerLayoutBasedOnSoundSystem(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout
        expanded_loudspeaker_layout,
    LoudspeakersSsConventionLayout::SoundSystem layout) {
  switch (expanded_loudspeaker_layout) {
    case kExpandedLayoutStereoS:
      return layout == kSoundSystemD_4_5_0;
    case kExpandedLayoutLFE:
    case kExpandedLayoutStereoSS:
    case kExpandedLayoutStereoRS:
    case kExpandedLayoutStereoTF:
    case kExpandedLayoutStereoTB:
    case kExpandedLayoutTop4Ch:
    case kExpandedLayout3_0_ch:
      return layout == kSoundSystemJ_4_7_0;
    case kExpandedLayout9_1_6_ch:
    case kExpandedLayoutStereoF:
    case kExpandedLayoutStereoSi:
    case kExpandedLayoutStereoTpSi:
    case kExpandedLayoutTop6Ch:
      return layout == kSoundSystem13_6_9_0;
    case kExpandedLayoutReserved13:
    case kExpandedLayoutReserved255:
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unknown expanded layout cannot be used for pass-through: ",
          expanded_loudspeaker_layout));
  }
}

absl::StatusOr<bool> CanChannelAudioLayerConfigPassThroguhToLayout(
    const ChannelAudioLayerConfig& channel_config, const Layout& layout) {
  switch (layout.layout_type) {
    case Layout::kLayoutTypeLoudspeakersSsConvention:
      // Pass-through the associated demixed layer.
      if (channel_config.loudspeaker_layout == kLayoutExpanded) {
        RETURN_IF_NOT_OK(
            ValidateHasValue(channel_config.expanded_loudspeaker_layout,
                             "expanded_loudspeaker_layout"));
        return IsExpandedLoudspeakerLayoutBasedOnSoundSystem(
            *channel_config.expanded_loudspeaker_layout,
            std::get<LoudspeakersSsConventionLayout>(layout.specific_layout)
                .sound_system);
      } else {
        return IsLoudspeakerLayoutEquivalentToSoundSystem(
            channel_config.loudspeaker_layout,
            std::get<LoudspeakersSsConventionLayout>(layout.specific_layout)
                .sound_system);
      }
    case Layout::kLayoutTypeBinaural:
      // Pass-through binaural.
      return channel_config.loudspeaker_layout == kLayoutBinaural;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown layout_type= ", layout.layout_type));
  }
}

// Finds the layer with the equivalent loudspeaker layout if present.
absl::StatusOr<ChannelAudioLayerConfig> FindEquivalentLayer(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& layout) {
  for (const auto& channel_audio_layer_config :
       scalable_channel_layout_config.channel_audio_layer_configs) {
    const auto can_pass_through = CanChannelAudioLayerConfigPassThroguhToLayout(
        channel_audio_layer_config, layout);
    if (!can_pass_through.ok()) {
      return can_pass_through.status();
    } else if (*can_pass_through) {
      return channel_audio_layer_config;
    } else {
      // Search in the remaining layers.
      continue;
    }
  }

  return absl::InvalidArgumentError(
      "No equivalent layers found for the requested layout. The passthrough "
      "render is not suitable here. Down-mixing may be required.");
}

}  // namespace

std::unique_ptr<AudioElementRendererPassThrough>
AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& playback_layout) {
  const auto& equivalent_layer =
      FindEquivalentLayer(scalable_channel_layout_config, playback_layout);
  if (!equivalent_layer.ok()) {
    return nullptr;
  }
  const auto& channel_order =
      ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
          equivalent_layer->loudspeaker_layout,
          equivalent_layer->expanded_loudspeaker_layout);
  if (!channel_order.ok()) {
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererPassThrough(*channel_order));
}

absl::StatusOr<int> AudioElementRendererPassThrough::RenderLabeledFrame(
    const LabeledFrame& labeled_frame) {
  std::vector<std::vector<int32_t>> samples_to_render;
  RETURN_IF_NOT_OK(iamf_tools::renderer_utils::ArrangeSamplesToRender(
      labeled_frame, channel_order_, samples_to_render));

  // Flatten the (time, channel) axes into interleaved samples.
  absl::MutexLock lock(&mutex_);
  for (const auto& tick : samples_to_render) {
    // Skip applying the identity matrix.
    rendered_samples_.insert(rendered_samples_.end(), tick.begin(), tick.end());
  }
  return samples_to_render.size();
}

}  // namespace iamf_tools
