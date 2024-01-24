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

#ifndef CLI_MIX_PRESENTATION_GENERATOR_H_
#define CLI_MIX_PRESENTATION_GENERATOR_H_

#include <cstdint>
#include <list>

#include "absl/status/status.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/mix_presentation.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

class MixPresentationGenerator {
 public:
  /*\!brief Copies the info type from the corresponding protocol buffer.
   *
   * \param input_loudness_info Input protocol buffer.
   * \param loudness_info_type Result.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status CopyInfoType(
      const iamf_tools_cli_proto::LoudnessInfo& input_loudness_info,
      uint8_t& loudness_info_type);

  /*\!brief Constructor.
   * \param mix_presentation_metadata Input mix presentation metadata.
   */
  MixPresentationGenerator(const ::google::protobuf::RepeatedPtrField<
                           iamf_tools_cli_proto::MixPresentationObuMetadata>&
                               mix_presentation_metadata)
      : mix_presentation_metadata_(mix_presentation_metadata) {}

  /*\!brief Generates a list of Mix Presentation OBUs from the input metadata.
   *
   * Note that `finalize_mix_presentation_obus` must be called afterwards to
   * populate the loudness information for the OBUs.
   *
   * \param mix_presentation_obus Output list of OBUs.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Generate(std::list<MixPresentationObu>& mix_presentation_obus);

 private:
  const ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::MixPresentationObuMetadata>
      mix_presentation_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_MIX_PRESENTATION_GENERATOR_H_
