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
#include <string>
#include <system_error>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/iamf_components.h"
#include "iamf/cli/iamf_encoder.h"
#include "iamf/cli/parameter_block_partitioner.h"
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
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

namespace {

using iamf_tools_cli_proto::ParameterBlockObuMetadata;
using iamf_tools_cli_proto::UserMetadata;

using ::absl::MakeConstSpan;

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

iamf_tools_cli_proto::OutputAudioFormat GetOutputAudioFormat(
    const iamf_tools_cli_proto::OutputAudioFormat output_audio_format,
    const iamf_tools_cli_proto::TestVectorMetadata& test_vector_metadata) {
  if (test_vector_metadata.has_output_wav_file_bit_depth_override()) {
    LOG(WARNING)
        << "`output_wav_file_bit_depth_override` takes no effect. Please "
           "upgrade to `encoder_control_metadata.output_rendered_file_format` "
           "instead."
           "\nSuggested upgrades:\n"
           "- `output_wav_file_bit_depth_override: 0 -> "
           "OUTPUT_FORMAT_WAV_BIT_DEPTH_AUTOMATIC`\n"
           "- `output_wav_file_bit_depth_override: 16 -> "
           "OUTPUT_FORMAT_WAV_BIT_DEPTH_SIXTEEN`\n"
           "- `output_wav_file_bit_depth_override: 24 -> "
           "OUTPUT_FORMAT_WAV_BIT_DEPTH_TWENTY_FOUR`\n"
           "- `output_wav_file_bit_depth_override: 32 -> "
           "OUTPUT_FORMAT_WAV_BIT_DEPTH_THIRTY_TWO`\n";
  }
  return output_audio_format;
}

absl::Status GenerateTemporalUnitObus(const UserMetadata& user_metadata,
                                      const std::string& input_wav_directory,
                                      IamfEncoder& iamf_encoder) {
  auto wav_sample_provider = WavSampleProvider::Create(
      user_metadata.audio_frame_metadata(), input_wav_directory,
      iamf_encoder.GetAudioElements());
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
        iamf_encoder.GetAudioElements(), *wav_sample_provider,
        id_to_labeled_samples, no_more_real_samples));

    for (const auto& [audio_element_id, labeled_samples] :
         id_to_labeled_samples) {
      for (const auto& [channel_label, samples] : labeled_samples) {
        iamf_encoder.AddSamples(audio_element_id, channel_label,
                                MakeConstSpan(samples));
      }
    }

    // In this program we always use up all samples from a WAV file, so we
    // call `IamfEncoder::FinalizeAddSamples()` only when there is no more
    // real samples. In other applications, the user may decide to stop adding
    // audio samples based on other criteria.
    if (no_more_real_samples) {
      RETURN_IF_NOT_OK(iamf_encoder.FinalizeAddSamples());
    }

    // Add parameter block metadata.
    for (const auto& metadata :
         time_parameter_block_metadata[input_timestamp]) {
      RETURN_IF_NOT_OK(iamf_encoder.AddParameterBlockMetadata(metadata));
    }

    // In a streaming based application these serialized OBUs would be useful.
    // Here we throw them away and rely on the `ObuSequencer`s to handle output
    // (such as to a .iamf file).
    std::vector<uint8_t> unused_temporal_unit_obus;
    RETURN_IF_NOT_OK(
        iamf_encoder.OutputTemporalUnit(unused_temporal_unit_obus));
  }
  LOG(INFO) << "\n============================= END of Generating Data OBUs"
            << " =============================\n\n";

  return absl::OkStatus();
}

}  // namespace

absl::Status TestMain(const UserMetadata& input_user_metadata,
                      const std::string& input_wav_directory,
                      const std::string& output_iamf_directory) {
  // Make a copy before modifying.
  UserMetadata user_metadata(input_user_metadata);

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
  ApplyOutputAudioFormatToSampleProcessorFactory(output_audio_format,
                                                 sample_processor_factory);

  // Adapt the "IAMF Components" sequencer to match the `IamfEncoder`. This
  // helps automatically create the output file(s).
  IamfEncoder::ObuSequencerFactory obu_sequencer_factory = [&]() {
    return CreateObuSequencers(user_metadata, output_iamf_directory,
                               user_metadata.temporal_delimiter_metadata()
                                   .enable_temporal_delimiters());
  };

  auto iamf_encoder =
      IamfEncoder::Create(user_metadata, CreateRendererFactory().get(),
                          CreateLoudnessCalculatorFactory().get(),
                          sample_processor_factory, obu_sequencer_factory);
  if (!iamf_encoder.ok()) {
    return iamf_encoder.status();
  }

  // In a streaming based application we would query `GetDescriptorObus()`
  // and broadcast them. Here we rely on the `ObuSequencer`s to manage the
  // output.

  // Push all audio data to the encoder, at the end, the file is closed and
  // finalized.
  RETURN_IF_NOT_OK(GenerateTemporalUnitObus(user_metadata, input_wav_directory,
                                            *iamf_encoder));

  // In a streaming based application we would query `GetDescriptorObus` for the
  // finalized descriptor OBUs. Then push the data to any clients that rely on
  // accurate loudness, if not too late. Here we rely on the `ObuSequencers`  to
  // manage the output.

  return absl::OkStatus();
}

}  // namespace iamf_tools
