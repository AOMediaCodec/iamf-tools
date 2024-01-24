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

#ifndef CLI_RECON_GAIN_GENERATOR_H_
#define CLI_RECON_GAIN_GENERATOR_H_

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "iamf/cli/demixing_module.h"

namespace iamf_tools {

class ReconGainGenerator {
 public:
  /*\!brief Constructor.
   * \param id_to_time_to_labeled_frame Data structure for samples.
   * \param id_to_time_to_labeled_decoded_frame Data structure for decoded
   *     samples.
   */
  ReconGainGenerator(
      const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
      const IdTimeLabeledFrameMap& id_to_time_to_labeled_decoded_frame)
      : id_to_time_to_labeled_frame_(id_to_time_to_labeled_frame),
        id_to_time_to_labeled_decoded_frame_(
            id_to_time_to_labeled_decoded_frame),
        additional_logging_(true) {}

  /*\!brief Computes the recon gain for the input channel.
   *
   * \param label Label of the channel to compute.
   * \param audio_element_id Audio Element ID.
   * \param start_timestamp Start timestamp of the frame to compute recon gain
   *     for.
   * \param recon_gain Result in the range [0, 1].
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ComputeReconGain(const std::string& label,
                                uint32_t audio_element_id,
                                int32_t start_timestamp,
                                double& recon_gain) const;

  /*\!brief Gets the additional logging flag.
   *
   * \return Whether to enable additional logging.
   */
  bool additional_logging() const { return additional_logging_; }

  /*\!brief Sets the additional logging flag.
   *
   * \param additional_logging Whether to enable additional logging.
   */
  void set_additional_logging(bool additional_logging) {
    additional_logging_ = additional_logging;
  }

 private:
  const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame_;
  const IdTimeLabeledFrameMap& id_to_time_to_labeled_decoded_frame_;
  bool additional_logging_;
};

}  // namespace iamf_tools

#endif  // CLI_RECON_GAIN_GENERATOR_H_
