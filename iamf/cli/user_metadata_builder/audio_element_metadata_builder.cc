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

#include "iamf/cli/user_metadata_builder/audio_element_metadata_builder.h"

#include <cstdint>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/common/utils/map_utils.h"

namespace iamf_tools {

namespace {

absl::StatusOr<int32_t> LookupNumSubstreamsFromInputLayout(
    IamfInputLayout input_layout) {
  // Map which holds the loudspeaker layout and the count of substream(s)
  // corresponding to it.
  using enum IamfInputLayout;
  static const absl::NoDestructor<absl::flat_hash_map<IamfInputLayout, int32_t>>
      kInputLayoutToNumSubstreams({
          {kMono, 1},
          {kStereo, 1},
          {k5_1, 4},
          {k5_1_2, 5},
          {k5_1_4, 6},
          {k5_1_4, 6},
          {k7_1, 5},
          {k7_1_4, 7},
          {kBinaural, 1},
          {kLFE, 1},
          {kAmbisonicsOrder1, 4},
          {kAmbisonicsOrder2, 9},
          {kAmbisonicsOrder3, 16},
      });

  return LookupInMap(*kInputLayoutToNumSubstreams, input_layout,
                     "Number of substreams for `IamfInputLayout`");
}

absl::StatusOr<int32_t> LookupCoupledSubstreamCountFromInputLayout(
    IamfInputLayout input_layout) {
  // Map which holds the loudspeaker layout and the count of coupled
  // substream(s) corresponding to it.
  using enum IamfInputLayout;
  static const absl::NoDestructor<absl::flat_hash_map<IamfInputLayout, int32_t>>
      kInputLayoutToCoupledSubstreamCount({
          {kMono, 0},
          {kStereo, 1},
          {k5_1, 2},
          {k5_1_2, 3},
          {k5_1_4, 4},
          {k7_1, 3},
          {k7_1_4, 5},
          {kBinaural, 1},
          {kLFE, 0},
      });

  return LookupInMap(*kInputLayoutToCoupledSubstreamCount, input_layout,
                     "Coupled substream count for `IamfInputLayout`");
}

absl::StatusOr<iamf_tools_cli_proto::LoudspeakerLayout>
LookupLoudspeakerLayoutFromInputLayout(IamfInputLayout input_layout) {
  using enum IamfInputLayout;
  using enum iamf_tools_cli_proto::LoudspeakerLayout;

  // Map which holds the channel layout and the corresponding loudspeaker layout
  // in IAMF.
  static const absl::NoDestructor<absl::flat_hash_map<
      IamfInputLayout, iamf_tools_cli_proto::LoudspeakerLayout>>
      KInputLayoutToLoudspeakerLayout({
          {kMono, LOUDSPEAKER_LAYOUT_MONO},
          {kStereo, LOUDSPEAKER_LAYOUT_STEREO},
          {k5_1, LOUDSPEAKER_LAYOUT_5_1_CH},
          {k5_1_2, LOUDSPEAKER_LAYOUT_5_1_2_CH},
          {k5_1_4, LOUDSPEAKER_LAYOUT_5_1_4_CH},
          {k7_1, LOUDSPEAKER_LAYOUT_7_1_CH},
          {k7_1_4, LOUDSPEAKER_LAYOUT_7_1_4_CH},
          {kBinaural, LOUDSPEAKER_LAYOUT_BINAURAL},
          {kLFE, LOUDSPEAKER_LAYOUT_EXPANDED},
      });

  return LookupInMap(*KInputLayoutToLoudspeakerLayout, input_layout,
                     "Proto `LoudspeakerLayout` for `IamfInputLayout`");
}

absl::StatusOr<iamf_tools_cli_proto::ExpandedLoudspeakerLayout>
LookupExpandedLoudspeakerLayoutFromInputLayout(IamfInputLayout input_layout) {
  using enum IamfInputLayout;
  using enum iamf_tools_cli_proto::ExpandedLoudspeakerLayout;

  // Map which holds the channel layout and the corresponding expanded
  // loudspeaker layout in IAMF.
  static const absl::NoDestructor<absl::flat_hash_map<
      IamfInputLayout, iamf_tools_cli_proto::ExpandedLoudspeakerLayout>>
      KInputLayoutToExpandedLoudspeakerLayout({
          {kLFE, EXPANDED_LOUDSPEAKER_LAYOUT_LFE},
      });

  return LookupInMap(*KInputLayoutToExpandedLoudspeakerLayout, input_layout,
                     "Proto `ExpandedLoudspeakerLayout` for `IamfInputLayout`");
}

absl::StatusOr<iamf_tools_cli_proto::AudioElementType>
LookupAudioElementTypeFromInputLayout(IamfInputLayout input_layout) {
  using enum IamfInputLayout;
  using enum iamf_tools_cli_proto::AudioElementType;

  // Map which holds the channel layout and the corresponding audio element
  // type.
  static const absl::NoDestructor<absl::flat_hash_map<
      IamfInputLayout, iamf_tools_cli_proto::AudioElementType>>
      KInputLayoutToAudioElementType({
          {kMono, AUDIO_ELEMENT_CHANNEL_BASED},
          {kStereo, AUDIO_ELEMENT_CHANNEL_BASED},
          {k5_1, AUDIO_ELEMENT_CHANNEL_BASED},
          {k5_1_2, AUDIO_ELEMENT_CHANNEL_BASED},
          {k5_1_4, AUDIO_ELEMENT_CHANNEL_BASED},
          {k7_1, AUDIO_ELEMENT_CHANNEL_BASED},
          {k7_1_4, AUDIO_ELEMENT_CHANNEL_BASED},
          {kBinaural, AUDIO_ELEMENT_CHANNEL_BASED},
          {kLFE, AUDIO_ELEMENT_CHANNEL_BASED},
          {kAmbisonicsOrder1, AUDIO_ELEMENT_SCENE_BASED},
          {kAmbisonicsOrder2, AUDIO_ELEMENT_SCENE_BASED},
          {kAmbisonicsOrder3, AUDIO_ELEMENT_SCENE_BASED},
      });

  return LookupInMap(*KInputLayoutToAudioElementType, input_layout,
                     "Proto `AudioElementType` for `IamfInputLayout`");
}

absl::Status PopulateChannelBasedAudioElementMetadata(
    IamfInputLayout input_layout, int32_t num_substreams,
    iamf_tools_cli_proto::ScalableChannelLayoutConfig&
        scalable_channel_layout_config) {
  // Simplistically choose one layer. This most closely matches other popular
  // formats (e.g. ADM).
  scalable_channel_layout_config.set_num_layers(1);

  auto* channel_audio_layer_config =
      scalable_channel_layout_config.add_channel_audio_layer_configs();

  const auto loudspeaker_layout =
      LookupLoudspeakerLayoutFromInputLayout(input_layout);
  if (!loudspeaker_layout.ok()) {
    return loudspeaker_layout.status();
  }
  channel_audio_layer_config->set_loudspeaker_layout(*loudspeaker_layout);

  // Set 'output_gain_is_present_flag' and 'recon_gain_is_present_flag' to agree
  // with the single-layer assumption.
  channel_audio_layer_config->set_output_gain_is_present_flag(0);
  channel_audio_layer_config->set_recon_gain_is_present_flag(0);

  // As 'num_layers' is set to 1, 'substream_count' is equal to
  // 'num_substreams'.
  channel_audio_layer_config->set_substream_count(num_substreams);
  const auto coupled_substream_count =
      LookupCoupledSubstreamCountFromInputLayout(input_layout);
  if (!coupled_substream_count.ok()) {
    return coupled_substream_count.status();
  }
  channel_audio_layer_config->set_coupled_substream_count(
      *coupled_substream_count);

  // Set the specific 'expanded_loudspeaker_layout' field when it is relevant
  // (e.g. LFE).
  if (channel_audio_layer_config->loudspeaker_layout() ==
      iamf_tools_cli_proto::LOUDSPEAKER_LAYOUT_EXPANDED) {
    const auto expanded_loudspeaker_layout =
        LookupExpandedLoudspeakerLayoutFromInputLayout(input_layout);
    if (!expanded_loudspeaker_layout.ok()) {
      return expanded_loudspeaker_layout.status();
    }
    channel_audio_layer_config->set_expanded_loudspeaker_layout(
        *expanded_loudspeaker_layout);
  }

  return absl::OkStatus();
}

void PopulateSceneBasedAudioElementMetadata(
    int32_t num_substreams,
    iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_obu_metadata) {
  audio_element_obu_metadata.set_audio_element_type(
      iamf_tools_cli_proto::AUDIO_ELEMENT_SCENE_BASED);

  auto* ambisonics_config =
      audio_element_obu_metadata.mutable_ambisonics_config();
  // For typeDefinition = HOA and since input contains LPCM audio samples, set
  // ambisonics_mode to AMBISONICS_MODE_MONO.
  ambisonics_config->set_ambisonics_mode(
      iamf_tools_cli_proto::AMBISONICS_MODE_MONO);

  auto* ambisonics_mono_config =
      ambisonics_config->mutable_ambisonics_mono_config();

  ambisonics_mono_config->set_output_channel_count(num_substreams);
  ambisonics_mono_config->set_substream_count(num_substreams);

  for (int32_t substream_id = 0; substream_id < num_substreams;
       ++substream_id) {
    ambisonics_mono_config->mutable_channel_mapping()->Add(substream_id);
  }
}

}  // namespace

// Sets the required textproto fields for audio_element_metadata.
absl::Status AudioElementMetadataBuilder::PopulateAudioElementMetadata(
    uint32_t audio_element_id, uint32_t codec_config_id,
    const IamfInputLayout input_layout,
    iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_obu_metadata) {
  audio_element_obu_metadata.set_audio_element_id(audio_element_id);
  audio_element_obu_metadata.set_codec_config_id(codec_config_id);

  const auto num_substreams = LookupNumSubstreamsFromInputLayout(input_layout);
  if (!num_substreams.ok()) {
    return num_substreams.status();
  }
  audio_element_obu_metadata.set_num_substreams(*num_substreams);

  // Generate sequential substream IDs. Although not REQUIRED by IAMF this helps
  // ensure that the substream IDs are unique between subsequent calls to this
  // function.
  for (int i = 0; i < *num_substreams; ++i) {
    audio_element_obu_metadata.mutable_audio_substream_ids()->Add(
        audio_stream_id_counter_);
    ++audio_stream_id_counter_;
  }

  // Simplistically set 'num_parameters' to zero.
  audio_element_obu_metadata.set_num_parameters(0);

  const auto audio_element_type =
      LookupAudioElementTypeFromInputLayout(input_layout);
  if (!audio_element_type.ok()) {
    return audio_element_type.status();
  }
  audio_element_obu_metadata.set_audio_element_type(*audio_element_type);

  switch (*audio_element_type) {
    using enum iamf_tools_cli_proto::AudioElementType;
    case AUDIO_ELEMENT_CHANNEL_BASED:
      return PopulateChannelBasedAudioElementMetadata(
          input_layout, *num_substreams,
          *audio_element_obu_metadata.mutable_scalable_channel_layout_config());
    case AUDIO_ELEMENT_SCENE_BASED:
      PopulateSceneBasedAudioElementMetadata(*num_substreams,
                                             audio_element_obu_metadata);
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unsupported audio_element_type= ", *audio_element_type));
  }
}

}  // namespace iamf_tools
