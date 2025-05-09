/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_RENDERER_RENDERER_UTILS_H_
#define CLI_RENDERER_RENDERER_UTILS_H_
#include <cstddef>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace renderer_utils {

/*!\brief Arranges the samples to be rendered in (channel, time) axes.
 *
 * \param labeled_frame Labeled frame determine which original or demixed
 *        samples to trim and render.
 * \param ordered_labels Ordered list of original labels.
 * \param empty_channel Vector of an all-zero channel. All output spans of
 *        channels corresponding to missing labels
 *        (`ChannelLabel::Label::kOmitted`) will point to this vector.
 * \param samples_to_render Output samples to render in (channel, time) axes.
 *        Samples which should be trimmed are omitted from the output.
 * \param num_valid_ticks Number of valid time ticks in the returned
 *        `samples_to_render`, which is the length of the time-axis.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status ArrangeSamplesToRender(
    const LabeledFrame& labeled_frame,
    const std::vector<ChannelLabel::Label>& ordered_labels,
    const std::vector<InternalSampleType>& empty_channel,
    std::vector<absl::Span<const InternalSampleType>>& samples_to_render,
    size_t& num_valid_ticks);

/*!\brief Gets a key associated with the playback layout.
 *
 * The output key is the layout name of sound systems described in [ITU2051-3],
 * e.g. "0+2+0", "4+7+0".
 *
 * \param output_layout Layout to get key from.
 * \return Key associated with the layout. Or a specific status on failure.
 */
absl::StatusOr<std::string> LookupOutputKeyFromPlaybackLayout(
    const Layout& output_layout);

}  // namespace renderer_utils

}  // namespace iamf_tools
#endif  // CLI_RENDERER_RENDERER_UTILS_H_
