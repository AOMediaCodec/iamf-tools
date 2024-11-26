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

#ifndef CLI_OBU_TO_PROTO_OBU_HEADER_METADATA_GENERATOR_H_

#include "absl/status/statusor.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

/*!\brief Static functions to convert `ObuHeader`s to protos. */
class ObuHeaderMetadataGenerator {
 public:
  /*!\brief Generates a proto representation of an `ObuHeader`
   *
   * \param obu_header Input `ObuHeader` to convert to a proto.
   * \return Proto representation of the `ObuHeader` or a specific
   *         error on failure.
   */
  static absl::StatusOr<iamf_tools_cli_proto::ObuHeaderMetadata> Generate(
      const ObuHeader& obu_header);
};

}  // namespace iamf_tools

#endif  // CLI_OBU_TO_PROTO_OBU_HEADER_METADATA_GENERATOR_H_
