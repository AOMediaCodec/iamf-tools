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
#ifndef CLI_PROTO_CONVERSION_PROTO_TO_OBU_METADATA_OBU_GENERATOR_H_
#define CLI_PROTO_CONVERSION_PROTO_TO_OBU_METADATA_OBU_GENERATOR_H_

#include <list>

#include "absl/status/status.h"
#include "iamf/cli/proto/metadata_obu.pb.h"
#include "iamf/obu/metadata_obu.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

class MetadataObuGenerator {
 public:
  /*!\brief Constructor.
   * \param metadata_obu_metadata Input Metadata OBU metadata.
   */
  MetadataObuGenerator(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::MetadataObuMetadata>& metadata_obu_metadata)
      : metadata_obu_metadata_(metadata_obu_metadata) {}

  /*!\brief Generates a list of Metadata OBUs from the input metadata.
   *
   * \param metadata_obus Output list of Metadata OBUs.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Generate(std::list<MetadataObu>& metadata_obus);

 private:
  const ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::MetadataObuMetadata>
      metadata_obu_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_PROTO_TO_OBU_METADATA_OBU_GENERATOR_H_
