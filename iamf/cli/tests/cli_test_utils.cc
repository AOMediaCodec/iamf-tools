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
#include "iamf/cli/tests/cli_test_utils.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

using ::absl_testing::IsOk;

void AddLpcmCodecConfigWithIdAndSampleRate(
    uint32_t codec_config_id, uint32_t sample_rate,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Initialize the Codec Config OBU.
  ASSERT_EQ(codec_config_obus.find(codec_config_id), codec_config_obus.end());

  CodecConfigObu obu(
      ObuHeader(), codec_config_id,
      {.codec_id = CodecConfig::kCodecIdLpcm,
       .num_samples_per_frame = 8,
       .audio_roll_distance = 0,
       .decoder_config = LpcmDecoderConfig{
           .sample_format_flags_bitmask_ = LpcmDecoderConfig::kLpcmLittleEndian,
           .sample_size_ = 16,
           .sample_rate_ = sample_rate}});
  EXPECT_THAT(obu.Initialize(), IsOk());
  codec_config_obus.emplace(codec_config_id, std::move(obu));
}

void AddOpusCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Initialize the Codec Config OBU.
  ASSERT_EQ(codec_config_obus.find(codec_config_id), codec_config_obus.end());

  CodecConfigObu obu(
      ObuHeader(), codec_config_id,
      {.codec_id = CodecConfig::kCodecIdOpus,
       .num_samples_per_frame = 8,
       .audio_roll_distance = -480,
       .decoder_config = OpusDecoderConfig{
           .version_ = 1, .pre_skip_ = 312, .input_sample_rate_ = 0}});
  ASSERT_THAT(obu.Initialize(), IsOk());
  codec_config_obus.emplace(codec_config_id, std::move(obu));
}

void AddAacCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Initialize the Codec Config OBU.
  ASSERT_EQ(codec_config_obus.find(codec_config_id), codec_config_obus.end());

  CodecConfigObu obu(ObuHeader(), codec_config_id,
                     {.codec_id = CodecConfig::kCodecIdAacLc,
                      .num_samples_per_frame = 1024,
                      .audio_roll_distance = -1,
                      .decoder_config = AacDecoderConfig{}});
  ASSERT_THAT(obu.Initialize(), IsOk());
  codec_config_obus.emplace(codec_config_id, std::move(obu));
}

void AddAmbisonicsMonoAudioElementWithSubstreamIds(
    DecodedUleb128 audio_element_id, uint32_t codec_config_id,
    const std::vector<DecodedUleb128>& substream_ids,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  // Check the `codec_config_id` is known and this is a new
  // `audio_element_id`.
  auto codec_config_iter = codec_config_obus.find(codec_config_id);
  ASSERT_NE(codec_config_iter, codec_config_obus.end());
  ASSERT_EQ(audio_elements.find(audio_element_id), audio_elements.end());

  // Initialize the Audio Element OBU without any parameters.
  AudioElementObu obu = AudioElementObu(
      ObuHeader(), audio_element_id, AudioElementObu::kAudioElementSceneBased,
      0, codec_config_id);
  obu.audio_substream_ids_ = substream_ids;
  obu.InitializeParams(0);
  obu.InitializeAudioSubstreams(substream_ids.size());
  obu.audio_substream_ids_ = substream_ids;

  // Initialize to n-th order ambisonics. Choose the lowest order that can fit
  // all `substream_ids`. This may result in mixed-order ambisonics.
  uint8_t next_valid_output_channel_count;
  ASSERT_THAT(AmbisonicsConfig::GetNextValidOutputChannelCount(
                  substream_ids.size(), next_valid_output_channel_count),
              IsOk());
  EXPECT_THAT(obu.InitializeAmbisonicsMono(next_valid_output_channel_count,
                                           substream_ids.size()),
              IsOk());

  auto& channel_mapping =
      std::get<AmbisonicsMonoConfig>(
          std::get<AmbisonicsConfig>(obu.config_).ambisonics_config)
          .channel_mapping;
  std::fill(channel_mapping.begin(), channel_mapping.end(),
            AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber);
  SubstreamIdLabelsMap substream_id_to_labels;
  for (int i = 0; i < substream_ids.size(); ++i) {
    // Map the first n channels from [0, n] in input order. Leave the rest of
    // the channels as unmapped.
    channel_mapping[i] = i;
    substream_id_to_labels[substream_ids[i]] = {absl::StrCat("A", i)};
  }

  AudioElementWithData audio_element = {
      .obu = std::move(obu),
      .codec_config = &codec_config_iter->second,
      .substream_id_to_labels = substream_id_to_labels};

  audio_elements.emplace(audio_element_id, std::move(audio_element));
}

// TODO(b/309658744): Populate the rest of `ScalableChannelLayout`.
// Adds a scalable Audio Element OBU based on the input arguments.
void AddScalableAudioElementWithSubstreamIds(
    DecodedUleb128 audio_element_id, uint32_t codec_config_id,
    const std::vector<DecodedUleb128>& substream_ids,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  // Check the `codec_config_id` is known and this is a new
  // `audio_element_id`.
  auto codec_config_iter = codec_config_obus.find(codec_config_id);
  ASSERT_NE(codec_config_iter, codec_config_obus.end());
  ASSERT_EQ(audio_elements.find(audio_element_id), audio_elements.end());

  // Initialize the Audio Element OBU without any parameters and a single layer.
  AudioElementObu obu(ObuHeader(), audio_element_id,
                      AudioElementObu::kAudioElementChannelBased, 0,
                      codec_config_id);
  obu.audio_substream_ids_ = substream_ids;
  obu.InitializeParams(0);

  EXPECT_THAT(obu.InitializeScalableChannelLayout(1, 0), IsOk());

  AudioElementWithData audio_element = {
      .obu = std::move(obu), .codec_config = &codec_config_iter->second};

  audio_elements.emplace(audio_element_id, std::move(audio_element));
}

void AddMixPresentationObuWithAudioElementIds(
    DecodedUleb128 mix_presentation_id, DecodedUleb128 audio_element_id,
    DecodedUleb128 common_parameter_id, DecodedUleb128 common_parameter_rate,
    std::list<MixPresentationObu>& mix_presentations) {
  MixGainParamDefinition common_mix_gain_param_definition;
  common_mix_gain_param_definition.parameter_id_ = common_parameter_id;
  common_mix_gain_param_definition.parameter_rate_ = common_parameter_rate;
  common_mix_gain_param_definition.param_definition_mode_ = true;
  common_mix_gain_param_definition.default_mix_gain_ = 0;

  // Configure one of the simplest mix presentation. Mix presentations REQUIRE
  // at least one sub-mix and a stereo layout.
  std::vector<MixPresentationSubMix> sub_mixes = {
      {.num_audio_elements = 1,
       .audio_elements = {{
           .audio_element_id = audio_element_id,
           .mix_presentation_element_annotations = {},
           .rendering_config =
               {.headphones_rendering_mode =
                    RenderingConfig::kHeadphonesRenderingModeStereo,
                .reserved = 0,
                .rendering_config_extension_size = 0,
                .rendering_config_extension_bytes = {}},
           .element_mix_config = {common_mix_gain_param_definition},
       }},
       .output_mix_config = {common_mix_gain_param_definition},
       .num_layouts = 1,
       .layouts = {
           {.loudness_layout =
                {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
                 .specific_layout =
                     LoudspeakersSsConventionLayout{
                         .sound_system = LoudspeakersSsConventionLayout::
                             kSoundSystemA_0_2_0,
                         .reserved = 0}},
            .loudness = {.info_type = 0,
                         .integrated_loudness = 0,
                         .digital_peak = 0}}}}};

  mix_presentations.push_back(
      MixPresentationObu(ObuHeader(), mix_presentation_id,
                         /*count_label=*/0, {}, {},
                         /*num_sub_mixes=*/1, sub_mixes));
}

void AddParamDefinitionWithMode0AndOneSubblock(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration,
    absl::flat_hash_map<DecodedUleb128, std::unique_ptr<ParamDefinition>>&
        param_definitions) {
  auto param_definition = std::make_unique<ParamDefinition>();
  param_definition->parameter_id_ = parameter_id;
  param_definition->parameter_rate_ = parameter_rate;
  param_definition->param_definition_mode_ = 0;
  param_definition->duration_ = duration;
  param_definition->constant_subblock_duration_ = duration;
  param_definitions.emplace(parameter_id, std::move(param_definition));
}

void AddDemixingParamDefinition(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration, AudioElementObu& audio_element_obu,
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>*
        demixing_param_definitions) {
  auto param_definition = std::make_unique<DemixingParamDefinition>();
  param_definition->parameter_id_ = parameter_id;
  param_definition->parameter_rate_ = parameter_rate;
  param_definition->param_definition_mode_ = 0;
  param_definition->reserved_ = 0;
  param_definition->duration_ = duration;
  param_definition->constant_subblock_duration_ = duration;
  param_definition->default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode1;
  param_definition->default_demixing_info_parameter_data_.reserved = 0;
  param_definition->default_demixing_info_parameter_data_.default_w = 10;
  param_definition->default_demixing_info_parameter_data_.reserved_default = 0;

  if (demixing_param_definitions != nullptr) {
    demixing_param_definitions->insert({parameter_id, param_definition.get()});
  }

  // Add to the Audio Element OBU.
  audio_element_obu.InitializeParams(audio_element_obu.num_parameters_ + 1);
  audio_element_obu.audio_element_params_.back() = AudioElementParam{
      .param_definition_type = ParamDefinition::kParameterDefinitionDemixing,
      .param_definition = std::move(param_definition)};
}

}  // namespace iamf_tools
