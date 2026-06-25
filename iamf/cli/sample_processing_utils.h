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

#ifndef CLI_SAMPLE_PROCESSING_UTILS_H_
#define CLI_SAMPLE_PROCESSING_UTILS_H_

#include <cstddef>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Arranges the input samples to render.
 *
 * \param labeled_frame Input labeled frame.
 * \param ordered_labels Arrangement of the output channels.
 * \param empty_channel Default samples to use when the requested channel
 *        is not found.
 * \param trimming_settings Settings to enable/disable trimming at start/end.
 * \param samples_to_render Output samples to render in (channel, time) axes.
 *        Samples which should be trimmed are omitted from the output.
 * \param num_valid_ticks Number of valid time ticks in the returned
 *        `samples_to_render`, which is the length of the time-axis.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status ArrangeSamples(
    const LabeledFrame& labeled_frame,
    absl::Span<const ChannelLabel::Label> ordered_labels,
    absl::Span<const InternalSampleType> empty_channel,
    TrimmingSettings trimming_settings,
    std::vector<absl::Span<const InternalSampleType>>& samples_to_render,
    size_t& num_valid_ticks);

}  // namespace iamf_tools

#endif  // CLI_SAMPLE_PROCESSING_UTILS_H_
