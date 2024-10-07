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

#ifndef CLI_PROTO_TO_OBU_CODEC_CONFIG_GENERATOR_H_
#define CLI_PROTO_TO_OBU_CODEC_CONFIG_GENERATOR_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/codec_config.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

class CodecConfigGenerator {
 public:
  /*!\brief Constructor.
   * \param codec_config_metadata Input codec config metadata.
   */
  CodecConfigGenerator(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadata)
      : codec_config_metadata_(codec_config_metadata) {}

  /*!\brief Generates a map of Codec Config OBUs from the input metadata.
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
   * \param codec_config_obus Map of Codec Config ID to generated Codec Config
   *        OBUs.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         invalid values of enumerations are used or if casting input fields
   *         would result in lost information. `kIamfInvalidBitstream` if
   *         `codec_id` is unrecognized.
   */
  absl::Status Generate(
      absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

 private:
  const ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::CodecConfigObuMetadata>
      codec_config_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_PROTO_TO_OBU_CODEC_CONFIG_GENERATOR_H_
