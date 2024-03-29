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
#ifndef CLI_PARAMETER_BLOCK_PARTITIONER_H_
#define CLI_PARAMETER_BLOCK_PARTITIONER_H_

#include <list>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/parameter_block.h"

namespace iamf_tools {

class ParameterBlockPartitioner {
 public:
  /*\!brief Constructor.
   * \param primary_profile Input primary profile version.
   */
  ParameterBlockPartitioner(ProfileVersion primary_profile)
      : primary_profile_(primary_profile) {}

  /*\!brief Finds the `constant_subblock_duration` for the input durations.
   *
   * \param subblock_durations Vector of subblock durations.
   * \return `constant_subblock_duration` which results in the best bit-rate.
   */
  static DecodedUleb128 FindConstantSubblockDuration(
      const std::vector<DecodedUleb128>& subblock_durations);

  /*\!brief Partitions the input parameter block into a smaller one.
   *
   * \param full_parameter_block Input full parameter block.
   * \param partitioned_start_time Start time of the output partitioned
   *     parameter block.
   * \param partitioned_end_time End time of the output partitioned parameter
   *     block.
   * \param per_id_metadata Per-ID parameter metadata.
   * \param partitioned_parameter_block Output partitioned parameter block which
   *     must be destroyed later only if this function was successful.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *     `partitioned_start_time >= partitioned_end_time`.
   *     `absl::InvalidArgumentError()` if the block cannot be partitioned to
   *     fully cover the interval or partitioned to fulfill restrictions.
   */
  absl::Status PartitionParameterBlock(
      const ParameterBlockWithData& full_parameter_block,
      int partitioned_start_time, int partitioned_end_time,
      PerIdParameterMetadata& per_id_metadata,
      ParameterBlockWithData& partitioned_parameter_block) const;

  /*\!brief Partitions the input parameter block into frame-aligned ones.
   *
   * \param partition_duration Duration of each partitioned parameter block.
   * \param full_parameter_block Input full parameter block.
   * \param per_id_metadata Per-ID parameter metadata.
   * \param parameter_blocks Output list to append partitioned parameter
   *     blocks to. Any parameter blocks added must be destroyed later.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PartitionFrameAligned(
      DecodedUleb128 partition_duration,
      ParameterBlockWithData& parameter_block_with_data,
      PerIdParameterMetadata& per_id_metadata,
      std::list<ParameterBlockWithData>& parameter_blocks) const;

 private:
  const ProfileVersion primary_profile_;
};

}  // namespace iamf_tools
#endif  // CLI_PARAMETER_BLOCK_PARTITIONER_H_
