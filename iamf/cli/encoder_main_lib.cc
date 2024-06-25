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

#include <algorithm>
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
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/iamf_components.h"
#include "iamf/cli/obu_sequencer.h"
#include "iamf/cli/parameter_block_partitioner.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_to_obu/arbitrary_obu_generator.h"
#include "iamf/cli/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/proto_to_obu/ia_sequence_header_generator.h"
#include "iamf/cli/proto_to_obu/mix_presentation_generator.h"
#include "iamf/cli/proto_to_obu/parameter_block_generator.h"
#include "iamf/cli/wav_sample_provider.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/common/macros.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

namespace {

std::unique_ptr<WavWriter> ProduceAllWavWriters(
    DecodedUleb128 mix_presentation_id, int sub_mix_index, int layout_index,
    const Layout&, const std::filesystem::path& prefix, int num_channels,
    int sample_rate, int bit_depth) {
  const auto wav_path = absl::StrCat(
      prefix.string(), "_rendered_id_", mix_presentation_id, "_sub_mix_",
      sub_mix_index, "_layout_", layout_index, ".wav");
  return std::make_unique<WavWriter>(wav_path, num_channels, sample_rate,
                                     bit_depth);
}

absl::Status PartitionParameterMetadata(
    iamf_tools_cli_proto::UserMetadata& user_metadata) {
  uint32_t partition_duration = 0;
  if (user_metadata.ia_sequence_header_metadata().empty() ||
      user_metadata.codec_config_metadata().empty()) {
    return absl::InvalidArgumentError(
        "Determining the partition duration requires at least one "
        "`ia_sequence_header_metadata` and one `codec_config_metadata`");
  }
  std::list<iamf_tools_cli_proto::ParameterBlockObuMetadata>
      partitioned_parameter_blocks;
  RETURN_IF_NOT_OK(ParameterBlockPartitioner::FindPartitionDuration(
      user_metadata.ia_sequence_header_metadata(0).primary_profile(),
      user_metadata.codec_config_metadata(0), partition_duration));
  for (const auto& parameter_block_metadata :
       user_metadata.parameter_block_metadata()) {
    RETURN_IF_NOT_OK(ParameterBlockPartitioner::PartitionFrameAligned(
        partition_duration, parameter_block_metadata,
        partitioned_parameter_blocks));
  }

  // Replace the original parameter block metadata.
  user_metadata.clear_parameter_block_metadata();
  for (const auto& partitioned_parameter_block : partitioned_parameter_blocks) {
    *user_metadata.add_parameter_block_metadata() = partitioned_parameter_block;
  }

  return absl::OkStatus();
}

// TODO(b/306319126): Remove when parameter block metadata are taken one frame
//                    at a time.
absl::Status AddAllParameterBlockMetadataForCurrentTimestamp(
    const google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::ParameterBlockObuMetadata>&
        parameter_block_metadata,
    const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions,
    const ParamDefinition::ParameterDefinitionType type_to_add,
    ParameterBlockGenerator& parameter_block_generator,
    int32_t& current_timestamp) {
  int32_t next_timestamp = current_timestamp;
  for (const auto& metadata : parameter_block_metadata) {
    auto param_definition_iter = param_definitions.find(
        static_cast<DecodedUleb128>(metadata.parameter_id()));
    if (param_definition_iter == param_definitions.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "No param definition found for param ID= ", metadata.parameter_id()));
    }
    if (param_definition_iter->second->GetType() != type_to_add) {
      continue;
    }

    if (metadata.start_timestamp() == current_timestamp) {
      uint32_t duration;
      RETURN_IF_NOT_OK(
          parameter_block_generator.AddMetadata(metadata, duration));
      const int32_t end_timestamp = current_timestamp + duration;
      if (next_timestamp == current_timestamp) {
        next_timestamp = end_timestamp;
      } else if (next_timestamp != end_timestamp) {
        return absl::InvalidArgumentError(
            absl::StrCat("Inconsistent end timestamp: expecting ",
                         next_timestamp, " but got ", end_timestamp));
      }
    }
  }

  current_timestamp = next_timestamp;
  return absl::OkStatus();
}

absl::Status MaybeGenerateDemixingAndMixGainParameterBlocks(
    const google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::ParameterBlockObuMetadata>&
        parameter_block_metadata,
    const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions,
    ParametersManager& parameters_manager,
    ParameterBlockGenerator& parameter_block_generator,
    GlobalTimingModule& global_timing_module, int32_t& current_timestamp,
    std::list<ParameterBlockWithData>& demixing_parameter_blocks,
    std::list<ParameterBlockWithData>& mix_gain_parameter_blocks) {
  std::optional<int32_t> global_audio_frame_timestamp;
  RETURN_IF_NOT_OK(global_timing_module.GetGlobalAudioFrameTimestamp(
      global_audio_frame_timestamp));

  // Only generate parameter blocks when all audio frames corresponding to
  // the same temporal units are ready.
  if (global_audio_frame_timestamp == current_timestamp) {
    int32_t current_timestamp_for_demixing = current_timestamp;
    int32_t current_timestamp_for_mix_gain = current_timestamp;
    RETURN_IF_NOT_OK(AddAllParameterBlockMetadataForCurrentTimestamp(
        parameter_block_metadata, param_definitions,
        ParamDefinition::kParameterDefinitionDemixing,
        parameter_block_generator, current_timestamp_for_demixing));
    RETURN_IF_NOT_OK(AddAllParameterBlockMetadataForCurrentTimestamp(
        parameter_block_metadata, param_definitions,
        ParamDefinition::kParameterDefinitionMixGain, parameter_block_generator,
        current_timestamp_for_mix_gain));
    current_timestamp = std::max(current_timestamp_for_demixing,
                                 current_timestamp_for_mix_gain);

    std::list<ParameterBlockWithData> mix_gain_parameter_blocks_for_frame;
    std::list<ParameterBlockWithData> demixing_parameter_blocks_for_frame;
    RETURN_IF_NOT_OK(parameter_block_generator.GenerateDemixing(
        global_timing_module, demixing_parameter_blocks_for_frame));
    RETURN_IF_NOT_OK(parameter_block_generator.GenerateMixGain(
        global_timing_module, mix_gain_parameter_blocks_for_frame));

    // Add the newly generated demixing parameter blocks to the parameters
    // manager so they can be easily queried by the audio frame genrator.
    for (const auto& demixing_parameter_block :
         demixing_parameter_blocks_for_frame) {
      parameters_manager.AddDemixingParameterBlock(&demixing_parameter_block);
    }

    demixing_parameter_blocks.splice(demixing_parameter_blocks.end(),
                                     demixing_parameter_blocks_for_frame);
    mix_gain_parameter_blocks.splice(mix_gain_parameter_blocks.end(),
                                     mix_gain_parameter_blocks_for_frame);
  }

  return absl::OkStatus();
}

void PrintAudioFrames(const std::list<AudioFrameWithData>& audio_frames) {
  // Print the first, last, and any audio frames with `trimming_status_flag`
  // set.
  int i = 0;
  for (const auto& audio_frame_with_data : audio_frames) {
    if (i == 0 || i == audio_frames.size() - 1 ||
        audio_frame_with_data.obu.header_.obu_trimming_status_flag) {
      LOG(INFO) << "Audio Frame OBU[" << i << "]";

      audio_frame_with_data.obu.PrintObu();
      LOG(INFO) << "    audio frame.start_timestamp= "
                << audio_frame_with_data.start_timestamp;
      LOG(INFO) << "    audio frame.end_timestamp= "
                << audio_frame_with_data.end_timestamp;
    }
    i++;
  }
}

absl::Status CreateOutputDirectory(const std::string& output_directory) {
  if (output_directory.empty() ||
      std::filesystem::is_directory(output_directory) ||
      std::filesystem::is_character_file(output_directory)) {
    return absl::OkStatus();
  }

  std::error_code error_code;
  if (!std::filesystem::create_directories(output_directory, error_code)) {
    return absl::UnknownError(
        absl::StrCat("Failed to create output directory = ", output_directory));
  }

  return absl::OkStatus();
}

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
    return absl::InvalidArgumentError(
        "Only one IA Sequence Header allowed in an IA Sequence.");
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
  GlobalTimingModule global_timing_module;
  RETURN_IF_NOT_OK(
      global_timing_module.Initialize(audio_elements, param_definitions));

  ParameterBlockGenerator parameter_block_generator(
      user_metadata.test_vector_metadata().override_computed_recon_gains(),
      parameter_id_to_metadata);
  RETURN_IF_NOT_OK(
      parameter_block_generator.Initialize(audio_elements, param_definitions));

  // Put generated parameter blocks in a manager that supports easier queries.
  ParametersManager parameters_manager(audio_elements);
  RETURN_IF_NOT_OK(parameters_manager.Initialize());

  // Audio frames.
  WavSampleProvider wav_sample_provider(user_metadata.audio_frame_metadata());
  RETURN_IF_NOT_OK(
      wav_sample_provider.Initialize(input_wav_directory, audio_elements));

  // Down-mix the audio samples and then demix audio samples while decoding
  // them. This is useful to create multi-layer audio elements and to determine
  // the recon gain parameters and to measuring loudness.
  DemixingModule demixing_module;
  RETURN_IF_NOT_OK(demixing_module.InitializeForDownMixingAndReconstruction(
      user_metadata, audio_elements));

  AudioFrameGenerator audio_frame_generator(
      user_metadata.audio_frame_metadata(),
      user_metadata.codec_config_metadata(), audio_elements, demixing_module,
      parameters_manager, global_timing_module);
  RETURN_IF_NOT_OK(audio_frame_generator.Initialize());

  // Initialize the audio frame decoder. It is needed to determine the recon
  // gain parameters and measure the loudness of the mixes.
  AudioFrameDecoder audio_frame_decoder;
  RETURN_IF_NOT_OK(InitAudioFrameDecoderForAllAudioElements(
      audio_elements, audio_frame_decoder));

  // TODO(b/329375123): Make these two while loops run on two threads. The
  //                    one below should be on Thread 1.
  std::list<ParameterBlockWithData> demixing_parameter_blocks;
  std::list<ParameterBlockWithData> mix_gain_parameter_blocks;
  int32_t current_timestamp = 0;
  while (audio_frame_generator.TakingSamples()) {
    RETURN_IF_NOT_OK(MaybeGenerateDemixingAndMixGainParameterBlocks(
        user_metadata.parameter_block_metadata(), param_definitions,
        parameters_manager, parameter_block_generator, global_timing_module,
        current_timestamp, demixing_parameter_blocks,
        mix_gain_parameter_blocks));

    bool no_more_real_samples = true;
    for (const auto& audio_frame_metadata :
         user_metadata.audio_frame_metadata()) {
      const auto audio_element_id = audio_frame_metadata.audio_element_id();
      LabelSamplesMap labeled_samples;

      bool no_more_real_samples_for_audio_element = false;
      RETURN_IF_NOT_OK(wav_sample_provider.ReadFrames(
          audio_element_id, labeled_samples,
          no_more_real_samples_for_audio_element));
      no_more_real_samples &= no_more_real_samples_for_audio_element;
      for (const auto& [channel_label, samples] : labeled_samples) {
        RETURN_IF_NOT_OK(audio_frame_generator.AddSamples(
            audio_element_id, channel_label, samples));
      }
    }
    if (no_more_real_samples) {
      RETURN_IF_NOT_OK(audio_frame_generator.Finalize());
    }
  }

  // TODO(b/329375123): This should be on Thread 2.
  IdTimeLabeledFrameMap id_to_time_to_labeled_frame;
  IdTimeLabeledFrameMap id_to_time_to_labeled_decoded_frame;
  std::list<ParameterBlockWithData> recon_gain_parameter_blocks;
  while (audio_frame_generator.GeneratingFrames()) {
    std::list<AudioFrameWithData> temp_audio_frames;
    RETURN_IF_NOT_OK(audio_frame_generator.OutputFrames(temp_audio_frames));
    if (temp_audio_frames.empty()) {
      absl::SleepFor(absl::Milliseconds(50));
      continue;
    }

    // Decode the audio frames. They are required to determine the demixed
    // frames.
    std::list<DecodedAudioFrame> temp_decoded_audio_frames;
    RETURN_IF_NOT_OK(audio_frame_decoder.Decode(temp_audio_frames,
                                                temp_decoded_audio_frames));

    // Demix the audio frames.
    IdLabeledFrameMap id_to_labeled_frame;
    IdLabeledFrameMap id_to_labeled_decoded_frame;
    RETURN_IF_NOT_OK(demixing_module.DemixAudioSamples(
        temp_audio_frames, temp_decoded_audio_frames, id_to_labeled_frame,
        id_to_labeled_decoded_frame));

    // Add recon gain parameter blocks' metadata.
    const auto start_timestamp = temp_audio_frames.front().start_timestamp;
    int32_t unused_current_timestamp = start_timestamp;
    RETURN_IF_NOT_OK(AddAllParameterBlockMetadataForCurrentTimestamp(
        user_metadata.parameter_block_metadata(), param_definitions,
        ParamDefinition::kParameterDefinitionReconGain,
        parameter_block_generator, unused_current_timestamp));

    // Recon gain parameter blocks are generated based on the original and
    // demixed audio frames.
    std::list<ParameterBlockWithData> temp_recon_gain_parameter_blocks;
    RETURN_IF_NOT_OK(parameter_block_generator.GenerateReconGain(
        id_to_labeled_frame, id_to_labeled_decoded_frame, global_timing_module,
        temp_recon_gain_parameter_blocks));
    recon_gain_parameter_blocks.splice(recon_gain_parameter_blocks.end(),
                                       temp_recon_gain_parameter_blocks);

    // Collect and organize generated audio frames in time.
    for (const auto& [id, labeled_frame] : id_to_labeled_frame) {
      id_to_time_to_labeled_frame[id][start_timestamp] = labeled_frame;
    }
    for (const auto& [id, labeled_decoded_frame] :
         id_to_labeled_decoded_frame) {
      id_to_time_to_labeled_decoded_frame[id][start_timestamp] =
          labeled_decoded_frame;
    }
    audio_frames.splice(audio_frames.end(), temp_audio_frames);
  }
  PrintAudioFrames(audio_frames);

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
      user_metadata.test_vector_metadata().file_name_prefix(),
      output_wav_file_bit_depth_override,
      user_metadata.test_vector_metadata().validate_user_loudness());
  RETURN_IF_NOT_OK(mix_presentation_finalizer->Finalize(
      audio_elements, id_to_time_to_labeled_frame, mix_gain_parameter_blocks,
      ProduceAllWavWriters, mix_presentation_obus));

  parameter_blocks.splice(parameter_blocks.end(), demixing_parameter_blocks);
  parameter_blocks.splice(parameter_blocks.end(), mix_gain_parameter_blocks);
  parameter_blocks.splice(parameter_blocks.end(), recon_gain_parameter_blocks);

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

absl::Status TestMain(
    const iamf_tools_cli_proto::UserMetadata& input_user_metadata,
    const std::string& input_wav_directory,
    const std::string& output_wav_directory,
    const std::string& output_iamf_directory) {
  // Make a copy before modifying.
  iamf_tools_cli_proto::UserMetadata user_metadata(input_user_metadata);

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

  // Partition parameter block metadata if necessary. This will overwrite
  // `user_metadata.mutable_parameter_block_metadata()`.
  if (user_metadata.test_vector_metadata()
          .partition_mix_gain_parameter_blocks()) {
    RETURN_IF_NOT_OK(PartitionParameterMetadata(user_metadata));
  }

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
