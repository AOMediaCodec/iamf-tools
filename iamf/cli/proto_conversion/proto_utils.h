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

#ifndef CLI_PROTO_CONVERSION_PROTO_UTILS_H_
#define CLI_PROTO_CONVERSION_PROTO_UTILS_H_

#include <memory>

#include "absl/status/status.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/common/leb_generator.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

/*!\brief Copies param definitions from the corresponding protocol buffer.
 *
 * \param input_param_definition Input protocol buffer.
 * \param param_definition Destination param definition.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status CopyParamDefinition(
    const iamf_tools_cli_proto::ParamDefinition& input_param_definition,
    ParamDefinition& param_definition);

/*!\brief Returns an `ObuHeader` based on the corresponding protocol buffer.
 *
 * \param input_obu_header Input protocol buffer.
 * \return Result.
 */
ObuHeader GetHeaderFromMetadata(
    const iamf_tools_cli_proto::ObuHeaderMetadata& input_obu_header);

/*!\brief Copies `DemixingInfoParameterData` from the input protocol buffer.
 *
 * \param input_demixing_info_parameter_data Input protocol buffer.
 * \param obu_demixing_param_data Reference to the result.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status CopyDemixingInfoParameterData(
    const iamf_tools_cli_proto::DemixingInfoParameterData&
        input_demixing_info_parameter_data,
    DemixingInfoParameterData& obu_demixing_param_data);

/*!\brief Copies `DMixPMode` to the output protocol buffer.
 *
 * \param obu_dmixp_mode Input `DMixPMode`.
 * \param dmixp_mode Reference to output protocol buffer.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status CopyDMixPMode(DemixingInfoParameterData::DMixPMode obu_dmixp_mode,
                           iamf_tools_cli_proto::DMixPMode& dmixp_mode);

/*!\brief Creates a `LebGenerator` based on the input config.
 *
 * \param user_config from a UserMetadata proto.
 * \return `LebGenerator` on success. `nullptr` if the config is invalid.
 */
std::unique_ptr<LebGenerator> CreateLebGenerator(
    const iamf_tools_cli_proto::Leb128Generator& user_config);

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_PROTO_UTILS_H_
