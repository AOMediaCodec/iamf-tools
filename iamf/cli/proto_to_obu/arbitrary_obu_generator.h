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
#ifndef CLI_PROTO_TO_OBU_ARBITRARY_OBU_GENERATOR_H_
#define CLI_PROTO_TO_OBU_ARBITRARY_OBU_GENERATOR_H_

#include <list>

#include "absl/status/status.h"
#include "iamf/cli/proto/arbitrary_obu.pb.h"
#include "iamf/obu/arbitrary_obu.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

class ArbitraryObuGenerator {
 public:
  /*!\brief Constructor.
   * \param arbitrary_obu_metadata Input arbitrary OBU metadata.
   */
  ArbitraryObuGenerator(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::ArbitraryObuMetadata>& arbitrary_obu_metadata)
      : arbitrary_obu_metadata_(arbitrary_obu_metadata) {}

  /*!\brief Generates a list of arbitrary OBUs from the input metadata.
   *
   * The generator only performs enough validation required to construct the
   * OBU; it validates that enumeration values are known. It does not validate
   * IAMF requirements or restrictions of the fields which is typically
   * performed in functions of the OBU class.
   *
   * Performing minimal validation allows OBUs which are not compliant with
   * IAMF to be generated. These can be used to create illegal streams for
   * debugging purposes.
   *
   * \param ArbitraryObu Output list of OBUs.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         invalid values of enumerations are used.
   */
  absl::Status Generate(std::list<ArbitraryObu>& arbitrary_obus_with_metadata);

 private:
  const ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::ArbitraryObuMetadata>
      arbitrary_obu_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_PROTO_TO_OBU_ARBITRARY_OBU_GENERATOR_H_
