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
#include "iamf/cli/proto_conversion/proto_iamf_encoder.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_frame_generator.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/iamf_encoder.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_generator.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/encoder_control_metadata.pb.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/channel_label_utils.h"
#include "iamf/cli/proto_conversion/downmixing_reconstruction_util.h"
#include "iamf/cli/proto_conversion/proto_to_obu/arbitrary_obu_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_frame_generator_factory.h"
#include "iamf/cli/proto_conversion/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/ia_sequence_header_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/metadata_obu_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/mix_presentation_generator.h"
#include "iamf/cli/proto_conversion/proto_to_obu/parameter_block_metadata_converter.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/types.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace iamf_tools {
namespace {

using ::iamf_tools_cli_proto::ChannelLabelMessage;
using ::iamf_tools_cli_proto::ParameterBlockObuMetadata;

api::IamfStatus AbslToIamfStatus(const absl::Status& absl_status) {
  if (absl_status.ok()) {
    return api::IamfStatus::OkStatus();
  }
  return api::IamfStatus::ErrorStatus(
      absl_status.ToString(absl::StatusToStringMode::kDefault));
}

// Arranges the `ArbitraryObu`s into the non-timestamped (descriptor) and
// timestamped lists.
void ArrangeArbitraryObus(
    std::list<ArbitraryObu>& original_arbitrary_obus,
    std::list<ArbitraryObu>& descriptor_arbitrary_obus,
    absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>&
        timestamp_to_arbitrary_obus) {
  while (!original_arbitrary_obus.empty()) {
    auto& arbitrary_obu = original_arbitrary_obus.front();
    // Arrange to the timestamps slot (if present), or the untimestamped slot,
    // which implies the OBUs are descriptor OBUs.
    auto& list_to_move_to =
        arbitrary_obu.insertion_tick_.has_value()
            ? timestamp_to_arbitrary_obus[*arbitrary_obu.insertion_tick_]
            : descriptor_arbitrary_obus;
    list_to_move_to.push_back(std::move(arbitrary_obu));
    original_arbitrary_obus.pop_front();
  }
}

}  // namespace

absl::StatusOr<std::unique_ptr<ProtoIamfEncoder>> ProtoIamfEncoder::Create(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    std::optional<TrimmingSettings> trimming_settings,
    const LoudnessCalculatorFactoryBase* absl_nullable
        loudness_calculator_factory,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    const IamfEncoder::ObuSequencerFactory& obu_sequencer_factory) {
  // IA Sequence Header OBU. Only one is allowed.
  if (user_metadata.ia_sequence_header_metadata_size() != 1) {
    return absl::InvalidArgumentError(
        "Only one IA Sequence Header allowed in an IA Sequence.");
  }

  std::optional<IASequenceHeaderObu> ia_sequence_header_obu;
  IaSequenceHeaderGenerator ia_sequence_header_generator(
      user_metadata.ia_sequence_header_metadata(0));
  RETURN_IF_NOT_OK(
      ia_sequence_header_generator.Generate(ia_sequence_header_obu));
  RETURN_IF_NOT_OK(
      ValidateHasValue(ia_sequence_header_obu, "IA Sequence Header"));

  // Metadata OBUs.
  std::list<MetadataObu> metadata_obus;
  MetadataObuGenerator metadata_obu_generator(
      user_metadata.metadata_obu_metadata());
  RETURN_IF_NOT_OK(metadata_obu_generator.Generate(metadata_obus));

  // Codec Config OBUs.
  // Held in a `unique_ptr`, so the underlying map can be moved without
  // invalidating pointers.
  auto codec_config_obus = std::make_unique<DescriptorObus::CodecConfigsById>();
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  RETURN_IF_NOT_OK(codec_config_generator.Generate(*codec_config_obus));

  // Audio Element OBUs.
  // Held in a `unique_ptr`, so the underlying map can be moved without
  // invalidating pointers.
  auto audio_elements = std::make_unique<DescriptorObus::AudioElementsById>();
  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  RETURN_IF_NOT_OK(
      audio_element_generator.Generate(*codec_config_obus, *audio_elements));

  // Generate the majority of Mix Presentation OBUs - loudness will be
  // calculated after all temporal units have been pushed.
  DescriptorObus::MixPresentationObus mix_presentation_obus;
  MixPresentationGenerator mix_presentation_generator(
      user_metadata.mix_presentation_metadata());
  RETURN_IF_NOT_OK(mix_presentation_generator.Generate(
      user_metadata.encoder_control_metadata().add_build_information_tag(),
      mix_presentation_obus));

  // Generate Arbitrary OBUs.
  std::list<ArbitraryObu> unorganized_arbitrary_obus;
  ArbitraryObuGenerator arbitrary_obu_generator(
      user_metadata.arbitrary_obu_metadata());
  RETURN_IF_NOT_OK(
      arbitrary_obu_generator.Generate(unorganized_arbitrary_obus));
  std::list<ArbitraryObu> descriptor_arbitrary_obus;
  absl::btree_map<InternalTimestamp, std::list<ArbitraryObu>>
      timestamp_to_arbitrary_obus;
  ArrangeArbitraryObus(unorganized_arbitrary_obus, descriptor_arbitrary_obus,
                       timestamp_to_arbitrary_obus);

  const auto leb_generator =
      CreateLebGenerator(user_metadata.test_vector_metadata().leb_generator());
  if (leb_generator == nullptr) {
    return absl::InvalidArgumentError("Failed to create LebGenerator.");
  }

  auto encoder = IamfEncoder::Create(
      user_metadata.test_vector_metadata().validate_user_loudness(),
      user_metadata.test_vector_metadata().override_computed_recon_gains(),
      user_metadata.temporal_delimiter_metadata().enable_temporal_delimiters(),
      *leb_generator, std::move(*ia_sequence_header_obu),
      std::move(metadata_obus), std::move(codec_config_obus),
      std::move(audio_elements), std::move(mix_presentation_obus),
      std::move(descriptor_arbitrary_obus),
      std::move(timestamp_to_arbitrary_obus), trimming_settings,
      loudness_calculator_factory, sample_processor_factory,
      obu_sequencer_factory,
      [&user_metadata](const DescriptorObus::AudioElementsById& elements,
                       ParametersManager& parameters_manager,
                       GlobalTimingModule& global_timing_module)
          -> absl::StatusOr<std::unique_ptr<AudioFrameGenerator> absl_nonnull> {
        auto audio_element_id_to_downmixing_config =
            CreateAudioElementIdToDownmixingConfig(user_metadata, elements);
        if (!audio_element_id_to_downmixing_config.ok()) {
          return audio_element_id_to_downmixing_config.status();
        }
        return CreateAudioFrameGeneratorFromMetadata(
            user_metadata.audio_frame_metadata(),
            user_metadata.codec_config_metadata(), elements,
            DownmixerManager::Make(
                std::move(*audio_element_id_to_downmixing_config)),
            parameters_manager, global_timing_module);
      });
  if (!encoder.ok()) {
    return encoder.status();
  }
  return absl::WrapUnique(new ProtoIamfEncoder(std::move(*encoder)));
}

api::IamfStatus ProtoIamfEncoder::GetDescriptorObus(
    bool redundant_copy, std::vector<uint8_t>& descriptor_obus,
    bool& output_obus_are_finalized) const {
  return encoder_->GetDescriptorObus(redundant_copy, descriptor_obus,
                                     output_obus_are_finalized);
}

uint32_t ProtoIamfEncoder::GetEncoderDelay() const {
  return encoder_->GetEncoderDelay();
}

uint32_t ProtoIamfEncoder::GetNumSamplesPerFrame() const {
  return encoder_->GetNumSamplesPerFrame();
}

bool ProtoIamfEncoder::GeneratingTemporalUnits() const {
  return encoder_->GeneratingTemporalUnits();
}

absl::Status ProtoIamfEncoder::GetInputTimestamp(
    InternalTimestamp& input_timestamp) {
  return encoder_->GetInputTimestamp(input_timestamp);
}

api::IamfStatus ProtoIamfEncoder::Encode(
    const api::IamfTemporalUnitData& temporal_unit_data) {
  std::vector<ParameterBlockMetadata> parameter_blocks;
  parameter_blocks.reserve(
      temporal_unit_data.parameter_block_id_to_metadata.size());
  ParameterBlockObuMetadata proto_metadata;
  for (const auto& [parameter_block_id, raw_parameter_block_metadata] :
       temporal_unit_data.parameter_block_id_to_metadata) {
    if (!proto_metadata.ParseFromString(raw_parameter_block_metadata)) {
      return api::IamfStatus::ErrorStatus(
          "Failed to deserialize a `ParameterBlockObuMetadata` protocol "
          "buffer.");
    }
    auto parameter_block_metadata =
        ConvertToParameterBlockMetadata(proto_metadata);
    if (!parameter_block_metadata.ok()) {
      return AbslToIamfStatus(parameter_block_metadata.status());
    }
    parameter_blocks.push_back(std::move(*parameter_block_metadata));
  }

  if (finalize_encode_called_) {
    if (!temporal_unit_data.audio_element_id_to_data.empty()) {
      ABSL_LOG_FIRST_N(WARNING, 3)
          << "Calling `Encode()` with samples after "
             "`FinalizeEncode()` drops the audio samples.";
    }
    return encoder_->Encode({}, parameter_blocks);
  }

  absl::flat_hash_map<
      DecodedUleb128,
      absl::flat_hash_map<ChannelLabel::Label, std::span<const double>>>
      audio_element_id_to_data;
  ChannelLabelMessage channel_label_message;
  for (const auto& [audio_element_id, labeled_samples] :
       temporal_unit_data.audio_element_id_to_data) {
    auto& converted_labeled_samples =
        audio_element_id_to_data[audio_element_id];
    for (const auto& [label, samples] : labeled_samples) {
      if (!channel_label_message.ParseFromString(label)) {
        return api::IamfStatus::ErrorStatus(
            "Failed to deserialize `ChannelLabelMessage` protocol buffer.");
      }
      auto internal_label = ChannelLabelUtils::ProtoToLabel(
          channel_label_message.channel_label());
      if (!internal_label.ok()) {
        return AbslToIamfStatus(internal_label.status());
      }
      converted_labeled_samples[*internal_label] = samples;
    }
  }

  return encoder_->Encode(audio_element_id_to_data, parameter_blocks);
}

api::IamfStatus ProtoIamfEncoder::Encode(
    std::span<const std::span<const double>> channel_samples) {
  return encoder_->Encode(channel_samples);
}

api::IamfStatus ProtoIamfEncoder::OutputTemporalUnit(
    std::vector<uint8_t>& temporal_unit_obus) {
  return encoder_->OutputTemporalUnit(temporal_unit_obus);
}

api::IamfStatus ProtoIamfEncoder::FinalizeEncode() {
  finalize_encode_called_ = true;
  return encoder_->FinalizeEncode();
}

const DescriptorObus::AudioElementsById& ProtoIamfEncoder::GetAudioElements()
    const {
  return encoder_->GetAudioElements();
}

const DescriptorObus::MixPresentationObus&
ProtoIamfEncoder::GetMixPresentationObus(bool& output_is_finalized) const {
  return encoder_->GetMixPresentationObus(output_is_finalized);
}

const std::list<ArbitraryObu>& ProtoIamfEncoder::GetDescriptorArbitraryObus()
    const {
  return encoder_->GetDescriptorArbitraryObus();
}

}  // namespace iamf_tools
