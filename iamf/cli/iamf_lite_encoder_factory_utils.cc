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
#include "iamf/cli/iamf_lite_encoder_factory_utils.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/cli/codec/lpcm_encoder.h"
#include "iamf/cli/codec/opus_encoder.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/utils/macros.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/loudness_info.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/substream_channel_count.h"
#include "iamf/obu/types.h"
#include "include/opus_defines.h"

namespace iamf_tools {
namespace {

using ::iamf_tools::api::AudioElementType;
using ::iamf_tools::api::PcmSampleRate;

// Default codec settings for single-element encoders.
constexpr uint8_t kLpcmBitDepth = 16;
constexpr uint8_t kOpusVersion = 1;
constexpr uint32_t kOpusSampleRate = 48000;
constexpr int32_t kOpusTargetBitratePerChannel = 64000;

// Default Mix Presentation parameter IDs and annotations.
constexpr DecodedUleb128 kElementMixGainParameterId = 999;
constexpr DecodedUleb128 kOutputMixGainParameterId = 998;
constexpr char kDefaultAnnotationsLanguage[] = "en-us";
constexpr char kDefaultMixPresentationAnnotation[] = "Default Mix Presentation";
constexpr char kDefaultAudioElementAnnotation[] = "Default Audio Element";

// Maximum Ambisonics Channel Number + 1 (output_channel_count) per order.
constexpr uint8_t kFirstOrderMaxAcnPlusOne = 4;
constexpr uint8_t kSecondOrderMaxAcnPlusOne = 9;
constexpr uint8_t kThirdOrderMaxAcnPlusOne = 16;
constexpr uint8_t kFourthOrderMaxAcnPlusOne = 25;

absl::StatusOr<AmbisonicsMonoConfig> CreateAmbisonicsMonoConfig(
    uint8_t output_channel_count, absl::Span<const uint8_t> active_acns) {
  std::vector<uint8_t> channel_mapping(
      output_channel_count,
      AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber);
  for (size_t substream_index = 0; substream_index < active_acns.size();
       ++substream_index) {
    channel_mapping[active_acns[substream_index]] =
        static_cast<uint8_t>(substream_index);
  }
  return AmbisonicsMonoConfig::Create(static_cast<uint8_t>(active_acns.size()),
                                      channel_mapping);
}

absl::StatusOr<AudioElementWithData> CreateAmbisonicsAudioElementWithData(
    DecodedUleb128 audio_element_id, uint8_t output_channel_count,
    absl::Span<const uint8_t> active_acns,
    const CodecConfigObu& codec_config_obu) {
  auto mono_config =
      CreateAmbisonicsMonoConfig(output_channel_count, active_acns);
  if (!mono_config.ok()) {
    return mono_config.status();
  }

  std::vector<DecodedUleb128> substream_ids(active_acns.size());
  std::iota(substream_ids.begin(), substream_ids.end(), DecodedUleb128{0});

  auto obu = AudioElementObu::CreateForAmbisonics(
      ObuHeader(), audio_element_id, /*reserved=*/0,
      codec_config_obu.GetCodecConfigId(), substream_ids,
      AmbisonicsConfig{.ambisonics_config = *std::move(mono_config)});
  if (!obu.ok()) {
    return obu.status();
  }

  SubstreamIdLabelsMap substream_id_to_labels;
  RETURN_IF_NOT_OK(ObuWithDataGenerator::FinalizeAmbisonicsConfig(
      *obu, substream_id_to_labels));
  return AudioElementWithData{
      .obu = *std::move(obu),
      .codec_config = &codec_config_obu,
      .substream_id_to_labels = std::move(substream_id_to_labels),
  };
}

absl::StatusOr<AudioElementWithData> CreateChannelBasedAudioElementWithData(
    DecodedUleb128 audio_element_id,
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
        expanded_loudspeaker_layout,
    const CodecConfigObu& codec_config_obu) {
  ScalableChannelLayoutConfig config{
      .channel_audio_layer_configs = {ChannelAudioLayerConfig{
          .loudspeaker_layout = loudspeaker_layout,
          .output_gain_is_present_flag = false,
          .recon_gain_is_present_flag = false,
          .reserved_a = 0,
          .expanded_loudspeaker_layout = expanded_loudspeaker_layout,
      }}};
  RETURN_IF_NOT_OK(ObuWithDataGenerator::FillSubstreamCounts(config));

  const uint8_t substream_count =
      config.channel_audio_layer_configs.front().substream_count;
  std::vector<DecodedUleb128> substream_ids(substream_count);
  std::iota(substream_ids.begin(), substream_ids.end(), DecodedUleb128{0});

  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), audio_element_id, /*reserved=*/0,
      codec_config_obu.GetCodecConfigId(), substream_ids, config);
  if (!obu.ok()) {
    return obu.status();
  }

  SubstreamIdLabelsMap substream_id_to_labels;
  LabelGainMap label_to_output_gain;
  std::vector<ChannelNumbers> channel_numbers_for_layers;
  RETURN_IF_NOT_OK(ObuWithDataGenerator::FinalizeScalableChannelLayoutConfig(
      obu->audio_substream_ids_, config, substream_id_to_labels,
      label_to_output_gain, channel_numbers_for_layers));
  return AudioElementWithData{
      .obu = *std::move(obu),
      .codec_config = &codec_config_obu,
      .substream_id_to_labels = std::move(substream_id_to_labels),
      .label_to_output_gain = std::move(label_to_output_gain),
      .channel_numbers_for_layers = std::move(channel_numbers_for_layers),
  };
}

}  // namespace

IASequenceHeaderObu MakeIaSequenceHeaderObu(
    AudioElementType audio_element_type) {
  ProfileVersion profile = ProfileVersion::kIamfSimpleProfile;
  // TODO(b/585126453): Use AmbisonicMixer.
  switch (audio_element_type) {
    case AudioElementType::kAmbisonicSphericalOrder4:
    case AudioElementType::kAmbisonicHorizontalOrder4:
    case AudioElementType::kLayout9_1_6:
      profile = ProfileVersion::kIamfBaseEnhancedProfile;
      break;
    case AudioElementType::kLayout7_1_5_4:
      profile = ProfileVersion::kIamfBaseAdvancedProfile;
      break;
    case AudioElementType::kLayout10_2_9_3:
      profile = ProfileVersion::kIamfAdvanced2Profile;
      break;
    default:
      profile = ProfileVersion::kIamfSimpleProfile;
      break;
  }
  return IASequenceHeaderObu(ObuHeader(), profile, profile);
}

absl::StatusOr<CodecConfigObu> CreateOpusCodecConfigObu(
    DecodedUleb128 codec_config_id, DecodedUleb128 num_samples_per_frame) {
  CodecConfig codec_config{.num_samples_per_frame = num_samples_per_frame,
                           .decoder_config = OpusDecoderConfig{
                               .version_ = kOpusVersion,
                               .pre_skip_ = 0,
                               .input_sample_rate_ = kOpusSampleRate,
                           }};
  auto codec_config_obu =
      CodecConfigObu::Create(ObuHeader(), codec_config_id, codec_config);
  if (!codec_config_obu.ok()) {
    return codec_config_obu.status();
  }
  OpusEncoder probe_encoder(
      OpusEncoder::Settings{
          .use_float_api = true,
          .libopus_application_mode = OPUS_APPLICATION_AUDIO,
          .target_substream_bitrate = kOpusTargetBitratePerChannel,
      },
      *codec_config_obu, SubstreamChannelCount::MakeSingular());
  RETURN_IF_NOT_OK(probe_encoder.Initialize(/*validate_codec_delay=*/false));
  codec_config_obu->SetCodecDelay(
      static_cast<uint16_t>(probe_encoder.GetNumberOfSamplesToDelayAtStart()));
  return codec_config_obu;
}

absl::StatusOr<CodecConfigObu> CreatePcmCodecConfigObu(
    PcmSampleRate sample_rate, DecodedUleb128 codec_config_id,
    DecodedUleb128 num_samples_per_frame) {
  uint32_t sample_rate_hz = 0;
  switch (sample_rate) {
    case PcmSampleRate::k16kHz:
      sample_rate_hz = 16000;
      break;
    case PcmSampleRate::k32kHz:
      sample_rate_hz = 32000;
      break;
    case PcmSampleRate::k44_1kHz:
      sample_rate_hz = 44100;
      break;
    case PcmSampleRate::k48kHz:
      sample_rate_hz = 48000;
      break;
    case PcmSampleRate::k96kHz:
      sample_rate_hz = 96000;
      break;
    default:
      return absl::InvalidArgumentError("Unsupported PcmSampleRate.");
  }
  CodecConfig codec_config{
      .num_samples_per_frame = num_samples_per_frame,
      .decoder_config = LpcmDecoderConfig{
          .sample_format_flags_bitmask_ = LpcmDecoderConfig::kLpcmLittleEndian,
          .sample_size_ = kLpcmBitDepth,
          .sample_rate_ = sample_rate_hz,
      }};
  return CodecConfigObu::Create(ObuHeader(), codec_config_id, codec_config);
}

absl::StatusOr<AudioElementWithData> CreateAudioElementWithData(
    AudioElementType audio_element_type, DecodedUleb128 audio_element_id,
    const CodecConfigObu& codec_config_obu) {
  using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;

  switch (audio_element_type) {
    case AudioElementType::kAmbisonicSphericalOrder1:
      return CreateAmbisonicsAudioElementWithData(
          audio_element_id, kFirstOrderMaxAcnPlusOne, {0, 1, 2, 3},
          codec_config_obu);
    case AudioElementType::kAmbisonicSphericalOrder2:
      return CreateAmbisonicsAudioElementWithData(
          audio_element_id, kSecondOrderMaxAcnPlusOne,
          {0, 1, 2, 3, 4, 5, 6, 7, 8}, codec_config_obu);
    case AudioElementType::kAmbisonicSphericalOrder3:
      return CreateAmbisonicsAudioElementWithData(
          audio_element_id, kThirdOrderMaxAcnPlusOne,
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
          codec_config_obu);
    case AudioElementType::kAmbisonicSphericalOrder4:
      return CreateAmbisonicsAudioElementWithData(
          audio_element_id, kFourthOrderMaxAcnPlusOne,
          {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
           13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},
          codec_config_obu);
    case AudioElementType::kAmbisonicHorizontalOrder1:
      return CreateAmbisonicsAudioElementWithData(audio_element_id,
                                                  kFirstOrderMaxAcnPlusOne,
                                                  {0, 1, 3}, codec_config_obu);
    case AudioElementType::kAmbisonicHorizontalOrder2:
      return CreateAmbisonicsAudioElementWithData(
          audio_element_id, kSecondOrderMaxAcnPlusOne, {0, 1, 3, 4, 8},
          codec_config_obu);
    case AudioElementType::kAmbisonicHorizontalOrder3:
      return CreateAmbisonicsAudioElementWithData(
          audio_element_id, kThirdOrderMaxAcnPlusOne, {0, 1, 3, 4, 8, 9, 15},
          codec_config_obu);
    case AudioElementType::kAmbisonicHorizontalOrder4:
      return CreateAmbisonicsAudioElementWithData(
          audio_element_id, kFourthOrderMaxAcnPlusOne,
          {0, 1, 3, 4, 8, 9, 15, 16, 24}, codec_config_obu);
    case AudioElementType::kLayoutMono:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayoutMono, std::nullopt, codec_config_obu);
    case AudioElementType::kLayoutStereo:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayoutStereo, std::nullopt, codec_config_obu);
    case AudioElementType::kLayout5_1:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayout5_1_ch, std::nullopt, codec_config_obu);
    case AudioElementType::kLayout5_1_2:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayout5_1_2_ch, std::nullopt, codec_config_obu);
    case AudioElementType::kLayout5_1_4:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayout5_1_4_ch, std::nullopt, codec_config_obu);
    case AudioElementType::kLayout7_1:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayout7_1_ch, std::nullopt, codec_config_obu);
    case AudioElementType::kLayout7_1_2:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayout7_1_2_ch, std::nullopt, codec_config_obu);
    case AudioElementType::kLayout7_1_4:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayout7_1_4_ch, std::nullopt, codec_config_obu);
    case AudioElementType::kLayout3_1_2:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayout3_1_2_ch, std::nullopt, codec_config_obu);
    case AudioElementType::kLayoutBinaural:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayoutBinaural, std::nullopt, codec_config_obu);
    case AudioElementType::kLayout9_1_6:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayoutExpanded, kExpandedLayout9_1_6_ch,
          codec_config_obu);
    case AudioElementType::kLayout10_2_9_3:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayoutExpanded, kExpandedLayout10_2_9_3,
          codec_config_obu);
    case AudioElementType::kLayout7_1_5_4:
      return CreateChannelBasedAudioElementWithData(
          audio_element_id, kLayoutExpanded, kExpandedLayout7_1_5_4Ch,
          codec_config_obu);
  }
  return absl::InvalidArgumentError("Unsupported AudioElementType.");
}

absl::StatusOr<absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>>
CreateSubstreamEncoders(const AudioElementWithData& audio_element,
                        const CodecConfigObu& codec_config_obu) {
  absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>
      substream_id_to_encoder;
  for (const auto& [substream_id, labels] :
       audio_element.substream_id_to_labels) {
    auto channel_count = SubstreamChannelCount::Create(labels.size());
    if (!channel_count.ok()) {
      return channel_count.status();
    }

    std::unique_ptr<EncoderBase> encoder;
    switch (codec_config_obu.GetCodecConfig().GetCodecId()) {
      case CodecConfig::kCodecIdLpcm:
        encoder =
            std::make_unique<LpcmEncoder>(codec_config_obu, *channel_count);
        break;
      case CodecConfig::kCodecIdOpus:
        encoder = std::make_unique<OpusEncoder>(
            OpusEncoder::Settings{
                .use_float_api = true,
                .libopus_application_mode = OPUS_APPLICATION_AUDIO,
                .target_substream_bitrate =
                    static_cast<int32_t>(kOpusTargetBitratePerChannel *
                                         channel_count->num_channels()),
            },
            codec_config_obu, *channel_count);
        break;
      default:
        return absl::InvalidArgumentError("Unsupported codec ID.");
    }
    RETURN_IF_NOT_OK(encoder->Initialize(/*validate_codec_delay=*/true));
    substream_id_to_encoder.emplace(substream_id, std::move(encoder));
  }
  return substream_id_to_encoder;
}

absl::StatusOr<MixPresentationObu> CreateMixPresentationObu(
    DecodedUleb128 mix_presentation_id, DecodedUleb128 audio_element_id,
    const CodecConfigObu& codec_config_obu) {
  const uint32_t output_sample_rate = codec_config_obu.GetOutputSampleRate();
  MixGainParamDefinition element_mix_gain(ParamDefinition::BaseArgs{
      .parameter_id = kElementMixGainParameterId,
      .parameter_rate = output_sample_rate,
      .schedule = std::nullopt,
  });
  MixGainParamDefinition output_mix_gain(ParamDefinition::BaseArgs{
      .parameter_id = kOutputMixGainParameterId,
      .parameter_rate = output_sample_rate,
      .schedule = std::nullopt,
  });

  std::vector<MixPresentationSubMix> sub_mixes = {MixPresentationSubMix{
      .audio_elements = {SubMixAudioElement{
          .audio_element_id = audio_element_id,
          .localized_element_annotations = {kDefaultAudioElementAnnotation},
          .rendering_config =
              RenderingConfig{
                  .headphones_rendering_mode = RenderingConfig::
                      kHeadphonesRenderingModeBinauralWorldLocked,
              },
          .element_mix_gain = element_mix_gain,
      }},
      .output_mix_gain = output_mix_gain,
      .layouts = {MixPresentationLayout{
          .loudness_layout =
              Layout{
                  .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
                  .specific_layout =
                      LoudspeakersSsConventionLayout{
                          .sound_system = LoudspeakersSsConventionLayout::
                              kSoundSystemA_0_2_0,
                          .reserved = 0,
                      },
              },
          .loudness =
              LoudnessInfo{
                  .integrated_loudness =
                      QFormatOrFloatingPoint::MakeFromQ7_8(0),
                  .digital_peak = QFormatOrFloatingPoint::MakeFromQ7_8(0),
              },
      }},
  }};

  return MixPresentationObu(ObuHeader(), mix_presentation_id, /*count_label=*/1,
                            {kDefaultAnnotationsLanguage},
                            {kDefaultMixPresentationAnnotation}, sub_mixes);
}

}  // namespace iamf_tools
