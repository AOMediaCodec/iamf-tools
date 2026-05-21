/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/obu/tests/obu_test_utils.h"

#include <cstdint>
#include <numeric>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/types/span.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

AmbisonicsConfig MakeFullOrderAmbisonicsMonoConfig(int order) {
  // The IAMF spec imposes a maximum valid Ambisonics order of 14, i.e. 225
  // channels.
  ABSL_CHECK_LE(order, 14);
  ABSL_CHECK_GE(order, 0);

  int channel_count = (order + 1) * (order + 1);
  std::vector<uint8_t> channel_mapping(channel_count);
  std::iota(channel_mapping.begin(), channel_mapping.end(), 0);

  return AmbisonicsConfig{.ambisonics_config = *AmbisonicsMonoConfig::Create(
                              channel_count, channel_mapping)};
}

ParamDefinition::BaseArgs MakeScheduleInParameterBlockBaseArgs(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate) {
  return ParamDefinition::BaseArgs{
      .parameter_id = parameter_id,
      .parameter_rate = parameter_rate,
      .param_definition_mode = ParamDefinition::kModeScheduleInParameterBlock,
  };
}

ParamDefinition::BaseArgs MakeOneSubblockParamDefinitionBaseArgs(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration) {
  return MakeConstantSubblocksParamDefinitionBaseArgs(
      parameter_id, parameter_rate, duration, duration);
}

ParamDefinition::BaseArgs MakeConstantSubblocksParamDefinitionBaseArgs(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration) {
  return ParamDefinition::BaseArgs{
      .parameter_id = parameter_id,
      .parameter_rate = parameter_rate,
      .param_definition_mode = ParamDefinition::kModeScheduleInParamDefinition,
      .duration = duration,
      .constant_subblock_duration = constant_subblock_duration,
  };
}

ParamDefinition::BaseArgs MakeVariableSubblocksParamDefinitionBaseArgs(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    absl::Span<const DecodedUleb128> subblock_durations) {
  // The duration is the sum of all the subblock durations.
  DecodedUleb128 duration = 0;
  for (const auto& subblock_duration : subblock_durations) {
    ABSL_CHECK_OK(
        AddUint32CheckOverflow(duration, subblock_duration, duration));
  }
  return ParamDefinition::BaseArgs{
      .parameter_id = parameter_id,
      .parameter_rate = parameter_rate,
      .param_definition_mode = ParamDefinition::kModeScheduleInParamDefinition,
      .duration = duration,
      .constant_subblock_duration = 0,
      .num_subblocks = static_cast<DecodedUleb128>(subblock_durations.size()),
      .subblock_durations = std::vector<DecodedUleb128>(
          subblock_durations.begin(), subblock_durations.end()),
  };
}

}  // namespace iamf_tools
