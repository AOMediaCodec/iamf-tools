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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/iamf_components.h"
#include "iamf/cli/iamf_encoder.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/parameter_block_partitioner.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/encoder_control_metadata.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/output_audio_format_utils.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/cli/wav_sample_provider.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

namespace {

using iamf_tools_cli_proto::ParameterBlockObuMetadata;
using iamf_tools_cli_proto::UserMetadata;

absl::Status PartitionParameterMetadata(UserMetadata& user_metadata) {
  uint32_t partition_duration = 0;
  if (user_metadata.ia_sequence_header_metadata().empty() ||
      user_metadata.codec_config_metadata().empty()) {
    return absl::InvalidArgumentError(
        "Determining the partition duration requires at least one "
        "`ia_sequence_header_metadata` and one `codec_config_metadata`");
  }
  std::list<ParameterBlockObuMetadata> partitioned_parameter_blocks;
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

// Mapping from the start timestamps to lists of parameter block metadata.
typedef absl::flat_hash_map<InternalTimestamp,
                            std::list<ParameterBlockObuMetadata>>
    TimeParameterBlockMetadataMap;
absl::Status OrganizeParameterBlockMetadata(
    const google::protobuf::RepeatedPtrField<ParameterBlockObuMetadata>&
        parameter_block_metadata,
    TimeParameterBlockMetadataMap& time_parameter_block_metadata) {
  for (const auto& metadata : parameter_block_metadata) {
    time_parameter_block_metadata[metadata.start_timestamp()].push_back(
        metadata);
  }

  return absl::OkStatus();
}

absl::Status CollectLabeledSamplesForAudioElements(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    WavSampleProvider& wav_sample_provider,
    absl::flat_hash_map<DecodedUleb128, LabelSamplesMap>& id_to_labeled_samples,
    bool& no_more_real_samples) {
  for (const auto& [audio_element_id, unused_audio_element] : audio_elements) {
    RETURN_IF_NOT_OK(wav_sample_provider.ReadFrames(
        audio_element_id, id_to_labeled_samples[audio_element_id],
        no_more_real_samples));
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

absl::StatusOr<iamf_tools_cli_proto::OutputAudioFormat> GetOutputAudioFormat(
    const iamf_tools_cli_proto::OutputAudioFormat output_audio_format,
    const iamf_tools_cli_proto::TestVectorMetadata& test_vector_metadata) {
  if (!test_vector_metadata.has_output_wav_file_bit_depth_override()) {
    return output_audio_format;
  }

  // OK. To maintain old test vectors, convert the deprecated field to the
  // new field.
  // TODO(b/390392510): Remove the conversion once test vectors are updated.
  LOG(WARNING) << "output_wav_file_bit_depth_override is deprecated. Use "
                  "encoder_control_metadata.output_rendered_file_format "
                  "instead.";
  return GetOutputAudioFormatFromBitDepth(
      test_vector_metadata.output_wav_file_bit_depth_override());
}

absl::Status GenerateTemporalUnitObus(
    const UserMetadata& user_metadata, const std::string& input_wav_directory,
    IamfEncoder& iamf_encoder,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    std::list<AudioFrameWithData>& audio_frames,
    std::list<ParameterBlockWithData>& parameter_blocks) {
  auto wav_sample_provider =
      WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                input_wav_directory, audio_elements);
  if (!wav_sample_provider.ok()) {
    return wav_sample_provider.status();
  }

  // Parameter blocks.
  TimeParameterBlockMetadataMap time_parameter_block_metadata;
  RETURN_IF_NOT_OK(OrganizeParameterBlockMetadata(
      user_metadata.parameter_block_metadata(), time_parameter_block_metadata));

  // TODO(b/329375123): Make two while loops that run on two threads: one for
  //                    adding samples and parameter block metadata, and one for
  //                    outputing OBUs.
  int data_obus_iteration = 0;  // Just for logging purposes.
  while (iamf_encoder.GeneratingDataObus()) {
    LOG_EVERY_N_SEC(INFO, 5)
        << "\n\n============================= Generating Data OBUs Iter #"
        << data_obus_iteration++ << " =============================\n";

    iamf_encoder.BeginTemporalUnit();

    InternalTimestamp input_timestamp = 0;
    RETURN_IF_NOT_OK(iamf_encoder.GetInputTimestamp(input_timestamp));

    // Add audio samples.
    absl::flat_hash_map<DecodedUleb128, LabelSamplesMap> id_to_labeled_samples;
    bool no_more_real_samples = false;
    RETURN_IF_NOT_OK(CollectLabeledSamplesForAudioElements(
        audio_elements, *wav_sample_provider, id_to_labeled_samples,
        no_more_real_samples));

    for (const auto& [audio_element_id, labeled_samples] :
         id_to_labeled_samples) {
      for (const auto& [channel_label, samples] : labeled_samples) {
        iamf_encoder.AddSamples(audio_element_id, channel_label, samples);
      }
    }

    // In this program we always use up all samples from a WAV file, so we
    // call `IamfEncoder::FinalizeAddSamples()` only when there is no more
    // real samples. In other applications, the user may decide to stop adding
    // audio samples based on other criteria.
    if (no_more_real_samples) {
      iamf_encoder.FinalizeAddSamples();
    }

    // Add parameter block metadata.
    for (const auto& metadata :
         time_parameter_block_metadata[input_timestamp]) {
      RETURN_IF_NOT_OK(iamf_encoder.AddParameterBlockMetadata(metadata));
    }

    std::list<AudioFrameWithData> temp_audio_frames;
    std::list<ParameterBlockWithData> temp_parameter_blocks;
    IdLabeledFrameMap id_to_labeled_frame;
    RETURN_IF_NOT_OK(iamf_encoder.OutputTemporalUnit(temp_audio_frames,
                                                     temp_parameter_blocks));

    if (temp_audio_frames.empty()) {
      // Some audio codec will only output an encoded frame after the next
      // frame "pushes" the old one out. So we wait till the next iteration to
      // retrieve it.
      LOG(INFO) << "No audio frame generated in this iteration; continue.";
      continue;
    }

    audio_frames.splice(audio_frames.end(), temp_audio_frames);
    parameter_blocks.splice(parameter_blocks.end(), temp_parameter_blocks);
  }
  LOG(INFO) << "\n============================= END of Generating Data OBUs"
            << " =============================\n\n";
  PrintAudioFrames(audio_frames);

  return absl::OkStatus();
}

absl::Status WriteObus(
    const UserMetadata& user_metadata, const std::string& output_iamf_directory,
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const std::list<ArbitraryObu>& arbitrary_obus) {
  const bool include_temporal_delimiters =
      user_metadata.temporal_delimiter_metadata().enable_temporal_delimiters();

  // TODO(b/349271859): Move the OBU sequencer inside `IamfEncoder`.
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

absl::Status TestMain(const UserMetadata& input_user_metadata,
                      const std::string& input_wav_directory,
                      const std::string& output_iamf_directory) {
  // Make a copy before modifying.
  UserMetadata user_metadata(input_user_metadata);

  std::optional<IASequenceHeaderObu> ia_sequence_header_obu;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> preliminary_mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  std::list<ArbitraryObu> arbitrary_obus;

  // Create output directories.
  RETURN_IF_NOT_OK(CreateOutputDirectory(output_iamf_directory));

  // Partition parameter block metadata if necessary. This will overwrite
  // `user_metadata.mutable_parameter_block_metadata()`.
  if (user_metadata.test_vector_metadata()
          .partition_mix_gain_parameter_blocks()) {
    RETURN_IF_NOT_OK(PartitionParameterMetadata(user_metadata));
  }

  // Write the output audio streams which were used to measure loudness to the
  // same directory as the IAMF file.
  const std::string output_wav_file_prefix =
      (std::filesystem::path(output_iamf_directory) /
       user_metadata.test_vector_metadata().file_name_prefix())
          .string();
  LOG(INFO) << "output_wav_file_prefix = " << output_wav_file_prefix;
  RenderingMixPresentationFinalizer::SampleProcessorFactory
      sample_processor_factory =
          [output_wav_file_prefix](DecodedUleb128 mix_presentation_id,
                                   int sub_mix_index, int layout_index,
                                   const Layout&, int num_channels,
                                   int sample_rate, int bit_depth,
                                   size_t max_input_samples_per_frame)
      -> std::unique_ptr<SampleProcessorBase> {
    // Generate a unique filename for each layout of each mix presentation.
    const auto wav_path = absl::StrCat(
        output_wav_file_prefix, "_rendered_id_", mix_presentation_id,
        "_sub_mix_", sub_mix_index, "_layout_", layout_index, ".wav");
    return WavWriter::Create(wav_path, num_channels, sample_rate, bit_depth,
                             max_input_samples_per_frame);
  };

  // Apply the bit depth override.
  const auto output_audio_format = GetOutputAudioFormat(
      user_metadata.encoder_control_metadata().output_rendered_file_format(),
      user_metadata.test_vector_metadata());
  if (!output_audio_format.ok()) {
    return output_audio_format.status();
  }
  ApplyOutputAudioFormatToSampleProcessorFactory(*output_audio_format,
                                                 sample_processor_factory);

  // We want to hold the `IamfEncoder` until all OBUs have been written.
  auto iamf_encoder = IamfEncoder::Create(
      user_metadata, CreateRendererFactory().get(),
      CreateLoudnessCalculatorFactory().get(), sample_processor_factory,
      ia_sequence_header_obu, codec_config_obus, audio_elements,
      preliminary_mix_presentation_obus, arbitrary_obus);
  if (!iamf_encoder.ok()) {
    return iamf_encoder.status();
  }
  // Discard the "preliminary" mix presentation OBUs. We only care about the
  // finalized ones, which are not possible to know until audio encoding is
  // complete.
  preliminary_mix_presentation_obus.clear();
  RETURN_IF_NOT_OK(GenerateTemporalUnitObus(user_metadata, input_wav_directory,
                                            *iamf_encoder, audio_elements,
                                            audio_frames, parameter_blocks));
  // Audio encoding is complete. Retrieve the OBUs with have the finalized
  // loudness information.
  const auto finalized_mix_presentation_obus =
      iamf_encoder->GetFinalizedMixPresentationObus();
  if (!finalized_mix_presentation_obus.ok()) {
    return finalized_mix_presentation_obus.status();
  }

  RETURN_IF_NOT_OK(WriteObus(user_metadata, output_iamf_directory,
                             ia_sequence_header_obu.value(), codec_config_obus,
                             audio_elements, *finalized_mix_presentation_obus,
                             audio_frames, parameter_blocks, arbitrary_obus));

  return absl::OkStatus();
}

}  // namespace iamf_tools
