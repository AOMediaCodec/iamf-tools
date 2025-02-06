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

#ifndef CLI_PROTO_CONVERSION_OBU_TO_PROTO_PARAMETER_BLOCK_METADATA_GENERATOR_H_
#define CLI_PROTO_CONVERSION_OBU_TO_PROTO_PARAMETER_BLOCK_METADATA_GENERATOR_H_

#include "absl/status/statusor.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"

namespace iamf_tools {

/*!\brief Static functions to convert parameter blocks and related to protos. */
class ParameterBlockMetadataGenerator {
 public:
  /*!\brief Generates a proto representation of a `ParameterSubblock`
   *
   * \param param_definition_type Type of the parameter subblock.
   * \param parameter_subblock Input parameter subblock to convert to a proto.
   * \return Proto representation of the parameter subblock or a specific
   *         error on failure.
   */
  static absl::StatusOr<iamf_tools_cli_proto::ParameterSubblock>
  GenerateParameterSubblockMetadata(
      ParamDefinition::ParameterDefinitionType param_definition_type,
      const ParameterSubblock& parameter_subblock);
};

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_OBU_TO_PROTO_PARAMETER_BLOCK_METADATA_GENERATOR_H_
