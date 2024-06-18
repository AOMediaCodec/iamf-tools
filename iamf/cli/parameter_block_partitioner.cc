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
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"

namespace iamf_tools {

using iamf_tools_cli_proto::ParameterBlockObuMetadata;

namespace {

absl::Status InterpolateMixGainParameterData(
    const iamf_tools_cli_proto::MixGainParameterData& mix_gain_parameter_data,
    int32_t start_time, int32_t end_time, int32_t target_time,
    int16_t& target_mix_gain) {
  const auto& param_data = mix_gain_parameter_data.param_data();
  return InterpolateMixGainValue(
      mix_gain_parameter_data.animation_type(),
      iamf_tools_cli_proto::ANIMATE_STEP, iamf_tools_cli_proto::ANIMATE_LINEAR,
      iamf_tools_cli_proto::ANIMATE_BEZIER,
      [&param_data]() {
        return static_cast<int16_t>(param_data.step().start_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.linear().start_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.linear().end_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.bezier().start_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.bezier().end_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(param_data.bezier().control_point_value());
      },
      [&param_data]() {
        return static_cast<int16_t>(
            param_data.bezier().control_point_relative_time());
      },
      start_time, end_time, target_time, target_mix_gain);
}

/*!\brief Partitions a `MixGainParameterData`
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
absl::Status PartitionMixGain(
    const iamf_tools_cli_proto::MixGainParameterData& subblock_mix_gain,
    int32_t subblock_start_time, int32_t subblock_end_time,
    int32_t partitioned_start_time, int32_t partitioned_end_time,
    iamf_tools_cli_proto::ParameterSubblock& partitioned_subblock) {
  // Copy over the animation type.
  auto* mix_gain_param_data =
      partitioned_subblock.mutable_mix_gain_parameter_data();
  mix_gain_param_data->set_animation_type(subblock_mix_gain.animation_type());

  // Partition the animated parameter.
  switch (subblock_mix_gain.animation_type()) {
    using enum iamf_tools_cli_proto::AnimationType;
    case ANIMATE_STEP: {
      int16_t start_point_value;
      RETURN_IF_NOT_OK(InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_start_time, start_point_value));
      mix_gain_param_data->mutable_param_data()
          ->mutable_step()
          ->set_start_point_value(static_cast<int32_t>(start_point_value));
      return absl::OkStatus();
    }
    case ANIMATE_LINEAR: {
      // Set partitioned start time to the value of the parameter at that time.
      LOG_FIRST_N(INFO, 3) << subblock_start_time << " " << subblock_end_time
                           << " " << partitioned_start_time << " "
                           << partitioned_end_time;
      // Calculate the subblock's parameter value at the start of the partition.
      int16_t start_point_value;
      int16_t end_point_value;
      auto status = InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_start_time, start_point_value);

      // Calculate the subblock's parameter value at the end of the partition.
      status.Update(InterpolateMixGainParameterData(
          subblock_mix_gain, subblock_start_time, subblock_end_time,
          partitioned_end_time, end_point_value));

      if (!status.ok()) {
        return absl::Status(
            status.code(),
            absl::StrCat("Failed to interpolate mix gain values. "
                         "`InterpolateMixGainParameterData` returned: ",
                         status.message()));
      }
      auto* linear =
          mix_gain_param_data->mutable_param_data()->mutable_linear();
      linear->set_start_point_value(static_cast<int32_t>(start_point_value));
      linear->set_end_point_value(static_cast<int32_t>(end_point_value));
      return absl::OkStatus();
    }
    case ANIMATE_BEZIER: {
      if (subblock_start_time == partitioned_start_time &&
          subblock_end_time == partitioned_end_time) {
        // Handle the simplest case where the subblock is aligned and does not
        // need partitioning.
        *mix_gain_param_data->mutable_param_data() =
            subblock_mix_gain.param_data();
        return absl::OkStatus();
      }
      // TODO(b/279581032): Carefully split the bezier curve. Be careful with
      //                    Q7.8 format.
      return absl::InvalidArgumentError(absl::StrCat(
          "The encoder does not fully support partitioning bezier ",
          "parameters yet."));
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unrecognized animation type = ",
                       subblock_mix_gain.animation_type()));
  }
}

/*!\brief Gets the subblocks that overlap with the input times.
 *
 * Finds all subblocks in the input Parameter Block that overlap with
 * `partitioned_start_time` and `partitioned_end_time.
 *
 * \param full_parameter_block Input full parameter block.
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
    const ParameterBlockObuMetadata& full_parameter_block,
    int32_t partitioned_start_time, int32_t partitioned_end_time,
    std::list<iamf_tools_cli_proto::ParameterSubblock>& partitioned_subblocks,
    uint32_t& constant_subblock_duration) {
  LOG_FIRST_N(INFO, 1) << "   full_parameter_block=\n" << full_parameter_block;

  int32_t current_time = full_parameter_block.start_timestamp();

  // Track that the split subblocks cover the whole partition.
  int32_t total_covered_duration = 0;

  // Loop through all subblocks in the original Parameter Block.
  const auto num_subblocks = full_parameter_block.num_subblocks();
  for (int i = 0; i < num_subblocks; ++i) {
    // Get the start and end time of this subblock.
    const int32_t subblock_start_time = current_time;

    // The partitioner works directly on the parameter block OBU metadata and
    // assumes all needed information (e.g. subblock duration) is in the
    // metadata themselves and does not support getting the information from
    // parameter definitions (i.e. parameter definition mode == 0).
    constexpr uint8_t kParamDefinitionModeOne = 1;
    const auto subblock_duration = GetParameterSubblockDuration<uint32_t>(
        i, full_parameter_block.num_subblocks(),
        full_parameter_block.constant_subblock_duration(),
        full_parameter_block.duration(), kParamDefinitionModeOne,
        [&full_parameter_block](int i) {
          return full_parameter_block.subblocks(i).subblock_duration();
        },
        [](int i) {
          return absl::InvalidArgumentError(
              "Parameter Block Partitioner does not support the case where "
              "`param_definition_mode == 0");
        });
    if (!subblock_duration.ok()) {
      return subblock_duration.status();
    }
    const int32_t subblock_end_time =
        subblock_start_time + static_cast<int32_t>(*subblock_duration);
    current_time = subblock_end_time;

    if (subblock_end_time <= partitioned_start_time ||
        partitioned_end_time <= subblock_start_time) {
      // The subblock ends before the partition or starts after. It can't
      // overlap.
      continue;
    }

    // Found an overlapping subblock. Create a new one for the partition that
    // represents the overlapped time.
    iamf_tools_cli_proto::ParameterSubblock partitioned_subblock;
    const int partitioned_subblock_start =
        std::max(partitioned_start_time, subblock_start_time);
    const int partitioned_subblock_end =
        std::min(partitioned_end_time, subblock_end_time);
    uint32_t partitioned_subblock_duration =
        partitioned_subblock_end - partitioned_subblock_start;
    total_covered_duration += partitioned_subblock_duration;
    partitioned_subblock.set_subblock_duration(partitioned_subblock_duration);

    const auto& subblock_i = full_parameter_block.subblocks(i);
    if (subblock_i.has_mix_gain_parameter_data()) {
      // Mix Gain animated parameters need to be partitioned.
      RETURN_IF_NOT_OK(PartitionMixGain(
          subblock_i.mix_gain_parameter_data(), subblock_start_time,
          subblock_end_time, partitioned_subblock_start,
          partitioned_subblock_end, partitioned_subblock));
    } else if (subblock_i.has_demixing_info_parameter_data()) {
      if (!partitioned_subblocks.empty()) {
        return absl::InvalidArgumentError(
            "There should only be one subblock for demixing info.");
      }
      *partitioned_subblock.mutable_demixing_info_parameter_data() =
          subblock_i.demixing_info_parameter_data();
    } else if (subblock_i.has_recon_gain_info_parameter_data()) {
      if (!partitioned_subblocks.empty()) {
        return absl::InvalidArgumentError(
            "There should only be one subblock for recon gain info.");
      }
      *partitioned_subblock.mutable_recon_gain_info_parameter_data() =
          subblock_i.recon_gain_info_parameter_data();
    } else {
      return absl::InvalidArgumentError("Unknown subblock type.");
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
  std::vector<uint32_t> subblock_durations;
  subblock_durations.reserve(partitioned_subblocks.size());
  for (const auto& subblock : partitioned_subblocks) {
    subblock_durations.push_back(subblock.subblock_duration());
  }

  constant_subblock_duration =
      ParameterBlockPartitioner::FindConstantSubblockDuration(
          subblock_durations);

  return absl::OkStatus();
}

}  // namespace

uint32_t ParameterBlockPartitioner::FindConstantSubblockDuration(
    const std::vector<uint32_t>& subblock_durations) {
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

absl::Status ParameterBlockPartitioner::FindPartitionDuration(
    const iamf_tools_cli_proto::ProfileVersion primary_profile,
    const iamf_tools_cli_proto::CodecConfigObuMetadata&
        codec_config_obu_metadata,
    uint32_t& partition_duration) {
  using enum iamf_tools_cli_proto::ProfileVersion;
  if (primary_profile != PROFILE_VERSION_SIMPLE &&
      primary_profile != PROFILE_VERSION_BASE) {
    // This function only implements limitations described in IAMF V1 are for
    // simple and base profile.
    return absl::InvalidArgumentError(absl::StrCat(
        "FindPartitionDuration() only works with Simple or Base profile"));
  }

  // TODO(b/283281856): Set the duration to a different value when
  //                    `parameter_rate != sample rate`.
  partition_duration =
      codec_config_obu_metadata.codec_config().num_samples_per_frame();
  return absl::OkStatus();
}

absl::Status ParameterBlockPartitioner::PartitionParameterBlock(
    const ParameterBlockObuMetadata& full_parameter_block,
    int32_t partitioned_start_time, int32_t partitioned_end_time,
    ParameterBlockObuMetadata& partitioned) {
  if (partitioned_start_time >= partitioned_end_time) {
    return absl::InvalidArgumentError(
        absl::StrCat("Cannot partition a parameter block with < 1 duration. "
                     "(partitioned_start_time= ",
                     partitioned_start_time,
                     " partitioned_end_time=", partitioned_end_time));
  }

  // Find the subblocks that overlap this partition.
  std::list<iamf_tools_cli_proto::ParameterSubblock> partitioned_subblocks;

  uint32_t constant_subblock_duration;
  RETURN_IF_NOT_OK(GetPartitionedSubblocks(
      full_parameter_block, partitioned_start_time, partitioned_end_time,
      partitioned_subblocks, constant_subblock_duration));

  // Create the partitioned parameter block OBU metadata. The first few fields
  // are always the same as the full metadata.
  partitioned.set_parameter_id(full_parameter_block.parameter_id());
  partitioned.set_duration(
      static_cast<uint32_t>(partitioned_end_time - partitioned_start_time));
  partitioned.set_num_subblocks(partitioned_subblocks.size());
  partitioned.set_constant_subblock_duration(constant_subblock_duration);
  *partitioned.mutable_obu_header() = full_parameter_block.obu_header();
  for (const auto& subblock : partitioned_subblocks) {
    *partitioned.add_subblocks() = subblock;
  }
  partitioned.set_start_timestamp(partitioned_start_time);

  return absl::OkStatus();
}

absl::Status ParameterBlockPartitioner::PartitionFrameAligned(
    uint32_t partition_duration,
    const ParameterBlockObuMetadata& full_parameter_block,
    std::list<ParameterBlockObuMetadata>& partitioned_parameter_blocks) {
  // Partition this parameter block into several blocks with the same duration.
  const int32_t end_timestamp =
      full_parameter_block.start_timestamp() + full_parameter_block.duration();
  for (int32_t t = full_parameter_block.start_timestamp(); t < end_timestamp;
       t += partition_duration) {
    LOG(INFO) << "Partitioning parameter blocks at timestamp= " << t;
    ParameterBlockObuMetadata partitioned_parameter_block;
    RETURN_IF_NOT_OK(PartitionParameterBlock(full_parameter_block, t,
                                             t + partition_duration,
                                             partitioned_parameter_block));
    partitioned_parameter_blocks.push_back(partitioned_parameter_block);
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
