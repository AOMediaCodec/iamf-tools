
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

#include <memory>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/renderer/precomputed_gains.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
namespace iamf_tools {
namespace {

absl::StatusOr<std::string> LookupInputKeyFromHighestLoudspeakerLayout(
    const std::vector<ChannelAudioLayerConfig>& channel_audio_layer_configs) {
  if (channel_audio_layer_configs.empty()) {
    return absl::InvalidArgumentError(
        "No channel audio layer configs provided.");
  }
  const auto& highest_layout =
      channel_audio_layer_configs.back().loudspeaker_layout;

  using enum LoudspeakersSsConventionLayout::SoundSystem;
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;

  static const absl::NoDestructor<absl::flat_hash_map<
      ChannelAudioLayerConfig::LoudspeakerLayout, std::string>>
      kLoudspeakerLayoutToInputKey({
          {kLayoutMono, "0+1+0"},
          {kLayoutStereo, "0+2+0"},
          {kLayout5_1_ch, "0+5+0"},
          {kLayout5_1_2_ch, "2+5+0"},
          {kLayout5_1_4_ch, "4+5+0"},
          {kLayout7_1_ch, "0+7+0"},
          {kLayout7_1_4_ch, "4+7+0"},
          {kLayout7_1_2_ch, "7.1.2"},
          {kLayout3_1_2_ch, "3.1.2"},
      });

  auto it = kLoudspeakerLayoutToInputKey->find(highest_layout);
  if (it == kLoudspeakerLayoutToInputKey->end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Input key not found for layout= ", highest_layout));
  }
  return it->second;
}

absl::StatusOr<std::string> LookupOutputKeyFromPlaybackLayout(
    const Layout& output_layout) {
  switch (output_layout.layout_type) {
    using enum Layout::LayoutType;
    case kLayoutTypeLoudspeakersSsConvention: {
      auto sound_system = std::get<LoudspeakersSsConventionLayout>(
                              output_layout.specific_layout)
                              .sound_system;
      using enum LoudspeakersSsConventionLayout::SoundSystem;
      static const absl::NoDestructor<absl::flat_hash_map<
          LoudspeakersSsConventionLayout::SoundSystem, std::string>>
          kSoundSystemToInputKey({
              {kSoundSystemA_0_2_0, "0+2+0"},
              {kSoundSystemB_0_5_0, "0+5+0"},
              {kSoundSystemC_2_5_0, "2+5+0"},
              {kSoundSystemD_4_5_0, "4+5+0"},
              {kSoundSystemE_4_5_1, "4+5+1"},
              {kSoundSystemF_3_7_0, "3+7+0"},
              {kSoundSystemG_4_9_0, "4+9+0"},
              {kSoundSystemH_9_10_3, "9+10+3"},
              {kSoundSystemI_0_7_0, "0+7+0"},
              {kSoundSystemJ_4_7_0, "4+7+0"},
              {kSoundSystem10_2_7_0, "7.1.2"},
              {kSoundSystem11_2_3_0, "3.1.2"},
              {kSoundSystem12_0_1_0, "0+1+0"},
          });

      auto it = kSoundSystemToInputKey->find(sound_system);
      if (it == kSoundSystemToInputKey->end()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Output key not found for sound_system= ", sound_system));
      }
      return it->second;
    }

    case kLayoutTypeBinaural:
      return absl::UnimplementedError(
          "Loudness layout key for BINAURAL not supported "
          "yet.");
    case kLayoutTypeReserved0:
    case kLayoutTypeReserved1:
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported layout_type= ", output_layout.layout_type));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<std::vector<double>>> LookupPrecomputedGains(
    absl::string_view input_key, absl::string_view output_key) {
  static const absl::NoDestructor<PrecomputedGains> precomputed_gains(
      InitPrecomputedGains());

  auto input_key_it = precomputed_gains->find(input_key);
  if (input_key_it != precomputed_gains->end()) {
    auto output_key_it = input_key_it->second.find(output_key);
    if (output_key_it != input_key_it->second.end()) {
      return output_key_it->second;
    }
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Precomputed gains not found for input_key= ", input_key,
                   " and output_key= ", output_key));
}

}  // namespace

std::unique_ptr<AudioElementRendererChannelToChannel>
AudioElementRendererChannelToChannel::CreateFromScalableChannelLayoutConfig(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& playback_layout) {
  const auto& input_key = LookupInputKeyFromHighestLoudspeakerLayout(
      scalable_channel_layout_config.channel_audio_layer_configs);
  if (!input_key.ok()) {
    LOG(ERROR) << input_key.status();
    return nullptr;
  }
  const auto& output_key = LookupOutputKeyFromPlaybackLayout(playback_layout);
  if (!output_key.ok()) {
    LOG(ERROR) << output_key.status();
    return nullptr;
  }

  const auto& gains = LookupPrecomputedGains(*input_key, *output_key);
  if (!gains.ok()) {
    LOG(ERROR) << gains.status();
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererChannelToChannel(*gains));
}

absl::StatusOr<int> AudioElementRendererChannelToChannel::RenderLabeledFrame(
    const LabeledFrame& labeled_frame) {
  return absl::UnimplementedError("Not implemented");
}

}  // namespace iamf_tools
