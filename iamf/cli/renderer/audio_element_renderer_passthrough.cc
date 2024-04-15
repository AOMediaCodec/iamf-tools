
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

#include <memory>

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
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
namespace iamf_tools {
namespace {

absl::StatusOr<ChannelAudioLayerConfig::LoudspeakerLayout>
LookupEquivalentLoudspeakerLayoutFromSoundSystem(
    const LoudspeakersSsConventionLayout::SoundSystem& layout) {
  using enum LoudspeakersSsConventionLayout::SoundSystem;
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;

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

  auto it = kSoundSystemToLoudspeakerLayout->find(layout);
  if (it == kSoundSystemToLoudspeakerLayout->end()) {
    return absl::NotFoundError(
        absl::StrCat("Sound system not found for layout= ", layout));
  }
  return it->second;
}

absl::StatusOr<ChannelAudioLayerConfig::LoudspeakerLayout>
LookupEquivalentLoudspeakerLayoutFromLayout(const Layout& layout) {
  switch (layout.layout_type) {
    case Layout::kLayoutTypeLoudspeakersSsConvention:
      // Pass-through the associated demixed layer.
      return LookupEquivalentLoudspeakerLayoutFromSoundSystem(
          std::get<LoudspeakersSsConventionLayout>(layout.specific_layout)
              .sound_system);
    case Layout::kLayoutTypeBinaural:
      // Pass-through binaural.
      return ChannelAudioLayerConfig::kLayoutBinaural;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown layout_type= ", layout.layout_type));
  }
}

// Finds the layer with the equivalent loudspeaker layout if present.
absl::StatusOr<ChannelAudioLayerConfig::LoudspeakerLayout> FindEquivalentLayer(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& layout) {
  const auto& equivalent_loudspeaker_layout =
      LookupEquivalentLoudspeakerLayoutFromLayout(layout);
  if (!equivalent_loudspeaker_layout.ok()) {
    return equivalent_loudspeaker_layout.status();
  }

  for (const auto& channel_config :
       scalable_channel_layout_config.channel_audio_layer_configs) {
    if (channel_config.loudspeaker_layout == *equivalent_loudspeaker_layout) {
      return channel_config.loudspeaker_layout;
    }
  }

  return absl::InvalidArgumentError(
      "Loudspeaker layout not found. The passthrough render is not suitable "
      "here. Downmixing may be required.");
}

}  // namespace

std::unique_ptr<AudioElementRendererPassThrough>
AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
    const ScalableChannelLayoutConfig& scalable_channel_layout_config,
    const Layout& playback_layout) {
  if (!FindEquivalentLayer(scalable_channel_layout_config, playback_layout)
           .ok()) {
    return nullptr;
  }

  return absl::WrapUnique(new AudioElementRendererPassThrough());
}

absl::Status AudioElementRendererPassThrough::RenderLabeledFrame(
    const LabeledFrame& labeled_frame) {
  return absl::UnimplementedError("Not implemented");
}

}  // namespace iamf_tools
