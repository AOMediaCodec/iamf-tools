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

#ifndef CLI_AUDIO_ELEMENT_GENERATOR_H_
#define CLI_AUDIO_ELEMENT_GENERATOR_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/codec_config.h"
#include "iamf/ia.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

class AudioElementGenerator {
 public:
  /*\!brief Constructor.
   * \param audio_element_metadata Input audio element metadata.
   */
  AudioElementGenerator(const ::google::protobuf::RepeatedPtrField<
                        iamf_tools_cli_proto::AudioElementObuMetadata>&
                            audio_element_metadata)
      : audio_element_metadata_(audio_element_metadata) {}

  /*\!brief Generates a list of Audio Element OBUs from the input metadata.
   *
   * \param codec_conifgs Map of Codec Config IDs to Codec Config OBUs.
   * \param audio_elements Map of Audio Element IDs to generated OBUs with data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Generate(
      const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_configs,
      absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements);

 private:
  const ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::AudioElementObuMetadata>
      audio_element_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_ELEMENT_GENERATOR_H_
