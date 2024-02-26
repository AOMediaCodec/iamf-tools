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
#include "iamf/cli/encoder_main_lib.h"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/arbitrary_obu.h"
#include "iamf/cli/arbitrary_obu_generator.h"
#include "iamf/cli/audio_element_generator.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_generator.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/codec_config_generator.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/ia_sequence_header_generator.h"
#include "iamf/cli/iamf_components.h"
#include "iamf/cli/mix_presentation_generator.h"
#include "iamf/cli/obu_sequencer.h"
#include "iamf/cli/parameter_block_generator.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/wav_sample_provider.h"
#include "iamf/codec_config.h"
#include "iamf/ia.h"
#include "iamf/ia_sequence_header.h"
#include "iamf/mix_presentation.h"
#include "iamf/param_definitions.h"
#include "iamf/parameter_block.h"

namespace iamf_tools {

namespace {

absl::Status CreateOutputDirectory(const std::string& output_directory) {
  if (output_directory.empty() ||
      std::filesystem::is_directory(output_directory) ||
      std::filesystem::is_character_file(output_directory)) {
    return absl::OkStatus();
  }

  std::error_code error_code;
  if (!std::filesystem::create_directories(output_directory, error_code)) {
    LOG(ERROR) << "Failed to create output directory: " << output_directory;
    return absl::UnknownError("");
  }

  return absl::OkStatus();
}

absl::Status GenerateObus(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const std::string& input_wav_directory,
    const std::string& output_wav_directory,
    std::optional<IASequenceHeaderObu>& ia_sequence_header_obu,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus,
    std::list<AudioFrameWithData>& audio_frames,
    std::list<ParameterBlockWithData>& parameter_blocks,
    absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>&
        parameter_id_to_metadata,
    std::list<ArbitraryObu>& arbitrary_obus) {
  // IA Sequence Header OBU. Only one is allowed.
  if (user_metadata.ia_sequence_header_metadata_size() != 1) {
    LOG(ERROR) << "Only one IA Sequence Header allowed in an IA Sequence.";
    return absl::InvalidArgumentError("");
  }
  IaSequenceHeaderGenerator ia_sequence_header_generator(
      user_metadata.ia_sequence_header_metadata(0));
  RETURN_IF_NOT_OK(
      ia_sequence_header_generator.Generate(ia_sequence_header_obu));

  // Codec Config OBUs.
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  RETURN_IF_NOT_OK(codec_config_generator.Generate(codec_config_obus));

  // Audio Element OBUs.
  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  RETURN_IF_NOT_OK(
      audio_element_generator.Generate(codec_config_obus, audio_elements));

  // Generate the majority of Mix Presentation OBUs - loudness will be
  // calculated later.
  MixPresentationGenerator mix_presentation_generator(
      user_metadata.mix_presentation_metadata());
  RETURN_IF_NOT_OK(mix_presentation_generator.Generate(mix_presentation_obus));

  // Collect and validate consistency of all `ParamDefinition`s in all
  // Audio Element and Mix Presentation OBUs.
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> param_definitions;
  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      audio_elements, mix_presentation_obus, param_definitions));

  // Global timing module.
  GlobalTimingModule global_timing_module(user_metadata);
  RETURN_IF_NOT_OK(global_timing_module.Initialize(
      audio_elements, codec_config_obus, param_definitions));

  // Parameter blocks.
  ParameterBlockGenerator parameter_block_generator(
      user_metadata.parameter_block_metadata(),
      user_metadata.test_vector_metadata().override_computed_recon_gains(),
      user_metadata.test_vector_metadata()
          .partition_mix_gain_parameter_blocks(),
      parameter_id_to_metadata);
  RETURN_IF_NOT_OK(parameter_block_generator.Initialize(
      ia_sequence_header_obu, audio_elements, mix_presentation_obus,
      param_definitions));

  // Generate demixing parameter blocks first. They are required to generate the
  // audio frames.
  RETURN_IF_NOT_OK(parameter_block_generator.GenerateDemixing(
      global_timing_module, parameter_blocks));
  RETURN_IF_NOT_OK(parameter_block_generator.GenerateMixGain(
      global_timing_module, parameter_blocks));

  // Put generated parameter blocks in a manager that supports easier queries.
  ParametersManager parameters_manager(audio_elements, parameter_blocks);
  RETURN_IF_NOT_OK(parameters_manager.Initialize());

  // Audio frames.
  WavSampleProvider wav_sample_provider(user_metadata.audio_frame_metadata());
  RETURN_IF_NOT_OK(
      wav_sample_provider.Initialize(input_wav_directory, audio_elements));
  DemixingModule demixing_module(user_metadata, audio_elements);
  AudioFrameGenerator audio_frame_generator(
      user_metadata.audio_frame_metadata(),
      user_metadata.codec_config_metadata(), audio_elements,
      output_wav_directory,
      user_metadata.test_vector_metadata().file_name_prefix(), demixing_module,
      parameters_manager, global_timing_module);

  RETURN_IF_NOT_OK(audio_frame_generator.Initialize());

  while (!audio_frame_generator.Finished()) {
    for (const auto& audio_frame_metadata :
         user_metadata.audio_frame_metadata()) {
      const auto audio_element_id = audio_frame_metadata.audio_element_id();
      LabelSamplesMap labeled_samples;
      RETURN_IF_NOT_OK(
          wav_sample_provider.ReadFrames(audio_element_id, labeled_samples));

      for (const auto& [channel_label, samples] : labeled_samples) {
        RETURN_IF_NOT_OK(audio_frame_generator.AddSamples(
            audio_element_id, channel_label, samples));
      }
    }

    RETURN_IF_NOT_OK(audio_frame_generator.GenerateFrames());
  }

  RETURN_IF_NOT_OK(audio_frame_generator.Finalize(audio_frames));

  AudioFrameDecoder audio_frame_decoder(
      output_wav_directory,
      user_metadata.test_vector_metadata().file_name_prefix());
  std::list<DecodedAudioFrame> decoded_audio_frames;
  RETURN_IF_NOT_OK(
      audio_frame_decoder.Decode(audio_frames, decoded_audio_frames));

  // Demix audio samples; useful for the following operations.
  IdTimeLabeledFrameMap id_to_time_to_labeled_frame;
  IdTimeLabeledFrameMap id_to_time_to_labeled_decoded_frame;
  RETURN_IF_NOT_OK(demixing_module.DemixAudioSamples(
      audio_frames, decoded_audio_frames, id_to_time_to_labeled_frame,
      id_to_time_to_labeled_decoded_frame));

  // Generate the remaining parameter blocks. Recon gain blocks depends on
  // decoded audio frames.
  RETURN_IF_NOT_OK(parameter_block_generator.GenerateReconGain(
      id_to_time_to_labeled_frame, id_to_time_to_labeled_decoded_frame,
      global_timing_module, parameter_blocks));

  ArbitraryObuGenerator arbitrary_obu_generator(
      user_metadata.arbitrary_obu_metadata());
  RETURN_IF_NOT_OK(arbitrary_obu_generator.Generate(arbitrary_obus));

  // Finalize mix presentation. Requires rendering data for every submix to
  // accurately compute loudness.
  std::optional<uint8_t> output_wav_file_bit_depth_override;
  if (user_metadata.test_vector_metadata()
          .has_output_wav_file_bit_depth_override()) {
    if (user_metadata.test_vector_metadata()
            .output_wav_file_bit_depth_override() >
        std::numeric_limits<uint8_t>::max()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Bit-depth too large. "
                       "output_wav_file_bit_depth_override= ",
                       user_metadata.test_vector_metadata()
                           .output_wav_file_bit_depth_override()));
    }
    output_wav_file_bit_depth_override =
        static_cast<uint8_t>(user_metadata.test_vector_metadata()
                                 .output_wav_file_bit_depth_override());
  }

  auto mix_presentation_finalizer = CreateMixPresentationFinalizer(
      user_metadata.mix_presentation_metadata(),
      user_metadata.test_vector_metadata().file_name_prefix(),
      output_wav_file_bit_depth_override);
  RETURN_IF_NOT_OK(mix_presentation_finalizer->Finalize(
      audio_elements, id_to_time_to_labeled_frame, parameter_blocks,
      mix_presentation_obus));

  return absl::OkStatus();
}

absl::Status WriteObus(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const std::string& output_iamf_directory,
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const std::list<ArbitraryObu>& arbitrary_obus) {
  bool include_temporal_delimiters;
  RETURN_IF_NOT_OK(GetIncludeTemporalDelimiterObus(
      user_metadata, ia_sequence_header_obu, include_temporal_delimiters));

  auto obu_sequencers = CreateObuSequencers(
      user_metadata, output_iamf_directory, include_temporal_delimiters);
  for (auto& obu_sequencer : obu_sequencers) {
    RETURN_IF_NOT_OK(obu_sequencer->PickAndPlace(
        ia_sequence_header_obu, codec_config_obus, audio_elements,
        mix_presentation_obus, audio_frames, parameter_blocks, arbitrary_obus));
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status TestMain(const iamf_tools_cli_proto::UserMetadata& user_metadata,
                      const std::string& input_wav_directory,
                      const std::string& output_wav_directory,
                      const std::string& output_iamf_directory) {
  std::optional<IASequenceHeaderObu> ia_sequence_header_obu;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  std::list<ArbitraryObu> arbitrary_obus;
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;

  // Create output directories.
  RETURN_IF_NOT_OK(CreateOutputDirectory(output_wav_directory));
  RETURN_IF_NOT_OK(CreateOutputDirectory(output_iamf_directory));

  RETURN_IF_NOT_OK(
      GenerateObus(user_metadata, input_wav_directory, output_wav_directory,
                   ia_sequence_header_obu, codec_config_obus, audio_elements,
                   mix_presentation_obus, audio_frames, parameter_blocks,
                   parameter_id_to_metadata, arbitrary_obus));

  RETURN_IF_NOT_OK(WriteObus(user_metadata, output_iamf_directory,
                             ia_sequence_header_obu.value(), codec_config_obus,
                             audio_elements, mix_presentation_obus,
                             audio_frames, parameter_blocks, arbitrary_obus));

  return absl::OkStatus();
}

}  // namespace iamf_tools
