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

#ifndef CLI_MIX_PRESENTATION_FINALIZER_H_
#define CLI_MIX_PRESENTATION_FINALIZER_H_

#include <cstdint>
#include <list>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/obu/mix_presentation.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

class MixPresentationFinalizerBase {
 public:
  /*\!brief Constructor.
   *
   * \param mix_presentation_metadata Input mix presentation metadata.
   */
  explicit MixPresentationFinalizerBase(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::MixPresentationObuMetadata>&
          mix_presentation_metadata)
      : mix_presentation_metadata_(mix_presentation_metadata) {}

  /*\!brief Destructor.
   */
  virtual ~MixPresentationFinalizerBase() = default;

  /*\!brief Copies over user provided integrated loudness and peak values.
   *
   * \param user_loudness User provided loudness information.
   * \param output_loudness Output loudness information with `info_type`
   *     initialized.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status CopyUserIntegratedLoudnessAndPeaks(
      const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
      LoudnessInfo& output_loudness);

  /*\!brief Copies over user provided anchored loudness.
   *
   * \param user_loudness User provided loudness information.
   * \param output_loudness Output loudness information with `info_type`
   *     initialized.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status CopyUserAnchoredLoudness(
      const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
      LoudnessInfo& output_loudness);

  /*\!brief Copies over user provided layout extension.
   *
   * \param user_loudness User provided loudness information.
   * \param output_loudness Output loudness information with `info_type`
   *     initialized.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status CopyUserLayoutExtension(
      const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
      LoudnessInfo& output_loudness);

  /*\!brief Finalizes the list of Mix Presentation OBUs.
   *
   * Populates the loudness information for each Mix Presentation OBU.
   *
   * \param audio_elements Input Audio Element OBUs with data.
   * \param id_to_time_to_labeled_frame Data structure of samples, keyed by
   *     audio element ID, starting timestamp, and channel label.
   * \param parameter_blocks Input Parameter Block OBUs.
   * \param mix_presentation_obus Output list of OBUs to finalize.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status Finalize(
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      std::list<MixPresentationObu>& mix_presentation_obus) = 0;

 protected:
  const ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::MixPresentationObuMetadata>
      mix_presentation_metadata_;
};

class DummyMixPresentationFinalizer : public MixPresentationFinalizerBase {
 public:
  explicit DummyMixPresentationFinalizer(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::MixPresentationObuMetadata>&
          mix_presentation_metadata)
      : MixPresentationFinalizerBase(mix_presentation_metadata) {}

  /*\!brief Destructor.
   */
  ~DummyMixPresentationFinalizer() override = default;

  /*\!brief Finalizes the list of Mix Presentation OBUs.
   *
   * Ignores most inputs and just copies user provided values over.
   *
   * \param audio_elements Ignored input Audio Element OBUs with data.
   * \param id_to_time_to_labeled_frame Ignored data structure of samples.
   * \param parameter_blocks Ignored input Parameter Block OBUs.
   * \param mix_presentation_obus Output list of OBUs to finalize.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Finalize(
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      std::list<MixPresentationObu>& mix_presentation_obus) override;
};

}  // namespace iamf_tools

#endif  // CLI_MIX_PRESENTATION_FINALIZER_H_
