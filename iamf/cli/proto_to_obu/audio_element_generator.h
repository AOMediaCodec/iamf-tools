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

#ifndef CLI_PROTO_TO_OBU_AUDIO_ELEMENT_GENERATOR_H_
#define CLI_PROTO_TO_OBU_AUDIO_ELEMENT_GENERATOR_H_

#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

class AudioElementGenerator {
 public:
  /*!\brief Constructor.
   * \param audio_element_metadata Input audio element metadata.
   */
  AudioElementGenerator(const ::google::protobuf::RepeatedPtrField<
                        iamf_tools_cli_proto::AudioElementObuMetadata>&
                            audio_element_metadata)
      : audio_element_metadata_(audio_element_metadata) {}

  /*!\brief Populates metadata about the layout config into the output params.
   *
   * \param audio_substream_ids Ordered list of substream IDs in the OBU.
   * \param config Scalable channel layout config to process.
   * \param substream_id_to_labels `audio_substream_id` to output label map.
   * \param label_to_output_gain Output param populated by this function.
   * \param channel_numbers_for_layers Output param populated by this function.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status FinalizeScalableChannelLayoutConfig(
      const std::vector<DecodedUleb128>& audio_substream_ids,
      const ScalableChannelLayoutConfig& config,
      SubstreamIdLabelsMap& substream_id_to_labels,
      LabelGainMap& label_to_output_gain,
      std::vector<ChannelNumbers>& channel_numbers_for_layers);

  /*!\brief Populates substream_id_to_labels for the ambisonics config.
   *
   * \param audio_element_obu Mono ambisonics config OBU to process.
   * \param substream_id_to_labels Output map of substream IDs to labels.
   * \return `absl::OkStatus()` on success. An error if the input OBU is not an
   *         ambisonics config. A specific status on failure.
   */
  static absl::Status FinalizeAmbisonicsConfig(
      const AudioElementObu& audio_element_obu,
      SubstreamIdLabelsMap& substream_id_to_labels);

  /*!\brief Generates a list of Audio Element OBUs from the input metadata.
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

#endif  // CLI_PROTO_TO_OBU_AUDIO_ELEMENT_GENERATOR_H_
