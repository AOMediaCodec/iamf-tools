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
#ifndef CLI_INTERNAL_RENDERER_LOUDSPEAKERS_RENDERER_H_
#define CLI_INTERNAL_RENDERER_LOUDSPEAKERS_RENDERER_H_

#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Looks up precomputed gains associated with the input/output layouts.
 *
 * \param input_key Key representing the input loudspeaker layout.
 * \param output_key Key representing the output loudspeaker layout.
 * \return Precomputed gains on success. A specific status on failure.
 */
absl::StatusOr<std::vector<std::vector<double>>> LookupPrecomputedGains(
    absl::string_view input_key, absl::string_view output_key);

/*!\brief Renders channel-based samples to loudspeaker channels.
 *
 * \param input_samples Input samples to render.
 * \param down_mixing_params Down-mixing parameters.
 * \param channel_labels Labels of input channels.
 * \param input_key Key representing the input loudspeaker layout.
 * \param output_key Key representing the output loudspeaker layout.
 * \param gains Gains matrix to apply to the output.
 * \param rendered_samples Output rendered samples.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status RenderChannelLayoutToLoudspeakers(
    absl::Span<const std::vector<InternalSampleType>>& input_samples,
    const DownMixingParams& down_mixing_params,
    const std::vector<ChannelLabel::Label>& channel_labels,
    absl::string_view input_key, absl::string_view output_key,
    const std::vector<std::vector<double>>& gains,
    std::vector<InternalSampleType>& rendered_samples);

/*!\brief Renders ambisonics samples to loudspeaker channels.
 *
 * \param input_samples Input samples to render.
 * \param ambisonics_config Config for the ambisonics layout.
 * \param gains Gains matrix to apply to the output.
 * \param rendered_samples Output rendered samples.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status RenderAmbisonicsToLoudspeakers(
    absl::Span<const std::vector<InternalSampleType>>& input_samples,
    const AmbisonicsConfig& ambisonics_config,
    const std::vector<std::vector<double>>& gains,
    std::vector<InternalSampleType>& rendered_samples);

}  // namespace iamf_tools

#endif  // CLI_INTERNAL_RENDERER_LOUDSPEAKERS_RENDERER_H_
