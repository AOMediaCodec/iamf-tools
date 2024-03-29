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
#include "iamf/cli/parameter_block_partitioner.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/common/macros.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"

namespace iamf_tools {

namespace {

/*\!brief Partitions a `MixGainParameterData`
 *
 * Partitions a `MixGainParameterData` including the nested fields that describe
 * animation.
 *
 * \param subblock_mix_gain Input mix gain to partition.
 * \param subblock_start_time Start time of the subblock being partitioned.
 * \param subblock_end_time End time of the subblock being partitioned.
 * \param partition_start_time Start time of the output partitioned parameter
 *     block.
 * \param partitioned_end_time End time of the output partitioned parameter
 *     block.
 * \param partitioned_subblock Output argument with `MixGainParameterData`
 *     filled in.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status PartitionMixGain(const MixGainParameterData& subblock_mix_gain,
                              int32_t subblock_start_time,
                              int32_t subblock_end_time,
                              int32_t partitioned_start_time,
                              int32_t partitioned_end_time,
                              ParameterSubblock& partitioned_subblock) {
  // Copy over the animation type.
  auto& mix_gain_param_data =
      std::get<MixGainParameterData>(partitioned_subblock.param_data);
  mix_gain_param_data.animation_type = subblock_mix_gain.animation_type;

  // Partition the animated parameter.
  switch (subblock_mix_gain.animation_type) {
    using enum MixGainParameterData::AnimationType;
    case kAnimateStep: {
      AnimationStepInt16 obu_step;
      RETURN_IF_NOT_OK(ParameterBlockObu::InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_start_time, obu_step.start_point_value));
      mix_gain_param_data.param_data = obu_step;
      return absl::OkStatus();
    }
    case kAnimateLinear: {
      AnimationLinearInt16 obu_linear;

      // Set partitioned start time to the value of the parameter at that time.
      LOG_FIRST_N(INFO, 3) << subblock_start_time << " " << subblock_end_time
                           << " " << partitioned_start_time << " "
                           << partitioned_end_time;
      // Calculate the subblock's parameter value at the start of the partition.
      auto status = ParameterBlockObu::InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_start_time, obu_linear.start_point_value);

      // Calculate the subblock's parameter value at the end of the partition.
      status.Update(ParameterBlockObu::InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_end_time, obu_linear.end_point_value));

      if (!status.ok()) {
        return absl::Status(
            status.code(),
            absl::StrCat("Failed to interpolate mix gain values. "
                         "`InterpolateMixGainParameterData` returned: ",
                         status.message()));
      }

      mix_gain_param_data.param_data = obu_linear;
      return absl::OkStatus();
    }
    case kAnimateBezier:
      if (subblock_start_time == partitioned_start_time &&
          subblock_end_time == partitioned_end_time) {
        // Handle the simplest case where the subblock is aligned and does not
        // need partitioning.
        mix_gain_param_data.param_data = subblock_mix_gain.param_data;
        return absl::OkStatus();
      }
      // TODO(b/279581032): Carefully split the bezier curve. Be careful with
      //                    Q7.8 format.
      LOG(ERROR) << "The encoder does not fully support partitioning bezier "
                    "parameters yet.";
      return absl::InvalidArgumentError("");
    default:
      LOG(ERROR) << "Unrecognized animation type: "
                 << subblock_mix_gain.animation_type;
      return absl::InvalidArgumentError("");
  }
}

/*\!brief Gets the subblocks that overlap with the input times.
 *
 * Finds all subblocks in the input Parameter Block that overlap with
 * `partitioned_start_time` and `partitioned_end_time.
 *
 * \param full_parameter_block Input full parameter block.
 * \param param_definition_type Input parameter definition type.
 * \param partitioned_start_time Start time of the output partitioned parameter
 *     block.
 * \param partitioned_end_time End time of the output partitioned parameter
 *     block.
 * \param partitioned_subblocks Output partitioned subblocks.
 * \param constant_subblock_duration Output argument which corresponds to the
 *     value of `constant_subblock_duration` in the OBU or metadata.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status GetPartitionedSubblocks(
    const ParameterBlockWithData& full_parameter_block,
    const ParamDefinition::ParameterDefinitionType param_definition_type,
    int32_t partitioned_start_time, int32_t partitioned_end_time,
    std::list<ParameterSubblock>& partitioned_subblocks,
    DecodedUleb128& constant_subblock_duration) {
  int32_t cur_time = full_parameter_block.start_timestamp;

  // Track that the split subblocks cover the whole partition.
  int32_t total_covered_duration = 0;
  // Loop through all subblocks in the original Parameter Block.
  const auto num_subblocks = full_parameter_block.obu->GetNumSubblocks();
  for (int i = 0; i < num_subblocks; ++i) {
    // Get the start and end time of this subblock.
    const int32_t subblock_start_time = cur_time;
    const auto subblock_duration =
        full_parameter_block.obu->GetSubblockDuration(i);
    if (!subblock_duration.ok()) {
      return subblock_duration.status();
    }

    const int32_t subblock_end_time =
        subblock_start_time + static_cast<int32_t>(subblock_duration.value());
    cur_time = subblock_end_time;

    if (subblock_end_time <= partitioned_start_time ||
        partitioned_end_time <= subblock_start_time) {
      // The subblock ends before the partition or starts after. It can't
      // overlap.
      continue;
    }

    // Found an overlapping subblock. Create a new one for the partition that
    // represents the overlapped time.
    ParameterSubblock partitioned_subblock;
    const int partitioned_subblock_start =
        std::max(partitioned_start_time, subblock_start_time);
    const int partitioned_subblock_end =
        std::min(partitioned_end_time, subblock_end_time);
    DecodedUleb128 partitioned_subblock_duration =
        partitioned_subblock_end - partitioned_subblock_start;
    total_covered_duration += partitioned_subblock_duration;
    partitioned_subblock.subblock_duration = partitioned_subblock_duration;

    const auto& subblock_param_data =
        full_parameter_block.obu->subblocks_[i].param_data;
    switch (param_definition_type) {
      using enum ParamDefinition ::ParameterDefinitionType;
      case kParameterDefinitionMixGain:
        // Mix Gain animated parameters need to be partitioned.
        RETURN_IF_NOT_OK(PartitionMixGain(
            std::get<MixGainParameterData>(subblock_param_data),
            subblock_start_time, subblock_end_time, partitioned_subblock_start,
            partitioned_subblock_end, partitioned_subblock));
        break;
      case kParameterDefinitionDemixing:
        if (i > 1) {
          LOG(ERROR) << "There should only be one subblock for demixing info.";
          return absl::InvalidArgumentError("");
        }
        std::get<DemixingInfoParameterData>(partitioned_subblock.param_data) =
            std::get<DemixingInfoParameterData>(subblock_param_data);
        break;
      case kParameterDefinitionReconGain:
        if (i > 1) {
          LOG(ERROR)
              << "There should only be one subblock for recon gain info.";
          return absl::InvalidArgumentError("");
        }
        std::get<ReconGainInfoParameterData>(partitioned_subblock.param_data) =
            std::get<ReconGainInfoParameterData>(subblock_param_data);
        break;
      default:
        return absl::InvalidArgumentError("");
    }
    partitioned_subblocks.push_back(partitioned_subblock);

    if (subblock_end_time >= partitioned_end_time) {
      // Subblock overlap is over. No more to find for this partition.
      break;
    }
  }

  const auto status = CompareTimestamps(
      partitioned_end_time - partitioned_start_time, total_covered_duration);
  if (!status.ok()) {
    return absl::Status(
        status.code(),
        absl::StrCat(
            "Unable to find enough subblocks to totally cover the duration "
            "of the partitioned Parameter Block OBU. Possible gap in the "
            "sequence. `CompareTimestamps` returned: ",
            status.message()));
  }

  // Get the value of `constant_subblock_duration`.
  std::vector<DecodedUleb128> subblock_durations;
  subblock_durations.reserve(partitioned_subblocks.size());
  for (const auto& subblock : partitioned_subblocks) {
    subblock_durations.push_back(subblock.subblock_duration);
  }

  constant_subblock_duration =
      ParameterBlockPartitioner::FindConstantSubblockDuration(
          subblock_durations);

  return absl::OkStatus();
}

}  // namespace

DecodedUleb128 ParameterBlockPartitioner::FindConstantSubblockDuration(
    const std::vector<DecodedUleb128>& subblock_durations) {
  if (subblock_durations.empty()) {
    return 0;
  }
  const auto constant_subblock_duration = subblock_durations[0];

  for (int i = 0; i < subblock_durations.size(); ++i) {
    if (i == subblock_durations.size() - 1 &&
        subblock_durations[i] <= constant_subblock_duration) {
      // The duration of the final subblock can be implicitly calculated. As
      // long as it has equal or smaller duration than
      // `constant_subblock_duration`.
      continue;
    }

    // Other than the special case all subblocks must have the same duration.
    if (subblock_durations[i] != constant_subblock_duration) {
      return 0;
    }
  }

  return constant_subblock_duration;
}

absl::Status ParameterBlockPartitioner::PartitionParameterBlock(
    const ParameterBlockWithData& full_parameter_block,
    int partitioned_start_time, int partitioned_end_time,
    PerIdParameterMetadata& metadata,
    ParameterBlockWithData& partitioned) const {
  if (partitioned_start_time >= partitioned_end_time) {
    LOG(ERROR) << "Cannot partition a parameter block starting at "
               << partitioned_start_time << " and ending at "
               << partitioned_end_time;
    return absl::InvalidArgumentError("");
  }

  // Make a copy before any modification.
  const ParamDefinition param_definition_mode_0 = metadata.param_definition;

  // Find the subblocks that overlap this partition.
  std::list<ParameterSubblock> partitioned_subblocks;
  DecodedUleb128 constant_subblock_duration;
  RETURN_IF_NOT_OK(GetPartitionedSubblocks(
      full_parameter_block, metadata.param_definition_type,
      partitioned_start_time, partitioned_end_time, partitioned_subblocks,
      constant_subblock_duration));

  // Create the OBU. The first field always the same as the original OBU.
  auto partitioned_obu = std::make_unique<ParameterBlockObu>(
      ObuHeader(), full_parameter_block.obu->parameter_id_, &metadata);

  // Allocate and populate the subblocks that overlap the partition.
  RETURN_IF_NOT_OK(partitioned_obu->InitializeSubblocks(
      static_cast<DecodedUleb128>(partitioned_end_time -
                                  partitioned_start_time),
      constant_subblock_duration,
      static_cast<DecodedUleb128>(partitioned_subblocks.size())));
  int i = 0;
  for (const auto& subblock : partitioned_subblocks) {
    auto& obu_subblock = partitioned_obu->subblocks_[i];

    RETURN_IF_NOT_OK(
        partitioned_obu->SetSubblockDuration(i, subblock.subblock_duration));
    obu_subblock.param_data = subblock.param_data;
    i++;
  }

  // Validate that this is the same as target `ParamDefinition`s when the mode
  // is 0.
  if (metadata.param_definition.param_definition_mode_ == 0 &&
      param_definition_mode_0 != metadata.param_definition) {
    LOG(ERROR) << "Inequivalent `param_definition_mode` for id= "
               << param_definition_mode_0.parameter_id_;
    return absl::InvalidArgumentError("");
  }

  // Populate the `ParameterBlockWithData`.
  partitioned.obu = std::move(partitioned_obu);
  partitioned.start_timestamp = partitioned_start_time;
  partitioned.end_timestamp = partitioned_end_time;

  return absl::OkStatus();
}

absl::Status ParameterBlockPartitioner::PartitionFrameAligned(
    DecodedUleb128 partition_duration,
    ParameterBlockWithData& parameter_block_with_data,
    PerIdParameterMetadata& metadata,
    std::list<ParameterBlockWithData>& parameter_blocks) const {
  // Partition this parameter block into several blocks with the same duration.
  for (int t = parameter_block_with_data.start_timestamp;
       t < parameter_block_with_data.end_timestamp; t += partition_duration) {
    LOG(INFO) << "Partitioning parameter blocks at timestamp= " << t;
    ParameterBlockWithData partitioned_parameter_block;
    RETURN_IF_NOT_OK(PartitionParameterBlock(parameter_block_with_data, t,
                                             t + partition_duration, metadata,
                                             partitioned_parameter_block));
    parameter_blocks.push_back(std::move(partitioned_parameter_block));
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
