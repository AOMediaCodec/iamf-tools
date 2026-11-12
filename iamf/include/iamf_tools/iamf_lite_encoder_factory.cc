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
#include "iamf/include/iamf_tools/iamf_lite_encoder_factory.h"

#include <memory>
#include <utility>

#include "absl/log/absl_log.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/iamf_components.h"
#include "iamf/cli/iamf_encoder.h"
#include "iamf/cli/iamf_lite_encoder_factory_utils.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/include/iamf_tools/iamf_encoder_interface.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace api {
namespace {

// Default descriptor OBU IDs and frame settings for single-element encoders.
constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kMixPresentationId = 42;
constexpr DecodedUleb128 kOpusNumSamplesPerFrame = 960;
constexpr DecodedUleb128 kPcmNumSamplesPerFrame = 1024;
constexpr bool kEnableTemporalDelimiters = false;

std::unique_ptr<IamfEncoderInterface>
CreateOneElementIamfEncoderFromCodecConfig(
    CodecConfigObu codec_config_obu, AudioElementType audio_element_type) {
  auto codec_config_obus = std::make_unique<DescriptorObus::CodecConfigsById>();
  const auto [codec_config_iter, unused_inserted] =
      codec_config_obus->emplace(kCodecConfigId, std::move(codec_config_obu));
  const CodecConfigObu& stored_codec_config = codec_config_iter->second;

  auto audio_element = CreateAudioElementWithData(
      audio_element_type, kAudioElementId, stored_codec_config);
  if (!audio_element.ok()) {
    ABSL_LOG(ERROR) << "Failed to create AudioElementWithData: "
                    << audio_element.status();
    return nullptr;
  }

  auto audio_elements = std::make_unique<DescriptorObus::AudioElementsById>();
  const auto [audio_element_iter, unused_element_inserted] =
      audio_elements->emplace(kAudioElementId, *std::move(audio_element));

  auto substream_id_to_encoder =
      CreateSubstreamEncoders(audio_element_iter->second, stored_codec_config);
  if (!substream_id_to_encoder.ok()) {
    ABSL_LOG(ERROR) << "Failed to create substream encoders: "
                    << substream_id_to_encoder.status();
    return nullptr;
  }

  auto mix_presentation_obu = CreateMixPresentationObu(
      kMixPresentationId, kAudioElementId, stored_codec_config);
  if (!mix_presentation_obu.ok()) {
    ABSL_LOG(ERROR) << "Failed to create MixPresentationObu: "
                    << mix_presentation_obu.status();
    return nullptr;
  }
  DescriptorObus::MixPresentationObus mix_presentation_obus;
  mix_presentation_obus.push_back(*std::move(mix_presentation_obu));

  const auto loudness_calculator_factory = CreateLoudnessCalculatorFactory();
  auto iamf_encoder = IamfEncoder::CreateFromObus(
      MakeIaSequenceHeaderObu(audio_element_type), std::move(codec_config_obus),
      std::move(audio_elements), std::move(mix_presentation_obus),
      *std::move(substream_id_to_encoder), kEnableTemporalDelimiters,
      iamf_tools::TrimmingSettings{}, loudness_calculator_factory.get(),
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      IamfEncoder::CreateNoObuSequencers);
  if (!iamf_encoder.ok()) {
    ABSL_LOG(ERROR) << "Failed to create encoder: " << iamf_encoder.status();
    return nullptr;
  }
  return *std::move(iamf_encoder);
}

}  // namespace

std::unique_ptr<IamfEncoderInterface>
IamfLiteEncoderFactory::CreateOneElementOpusIamfEncoder(
    AudioElementType audio_element_type) {
  auto codec_config_obu =
      CreateOpusCodecConfigObu(kCodecConfigId, kOpusNumSamplesPerFrame);
  if (!codec_config_obu.ok()) {
    ABSL_LOG(ERROR) << "Failed to create Opus CodecConfigObu: "
                    << codec_config_obu.status();
    return nullptr;
  }
  return CreateOneElementIamfEncoderFromCodecConfig(
      *std::move(codec_config_obu), audio_element_type);
}

std::unique_ptr<IamfEncoderInterface>
IamfLiteEncoderFactory::CreateOneElementInt16LePcmIamfEncoder(
    PcmSampleRate sample_rate, AudioElementType audio_element_type) {
  auto codec_config_obu = CreatePcmCodecConfigObu(sample_rate, kCodecConfigId,
                                                  kPcmNumSamplesPerFrame);
  if (!codec_config_obu.ok()) {
    ABSL_LOG(ERROR) << "Failed to create PCM CodecConfigObu: "
                    << codec_config_obu.status();
    return nullptr;
  }
  return CreateOneElementIamfEncoderFromCodecConfig(
      *std::move(codec_config_obu), audio_element_type);
}

}  // namespace api
}  // namespace iamf_tools
