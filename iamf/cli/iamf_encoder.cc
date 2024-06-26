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
#include "iamf/cli/iamf_encoder.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/proto_to_obu/ia_sequence_header_generator.h"
#include "iamf/cli/proto_to_obu/mix_presentation_generator.h"
#include "iamf/cli/proto_to_obu/parameter_block_generator.h"
#include "iamf/common/macros.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

namespace {
absl::Status InitAudioFrameDecoderForAllAudioElements(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    AudioFrameDecoder& audio_frame_decoder) {
  for (const auto& [unused_audio_element_id, audio_element] : audio_elements) {
    if (audio_element.codec_config == nullptr) {
      // Skip stray audio elements. We won't know how to decode their
      // substreams.
      continue;
    }

    RETURN_IF_NOT_OK(audio_frame_decoder.InitDecodersForSubstreams(
        audio_element.substream_id_to_labels, *audio_element.codec_config));
  }
  return absl::OkStatus();
}

}  // namespace

IamfEncoder::IamfEncoder(
    const iamf_tools_cli_proto::UserMetadata& user_metadata)
    : user_metadata_(user_metadata),
      add_samples_finalized_(false),
      parameter_block_generator_(
          user_metadata.test_vector_metadata().override_computed_recon_gains(),
          parameter_id_to_metadata_) {}

absl::Status IamfEncoder::GenerateDescriptorObus(
    std::optional<IASequenceHeaderObu>& ia_sequence_header_obu,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  // IA Sequence Header OBU. Only one is allowed.
  if (user_metadata_.ia_sequence_header_metadata_size() != 1) {
    return absl::InvalidArgumentError(
        "Only one IA Sequence Header allowed in an IA Sequence.");
  }

  IaSequenceHeaderGenerator ia_sequence_header_generator(
      user_metadata_.ia_sequence_header_metadata(0));
  RETURN_IF_NOT_OK(
      ia_sequence_header_generator.Generate(ia_sequence_header_obu));

  // Codec Config OBUs.
  CodecConfigGenerator codec_config_generator(
      user_metadata_.codec_config_metadata());
  RETURN_IF_NOT_OK(codec_config_generator.Generate(codec_config_obus));

  // Audio Element OBUs.
  AudioElementGenerator audio_element_generator(
      user_metadata_.audio_element_metadata());
  RETURN_IF_NOT_OK(
      audio_element_generator.Generate(codec_config_obus, audio_elements));

  // Generate the majority of Mix Presentation OBUs - loudness will be
  // calculated later.
  MixPresentationGenerator mix_presentation_generator(
      user_metadata_.mix_presentation_metadata());
  RETURN_IF_NOT_OK(mix_presentation_generator.Generate(mix_presentation_obus));

  // Collect and validate consistency of all `ParamDefinition`s in all
  // Audio Element and Mix Presentation OBUs.
  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      audio_elements, mix_presentation_obus, param_definitions_));

  // Initialize the global timing module.
  RETURN_IF_NOT_OK(
      global_timing_module_.Initialize(audio_elements, param_definitions_));

  // Initialize the parameter block generator.
  RETURN_IF_NOT_OK(parameter_block_generator_.Initialize(audio_elements,
                                                         param_definitions_));

  // Put generated parameter blocks in a manager that supports easier queries.
  parameters_manager_ = std::make_unique<ParametersManager>(audio_elements);
  RETURN_IF_NOT_OK(parameters_manager_->Initialize());

  // Down-mix the audio samples and then demix audio samples while decoding
  // them. This is useful to create multi-layer audio elements and to determine
  // the recon gain parameters and to measuring loudness.
  RETURN_IF_NOT_OK(demixing_module_.InitializeForDownMixingAndReconstruction(
      user_metadata_, audio_elements));

  audio_frame_generator_ = std::make_unique<AudioFrameGenerator>(
      user_metadata_.audio_frame_metadata(),
      user_metadata_.codec_config_metadata(), audio_elements, demixing_module_,
      *parameters_manager_, global_timing_module_);
  RETURN_IF_NOT_OK(audio_frame_generator_->Initialize());

  // Initialize the audio frame decoder. It is needed to determine the recon
  // gain parameters and measure the loudness of the mixes.
  audio_frame_decoder_ = std::make_unique<AudioFrameDecoder>();
  RETURN_IF_NOT_OK(InitAudioFrameDecoderForAllAudioElements(
      audio_elements, *audio_frame_decoder_));

  return absl::OkStatus();
}

bool IamfEncoder::GeneratingDataObus() {
  // Clear cached samples for this iteration of data OBU generation.
  for (auto& [audio_element_id, labeled_samples] : id_to_labeled_samples_) {
    for (auto& [label, samples] : labeled_samples) {
      samples.clear();
    }
  }

  return (audio_frame_generator_ != nullptr) &&
         (audio_frame_generator_->TakingSamples() ||
          audio_frame_generator_->GeneratingFrames());
}

absl::Status IamfEncoder::GetInputTimestamp(int32_t& input_timestamp) {
  std::optional<int32_t> timestamp;
  RETURN_IF_NOT_OK(
      global_timing_module_.GetGlobalAudioFrameTimestamp(timestamp));
  if (!timestamp.has_value()) {
    return absl::InvalidArgumentError("Global timestamp has no value");
  }
  input_timestamp = *timestamp;
  return absl::OkStatus();
}

void IamfEncoder::AddSamples(const DecodedUleb128 audio_element_id,
                             const std::string& label,
                             const std::vector<int32_t>& samples) {
  id_to_labeled_samples_[audio_element_id][label] = samples;
}

void IamfEncoder::FinalizeAddSamples() { add_samples_finalized_ = true; }

absl::Status IamfEncoder::AddParameterBlockMetadata(
    const iamf_tools_cli_proto::ParameterBlockObuMetadata&
        parameter_block_metadata) {
  RETURN_IF_NOT_OK(
      parameter_block_generator_.AddMetadata(parameter_block_metadata));
  return absl::OkStatus();
}

absl::Status IamfEncoder::OutputTemporalUnit(
    std::list<AudioFrameWithData>& audio_frames,
    std::list<ParameterBlockWithData>& parameter_blocks,
    IdLabeledFrameMap& id_to_labeled_frame, int32_t& output_timestamp) {
  audio_frames.clear();
  parameter_blocks.clear();

  // Generate mix gain and demixing parameter blocks.
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateDemixing(
      global_timing_module_, temp_demixing_parameter_blocks_));
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateMixGain(
      global_timing_module_, temp_mix_gain_parameter_blocks_));

  // Add the newly generated demixing parameter blocks to the parameters
  // manager so they can be easily queried by the audio frame generator.
  for (const auto& demixing_parameter_block : temp_demixing_parameter_blocks_) {
    parameters_manager_->AddDemixingParameterBlock(&demixing_parameter_block);
  }

  for (const auto& [audio_element_id, labeled_samples] :
       id_to_labeled_samples_) {
    for (const auto& [label, samples] : labeled_samples) {
      RETURN_IF_NOT_OK(
          audio_frame_generator_->AddSamples(audio_element_id, label, samples));
    }
  }
  if (add_samples_finalized_) {
    RETURN_IF_NOT_OK(audio_frame_generator_->Finalize());
  }

  RETURN_IF_NOT_OK(audio_frame_generator_->OutputFrames(audio_frames));
  if (audio_frames.empty()) {
    return absl::OkStatus();
  }

  // Decode the audio frames. They are required to determine the demixed
  // frames.
  std::list<DecodedAudioFrame> decoded_audio_frames;
  RETURN_IF_NOT_OK(
      audio_frame_decoder_->Decode(audio_frames, decoded_audio_frames));

  // Demix the audio frames.
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  RETURN_IF_NOT_OK(demixing_module_.DemixAudioSamples(
      audio_frames, decoded_audio_frames, id_to_labeled_frame,
      id_to_labeled_decoded_frame));

  // Recon gain parameter blocks are generated based on the original and
  // demixed audio frames.
  RETURN_IF_NOT_OK(parameter_block_generator_.GenerateReconGain(
      id_to_labeled_frame, id_to_labeled_decoded_frame, global_timing_module_,
      temp_recon_gain_parameter_blocks_));

  // Move all generated parameter blocks belonging to this temporal unit to
  // the output.
  output_timestamp = audio_frames.front().start_timestamp;
  for (auto* temp_parameter_blocks :
       {&temp_mix_gain_parameter_blocks_, &temp_demixing_parameter_blocks_,
        &temp_recon_gain_parameter_blocks_}) {
    auto last_same_timestamp_iter = std::find_if(
        temp_parameter_blocks->begin(), temp_parameter_blocks->end(),
        [output_timestamp](const auto& parameter_block) {
          return parameter_block.start_timestamp > output_timestamp;
        });
    parameter_blocks.splice(parameter_blocks.end(), *temp_parameter_blocks,
                            temp_parameter_blocks->begin(),
                            last_same_timestamp_iter);
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
