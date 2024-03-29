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

#ifndef CLI_PARAMETER_BLOCK_GENERATOR_H_
#define CLI_PARAMETER_BLOCK_GENERATOR_H_

#include <list>
#include <memory>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_partitioner.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/recon_gain_generator.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

// TODO(b/296815263): Add tests for this class.
// TODO(b/306319126): Generate one parameter block at a time.
class ParameterBlockGenerator {
 public:
  /*\!brief Constructor.
   *
   * \param parameter_block_metadata Input parameter block metadata.
   * \param parameter_id_to_metadata Mapping from parameter IDs to per-ID
   *     parameter metadata.
   */
  ParameterBlockGenerator(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::ParameterBlockObuMetadata>&
          parameter_block_metadata,
      bool override_computed_recon_gains,
      bool partition_mix_gain_parameter_blocks,
      absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>&
          parameter_id_to_metadata)
      : parameter_block_metadata_(parameter_block_metadata),
        override_computed_recon_gains_(override_computed_recon_gains),
        partition_mix_gain_parameter_blocks_(
            partition_mix_gain_parameter_blocks),
        parameter_id_to_metadata_(parameter_id_to_metadata) {}

  /*\!brief Initializes the class.
   *
   * Must be called before any `Generate*()` function, otherwise they will
   * be no-ops (not failing).
   *
   * \param ia_sequence_header_obu IA Sequence Header OBU.
   * \param audio_elements Input Audio Element OBUs with data.
   * \param mix_presentation_obus Input Mix Presentation OBUs with all
   *     `ParameterDefinitions` filled in.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize(
      const std::optional<IASequenceHeaderObu>& ia_sequence_header_obu,
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements,
      const std::list<MixPresentationObu>& mix_presentation_obus,
      const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
          param_definitions);

  /*\!brief Generates a list of demixing parameter blocks with data.
   *
   * \param output_parameter_blocks Output list of parameter blocks with data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GenerateDemixing(
      GlobalTimingModule& global_timing_module,
      std::list<ParameterBlockWithData>& output_parameter_blocks);

  /*\!brief Generates a list of mix gain parameter blocks with data.
   *
   * \param output_parameter_blocks Output list of parameter blocks with data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GenerateMixGain(
      GlobalTimingModule& global_timing_module,
      std::list<ParameterBlockWithData>& output_parameter_blocks);

  /*\!brief Generates a list of recon gain parameter blocks with data.
   *
   * \param id_to_time_to_labeled_frame Data structure for samples.
   * \param id_to_time_to_labeled_decoded_frame Data structure for decoded
   *     samples.
   * \param output_parameter_blocks Output list of parameter blocks with data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GenerateReconGain(
      const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
      const IdTimeLabeledFrameMap& id_to_time_to_labeled_decoded_frame,
      GlobalTimingModule& global_timing_module,
      std::list<ParameterBlockWithData>& output_parameter_blocks);

 private:
  /*\!brief Generates a list of parameter blocks with data.
   *
   * \param proto_metadata_list Input list of user-defined metadata about
   *     parameter blocks.
   * \param global_timing_module Global Timing Module.
   * \param output_parameter_blocks Output list of parameter blocks with data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GenerateParameterBlocks(
      const std::list<iamf_tools_cli_proto::ParameterBlockObuMetadata>&
          proto_metadata_list,
      GlobalTimingModule& global_timing_module,
      std::list<ParameterBlockWithData>& output_parameter_blocks);

  absl::Status ValidateParameterCoverage(
      const std::list<ParameterBlockWithData>& parameter_blocks,
      const GlobalTimingModule& global_timing_module);

  const ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::ParameterBlockObuMetadata>
      parameter_block_metadata_;

  const bool override_computed_recon_gains_;
  const bool partition_mix_gain_parameter_blocks_;

  // Mapping from parameter IDs to sets of audio element with data.
  absl::flat_hash_map<DecodedUleb128,
                      absl::flat_hash_set<const AudioElementWithData*>>
      associated_audio_elements_;

  // Mapping from parameter IDs to parameter metadata.
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>&
      parameter_id_to_metadata_;

  ProfileVersion primary_profile_;

  std::unique_ptr<ParameterBlockPartitioner> partitioner_;

  std::unique_ptr<ReconGainGenerator> recon_gain_generator_;

  // User metadata about Parameter Block OBUs categorized based on
  // the parameter definition type.
  absl::flat_hash_map<
      ParamDefinition::ParameterDefinitionType,
      std::list<iamf_tools_cli_proto::ParameterBlockObuMetadata>>
      typed_proto_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_PARAMETER_BLOCK_GENERATOR_H_
