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
#ifndef CLI_PROTO_CONVERSION_PROTO_TO_OBU_IA_SEQUENCE_HEADER_GENERATOR_H_
#define CLI_PROTO_CONVERSION_PROTO_TO_OBU_IA_SEQUENCE_HEADER_GENERATOR_H_

#include <optional>

#include "absl/status/status.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {

class IaSequenceHeaderGenerator {
 public:
  /*!\brief Constructor.
   * \param user_metadata Input user metadata.
   */
  IaSequenceHeaderGenerator(
      const iamf_tools_cli_proto::IASequenceHeaderObuMetadata&
          ia_sequence_header_metadata)
      : ia_sequence_header_metadata_(ia_sequence_header_metadata) {}

  /*!\brief Generates an IA Sequence Header OBU from the input metadata.
   *
   * The generator only performs enough validation required to construct the
   * OBU; it validates that enumeration values are known and casting of fields
   * does not result in lost information. It does not validate IAMF requirements
   * or restrictions of the fields which is typically performed in functions of
   * the OBU class.
   *
   * Performing minimal validation allows OBUs which are not compliant with
   * IAMF to be generated. These can be used to create illegal streams for
   * debugging purposes.
   *
   * \param ia_sequence_header_obu Output OBU.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         invalid values of enumerations are used.
   */
  absl::Status Generate(
      std::optional<IASequenceHeaderObu>& ia_sequence_header_obu) const;

 private:
  const iamf_tools_cli_proto::IASequenceHeaderObuMetadata
      ia_sequence_header_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_PROTO_TO_OBU_IA_SEQUENCE_HEADER_GENERATOR_H_
