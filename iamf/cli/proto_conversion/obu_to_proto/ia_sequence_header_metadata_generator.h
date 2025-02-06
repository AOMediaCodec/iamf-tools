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

#ifndef CLI_PROTO_CONVERSION_OBU_TO_PROTO_IA_SEQUENCE_HEADER_METADATA_GENERATOR_H_
#define CLI_PROTO_CONVERSION_OBU_TO_PROTO_IA_SEQUENCE_HEADER_METADATA_GENERATOR_H_

#include "absl/status/statusor.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {

/*!\brief Static functions to convert `IaSequenceHeaderObu`s to protos. */
class IaSequenceHeaderMetadataGenerator {
 public:
  /*!\brief Generates a proto representation of an `IaSequenceHeaderObu`
   *
   * \param ia_sequence_header Input `IaSequenceHeaderObu` to convert to a
   *        proto.
   * \return Proto representation of the `IaSequenceHeaderObu` or a specific
   *         error on failure.
   */
  static absl::StatusOr<iamf_tools_cli_proto::IASequenceHeaderObuMetadata>
  Generate(const IASequenceHeaderObu& ia_sequence_header);
};

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_OBU_TO_PROTO_IA_SEQUENCE_HEADER_METADATA_GENERATOR_H_
